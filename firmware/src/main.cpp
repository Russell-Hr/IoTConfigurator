#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>
#include <ESPmDNS.h>
#include "../include/CONFIG_TEMPLATE.h"

#ifndef CONFIG_VERSION
#define CONFIG_VERSION 18
#endif

#ifndef HEATCONTROL_TEST_MODE
#define HEATCONTROL_TEST_MODE 0
#endif

#define STORAGE_NAMESPACE "heatcfg" // V0.11 compact storage; read-only migration source
#define STORAGE_SLOT0 "heatcfg0"
#define STORAGE_SLOT1 "heatcfg1"
#define STORAGE_META "heatmeta"
#define LEGACY_NAMESPACE "heatcontrol"

// HEATCONTROL V0.11 - added simultaneous AP+STA Wi-Fi (device keeps its own AP as a local
// fallback and can also join the home Wi-Fi), a per-chip network ID (mDNS + AP SSID),
// an editable display name, and an unauthenticated /api/discover endpoint + PUT
// /api/system/wifi for provisioning from the companion Android app (see docs/REVIEW_REPORT.md
// V0.8 section). V0.7 added HTTP Basic Auth on the whole web UI + API. V0.6 added a
// name-recovery fallback for a corrupted/empty NVS name field. V0.5 fixed a struct-member
// typo (compile error) and a missing loop increment (infinite loop in jsonChannel).
// Development firmware. Do not connect mains loads until relay logic, isolation,
// ratings and fail-safe behavior have been verified by a qualified electrician.

enum ActuatorType { ACTUATOR_VALVE=0, ACTUATOR_HEATER=1 };
enum OperatingMode { MODE_AUTO=0, MODE_COMFORT=1, MODE_ECONOMY=2, MODE_OFF=3, MODE_MANUAL=4 };
enum DayType { DAY_WORKING=0, DAY_HOLIDAY=1 };
enum ValveState { VALVE_STOPPED=0, VALVE_OPENING=1, VALVE_CLOSING=2 };
enum ValveFeedbackMode { VALVE_FEEDBACK_TIME=0, VALVE_FEEDBACK_LIMITS=1 };
enum ServoOperationMode { SERVO_OPERATION_REGULATOR=0, SERVO_OPERATION_VALVE=1 };
enum ScheduleTransitionType { SCHEDULE_START=0, SCHEDULE_REACH=1 };
enum TemplateLogic { TEMPLATE_LOGIC_START=0, TEMPLATE_LOGIC_REACH=1 };
enum AuxPostReachPolicy { AUX_POST_OFF=0, AUX_POST_MINUTES=1, AUX_POST_TO_COMFORT=2 };

// V0.18.3: servo operation mode (REGULATOR angle/intermediate vs VALVE open/closed)
// V0.18.2: normalized relative-flow characteristic for a typical round-port
// motorized ball valve used in heating. The source description is expressed
// as valve angle 0..90 degrees -> relative flow capacity 0..100%.
// This is an engineering control model, not a manufacturer Cv curve.
static constexpr uint8_t BALL_VALVE_FLOW_ANGLE_DEG[] = {0,10,20,30,40,50,60,70,80,90};
static constexpr uint8_t BALL_VALVE_FLOW_PERCENT[] = {0,0,2,10,30,55,72,85,96,100};
static constexpr size_t BALL_VALVE_FLOW_POINTS = sizeof(BALL_VALVE_FLOW_ANGLE_DEG)/sizeof(BALL_VALVE_FLOW_ANGLE_DEG[0]);

float ballValveFlowPercent(uint8_t mechanicalPosition){
  if(mechanicalPosition>=100)return 100.0f;
  float angle=(float)mechanicalPosition*0.9f; // 0..100% mechanical travel == 0..90 degrees
  if(angle<=BALL_VALVE_FLOW_ANGLE_DEG[0])return BALL_VALVE_FLOW_PERCENT[0];
  for(size_t i=1;i<BALL_VALVE_FLOW_POINTS;i++){
    float a0=BALL_VALVE_FLOW_ANGLE_DEG[i-1], a1=BALL_VALVE_FLOW_ANGLE_DEG[i];
    if(angle<=a1){
      float f0=BALL_VALVE_FLOW_PERCENT[i-1], f1=BALL_VALVE_FLOW_PERCENT[i];
      float t=(angle-a0)/(a1-a0);
      return f0+(f1-f0)*t;
    }
  }
  return 100.0f;
}
struct ScheduleSlot { uint8_t hour, minute; OperatingMode mode; bool enabled; ScheduleTransitionType transitionType; };
struct LegacyScheduleSlot { uint8_t hour, minute; OperatingMode mode; bool enabled; };
struct LegacyDaySchedule { LegacyScheduleSlot slots[6]; };
struct LegacyWeekTemplate { LegacyDaySchedule days[7]; };
struct DaySchedule { ScheduleSlot slots[6]; };
struct WeekTemplate { DaySchedule days[7]; };
struct ChannelConfig {
  char name[32]; ActuatorType actuator; OperatingMode mode;
  float comfortTemperature, economyTemperature, hysteresis; uint8_t sensorIndex;
  uint8_t sensorRom[8]; uint32_t valveOpenTime, valveCloseTime; ValveFeedbackMode valveFeedback;
  DayType dayType[7]; WeekTemplate working; WeekTemplate holiday;
};
// V0.12 binary layout, kept only for one-time migration to the V0.13 template model.
struct OldDayScheduleV12 { DayType type; LegacyScheduleSlot slots[6]; };
struct OldChannelConfigV13 {
  char name[32]; ActuatorType actuator; OperatingMode mode;
  float comfortTemperature, economyTemperature, hysteresis; uint8_t sensorIndex;
  uint8_t sensorRom[8]; uint32_t valveOpenTime, valveCloseTime;
  DayType dayType[7]; LegacyWeekTemplate working; LegacyWeekTemplate holiday;
};
struct OldChannelConfigV12 {
  char name[32]; ActuatorType actuator; OperatingMode mode;
  float comfortTemperature, economyTemperature, hysteresis; uint8_t sensorIndex;
  uint8_t sensorRom[8]; uint32_t valveOpenTime, valveCloseTime; OldDayScheduleV12 calendar[7];
};
struct OldChannelConfigV14 {
  char name[32]; ActuatorType actuator; OperatingMode mode;
  float comfortTemperature, economyTemperature, hysteresis; uint8_t sensorIndex;
  uint8_t sensorRom[8]; uint32_t valveOpenTime, valveCloseTime; ValveFeedbackMode valveFeedback;
  DayType dayType[7]; LegacyWeekTemplate working; LegacyWeekTemplate holiday;
};
struct OldChannelConfigV16 {
  char name[32]; ActuatorType actuator; OperatingMode mode;
  float comfortTemperature, economyTemperature, hysteresis; uint8_t sensorIndex;
  uint8_t sensorRom[8]; uint32_t valveOpenTime, valveCloseTime; ValveFeedbackMode valveFeedback;
  DayType dayType[7]; LegacyWeekTemplate working; LegacyWeekTemplate holiday;
};
struct ReachLearningSample {
  float initialIndoor;
  float target;
  float outdoor;
  float minutes;
};
struct ReachLearningStats {
  uint32_t samples;
  float globalMinutesPerDegree;
  uint32_t binSamples[5];
  float binMinutesPerDegree[5];
  ReachLearningSample history[12];
};
struct ChannelRuntime {
  float temperature; bool sensorOK; bool outputState; uint8_t valvePosition; bool positionKnown; bool calibrating;
  uint8_t calibrationInitialPosition; ValveState calibrationDirection; unsigned long calibrationStartedAt;
  uint32_t calibrationOpenCandidate; uint32_t calibrationCloseCandidate; bool calibrationHasOpen; bool calibrationHasClose;
  ValveState valveState; bool valveFault; bool openLimit; bool closeLimit; uint8_t valveTargetPosition; bool valveTargetActive;
  unsigned long movementStarted; uint8_t movementStartPosition; unsigned long lastOutputChange; bool limitHardwareFault;
  uint32_t reachCacheDateKey; int reachStartMinutes[12]; uint8_t reachCacheValidMask; uint8_t reachActivatedMask;
  bool manualOverrideActive; OperatingMode manualOverrideMode; uint32_t manualOverrideDateKey; uint8_t manualOverrideEventSlot; bool autoInverted;
  // Sensor-loss recovery: remember the last safe temperature-controlled mode and AUTO inversion.
  bool sensorRecoveryPending; OperatingMode sensorRecoveryMode; bool sensorRecoveryAutoInverted; bool sensorRecoveryUserOverride; uint8_t sensorRecoveryGoodReads;
  bool reachLearningActive; uint8_t reachLearningSlot; uint32_t reachLearningDateKey; int reachLearningDeadlineMinutes; unsigned long reachLearningStartedAt; float reachLearningInitialTemp; float reachLearningOutdoor; float reachLearningTarget;
};

OneWire oneWire(ONE_WIRE_PIN); DallasTemperature sensors(&oneWire); RTC_DS3231 rtc; Preferences preferences; WebServer server(80);
ChannelConfig channels[MAX_CHANNELS]; ChannelRuntime runtime[MAX_CHANNELS]; DeviceAddress sensorAddresses[MAX_SENSORS]; uint8_t sensorCount=0; uint8_t outdoorSensorRom[8]={0}; float outdoorTemperature=NAN; bool outdoorSensorOK=false;
ReachLearningStats reachLearning[MAX_CHANNELS]; TemplateLogic templateLogic[MAX_CHANNELS]={TEMPLATE_LOGIC_START,TEMPLATE_LOGIC_START,TEMPLATE_LOGIC_START,TEMPLATE_LOGIC_START,TEMPLATE_LOGIC_START,TEMPLATE_LOGIC_START}; constexpr const char* REACH_LEARN_NAMESPACE="reachlearn"; constexpr uint8_t REACH_BIN_COUNT=5;
const uint8_t relayA[MAX_CHANNELS]={CH1_OUT_A,CH2_OUT_A,CH3_OUT_A,CH4_OUT_A,CH5_OUT_A,CH6_OUT_A};
const uint8_t relayB[MAX_CHANNELS]={CH1_OUT_B,CH2_OUT_B,CH3_OUT_B,CH4_OUT_B,CH5_OUT_B,CH6_OUT_B};
bool heaterInverted[MAX_CHANNELS]={false,false,false,false,false,false};
ServoOperationMode servoOperationMode[MAX_CHANNELS]={SERVO_OPERATION_REGULATOR,SERVO_OPERATION_REGULATOR,SERVO_OPERATION_REGULATOR,SERVO_OPERATION_REGULATOR,SERVO_OPERATION_REGULATOR,SERVO_OPERATION_REGULATOR};

// Servo-valve preventive maintenance: one valve at a time, fixed 3-minute queue pause.
constexpr const char* SERVO_MAINT_NAMESPACE="servomaint";
constexpr uint8_t SERVO_MAINT_VERSION=3;
constexpr uint8_t SERVO_MAINT_DEFAULT_HOUR=3;
constexpr uint8_t SERVO_MAINT_DEFAULT_MINUTE=0;
constexpr uint32_t SERVO_MAINT_INTERVAL_SECONDS=7UL*24UL*60UL*60UL;
constexpr uint32_t SERVO_MAINT_QUEUE_PAUSE_SECONDS=180UL;
bool servoMaintenanceEnabled=true; uint8_t servoMaintenanceHour=3; uint8_t servoMaintenanceMinute=0; uint32_t servoMaintenanceLastRun=0;
enum ServoMaintenanceState { SERVO_MAINT_IDLE=0, SERVO_MAINT_CLOSING, SERVO_MAINT_OPENING, SERVO_MAINT_RETURNING, SERVO_MAINT_PAUSE };
ServoMaintenanceState servoMaintenanceState=SERVO_MAINT_IDLE; int8_t servoMaintenanceChannel=-1; uint8_t servoMaintenanceReturnPosition=0; bool servoMaintenanceInterrupted=false;
uint8_t servoMaintenanceQueue[MAX_CHANNELS]={0}; uint8_t servoMaintenanceQueueCount=0; uint8_t servoMaintenanceQueueIndex=0; unsigned long servoMaintenancePauseStarted=0;

// One controller-wide AUX TEN. It is separate from CH1..CH6 and is used only as REACH assistance.
bool auxEnabled=false;
uint8_t auxLocationChannel=4; // CH5 by default as a common corridor example
uint8_t auxChannelMask=0;
uint32_t auxMaxRuntimeSeconds=AUX_DEFAULT_MAX_RUNTIME_SECONDS;
float auxMaxLocationTemp=AUX_DEFAULT_MAX_LOCATION_TEMP_C;
AuxPostReachPolicy auxPostReachPolicy=AUX_POST_OFF;
uint32_t auxPostReachMinutes=30;
uint32_t auxPostReachMaxSeconds=7200;
bool auxOutputState=false;
bool auxRunLimited=false;
unsigned long auxStartedAt=0;
unsigned long auxLastOutputChange=0;
char auxReason[96]="Вимкнено";
char apPassword[64]; bool rtcAvailable=false; bool mdnsStarted=false;
constexpr uint8_t MCP23017_ADDR=0x20; constexpr uint8_t MCP_IODIRA=0x00; constexpr uint8_t MCP_IODIRB=0x01; constexpr uint8_t MCP_GPPUA=0x0C; constexpr uint8_t MCP_GPPUB=0x0D; constexpr uint8_t MCP_GPIOA=0x12; constexpr uint8_t MCP_GPIOB=0x13;
bool gpioExpanderAvailable=false; uint16_t limitInputs=0, limitStable=0; unsigned long limitChangedAt[12]={0}; unsigned long lastGpioExpanderProbe=0;
char authUser[32]; char authPass[32];
bool checkAuth(){if(server.authenticate(authUser,authPass))return true;delay(250);server.requestAuthentication();return false;}
char deviceName[32]; char netId[16]; char staSSID[33]; char staPassword[64]; bool staConfigured=false;
void generateNetId(){uint64_t mac=ESP.getEfuseMac();uint8_t b0=(mac>>16)&0xFF,b1=(mac>>8)&0xFF,b2=mac&0xFF;snprintf(netId,sizeof(netId),"hc-%02x%02x%02x",b0,b1,b2);}
void setDefaultNetwork(){strncpy(deviceName,"HeatControl",sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;staSSID[0]=0;staPassword[0]=0;staConfigured=false;strncpy(apPassword,DEFAULT_AP_PASSWORD,sizeof(apPassword)-1);apPassword[sizeof(apPassword)-1]=0;}

const char* actuatorText(ActuatorType a){return a==ACTUATOR_VALVE?"valve":"heater";}
const char* modeText(OperatingMode m){switch(m){case MODE_AUTO:return "auto";case MODE_COMFORT:return "comfort";case MODE_ECONOMY:return "economy";case MODE_MANUAL:return "manual";default:return "off";}}
bool parseModeStrict(const char* s, OperatingMode& out){
  if(!s)return false;
  if(!strcmp(s,"auto")){out=MODE_AUTO;return true;}
  if(!strcmp(s,"comfort")){out=MODE_COMFORT;return true;}
  if(!strcmp(s,"economy")){out=MODE_ECONOMY;return true;}
  if(!strcmp(s,"off")){out=MODE_OFF;return true;}
  if(!strcmp(s,"manual")){out=MODE_MANUAL;return true;}
  return false;
}
void addJsonString(JsonObject o,const char* k,const char* v){o[k]=v?v:"";}

bool writeConfigurationSlot(const char* ns);
bool verifyConfigurationSlot(const char* ns);
bool getActiveSlot(uint8_t &slot);
bool setActiveSlot(uint8_t slot);
bool validateChannel(const ChannelConfig& c);
void resetReachLearning();
void loadReachLearning();
void loadValvePositions();
bool saveValvePosition(uint8_t ch);
void saveAllValvePositions();
void loadServoMaintenance(); bool saveServoMaintenance();
bool saveReachLearningChannel(uint8_t ch);
void resetReachLearningChannel(uint8_t ch);
bool parseValveChannel(int &ch);
const char* auxPostReachPolicyText(AuxPostReachPolicy p){switch(p){case AUX_POST_MINUTES:return "minutes";case AUX_POST_TO_COMFORT:return "comfort";default:return "off";}}
bool parseAuxPostReachPolicy(const char* s, AuxPostReachPolicy& out){if(!s)return false;if(!strcmp(s,"off")){out=AUX_POST_OFF;return true;}if(!strcmp(s,"minutes")){out=AUX_POST_MINUTES;return true;}if(!strcmp(s,"comfort")){out=AUX_POST_TO_COMFORT;return true;}return false;}

void relayOff(uint8_t ch){if(ch<MAX_CHANNELS){digitalWrite(relayA[ch],RELAY_OFF);digitalWrite(relayB[ch],RELAY_OFF);}}
bool outputsAllowed(){return HEATCONTROL_TEST_MODE==0;}
void allRelaysOff(){for(uint8_t i=0;i<MAX_CHANNELS;i++)relayOff(i);}
void heaterPhysicalOff(uint8_t ch){if(ch>=MAX_CHANNELS)return;digitalWrite(relayA[ch],RELAY_OFF);digitalWrite(relayB[ch],RELAY_OFF);}
void applyHeaterOutput(uint8_t ch,bool logicalOn){if(ch>=MAX_CHANNELS)return;if(!outputsAllowed()){heaterPhysicalOff(ch);return;}bool physicalOn=heaterInverted[ch]?!logicalOn:logicalOn;digitalWrite(relayB[ch],RELAY_OFF);digitalWrite(relayA[ch],physicalOn?RELAY_ON:RELAY_OFF);}
void auxPhysicalOff(){digitalWrite(AUX_TEN_RELAY_PIN,RELAY_OFF);auxOutputState=false;}
void setAuxOutput(bool on,const char* reason,bool forceOff=false){
  if(!outputsAllowed()){auxPhysicalOff();return;}
  if(on==auxOutputState)return;
  // Anti-chatter applies to starting AUX. Protective OFF is always immediate.
  if(on && !forceOff && millis()-auxLastOutputChange<AUX_MIN_SWITCH_INTERVAL_MS)return;
  digitalWrite(AUX_TEN_RELAY_PIN,on?RELAY_ON:RELAY_OFF);
  auxOutputState=on;
  auxLastOutputChange=millis();
  if(on)auxStartedAt=millis();
  if(reason){strncpy(auxReason,reason,sizeof(auxReason)-1);auxReason[sizeof(auxReason)-1]=0;}
}
void valveStop(uint8_t ch){if(ch>=MAX_CHANNELS)return;auto&r=runtime[ch];bool wasMoving=r.valveState!=VALVE_STOPPED;digitalWrite(relayA[ch],RELAY_OFF);digitalWrite(relayB[ch],RELAY_OFF);r.valveState=VALVE_STOPPED;r.valveTargetActive=false;if(wasMoving&&channels[ch].actuator==ACTUATOR_VALVE&&r.positionKnown)saveValvePosition(ch);}
void valveOpen(uint8_t ch){if(ch>=MAX_CHANNELS)return;auto&r=runtime[ch];auto&c=channels[ch];if(c.valveFeedback==VALVE_FEEDBACK_LIMITS&&!gpioExpanderAvailable){r.valveFault=true;r.limitHardwareFault=true;valveStop(ch);return;}if(r.openLimit&&r.closeLimit){r.valveFault=true;valveStop(ch);return;}if(r.openLimit){r.valvePosition=100;r.positionKnown=true;valveStop(ch);return;}if(c.valveFeedback==VALVE_FEEDBACK_TIME&&!r.positionKnown&&!r.calibrating){r.valveFault=true;valveStop(ch);return;}if(!outputsAllowed()){valveStop(ch);return;}digitalWrite(relayB[ch],RELAY_OFF);delay(VALVE_DIRECTION_DEADTIME_MS);digitalWrite(relayA[ch],RELAY_ON);r.valveState=VALVE_OPENING;r.movementStarted=millis();r.movementStartPosition=r.valvePosition;r.lastOutputChange=millis();}
void valveClose(uint8_t ch){if(ch>=MAX_CHANNELS)return;auto&r=runtime[ch];auto&c=channels[ch];if(c.valveFeedback==VALVE_FEEDBACK_LIMITS&&!gpioExpanderAvailable){r.valveFault=true;r.limitHardwareFault=true;valveStop(ch);return;}if(r.openLimit&&r.closeLimit){r.valveFault=true;valveStop(ch);return;}if(r.closeLimit){r.valvePosition=0;r.positionKnown=true;valveStop(ch);return;}if(c.valveFeedback==VALVE_FEEDBACK_TIME&&!r.positionKnown&&!r.calibrating){r.valveFault=true;valveStop(ch);return;}if(!outputsAllowed()){valveStop(ch);return;}digitalWrite(relayA[ch],RELAY_OFF);delay(VALVE_DIRECTION_DEADTIME_MS);digitalWrite(relayB[ch],RELAY_ON);r.valveState=VALVE_CLOSING;r.movementStarted=millis();r.movementStartPosition=r.valvePosition;r.lastOutputChange=millis();}
void valveMoveToPosition(uint8_t ch,uint8_t target){if(ch>=MAX_CHANNELS)return;auto&r=runtime[ch];target=(uint8_t)constrain((int)target,0,100);r.valveTargetPosition=target;r.valveTargetActive=true;if(!r.positionKnown){r.valveTargetActive=false;r.valveFault=true;valveStop(ch);return;}if(target==r.valvePosition){valveStop(ch);return;}if(target==0){valveClose(ch);return;}if(target==100){valveOpen(ch);return;}if(target>r.valvePosition)valveOpen(ch);else valveClose(ch);}
static constexpr float REGULATOR_CONTROL_BAND_C=4.0f;
uint8_t regulatorTargetPosition(float error){if(error<=0.0f)return 0;float p=(error/REGULATOR_CONTROL_BAND_C)*100.0f;if(p>100.0f)p=100.0f;return (uint8_t)lroundf(p);}
void updateValvePosition(uint8_t ch){if(ch>=MAX_CHANNELS)return;auto&r=runtime[ch];auto&c=channels[ch];if(r.valveState==VALVE_STOPPED)return;if(c.valveFeedback==VALVE_FEEDBACK_LIMITS){if(!gpioExpanderAvailable){r.valveFault=true;r.limitHardwareFault=true;valveStop(ch);return;}if((r.valveState==VALVE_OPENING&&r.openLimit)||(r.valveState==VALVE_CLOSING&&r.closeLimit)){r.valvePosition=r.valveState==VALVE_OPENING?100:0;r.positionKnown=true;valveStop(ch);return;}uint32_t tt=r.valveState==VALVE_OPENING?c.valveOpenTime:c.valveCloseTime;if(tt<1)tt=1;unsigned long e=millis()-r.movementStarted;float pp=(float)e/((float)tt*1000.0f);if(pp>=1.0f){r.valveFault=true;valveStop(ch);return;}int d=(int)(pp*100.0f);int pos=r.valveState==VALVE_OPENING?(int)r.movementStartPosition+d:(int)r.movementStartPosition-d;r.valvePosition=(uint8_t)constrain(pos,0,100);r.positionKnown=true;if(r.valveTargetActive&&((r.valveState==VALVE_OPENING&&r.valvePosition>=r.valveTargetPosition)||(r.valveState==VALVE_CLOSING&&r.valvePosition<=r.valveTargetPosition))){r.valvePosition=r.valveTargetPosition;valveStop(ch);return;}if(e>((unsigned long)MAX_VALVE_RUN_SECONDS*1000UL)){r.valveFault=true;valveStop(ch);}return;}uint32_t tt=r.valveState==VALVE_OPENING?c.valveOpenTime:c.valveCloseTime;if(tt<1)tt=1;unsigned long e=millis()-r.movementStarted;if(r.calibrating){if(e>((unsigned long)MAX_VALVE_RUN_SECONDS*1000UL)){r.valveFault=true;r.calibrating=false;r.calibrationDirection=VALVE_STOPPED;valveStop(ch);}return;}float pp=(float)e/((float)tt*1000.0f);if(pp>=1.0f){r.valvePosition=r.valveState==VALVE_OPENING?100:0;r.positionKnown=true;valveStop(ch);return;}int d=(int)(pp*100.0f);int pos=r.valveState==VALVE_OPENING?(int)r.movementStartPosition+d:(int)r.movementStartPosition-d;r.valvePosition=(uint8_t)constrain(pos,0,100);if(r.valveTargetActive&&((r.valveState==VALVE_OPENING&&r.valvePosition>=r.valveTargetPosition)||(r.valveState==VALVE_CLOSING&&r.valvePosition<=r.valveTargetPosition))){r.valvePosition=r.valveTargetPosition;valveStop(ch);return;}if(e>((unsigned long)MAX_VALVE_RUN_SECONDS*1000UL)){r.valveFault=true;valveStop(ch);}}

// Persist the last known valve position separately from ChannelConfig.
// This survives ESP32 reboot without forcing a configuration rewrite and avoids
// coupling volatile runtime position to the transactional configuration slots.
constexpr const char* VALVE_POSITION_NAMESPACE="valvepos";
constexpr uint8_t VALVE_POSITION_VERSION=1;
constexpr unsigned long VALVE_POSITION_AUTOSAVE_MS=10000UL;
unsigned long lastValvePositionPersist=0;

bool saveValvePosition(uint8_t ch){
  if(ch>=MAX_CHANNELS)return false;
  Preferences p;
  if(!p.begin(VALVE_POSITION_NAMESPACE,false))return false;
  char k[8]; snprintf(k,sizeof(k),"p%u",ch);
  char v[8]; snprintf(v,sizeof(v),"v%u",ch);
  bool ok=p.putUChar(k,runtime[ch].valvePosition)==1;
  ok=ok&&p.putBool(v,runtime[ch].positionKnown)==1;
  ok=ok&&p.putUChar("ver",VALVE_POSITION_VERSION)==1;
  p.end();
  return ok;
}

void saveAllValvePositions(){
  Preferences p;
  if(!p.begin(VALVE_POSITION_NAMESPACE,false))return;
  bool ok=true;
  ok=ok&&p.putUChar("ver",VALVE_POSITION_VERSION)==1;
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){
    char k[8];snprintf(k,sizeof(k),"p%u",ch);ok=ok&&p.putUChar(k,runtime[ch].valvePosition)==1;
    snprintf(k,sizeof(k),"v%u",ch);ok=ok&&p.putBool(k,runtime[ch].positionKnown)==1;
  }
  p.end();
  if(!ok)Serial.println("ERROR: valve position NVS save failed");
}

void loadValvePositions(){
  Preferences p;
  if(!p.begin(VALVE_POSITION_NAMESPACE,true))return;
  uint8_t ver=p.getUChar("ver",0);
  if(ver!=VALVE_POSITION_VERSION){p.end();return;}
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){
    char k[8];snprintf(k,sizeof(k),"p%u",ch);
    char v[8];snprintf(v,sizeof(v),"v%u",ch);
    uint8_t pos=p.getUChar(k,0);
    bool known=p.getBool(v,false);
    runtime[ch].valvePosition=(uint8_t)constrain((int)pos,0,100);
    runtime[ch].positionKnown=known;
  }
  p.end();
}

bool saveServoMaintenance(){Preferences p;if(!p.begin(SERVO_MAINT_NAMESPACE,false))return false;bool ok=true;ok=ok&&p.putUChar("ver",SERVO_MAINT_VERSION)==1;ok=ok&&p.putBool("en",servoMaintenanceEnabled)==1;ok=ok&&p.putUChar("hr",servoMaintenanceHour)==1;ok=ok&&p.putUChar("min",servoMaintenanceMinute)==1;ok=ok&&p.putUInt("last",servoMaintenanceLastRun)==1;ok=ok&&p.putBool("active",servoMaintenanceState!=SERVO_MAINT_IDLE)==1;ok=ok&&p.putUChar("phase",(uint8_t)servoMaintenanceState)==1;ok=ok&&p.putChar("ch",servoMaintenanceChannel)==1;ok=ok&&p.putUChar("ret",servoMaintenanceReturnPosition)==1;ok=ok&&p.putBool("interrupted",servoMaintenanceInterrupted)==1;p.end();return ok;}
void loadServoMaintenance(){Preferences p;servoMaintenanceEnabled=true;servoMaintenanceHour=3;servoMaintenanceMinute=0;servoMaintenanceLastRun=0;servoMaintenanceState=SERVO_MAINT_IDLE;servoMaintenanceChannel=-1;servoMaintenanceReturnPosition=0;servoMaintenanceInterrupted=false;if(!p.begin(SERVO_MAINT_NAMESPACE,true))return;uint8_t ver=p.getUChar("ver",0);if(ver==SERVO_MAINT_VERSION||ver==2||ver==1){servoMaintenanceEnabled=p.getBool("en",true);servoMaintenanceHour=(uint8_t)constrain((int)p.getUChar("hr",3),0,23);servoMaintenanceMinute=(uint8_t)constrain((int)p.getUChar("min",0),0,59);servoMaintenanceLastRun=p.getUInt("last",0);if(ver>=2&&p.getBool("active",false)){servoMaintenanceInterrupted=true;int storedCh=(int)p.getChar("ch",-1);if(storedCh>=0&&storedCh<MAX_CHANNELS)servoMaintenanceChannel=(int8_t)storedCh;servoMaintenanceReturnPosition=(uint8_t)constrain((int)p.getUChar("ret",0),0,100);}}p.end();/* Never resume a timed exercise blindly after reboot; retain only the interrupted channel for safe revalidation. */servoMaintenanceState=SERVO_MAINT_IDLE;servoMaintenanceQueueCount=0;servoMaintenanceQueueIndex=0;}
void resetServoMaintenanceQueue(){servoMaintenanceQueueCount=0;servoMaintenanceQueueIndex=0;for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){auto &c=channels[ch];auto&r=runtime[ch];if(c.actuator==ACTUATOR_VALVE&&servoOperationMode[ch]==SERVO_OPERATION_REGULATOR&&c.mode==MODE_AUTO&&r.positionKnown&&!r.valveFault&&!r.calibrating)servoMaintenanceQueue[servoMaintenanceQueueCount++]=ch;}}
void abortServoMaintenanceValve(){if(servoMaintenanceChannel>=0){uint8_t ch=(uint8_t)servoMaintenanceChannel;valveStop(ch);runtime[ch].positionKnown=false;runtime[ch].valveFault=true;saveValvePosition(ch);}servoMaintenanceChannel=-1;servoMaintenanceState=SERVO_MAINT_IDLE;servoMaintenanceQueueCount=0;servoMaintenanceQueueIndex=0;saveServoMaintenance();}
void startNextServoMaintenanceValve(){if(servoMaintenanceQueueIndex>=servoMaintenanceQueueCount){servoMaintenanceState=SERVO_MAINT_IDLE;saveServoMaintenance();return;}uint8_t ch=servoMaintenanceQueue[servoMaintenanceQueueIndex];auto&r=runtime[ch];auto&c=channels[ch];if(c.actuator!=ACTUATOR_VALVE||servoOperationMode[ch]!=SERVO_OPERATION_REGULATOR||c.mode!=MODE_AUTO||!r.positionKnown||r.valveFault||r.calibrating){abortServoMaintenanceValve();return;}servoMaintenanceChannel=(int8_t)ch;servoMaintenanceReturnPosition=r.valvePosition;servoMaintenanceState=SERVO_MAINT_CLOSING;servoMaintenanceInterrupted=false;saveServoMaintenance();valveClose(ch);}
void finishServoMaintenanceValve(){if(servoMaintenanceChannel<0)return;uint8_t ch=(uint8_t)servoMaintenanceChannel;valveStop(ch);runtime[ch].valvePosition=servoMaintenanceReturnPosition;runtime[ch].positionKnown=true;runtime[ch].valveFault=false;saveValvePosition(ch);servoMaintenanceChannel=-1;if(servoMaintenanceQueueIndex+1<servoMaintenanceQueueCount){servoMaintenanceQueueIndex++;servoMaintenanceState=SERVO_MAINT_PAUSE;servoMaintenancePauseStarted=millis();saveServoMaintenance();}else{servoMaintenanceState=SERVO_MAINT_IDLE;servoMaintenanceQueueCount=0;servoMaintenanceQueueIndex=0;servoMaintenanceLastRun=rtcAvailable?rtc.now().unixtime():servoMaintenanceLastRun;saveServoMaintenance();}}
bool servoMaintenanceDue(const DateTime&now){if(!servoMaintenanceEnabled||!rtcAvailable||rtc.lostPower())return false;uint32_t ts=now.unixtime();if(servoMaintenanceLastRun&&ts<servoMaintenanceLastRun+SERVO_MAINT_INTERVAL_SECONDS)return false;return now.hour()==servoMaintenanceHour&&now.minute()==servoMaintenanceMinute;}
void serviceServoMaintenance(){if(!rtcAvailable||rtc.lostPower())return;DateTime now=rtc.now();if(servoMaintenanceState==SERVO_MAINT_IDLE){if(servoMaintenanceDue(now)){resetServoMaintenanceQueue();servoMaintenanceLastRun=now.unixtime();saveServoMaintenance();if(servoMaintenanceQueueCount)startNextServoMaintenanceValve();}return;}if(servoMaintenanceState==SERVO_MAINT_PAUSE){if(millis()-servoMaintenancePauseStarted>=SERVO_MAINT_QUEUE_PAUSE_SECONDS*1000UL)startNextServoMaintenanceValve();return;}if(servoMaintenanceChannel<0){servoMaintenanceState=SERVO_MAINT_IDLE;saveServoMaintenance();return;}uint8_t ch=(uint8_t)servoMaintenanceChannel;auto&r=runtime[ch];if(r.valveFault){abortServoMaintenanceValve();return;}if(servoMaintenanceState==SERVO_MAINT_CLOSING&&r.valveState==VALVE_STOPPED&&r.positionKnown&&r.valvePosition==0){servoMaintenanceState=SERVO_MAINT_OPENING;saveServoMaintenance();valveOpen(ch);return;}if(servoMaintenanceState==SERVO_MAINT_OPENING&&r.valveState==VALVE_STOPPED&&r.positionKnown&&r.valvePosition==100){servoMaintenanceState=SERVO_MAINT_RETURNING;saveServoMaintenance();if(servoMaintenanceReturnPosition==0)valveClose(ch);else if(servoMaintenanceReturnPosition==100)valveOpen(ch);else if(r.valvePosition>servoMaintenanceReturnPosition)valveClose(ch);else valveOpen(ch);return;}if(servoMaintenanceState==SERVO_MAINT_RETURNING&&r.valveState==VALVE_STOPPED&&r.positionKnown&&r.valvePosition==servoMaintenanceReturnPosition)finishServoMaintenanceValve();}

void maybePersistValvePositions(){
  if(millis()-lastValvePositionPersist<VALVE_POSITION_AUTOSAVE_MS)return;
  lastValvePositionPersist=millis();
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){
    if(channels[ch].actuator==ACTUATOR_VALVE && runtime[ch].positionKnown && runtime[ch].valveState!=VALVE_STOPPED){
      saveValvePosition(ch);
    }
  }
}

bool saveConfiguration(){
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++) if(!validateChannel(channels[ch])) return false;
  uint8_t active=0; getActiveSlot(active);
  uint8_t target=active^1;
  const char* ns=target?STORAGE_SLOT1:STORAGE_SLOT0;
  if(!writeConfigurationSlot(ns)) return false;
  if(!verifyConfigurationSlot(ns)) return false;
  if(!setActiveSlot(target)) return false;
  return true;
}

void startCalibration(uint8_t ch,uint8_t initialPosition){
  if(ch>=MAX_CHANNELS)return;
  auto&r=runtime[ch]; auto&c=channels[ch];
  if(c.valveFeedback!=VALVE_FEEDBACK_TIME){r.valveFault=true;return;}
  if(initialPosition>1){r.valveFault=true;return;}
  if(r.valveState!=VALVE_STOPPED){valveStop(ch);}
  r.valveFault=false;r.calibrating=true;r.calibrationInitialPosition=initialPosition;r.calibrationStartedAt=millis();r.calibrationDirection=(initialPosition==0)?VALVE_OPENING:VALVE_CLOSING;r.positionKnown=false;
  if(initialPosition==0)valveOpen(ch); else valveClose(ch);
  r.calibrationStartedAt=millis();
}
void stopCalibration(uint8_t ch){
  if(ch>=MAX_CHANNELS)return;
  auto&r=runtime[ch]; auto&c=channels[ch];
  if(!r.calibrating)return;
  valveStop(ch);
  unsigned long elapsed=(millis()-r.calibrationStartedAt+500UL)/1000UL;
  if(elapsed<1)elapsed=1;
  if(elapsed>MAX_VALVE_RUN_SECONDS){r.calibrating=false;r.valveFault=true;r.positionKnown=false;return;}
  uint32_t oldOpen=c.valveOpenTime, oldClose=c.valveCloseTime;
  if(r.calibrationDirection==VALVE_OPENING){r.calibrationOpenCandidate=(uint32_t)elapsed;r.calibrationHasOpen=true;c.valveOpenTime=(uint32_t)elapsed;r.valvePosition=100;}
  else if(r.calibrationDirection==VALVE_CLOSING){r.calibrationCloseCandidate=(uint32_t)elapsed;r.calibrationHasClose=true;c.valveCloseTime=(uint32_t)elapsed;r.valvePosition=0;}
  r.calibrating=false;r.calibrationDirection=VALVE_STOPPED;
  // A TIME-mode valve is authoritative only after both full travel directions
  // have been measured in the current calibration state.
  r.positionKnown=r.calibrationHasOpen&&r.calibrationHasClose;
  if(r.positionKnown){
    saveValvePosition(ch);
  if(!saveConfiguration()){
      c.valveOpenTime=oldOpen;c.valveCloseTime=oldClose;
      r.valveFault=true;r.positionKnown=false;
    }
  }
}

void mcpWriteReg(uint8_t reg,uint8_t value){Wire.beginTransmission(MCP23017_ADDR);Wire.write(reg);Wire.write(value);Wire.endTransmission();}
bool mcpReadRegs(uint8_t reg,uint8_t& a,uint8_t& b){Wire.beginTransmission(MCP23017_ADDR);Wire.write(reg);if(Wire.endTransmission(false)!=0)return false;Wire.requestFrom((int)MCP23017_ADDR,2);if(Wire.available()!=2)return false;a=Wire.read();b=Wire.read();return true;}
bool detectAndInitGpioExpander(){Wire.beginTransmission(MCP23017_ADDR);if(Wire.endTransmission()!=0)return false;mcpWriteReg(MCP_IODIRA,0xFF);mcpWriteReg(MCP_IODIRB,0xFF);mcpWriteReg(MCP_GPPUA,0xFF);mcpWriteReg(MCP_GPPUB,0xFF);uint8_t a,b,pa,pb;if(!mcpReadRegs(MCP_IODIRA,a,b)||!mcpReadRegs(MCP_GPPUA,pa,pb))return false;return a==0xFF&&b==0xFF&&pa==0xFF&&pb==0xFF;}
bool limitBit(uint8_t index){return index<12?((limitStable>>index)&1U)!=0:false;}
void readLimitInputs(){if(!gpioExpanderAvailable)return;uint8_t a,b;if(!mcpReadRegs(MCP_GPIOA,a,b)){gpioExpanderAvailable=false;for(uint8_t ch=0;ch<MAX_CHANNELS;ch++)if(channels[ch].valveFeedback==VALVE_FEEDBACK_LIMITS){runtime[ch].valveFault=true;runtime[ch].limitHardwareFault=true;valveStop(ch);}return;}uint16_t raw=((uint16_t)a|((uint16_t)b<<8))&0x0FFFU;
#if LIMIT_SWITCH_ACTIVE_LOW
raw=(~raw)&0x0FFFU;
#endif
unsigned long now=millis();for(uint8_t i=0;i<12;i++){bool v=(raw>>i)&1U;bool old=(limitInputs>>i)&1U;if(v!=old){limitInputs=(limitInputs&~(1U<<i))|(v?(1U<<i):0);limitChangedAt[i]=now;}if(now-limitChangedAt[i]>=LIMIT_SWITCH_DEBOUNCE_MS)limitStable=(limitStable&~(1U<<i))|(v?(1U<<i):0);}for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){runtime[ch].openLimit=limitBit(ch*2);runtime[ch].closeLimit=limitBit(ch*2+1);if(runtime[ch].openLimit&&runtime[ch].closeLimit){runtime[ch].valveFault=true;runtime[ch].limitHardwareFault=true;valveStop(ch);}else if(runtime[ch].limitHardwareFault){runtime[ch].limitHardwareFault=false;runtime[ch].valveFault=false;}}}
void maintainGpioExpander(){unsigned long now=millis();if(gpioExpanderAvailable&&now-lastGpioExpanderProbe<GPIO_EXPANDER_RECHECK_MS)return;if(!gpioExpanderAvailable&&now-lastGpioExpanderProbe<GPIO_EXPANDER_RECHECK_MS)return;lastGpioExpanderProbe=now;bool found=detectAndInitGpioExpander();if(found&&!gpioExpanderAvailable){gpioExpanderAvailable=true;limitInputs=0;limitStable=0;memset(limitChangedAt,0,sizeof(limitChangedAt));Serial.println("GPIO expander MCP23017: RECOVERED; validating limit inputs before clearing faults");}else if(!found&&gpioExpanderAvailable){gpioExpanderAvailable=false;for(uint8_t ch=0;ch<MAX_CHANNELS;ch++)if(channels[ch].valveFeedback==VALVE_FEEDBACK_LIMITS){runtime[ch].valveFault=true;runtime[ch].limitHardwareFault=true;valveStop(ch);}Serial.println("GPIO expander MCP23017: LOST");}else if(found){gpioExpanderAvailable=true;}}
void updateLimitInputs(){static unsigned long last=0;if(millis()-last>=LIMIT_SWITCH_POLL_MS){last=millis();readLimitInputs();}}

void discoverSensors(){
  sensorCount=0;
  uint8_t found=sensors.getDeviceCount();
  for(uint8_t physical=0; physical<found && sensorCount<MAX_SENSORS; physical++){
    DeviceAddress rom;
    if(!sensors.getAddress(rom,physical)) continue;
    if(rom[0]!=0x28 || OneWire::crc8(rom,7)!=rom[7]) continue;
    bool duplicate=false;
    for(uint8_t j=0;j<sensorCount;j++) if(memcmp(sensorAddresses[j],rom,8)==0){duplicate=true;break;}
    if(duplicate) continue;
    memcpy(sensorAddresses[sensorCount],rom,8);
    Serial.printf("Sensor #%u ROM: ",sensorCount+1);
    for(uint8_t b=0;b<8;b++)Serial.printf("%02X",sensorAddresses[sensorCount][b]);
    Serial.println();
    sensorCount++;
  }
  Serial.printf("DS18B20 valid count: %u\n",sensorCount);
}
int findSensorByRom(const uint8_t* rom){for(uint8_t i=0;i<sensorCount;i++)if(memcmp(rom,sensorAddresses[i],8)==0)return i;return -1;}
void handleSensorLost(uint8_t ch){
  if(ch>=MAX_CHANNELS)return;
  auto &r=runtime[ch];
  if(!r.sensorRecoveryPending && channels[ch].mode!=MODE_MANUAL){
    r.sensorRecoveryPending=true;
    r.sensorRecoveryMode=channels[ch].mode;
    r.sensorRecoveryAutoInverted=(channels[ch].mode==MODE_AUTO)?r.autoInverted:false;
    r.sensorRecoveryUserOverride=false;
    r.sensorRecoveryGoodReads=0;
    channels[ch].mode=MODE_MANUAL;
    r.autoInverted=false;
    r.outputState=false;
    valveStop(ch);
    heaterPhysicalOff(ch);
    saveConfiguration();
  } else if(r.sensorRecoveryPending){
    r.sensorRecoveryGoodReads=0;
  }
}

void handleSensorRecovered(uint8_t ch){
  if(ch>=MAX_CHANNELS)return;
  auto &r=runtime[ch];
  if(!r.sensorRecoveryPending)return;
  if(++r.sensorRecoveryGoodReads < SENSOR_RECOVERY_STABLE_READS)return;
  const bool restore = !r.sensorRecoveryUserOverride;
  const OperatingMode previousMode=r.sensorRecoveryMode;
  const bool previousAutoInverted=r.sensorRecoveryAutoInverted;
  r.sensorRecoveryPending=false;
  r.sensorRecoveryGoodReads=0;
  if(!restore){
    r.autoInverted=false;
    return;
  }
  channels[ch].mode=previousMode;
  r.autoInverted=(previousMode==MODE_AUTO)?previousAutoInverted:false;
  r.outputState=false;
  if(channels[ch].actuator==ACTUATOR_VALVE)valveStop(ch);
  else heaterPhysicalOff(ch);
  saveConfiguration();
}

void readTemperatures(){
  sensors.requestTemperatures();
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){
    int idx=findSensorByRom(channels[ch].sensorRom);
    if(idx<0){
      runtime[ch].sensorOK=false; runtime[ch].temperature=NAN;
      handleSensorLost(ch);
      continue;
    }
    float t=sensors.getTempC(sensorAddresses[idx]);
    if(t==DEVICE_DISCONNECTED_C||t<-55.0f||t>125.0f){
      runtime[ch].sensorOK=false; runtime[ch].temperature=NAN;
      handleSensorLost(ch);
    } else {
      runtime[ch].sensorOK=true; runtime[ch].temperature=t;
      handleSensorRecovered(ch);
    }
  }
  int oi=findSensorByRom(outdoorSensorRom);
  bool outdoorConfigured=false;for(uint8_t b=0;b<8;b++)if(outdoorSensorRom[b]!=0){outdoorConfigured=true;break;}
  if(oi<0 || !outdoorConfigured){outdoorSensorOK=false;outdoorTemperature=NAN;}
  else{
    float t=sensors.getTempC(sensorAddresses[oi]);
    if(t==DEVICE_DISCONNECTED_C||t<-55.0f||t>125.0f){outdoorSensorOK=false;outdoorTemperature=NAN;}
    else{outdoorSensorOK=true;outdoorTemperature=t;}
  }
}

const char* templateLogicText(TemplateLogic v){return v==TEMPLATE_LOGIC_REACH?"reach":"start";}
bool parseTemplateLogic(const char* s,TemplateLogic& out){if(!s)return false;if(!strcmp(s,"start")){out=TEMPLATE_LOGIC_START;return true;}if(!strcmp(s,"reach")){out=TEMPLATE_LOGIC_REACH;return true;}return false;}
void applyTemplateLogicToSlots(uint8_t ch){if(ch>=MAX_CHANNELS)return;ScheduleTransitionType t=(templateLogic[ch]==TEMPLATE_LOGIC_REACH)?SCHEDULE_REACH:SCHEDULE_START;for(uint8_t d=0;d<7;d++)for(uint8_t ss=0;ss<6;ss++){channels[ch].working.days[d].slots[ss].transitionType=t;channels[ch].holiday.days[d].slots[ss].transitionType=t;}}
void initCalendarDefaults(ChannelConfig&c){
  for(uint8_t d=0;d<7;d++){
    c.dayType[d]=d<5?DAY_WORKING:DAY_HOLIDAY;
    for(uint8_t t=0;t<2;t++)for(uint8_t s=0;s<6;s++){
      ScheduleSlot &x=(t==0?c.working.days[d].slots[s]:c.holiday.days[d].slots[s]);
      x.hour=0;x.minute=0;x.mode=MODE_ECONOMY;x.enabled=false;x.transitionType=SCHEDULE_START;
    }
  }
}
void copyOldV12ToCurrent(const OldChannelConfigV12& old, ChannelConfig& c){
  memset(&c,0,sizeof(c));
  memcpy(c.name,old.name,sizeof(c.name)); c.actuator=old.actuator;c.mode=old.mode;
  c.comfortTemperature=old.comfortTemperature;c.economyTemperature=old.economyTemperature;c.hysteresis=old.hysteresis;c.sensorIndex=old.sensorIndex;
  memcpy(c.sensorRom,old.sensorRom,8);c.valveOpenTime=old.valveOpenTime;c.valveCloseTime=old.valveCloseTime;c.valveFeedback=VALVE_FEEDBACK_TIME;
  initCalendarDefaults(c);
  for(uint8_t d=0;d<7;d++){
    c.dayType[d]=old.calendar[d].type;
    for(uint8_t s=0;s<6;s++){
      ScheduleSlot dst{}; dst.hour=old.calendar[d].slots[s].hour; dst.minute=old.calendar[d].slots[s].minute; dst.mode=old.calendar[d].slots[s].mode; dst.enabled=old.calendar[d].slots[s].enabled; dst.transitionType=SCHEDULE_START;
      if(old.calendar[d].type==DAY_WORKING)c.working.days[d].slots[s]=dst;
      else c.holiday.days[d].slots[s]=dst;
    }
  }
}
void copyOldV11ToCurrent(const OldChannelConfigV12& old, ChannelConfig& c){copyOldV12ToCurrent(old,c);}

void setDefaultAuth(){strncpy(authUser,DEFAULT_AUTH_USER,sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,DEFAULT_AUTH_PASS,sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;}
void setDefaultConfig(){const char* n[MAX_CHANNELS]={DEFAULT_CH1_NAME,DEFAULT_CH2_NAME,DEFAULT_CH3_NAME,DEFAULT_CH4_NAME,DEFAULT_CH5_NAME,DEFAULT_CH6_NAME};for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){memset(&channels[ch],0,sizeof(ChannelConfig));templateLogic[ch]=TEMPLATE_LOGIC_START;heaterInverted[ch]=false;strncpy(channels[ch].name,n[ch],sizeof(channels[ch].name)-1);channels[ch].actuator=ACTUATOR_VALVE;channels[ch].mode=MODE_AUTO;channels[ch].comfortTemperature=22;channels[ch].economyTemperature=19;channels[ch].hysteresis=.3f;channels[ch].sensorIndex=ch;channels[ch].valveOpenTime=90;channels[ch].valveCloseTime=90;channels[ch].valveFeedback=VALVE_FEEDBACK_TIME;servoOperationMode[ch]=SERVO_OPERATION_REGULATOR;if(ch<sensorCount)memcpy(channels[ch].sensorRom,sensorAddresses[ch],8);initCalendarDefaults(channels[ch]);}auxEnabled=false;auxLocationChannel=4;auxChannelMask=0;auxMaxRuntimeSeconds=AUX_DEFAULT_MAX_RUNTIME_SECONDS;auxMaxLocationTemp=AUX_DEFAULT_MAX_LOCATION_TEMP_C;auxPostReachPolicy=AUX_POST_OFF;auxPostReachMinutes=AUX_DEFAULT_POST_REACH_MINUTES;auxPostReachMaxSeconds=AUX_DEFAULT_POST_REACH_MAX_SECONDS;auxOutputState=false;auxRunLimited=false;auxStartedAt=0;auxLastOutputChange=millis()-AUX_MIN_SWITCH_INTERVAL_MS;strncpy(auxReason,"Вимкнено",sizeof(auxReason)-1);}

bool validateTemplate(const WeekTemplate& t){for(uint8_t d=0;d<7;d++)for(uint8_t s=0;s<6;s++){const auto&x=t.days[d].slots[s];if(x.hour>23||x.minute>59)return false;if(x.transitionType>SCHEDULE_REACH)return false;if(x.enabled&&(x.mode!=MODE_COMFORT&&x.mode!=MODE_ECONOMY))return false;}return true;}
bool validateChannel(const ChannelConfig&c){if(!strlen(c.name)||strlen(c.name)>=sizeof(c.name))return false;if(c.actuator>ACTUATOR_HEATER||c.mode>MODE_MANUAL)return false;for(uint8_t d=0;d<7;d++)if(c.dayType[d]>DAY_HOLIDAY)return false;if(!validateTemplate(c.working)||!validateTemplate(c.holiday))return false;if(c.comfortTemperature<MIN_SETPOINT_C||c.comfortTemperature>MAX_SETPOINT_C)return false;if(c.economyTemperature<MIN_SETPOINT_C||c.economyTemperature>MAX_SETPOINT_C)return false;if(c.hysteresis<MIN_HYSTERESIS_C||c.hysteresis>MAX_HYSTERESIS_C)return false;if(c.valveOpenTime<1||c.valveOpenTime>MAX_VALVE_RUN_SECONDS||c.valveCloseTime<1||c.valveCloseTime>MAX_VALVE_RUN_SECONDS)return false;if(c.valveFeedback>VALVE_FEEDBACK_LIMITS)return false;return true;}
bool romIsZero(const uint8_t* rom){for(uint8_t i=0;i<8;i++)if(rom[i]!=0)return false;return true;}
bool validateSensorAssignments(){
  for(uint8_t a=0;a<MAX_CHANNELS;a++){
    if(channels[a].sensorIndex>=sensorCount) return false;
    if(romIsZero(channels[a].sensorRom)) return false;
    for(uint8_t b=0;b<a;b++) if(!memcmp(channels[a].sensorRom,channels[b].sensorRom,8)) return false;
  }
  if(!romIsZero(outdoorSensorRom)){
    for(uint8_t ch=0;ch<MAX_CHANNELS;ch++) if(!memcmp(outdoorSensorRom,channels[ch].sensorRom,8)) return false;
    bool found=false; for(uint8_t i=0;i<sensorCount;i++) if(!memcmp(outdoorSensorRom,sensorAddresses[i],8)){found=true;break;}
    if(!found) return false;
  }
  return true;
}

bool writeConfigurationSlot(const char* ns){
  Preferences p;
  if(!p.begin(ns,false)) return false;
  bool ok=true;
  ok &= p.putBool("configured",true)==1;
  ok &= p.putUChar("cfgVer",CONFIG_VERSION)==1;
  ok &= p.putString("authUser",authUser)>0;
  ok &= p.putString("authPass",authPass)>0;
  ok &= p.putString("devName",deviceName)>0;
  ok &= p.putString("apPass",apPassword)>0;
  // Empty STA credentials are valid when the controller uses AP-only mode.
  // STA credentials may legitimately be empty in AP-only mode.
  p.putString("staSsid",staSSID);
  p.putString("staPass",staPassword);
  ok &= p.putBool("staCfg",staConfigured)==1;
  ok &= p.putBytes("outRom",outdoorSensorRom,8)==8;
  ok &= p.putBool("auxEn",auxEnabled)==1;
  ok &= p.putUChar("auxLoc",auxLocationChannel)==1;
  ok &= p.putUChar("auxMask",auxChannelMask)==1;
  ok &= p.putUInt("auxMaxRun",auxMaxRuntimeSeconds)==4;
  ok &= p.putFloat("auxMaxTemp",auxMaxLocationTemp)>0;
  ok &= p.putUChar("auxPostPol",(uint8_t)auxPostReachPolicy)==1;
  ok &= p.putUInt("auxPostMin",auxPostReachMinutes)==4;
  ok &= p.putUInt("auxPostMax",auxPostReachMaxSeconds)==4;
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){
    char k[8]; snprintf(k,sizeof(k),"ch%u",ch);
    ok &= p.putBytes(k,&channels[ch],sizeof(ChannelConfig))==sizeof(ChannelConfig);
    snprintf(k,sizeof(k),"inv%u",ch);
    ok &= p.putBool(k,heaterInverted[ch])==1;
    snprintf(k,sizeof(k),"logic%u",ch);
    ok &= p.putUChar(k,(uint8_t)templateLogic[ch])==1;
    snprintf(k,sizeof(k),"servo%u",ch);
    ok &= p.putUChar(k,(uint8_t)servoOperationMode[ch])==1;
  }
  p.end();
  return ok;
}

bool verifyConfigurationSlot(const char* ns){
  Preferences p;
  if(!p.begin(ns,true)) return false;
  bool ok=p.getBool("configured",false) && p.getUChar("cfgVer",0)==CONFIG_VERSION;
  if(ok){
    String au=p.getString("authUser","");
    String ap=p.getString("authPass","");
    String dn=p.getString("devName","");
    String aps=p.getString("apPass","");
    String ss=p.getString("staSsid","");
    String sp=p.getString("staPass","");
    bool sc=p.getBool("staCfg",false);
    uint8_t orom[8]={0};
    ok = p.getBytesLength("outRom")==8 && p.getBytes("outRom",orom,8)==8;
    ok = ok && memcmp(orom,outdoorSensorRom,8)==0;
    uint8_t vLoc=p.getUChar("auxLoc",auxLocationChannel); uint8_t vMask=p.getUChar("auxMask",auxChannelMask);
    uint32_t vRun=p.getUInt("auxMaxRun",auxMaxRuntimeSeconds); float vTemp=p.getFloat("auxMaxTemp",auxMaxLocationTemp); bool vEn=p.getBool("auxEn",auxEnabled); uint8_t vPol=p.getUChar("auxPostPol",(uint8_t)auxPostReachPolicy); uint32_t vPostMin=p.getUInt("auxPostMin",auxPostReachMinutes); uint32_t vPostMax=p.getUInt("auxPostMax",auxPostReachMaxSeconds);
    ok = ok && vEn==auxEnabled && vLoc==auxLocationChannel && vMask==auxChannelMask && vRun==auxMaxRuntimeSeconds && fabsf(vTemp-auxMaxLocationTemp)<0.001f && vPol==(uint8_t)auxPostReachPolicy && vPostMin==auxPostReachMinutes && vPostMax==auxPostReachMaxSeconds;
    ok = ok && au == String(authUser) && ap == String(authPass) && dn == String(deviceName);
    ok = ok && aps == String(apPassword);
    ok = ok && ss == String(staSSID) && sp == String(staPassword) && sc == staConfigured;
  }
  for(uint8_t ch=0;ok && ch<MAX_CHANNELS;ch++){
    char k[8]; snprintf(k,sizeof(k),"ch%u",ch);
    if(p.getBytesLength(k)!=sizeof(ChannelConfig)) {ok=false;break;}
    ChannelConfig check; if(p.getBytes(k,&check,sizeof(check))!=sizeof(check) || memcmp(&check,&channels[ch],sizeof(check))!=0) ok=false;
    snprintf(k,sizeof(k),"inv%u",ch);
    if(ok && p.getBool(k,!heaterInverted[ch])!=heaterInverted[ch]) ok=false;
    snprintf(k,sizeof(k),"logic%u",ch);
    if(ok && p.getUChar(k,0)!=(uint8_t)templateLogic[ch]) ok=false;
    snprintf(k,sizeof(k),"servo%u",ch);
    if(ok && p.getUChar(k,0)!=(uint8_t)servoOperationMode[ch]) ok=false;
  }
  p.end();
  return ok;
}

bool readConfigurationSlot(const char* ns){
  Preferences p;
  if(!p.begin(ns,true)) return false;
  bool ok=p.getBool("configured",false) && p.getUChar("cfgVer",0)==CONFIG_VERSION;
  if(ok){
    String au=p.getString("authUser",""); String ap=p.getString("authPass",""); String dn=p.getString("devName","");
    String aps=p.getString("apPass","");
    String ss=p.getString("staSsid",""); String sp=p.getString("staPass","");
    if(!au.length()||!ap.length()||!dn.length()||dn.length()>=sizeof(deviceName)){ok=false;}
    if(ok){
      strncpy(authUser,au.c_str(),sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;
      strncpy(authPass,ap.c_str(),sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;
      if(aps.length()>=MIN_AP_PASSWORD_LEN && aps.length()<sizeof(apPassword)){strncpy(apPassword,aps.c_str(),sizeof(apPassword)-1);apPassword[sizeof(apPassword)-1]=0;}else {ok=false;}
      strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;
      strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;
      strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;
      staConfigured=p.getBool("staCfg",false);
      if(p.getBytesLength("outRom")==8)p.getBytes("outRom",outdoorSensorRom,8); else memset(outdoorSensorRom,0,8);
      auxEnabled=p.getBool("auxEn",false);
      auxLocationChannel=p.getUChar("auxLoc",4);
      auxChannelMask=p.getUChar("auxMask",0);
      auxMaxRuntimeSeconds=p.getUInt("auxMaxRun",AUX_DEFAULT_MAX_RUNTIME_SECONDS);
      auxMaxLocationTemp=p.getFloat("auxMaxTemp",AUX_DEFAULT_MAX_LOCATION_TEMP_C);
      uint8_t postPol=p.getUChar("auxPostPol",(uint8_t)AUX_POST_OFF); auxPostReachPolicy=(postPol<=AUX_POST_TO_COMFORT)?(AuxPostReachPolicy)postPol:AUX_POST_OFF;
      auxPostReachMinutes=p.getUInt("auxPostMin",AUX_DEFAULT_POST_REACH_MINUTES);
      auxPostReachMaxSeconds=p.getUInt("auxPostMax",AUX_DEFAULT_POST_REACH_MAX_SECONDS);
      if(auxPostReachMinutes<5||auxPostReachMinutes>240)auxPostReachMinutes=AUX_DEFAULT_POST_REACH_MINUTES;
      if(auxPostReachMaxSeconds<60||auxPostReachMaxSeconds>7200)auxPostReachMaxSeconds=AUX_DEFAULT_POST_REACH_MAX_SECONDS;
      if(auxLocationChannel>=MAX_CHANNELS)auxLocationChannel=MAX_CHANNELS-1;
      if(auxMaxRuntimeSeconds<60||auxMaxRuntimeSeconds>7200)auxMaxRuntimeSeconds=AUX_DEFAULT_MAX_RUNTIME_SECONDS;
      if(auxMaxLocationTemp<5.0f||auxMaxLocationTemp>35.0f)auxMaxLocationTemp=AUX_DEFAULT_MAX_LOCATION_TEMP_C;
      for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){
        char k[8];snprintf(k,sizeof(k),"ch%u",ch);
        if(p.getBytesLength(k)!=sizeof(ChannelConfig) || p.getBytes(k,&channels[ch],sizeof(ChannelConfig))!=sizeof(ChannelConfig)){ok=false;break;}
        snprintf(k,sizeof(k),"inv%u",ch); heaterInverted[ch]=p.getBool(k,false);
        snprintf(k,sizeof(k),"logic%u",ch); uint8_t lv=p.getUChar(k,0); templateLogic[ch]=(lv<=TEMPLATE_LOGIC_REACH)?(TemplateLogic)lv:TEMPLATE_LOGIC_START; applyTemplateLogicToSlots(ch);
        snprintf(k,sizeof(k),"servo%u",ch); uint8_t sv=p.getUChar(k,0); servoOperationMode[ch]=(sv<=SERVO_OPERATION_VALVE)?(ServoOperationMode)sv:SERVO_OPERATION_REGULATOR;
      }
      if(ok){for(uint8_t ch=0;ch<MAX_CHANNELS;ch++)if(!validateChannel(channels[ch])){ok=false;break;}}
    }
  }
  p.end();
  return ok;
}

bool readOldConfigurationSlotV17(const char* ns){Preferences p;if(!p.begin(ns,true))return false;bool ok=p.getBool("configured",false)&&p.getUChar("cfgVer",0)==17;if(ok){String au=p.getString("authUser","");String ap=p.getString("authPass","");String dn=p.getString("devName","");String aps=p.getString("apPass","");String ss=p.getString("staSsid","");String sp=p.getString("staPass","");if(!au.length()||!ap.length()||!dn.length()||dn.length()>=sizeof(deviceName)||aps.length()<MIN_AP_PASSWORD_LEN||aps.length()>=sizeof(apPassword))ok=false;if(ok){strncpy(authUser,au.c_str(),sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,ap.c_str(),sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;strncpy(apPassword,aps.c_str(),sizeof(apPassword)-1);apPassword[sizeof(apPassword)-1]=0;strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;staConfigured=p.getBool("staCfg",false);if(p.getBytesLength("outRom")==8)p.getBytes("outRom",outdoorSensorRom,8);else memset(outdoorSensorRom,0,8);}}if(ok){for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){char k[8];snprintf(k,sizeof(k),"ch%u",ch);if(p.getBytesLength(k)!=sizeof(ChannelConfig)||p.getBytes(k,&channels[ch],sizeof(ChannelConfig))!=sizeof(ChannelConfig)){ok=false;break;}snprintf(k,sizeof(k),"inv%u",ch);heaterInverted[ch]=p.getBool(k,false);templateLogic[ch]=TEMPLATE_LOGIC_START;applyTemplateLogicToSlots(ch);}}p.end();return ok;}
bool readOldConfigurationSlotV16(const char* ns){
  Preferences p;if(!p.begin(ns,true))return false;bool ok=p.getBool("configured",false)&&p.getUChar("cfgVer",0)==16;
  if(ok){String au=p.getString("authUser","");String ap=p.getString("authPass","");String dn=p.getString("devName","");String aps=p.getString("apPass","");String ss=p.getString("staSsid","");String sp=p.getString("staPass","");if(!au.length()||!ap.length()||!dn.length()||dn.length()>=sizeof(deviceName)||aps.length()<MIN_AP_PASSWORD_LEN||aps.length()>=sizeof(apPassword))ok=false;if(ok){strncpy(authUser,au.c_str(),sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,ap.c_str(),sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;strncpy(apPassword,aps.c_str(),sizeof(apPassword)-1);apPassword[sizeof(apPassword)-1]=0;strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;staConfigured=p.getBool("staCfg",false);if(p.getBytesLength("outRom")==8)p.getBytes("outRom",outdoorSensorRom,8);else memset(outdoorSensorRom,0,8);}}
  if(ok){for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){char k[8];OldChannelConfigV16 old;snprintf(k,sizeof(k),"ch%u",ch);if(p.getBytesLength(k)!=sizeof(old)||p.getBytes(k,&old,sizeof(old))!=sizeof(old)){ok=false;break;}memset(&channels[ch],0,sizeof(ChannelConfig));memcpy(channels[ch].name,old.name,sizeof(channels[ch].name));channels[ch].actuator=old.actuator;channels[ch].mode=old.mode;channels[ch].comfortTemperature=old.comfortTemperature;channels[ch].economyTemperature=old.economyTemperature;channels[ch].hysteresis=old.hysteresis;channels[ch].sensorIndex=old.sensorIndex;memcpy(channels[ch].sensorRom,old.sensorRom,8);channels[ch].valveOpenTime=old.valveOpenTime;channels[ch].valveCloseTime=old.valveCloseTime;channels[ch].valveFeedback=old.valveFeedback;memcpy(channels[ch].dayType,old.dayType,sizeof(channels[ch].dayType));for(uint8_t d=0;d<7;d++)for(uint8_t ss=0;ss<6;ss++){channels[ch].working.days[d].slots[ss].hour=old.working.days[d].slots[ss].hour;channels[ch].working.days[d].slots[ss].minute=old.working.days[d].slots[ss].minute;channels[ch].working.days[d].slots[ss].mode=old.working.days[d].slots[ss].mode;channels[ch].working.days[d].slots[ss].enabled=old.working.days[d].slots[ss].enabled;channels[ch].working.days[d].slots[ss].transitionType=SCHEDULE_START;channels[ch].holiday.days[d].slots[ss].hour=old.holiday.days[d].slots[ss].hour;channels[ch].holiday.days[d].slots[ss].minute=old.holiday.days[d].slots[ss].minute;channels[ch].holiday.days[d].slots[ss].mode=old.holiday.days[d].slots[ss].mode;channels[ch].holiday.days[d].slots[ss].enabled=old.holiday.days[d].slots[ss].enabled;channels[ch].holiday.days[d].slots[ss].transitionType=SCHEDULE_START;}if(!validateChannel(channels[ch])){ok=false;break;}}}
  p.end();return ok;
}
bool readOldConfigurationSlotV15(const char* ns){
  Preferences p;if(!p.begin(ns,true))return false;bool ok=p.getBool("configured",false)&&p.getUChar("cfgVer",0)==15;
  if(ok){String au=p.getString("authUser","");String ap=p.getString("authPass","");String dn=p.getString("devName","");String aps=p.getString("apPass","");String ss=p.getString("staSsid","");String sp=p.getString("staPass","");if(!au.length()||!ap.length()||!dn.length()||dn.length()>=sizeof(deviceName)||aps.length()<MIN_AP_PASSWORD_LEN||aps.length()>=sizeof(apPassword))ok=false;if(ok){strncpy(authUser,au.c_str(),sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,ap.c_str(),sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;strncpy(apPassword,aps.c_str(),sizeof(apPassword)-1);apPassword[sizeof(apPassword)-1]=0;strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;staConfigured=p.getBool("staCfg",false);if(p.getBytesLength("outRom")==8)p.getBytes("outRom",outdoorSensorRom,8);else memset(outdoorSensorRom,0,8);}}
  if(ok){for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){char k[8];OldChannelConfigV16 old;snprintf(k,sizeof(k),"ch%u",ch);if(p.getBytesLength(k)!=sizeof(old)||p.getBytes(k,&old,sizeof(old))!=sizeof(old)){ok=false;break;}memset(&channels[ch],0,sizeof(ChannelConfig));memcpy(channels[ch].name,old.name,sizeof(channels[ch].name));channels[ch].actuator=old.actuator;channels[ch].mode=old.mode;channels[ch].comfortTemperature=old.comfortTemperature;channels[ch].economyTemperature=old.economyTemperature;channels[ch].hysteresis=old.hysteresis;channels[ch].sensorIndex=old.sensorIndex;memcpy(channels[ch].sensorRom,old.sensorRom,8);channels[ch].valveOpenTime=old.valveOpenTime;channels[ch].valveCloseTime=old.valveCloseTime;channels[ch].valveFeedback=old.valveFeedback;memcpy(channels[ch].dayType,old.dayType,sizeof(channels[ch].dayType));for(uint8_t d=0;d<7;d++)for(uint8_t ss=0;ss<6;ss++){channels[ch].working.days[d].slots[ss].hour=old.working.days[d].slots[ss].hour;channels[ch].working.days[d].slots[ss].minute=old.working.days[d].slots[ss].minute;channels[ch].working.days[d].slots[ss].mode=old.working.days[d].slots[ss].mode;channels[ch].working.days[d].slots[ss].enabled=old.working.days[d].slots[ss].enabled;channels[ch].working.days[d].slots[ss].transitionType=SCHEDULE_START;channels[ch].holiday.days[d].slots[ss].hour=old.holiday.days[d].slots[ss].hour;channels[ch].holiday.days[d].slots[ss].minute=old.holiday.days[d].slots[ss].minute;channels[ch].holiday.days[d].slots[ss].mode=old.holiday.days[d].slots[ss].mode;channels[ch].holiday.days[d].slots[ss].enabled=old.holiday.days[d].slots[ss].enabled;channels[ch].holiday.days[d].slots[ss].transitionType=SCHEDULE_START;}if(!validateChannel(channels[ch])){ok=false;break;}}}
  p.end();return ok;
}
bool readOldConfigurationSlotV14(const char* ns){
  Preferences p;if(!p.begin(ns,true))return false;bool ok=p.getBool("configured",false)&&p.getUChar("cfgVer",0)==14;
  if(ok){String au=p.getString("authUser","");String ap=p.getString("authPass","");String dn=p.getString("devName","");String ss=p.getString("staSsid","");String sp=p.getString("staPass","");if(!au.length()||!ap.length()||!dn.length()||dn.length()>=sizeof(deviceName))ok=false;if(ok){strncpy(authUser,au.c_str(),sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,ap.c_str(),sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;staConfigured=p.getBool("staCfg",false);strncpy(apPassword,DEFAULT_AP_PASSWORD,sizeof(apPassword)-1);apPassword[sizeof(apPassword)-1]=0;}}
  if(ok){for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){char k[8];OldChannelConfigV14 old;snprintf(k,sizeof(k),"ch%u",ch);if(p.getBytesLength(k)!=sizeof(old)||p.getBytes(k,&old,sizeof(old))!=sizeof(old)){ok=false;break;}memset(&channels[ch],0,sizeof(ChannelConfig));memcpy(channels[ch].name,old.name,sizeof(channels[ch].name));channels[ch].actuator=old.actuator;channels[ch].mode=old.mode;channels[ch].comfortTemperature=old.comfortTemperature;channels[ch].economyTemperature=old.economyTemperature;channels[ch].hysteresis=old.hysteresis;channels[ch].sensorIndex=old.sensorIndex;memcpy(channels[ch].sensorRom,old.sensorRom,8);channels[ch].valveOpenTime=old.valveOpenTime;channels[ch].valveCloseTime=old.valveCloseTime;channels[ch].valveFeedback=old.valveFeedback;memcpy(channels[ch].dayType,old.dayType,sizeof(channels[ch].dayType));for(uint8_t d=0;d<7;d++)for(uint8_t ss=0;ss<6;ss++){channels[ch].working.days[d].slots[ss].hour=old.working.days[d].slots[ss].hour;channels[ch].working.days[d].slots[ss].minute=old.working.days[d].slots[ss].minute;channels[ch].working.days[d].slots[ss].mode=old.working.days[d].slots[ss].mode;channels[ch].working.days[d].slots[ss].enabled=old.working.days[d].slots[ss].enabled;channels[ch].working.days[d].slots[ss].transitionType=SCHEDULE_START;channels[ch].holiday.days[d].slots[ss].hour=old.holiday.days[d].slots[ss].hour;channels[ch].holiday.days[d].slots[ss].minute=old.holiday.days[d].slots[ss].minute;channels[ch].holiday.days[d].slots[ss].mode=old.holiday.days[d].slots[ss].mode;channels[ch].holiday.days[d].slots[ss].enabled=old.holiday.days[d].slots[ss].enabled;channels[ch].holiday.days[d].slots[ss].transitionType=SCHEDULE_START;}if(!validateChannel(channels[ch])){ok=false;break;}}}
  p.end();return ok;
}

bool readOldConfigurationSlotV13(const char* ns){
  Preferences p;if(!p.begin(ns,true))return false;bool ok=p.getBool("configured",false)&&p.getUChar("cfgVer",0)==13;
  if(ok){String au=p.getString("authUser","");String ap=p.getString("authPass","");String dn=p.getString("devName","");if(!au.length()||!ap.length()||!dn.length()||dn.length()>=sizeof(deviceName))ok=false;String ss=p.getString("staSsid","");String sp=p.getString("staPass","");if(ok){strncpy(authUser,au.c_str(),sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,ap.c_str(),sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;staConfigured=p.getBool("staCfg",false);}}
  if(ok){for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){char k[8];OldChannelConfigV13 old;snprintf(k,sizeof(k),"ch%u",ch);if(p.getBytesLength(k)!=sizeof(old)||p.getBytes(k,&old,sizeof(old))!=sizeof(old)){ok=false;break;}memset(&channels[ch],0,sizeof(ChannelConfig));memcpy(channels[ch].name,old.name,sizeof(channels[ch].name));channels[ch].actuator=old.actuator;channels[ch].mode=old.mode;channels[ch].comfortTemperature=old.comfortTemperature;channels[ch].economyTemperature=old.economyTemperature;channels[ch].hysteresis=old.hysteresis;channels[ch].sensorIndex=old.sensorIndex;memcpy(channels[ch].sensorRom,old.sensorRom,8);channels[ch].valveOpenTime=old.valveOpenTime;channels[ch].valveCloseTime=old.valveCloseTime;channels[ch].valveFeedback=VALVE_FEEDBACK_TIME;memcpy(channels[ch].dayType,old.dayType,sizeof(channels[ch].dayType));for(uint8_t d=0;d<7;d++)for(uint8_t ss=0;ss<6;ss++){channels[ch].working.days[d].slots[ss].hour=old.working.days[d].slots[ss].hour;channels[ch].working.days[d].slots[ss].minute=old.working.days[d].slots[ss].minute;channels[ch].working.days[d].slots[ss].mode=old.working.days[d].slots[ss].mode;channels[ch].working.days[d].slots[ss].enabled=old.working.days[d].slots[ss].enabled;channels[ch].working.days[d].slots[ss].transitionType=SCHEDULE_START;channels[ch].holiday.days[d].slots[ss].hour=old.holiday.days[d].slots[ss].hour;channels[ch].holiday.days[d].slots[ss].minute=old.holiday.days[d].slots[ss].minute;channels[ch].holiday.days[d].slots[ss].mode=old.holiday.days[d].slots[ss].mode;channels[ch].holiday.days[d].slots[ss].enabled=old.holiday.days[d].slots[ss].enabled;channels[ch].holiday.days[d].slots[ss].transitionType=SCHEDULE_START;}if(!validateChannel(channels[ch])){ok=false;break;}}}
  p.end();return ok;
}

bool readOldConfigurationSlotV12(const char* ns){
  Preferences p;if(!p.begin(ns,true))return false;
  bool ok=p.getBool("configured",false)&&p.getUChar("cfgVer",0)==12;
  if(ok){
    String au=p.getString("authUser","");String ap=p.getString("authPass","");String dn=p.getString("devName","");
    if(!au.length()||!ap.length()||!dn.length()||dn.length()>=sizeof(deviceName))ok=false;
    if(ok){strncpy(authUser,au.c_str(),sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,ap.c_str(),sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;String ss=p.getString("staSsid","");String sp=p.getString("staPass","");strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;staConfigured=p.getBool("staCfg",false);
    }
  }
  if(ok){for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){char k[8];OldChannelConfigV12 old;snprintf(k,sizeof(k),"ch%u",ch);if(p.getBytesLength(k)!=sizeof(old)||p.getBytes(k,&old,sizeof(old))!=sizeof(old)){ok=false;break;}copyOldV12ToCurrent(old,channels[ch]);if(!validateChannel(channels[ch])){ok=false;break;}}}
  p.end();return ok;
}

bool getActiveSlot(uint8_t &slot){
  Preferences p;
  if(!p.begin(STORAGE_META,true)) return false;
  bool ok=p.getBool("valid",false);
  uint8_t s=p.getUChar("active",0);
  p.end();
  if(!ok||s>1)return false;
  slot=s; return true;
}

bool setActiveSlot(uint8_t slot){
  Preferences p;
  if(!p.begin(STORAGE_META,false)) return false;
  bool ok=p.putUChar("active",slot)==1;
  if(ok) ok &= p.putBool("valid",true)==1;
  p.end();
  return ok;
}

void loadLegacyConfiguration(bool &ok, bool &needsSave){
  Preferences p;
  if(!p.begin(LEGACY_NAMESPACE,true)){ok=false;needsSave=true;return;}
  ok=p.getBool("configured",false);
  uint8_t storedVersion=p.getUChar("cfgVer",0);
  if(ok){
    String au=p.getString("authUser",authUser); strncpy(authUser,au.c_str(),sizeof(authUser)-1); authUser[sizeof(authUser)-1]=0;
    String ap=p.getString("authPass",authPass); strncpy(authPass,ap.c_str(),sizeof(authPass)-1); authPass[sizeof(authPass)-1]=0;
    if(!strlen(authUser)||!strlen(authPass)) setDefaultAuth();
    String dn=p.getString("devName",deviceName); strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1); deviceName[sizeof(deviceName)-1]=0;
    if(!strlen(deviceName)){setDefaultNetwork();needsSave=true;}
    String ss=p.getString("staSsid",staSSID); strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1); staSSID[sizeof(staSSID)-1]=0;
    String sp=p.getString("staPass",staPassword); strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1); staPassword[sizeof(staPassword)-1]=0;
    staConfigured=p.getBool("staCfg",false);
    for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){
      char k[16];
      snprintf(k,sizeof(k),"name%u",ch); String n=p.getString(k,channels[ch].name); strncpy(channels[ch].name,n.c_str(),sizeof(channels[ch].name)-1); channels[ch].name[sizeof(channels[ch].name)-1]=0;
      snprintf(k,sizeof(k),"act%u",ch); channels[ch].actuator=(ActuatorType)p.getUChar(k,channels[ch].actuator);
      snprintf(k,sizeof(k),"mode%u",ch); channels[ch].mode=(OperatingMode)p.getUChar(k,channels[ch].mode);
      snprintf(k,sizeof(k),"sensor%u",ch); channels[ch].sensorIndex=p.getUChar(k,channels[ch].sensorIndex);
      snprintf(k,sizeof(k),"rom%u",ch); bool hasRom=p.getBytesLength(k)==8; if(hasRom) p.getBytes(k,channels[ch].sensorRom,8);
      if(!hasRom && channels[ch].sensorIndex<sensorCount){memcpy(channels[ch].sensorRom,sensorAddresses[channels[ch].sensorIndex],8);needsSave=true;}
      snprintf(k,sizeof(k),"comfort%u",ch); channels[ch].comfortTemperature=p.getFloat(k,channels[ch].comfortTemperature);
      snprintf(k,sizeof(k),"economy%u",ch); channels[ch].economyTemperature=p.getFloat(k,channels[ch].economyTemperature);
      snprintf(k,sizeof(k),"hyst%u",ch); channels[ch].hysteresis=p.getFloat(k,channels[ch].hysteresis);
      snprintf(k,sizeof(k),"open%u",ch); channels[ch].valveOpenTime=p.getUInt(k,channels[ch].valveOpenTime);
      snprintf(k,sizeof(k),"close%u",ch); channels[ch].valveCloseTime=p.getUInt(k,channels[ch].valveCloseTime);
      initCalendarDefaults(channels[ch]);
      for(uint8_t d=0;d<7;d++){
        uint8_t idx=ch*7+d;
        snprintf(k,sizeof(k),"dt%02u",idx); channels[ch].dayType[d]=(DayType)p.getUChar(k,channels[ch].dayType[d]);
        for(uint8_t slot=0;slot<6;slot++){
          ScheduleSlot *dst=(channels[ch].dayType[d]==DAY_HOLIDAY)?&channels[ch].holiday.days[d].slots[slot]:&channels[ch].working.days[d].slots[slot];
          snprintf(k,sizeof(k),"e%02u%u",idx,slot); dst->enabled=p.getBool(k,false);
          snprintf(k,sizeof(k),"h%02u%u",idx,slot); dst->hour=p.getUChar(k,0);
          snprintf(k,sizeof(k),"m%02u%u",idx,slot); dst->minute=p.getUChar(k,0);
          snprintf(k,sizeof(k),"o%02u%u",idx,slot); dst->mode=(OperatingMode)p.getUChar(k,MODE_ECONOMY);
        }
      }
      if(channels[ch].actuator>ACTUATOR_HEATER){channels[ch].actuator=ACTUATOR_VALVE;needsSave=true;}
      if(channels[ch].mode>MODE_MANUAL){channels[ch].mode=MODE_AUTO;needsSave=true;}
      if(!strlen(channels[ch].name)||strlen(channels[ch].name)>=sizeof(channels[ch].name)){
        const char* dn[MAX_CHANNELS]={DEFAULT_CH1_NAME,DEFAULT_CH2_NAME,DEFAULT_CH3_NAME,DEFAULT_CH4_NAME,DEFAULT_CH5_NAME,DEFAULT_CH6_NAME};
        strncpy(channels[ch].name,dn[ch],sizeof(channels[ch].name)-1);channels[ch].name[sizeof(channels[ch].name)-1]=0;needsSave=true;
      }
      if(!validateChannel(channels[ch])){
        channels[ch].comfortTemperature=22;channels[ch].economyTemperature=19;channels[ch].hysteresis=.3f;channels[ch].valveOpenTime=90;channels[ch].valveCloseTime=90;initCalendarDefaults(channels[ch]);needsSave=true;
      }
    }
    if(storedVersion!=CONFIG_VERSION) needsSave=true;
  }
  p.end();
}

void clearLegacyStorage(){Preferences p;if(p.begin(LEGACY_NAMESPACE,false)){p.clear();p.end();}}
void clearCompactStorage(){for(const char*ns:{STORAGE_SLOT0,STORAGE_SLOT1,STORAGE_META}){Preferences p;if(p.begin(ns,false)){p.clear();p.end();}}}
void clearV11CompactStorage(){Preferences p;if(p.begin(STORAGE_NAMESPACE,false)){p.clear();p.end();}}

void loadConfiguration(){
  setDefaultConfig(); setDefaultAuth(); setDefaultNetwork();
  uint8_t active=0; bool loaded=false;
  if(getActiveSlot(active)){
    loaded=readConfigurationSlot(active?STORAGE_SLOT1:STORAGE_SLOT0);
    if(!loaded) loaded=readConfigurationSlot(active?STORAGE_SLOT0:STORAGE_SLOT1);
  } else {
    loaded=readConfigurationSlot(STORAGE_SLOT0);
    if(!loaded) loaded=readConfigurationSlot(STORAGE_SLOT1);
  }
  if(loaded) return;
  setDefaultConfig(); setDefaultAuth(); setDefaultNetwork();

  // V0.17 migration: preserve the existing channel configuration and initialize the new per-channel template logic to START.
  if(getActiveSlot(active)){
    if(readOldConfigurationSlotV17(active?STORAGE_SLOT1:STORAGE_SLOT0)||readOldConfigurationSlotV17(active?STORAGE_SLOT0:STORAGE_SLOT1)){ if(saveConfiguration()) return; }
  } else if(readOldConfigurationSlotV17(STORAGE_SLOT0)||readOldConfigurationSlotV17(STORAGE_SLOT1)){
    if(saveConfiguration()) return;
  }

  // V0.15 migration: preserve channel/AP configuration and add the dedicated outdoor sensor ROM.
  if(getActiveSlot(active)){
    if(readOldConfigurationSlotV16(active?STORAGE_SLOT1:STORAGE_SLOT0)||readOldConfigurationSlotV16(active?STORAGE_SLOT0:STORAGE_SLOT1)){ if(saveConfiguration()) return; }
  if(readOldConfigurationSlotV15(active?STORAGE_SLOT1:STORAGE_SLOT0)||readOldConfigurationSlotV15(active?STORAGE_SLOT0:STORAGE_SLOT1)){
      if(saveConfiguration()){Serial.println("Migrated V0.15 configuration to V0.14.2 (outdoor sensor)");return;}
    }
  } else if(readOldConfigurationSlotV15(STORAGE_SLOT0)||readOldConfigurationSlotV15(STORAGE_SLOT1)){
    if(saveConfiguration()){Serial.println("Migrated V0.15 configuration to V0.14.2 (outdoor sensor)");return;}
  }

  // V0.14 migration: preserve the V0.14 channel model and add a configurable AP password.
  if(getActiveSlot(active)){
    if(readOldConfigurationSlotV14(active?STORAGE_SLOT1:STORAGE_SLOT0)||readOldConfigurationSlotV14(active?STORAGE_SLOT0:STORAGE_SLOT1)){
      if(saveConfiguration()){Serial.println("Migrated V0.14 configuration to V0.14.1 (AP password support)");return;}
    }
  } else if(readOldConfigurationSlotV14(STORAGE_SLOT0)||readOldConfigurationSlotV14(STORAGE_SLOT1)){
    if(saveConfiguration()){Serial.println("Migrated V0.14 configuration to V0.14.1 (AP password support)");return;}
  }

  // V0.13 migration: add per-channel valve feedback mode. Existing channels default to TIME.
  if(getActiveSlot(active)){
    if(readOldConfigurationSlotV13(active?STORAGE_SLOT1:STORAGE_SLOT0)||readOldConfigurationSlotV13(active?STORAGE_SLOT0:STORAGE_SLOT1)){
      if(saveConfiguration()){Serial.println("Migrated V0.13 valve feedback settings to V0.14 (TIME)");return;}
    }
  } else if(readOldConfigurationSlotV13(STORAGE_SLOT0)||readOldConfigurationSlotV13(STORAGE_SLOT1)){
    if(saveConfiguration()){Serial.println("Migrated V0.13 valve feedback settings to V0.14 (TIME)");return;}
  }

  // V0.12 transactional binary migration: convert the old per-day schedule into
  // separate WORKING/HOLIDAY templates, preserving each day's currently active schedule.
  if(getActiveSlot(active)){
    if(readOldConfigurationSlotV12(active?STORAGE_SLOT1:STORAGE_SLOT0)||readOldConfigurationSlotV12(active?STORAGE_SLOT0:STORAGE_SLOT1)){
      if(saveConfiguration()){Serial.println("Migrated V0.12 calendar to WORKING/HOLIDAY templates");return;}
    }
  } else if(readOldConfigurationSlotV12(STORAGE_SLOT0)||readOldConfigurationSlotV12(STORAGE_SLOT1)){
    if(saveConfiguration()){Serial.println("Migrated V0.12 calendar to WORKING/HOLIDAY templates");return;}
  }

  // V0.11 compact namespace migration: import only after transactional slots are verified.
  Preferences old;
  if(old.begin(STORAGE_NAMESPACE,true)){
    bool oldOk=old.getBool("configured",false) && old.getUChar("cfgVer",0)==11;
    old.end();
    if(oldOk){
      Preferences p;
      if(p.begin(STORAGE_NAMESPACE,true)){
        String au=p.getString("authUser",authUser);strncpy(authUser,au.c_str(),sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;
        String ap=p.getString("authPass",authPass);strncpy(authPass,ap.c_str(),sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;
        String dn=p.getString("devName",deviceName);strncpy(deviceName,dn.c_str(),sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;
        String ss=p.getString("staSsid",staSSID);strncpy(staSSID,ss.c_str(),sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;
        String sp=p.getString("staPass",staPassword);strncpy(staPassword,sp.c_str(),sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;
        staConfigured=p.getBool("staCfg",false);
        bool valid=true;for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){char k[8];OldChannelConfigV12 oldCfg;snprintf(k,sizeof(k),"ch%u",ch);if(p.getBytesLength(k)!=sizeof(oldCfg)||p.getBytes(k,&oldCfg,sizeof(oldCfg))!=sizeof(oldCfg)){valid=false;break;}copyOldV11ToCurrent(oldCfg,channels[ch]);}
        p.end();
        for(uint8_t ch=0;ch<MAX_CHANNELS;ch++)if(!validateChannel(channels[ch]))valid=false;
        if(valid && saveConfiguration()){clearV11CompactStorage();return;}
      }
    }
  }

  bool legacyOk=false, needsSave=false;
  loadLegacyConfiguration(legacyOk,needsSave);
  if(legacyOk){
    if(saveConfiguration()) clearLegacyStorage();
    else Serial.println("ERROR: transactional NVS migration failed; legacy settings retained");
    return;
  }
  if(!saveConfiguration()) Serial.println("ERROR: initial transactional NVS save failed");
}

bool storageSelfTest(){
  uint8_t active=0;if(!getActiveSlot(active))return false;
  return readConfigurationSlot(active?STORAGE_SLOT1:STORAGE_SLOT0);
}

bool rtcTimeValid(){return rtcAvailable && !rtc.lostPower();}

uint8_t calendarDayIndex(){if(!rtcTimeValid())return 0;uint8_t dow=rtc.now().dayOfTheWeek();return (dow+6)%7;} // RTClib: Sunday=0 -> Monday=0
uint32_t reachDateKey(const DateTime& dt){return (uint32_t)dt.year()*10000UL+(uint32_t)dt.month()*100UL+(uint32_t)dt.day();}
int reachOutdoorBin(float outdoor){
  if(isnan(outdoor)) return -1;
  if(outdoor < -10.0f) return 0;
  if(outdoor < 0.0f) return 1;
  if(outdoor < 10.0f) return 2;
  if(outdoor < 20.0f) return 3;
  return 4;
}

void resetReachLearningChannel(uint8_t ch){
  if(ch>=MAX_CHANNELS)return;
  memset(&reachLearning[ch],0,sizeof(ReachLearningStats));
  reachLearning[ch].globalMinutesPerDegree=10.0f;
  for(uint8_t i=0;i<REACH_BIN_COUNT;i++) reachLearning[ch].binMinutesPerDegree[i]=10.0f;
  if(ch<MAX_CHANNELS){
    runtime[ch].reachLearningActive=false; runtime[ch].reachLearningSlot=255;
    runtime[ch].reachLearningDateKey=0; runtime[ch].reachLearningDeadlineMinutes=-1; runtime[ch].reachLearningStartedAt=0;
    runtime[ch].reachLearningInitialTemp=NAN; runtime[ch].reachLearningOutdoor=NAN; runtime[ch].reachLearningTarget=NAN;
  }
}
void resetReachLearning(){
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++) resetReachLearningChannel(ch);
  Preferences p; if(p.begin(REACH_LEARN_NAMESPACE,false)){p.clear();p.end();}
}
void loadReachLearning(){
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++) resetReachLearningChannel(ch);
  Preferences p; if(!p.begin(REACH_LEARN_NAMESPACE,true)) return;
  for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){
    char key[5]; snprintf(key,sizeof(key),"ch%u",ch);
    if(p.getBytesLength(key)==sizeof(ReachLearningStats)) p.getBytes(key,&reachLearning[ch],sizeof(ReachLearningStats));
    if(reachLearning[ch].globalMinutesPerDegree<1.0f||reachLearning[ch].globalMinutesPerDegree>30.0f) resetReachLearningChannel(ch);
    for(uint8_t b=0;b<REACH_BIN_COUNT;b++) if(reachLearning[ch].binMinutesPerDegree[b]<1.0f||reachLearning[ch].binMinutesPerDegree[b]>30.0f) reachLearning[ch].binMinutesPerDegree[b]=reachLearning[ch].globalMinutesPerDegree;
  }
  p.end();
}
bool saveReachLearningChannel(uint8_t ch){
  if(ch>=MAX_CHANNELS)return false;
  Preferences p; if(!p.begin(REACH_LEARN_NAMESPACE,false)) return false;
  char key[5]; snprintf(key,sizeof(key),"ch%u",ch);
  size_t written=p.putBytes(key,&reachLearning[ch],sizeof(ReachLearningStats));
  bool ok=(written==sizeof(ReachLearningStats));
  p.end();
  return ok;
}
float learnedMinutesPerDegree(uint8_t ch){
  if(ch>=MAX_CHANNELS)return 10.0f;
  const auto &st=reachLearning[ch];
  float base=(st.samples>0)?st.globalMinutesPerDegree:10.0f;
  int bin=outdoorSensorOK?reachOutdoorBin(outdoorTemperature):-1;
  if(st.samples==0) return constrain(base,3.0f,25.0f);

  // Richer model: use the most similar historical heating samples.
  // Similarity considers initial indoor temperature, target temperature and outdoor temperature.
  float weighted=0.0f, weightSum=0.0f;
  uint8_t used=0;
  for(uint8_t i=0;i<12;i++){
    const ReachLearningSample &h=st.history[i];
    if(h.minutes<=0.0f || h.target<=-40.0f || h.target>=80.0f || h.initialIndoor<=-55.0f || h.initialIndoor>=125.0f) continue;
    float historicalDelta=h.target-h.initialIndoor;
    if(historicalDelta<=0.25f) continue;
    // Historical samples store total minutes, but the model predicts minutes/°C.
    // Normalize each sample before applying similarity weighting.
    float historicalMpd=h.minutes/historicalDelta;
    if(historicalMpd<1.0f || historicalMpd>30.0f) continue;
    float dIndoor=fabsf(runtime[ch].temperature-h.initialIndoor);
    float dTarget=fabsf(channels[ch].comfortTemperature-h.target);
    float dOutdoor=0.0f;
    if(outdoorSensorOK && !isnan(h.outdoor)) dOutdoor=fabsf(outdoorTemperature-h.outdoor);
    else if(outdoorSensorOK || !isnan(h.outdoor)) dOutdoor=8.0f;
    float distance=1.0f + dIndoor/3.0f + dTarget/2.0f + dOutdoor/10.0f;
    float w=1.0f/distance;
    weighted += w*historicalMpd;
    weightSum += w;
    used++;
  }
  if(weightSum>0.0f && used>=2){
    float v=weighted/weightSum;
    // Keep a small anchor to the long-term EWMA for stability.
    v=0.8f*v+0.2f*base;
    return constrain(v,3.0f,25.0f);
  }
  if(bin>=0 && st.binSamples[bin]>=2){
    float v=0.7f*st.binMinutesPerDegree[bin]+0.3f*base;
    return constrain(v,3.0f,25.0f);
  }
  return constrain(base,3.0f,25.0f);
}
int transitionLeadMinutes(uint8_t ch,const ScheduleSlot& slot){
  if(ch>=MAX_CHANNELS||templateLogic[ch]!=TEMPLATE_LOGIC_REACH)return 0;
  if(slot.mode==MODE_ECONOMY)return 0;
  if(!runtime[ch].sensorOK)return 60;
  float target=channels[ch].comfortTemperature;
  float delta=target-runtime[ch].temperature;
  if(delta<=channels[ch].hysteresis)return 0;
  int lead=(int)ceil(delta*learnedMinutesPerDegree(ch));
  return constrain(lead,5,120);
}
int reachEffectiveStartMinutes(uint8_t ch,uint8_t slotIndex,const ScheduleSlot& slot,const DateTime& nowDt){
  if(ch>=MAX_CHANNELS||templateLogic[ch]!=TEMPLATE_LOGIC_REACH)return slot.hour*60+slot.minute;
  int deadline=slot.hour*60+slot.minute;
  if(slot.mode==MODE_ECONOMY)return deadline;
  uint32_t key=reachDateKey(nowDt);
  if(runtime[ch].reachCacheDateKey!=key){runtime[ch].reachCacheDateKey=key;runtime[ch].reachCacheValidMask=0;runtime[ch].reachActivatedMask=0;runtime[ch].reachLearningActive=false;runtime[ch].reachLearningSlot=255;}
  uint8_t bit=(uint8_t)(1u<<slotIndex);
  if(!(runtime[ch].reachCacheValidMask&bit)){
    int lead=transitionLeadMinutes(ch,slot); int start=deadline-lead; if(start<0)start=0;
    runtime[ch].reachStartMinutes[slotIndex]=start; runtime[ch].reachCacheValidMask|=bit;
  }
  int start=runtime[ch].reachStartMinutes[slotIndex]; int nowMinutes=nowDt.hour()*60+nowDt.minute();
  if(runtime[ch].reachActivatedMask&bit)return start;
  if(nowMinutes>=start){runtime[ch].reachActivatedMask|=bit;return start;}
  return start;
}
struct ActiveScheduleEvent { bool valid; uint8_t slot; ScheduleTransitionType transition; OperatingMode mode; int effectiveMinutes; int deadlineMinutes; };
ActiveScheduleEvent activeScheduleEvent(uint8_t ch,const DateTime& nowDt){
  ActiveScheduleEvent out{false,255,SCHEDULE_START,MODE_ECONOMY,-1,-1};
  if(ch>=MAX_CHANNELS)return out;
  uint8_t d=(nowDt.dayOfTheWeek()+6)%7;
  const WeekTemplate& tpl=(channels[ch].dayType[d]==DAY_HOLIDAY)?channels[ch].holiday:channels[ch].working;
  const DaySchedule& day=tpl.days[d]; int now=nowDt.hour()*60+nowDt.minute();
  for(uint8_t s=0;s<6;s++){const auto&x=day.slots[s];if(!x.enabled)continue;int effective=reachEffectiveStartMinutes(ch,s,x,nowDt);if(effective<=now&&effective>=out.effectiveMinutes){ScheduleTransitionType tr=(templateLogic[ch]==TEMPLATE_LOGIC_REACH && x.mode==MODE_COMFORT)?SCHEDULE_REACH:SCHEDULE_START;out={true,s,tr,x.mode,effective,x.hour*60+x.minute};}}
  return out;
}
void clearManualOverride(uint8_t ch){
  if(ch>=MAX_CHANNELS)return;
  runtime[ch].manualOverrideActive=false;
  runtime[ch].manualOverrideMode=MODE_AUTO;
  runtime[ch].manualOverrideDateKey=0;
  runtime[ch].manualOverrideEventSlot=255;
}
void refreshManualOverride(uint8_t ch){
  if(ch>=MAX_CHANNELS||!runtime[ch].manualOverrideActive)return;
  if(channels[ch].mode!=MODE_AUTO){clearManualOverride(ch);return;}
  if(!rtcTimeValid()){clearManualOverride(ch);return;}
  DateTime nowDt=rtc.now(); uint32_t dateKey=reachDateKey(nowDt);
  ActiveScheduleEvent e=activeScheduleEvent(ch,nowDt);
  // One-shot override belongs to the currently active calendar event. It is cleared
  // automatically when the next calendar event becomes effective or the date changes.
  if(dateKey!=runtime[ch].manualOverrideDateKey ||
     !e.valid || e.slot!=runtime[ch].manualOverrideEventSlot){
    clearManualOverride(ch);
  }
}
OperatingMode getAutoMode(uint8_t ch){
  if(ch>=MAX_CHANNELS||!rtcTimeValid())return MODE_ECONOMY;
  refreshManualOverride(ch);
  if(runtime[ch].manualOverrideActive)return runtime[ch].manualOverrideMode;
  DateTime nowDt=rtc.now(); ActiveScheduleEvent e=activeScheduleEvent(ch,nowDt); return e.valid?e.mode:MODE_ECONOMY;
}
OperatingMode scheduledAutoMode(uint8_t ch){
  if(ch>=MAX_CHANNELS||!rtcTimeValid())return MODE_ECONOMY;
  DateTime nowDt=rtc.now(); ActiveScheduleEvent e=activeScheduleEvent(ch,nowDt);
  return e.valid?e.mode:MODE_ECONOMY;
}
OperatingMode effectiveMode(uint8_t ch){
  if(ch>=MAX_CHANNELS)return MODE_OFF;
  if(channels[ch].mode!=MODE_AUTO)return channels[ch].mode;
  OperatingMode m=getAutoMode(ch);
  if(runtime[ch].autoInverted){if(m==MODE_COMFORT)m=MODE_ECONOMY;else if(m==MODE_ECONOMY)m=MODE_COMFORT;}
  return m;
}

void updateReachLearning(uint8_t ch){
  if(ch>=MAX_CHANNELS||!rtcTimeValid()||!runtime[ch].sensorOK)return;
  DateTime nowDt=rtc.now(); uint32_t dateKey=reachDateKey(nowDt);
  ActiveScheduleEvent e=activeScheduleEvent(ch,nowDt);
  if(runtime[ch].reachLearningActive && runtime[ch].reachLearningDateKey!=dateKey){
    runtime[ch].reachLearningActive=false; runtime[ch].reachLearningSlot=255; runtime[ch].reachLearningDeadlineMinutes=-1;
  }
  if(!runtime[ch].reachLearningActive){
    if(!e.valid || e.transition!=SCHEDULE_REACH || e.mode!=MODE_COMFORT)return;
    float target=channels[ch].comfortTemperature;
    if(runtime[ch].temperature>=target) return; // already warm; no valid heating-time sample
    // Start only from a known non-heating baseline. This prevents previous heating from
    // contaminating the measured time-to-temperature sample.
    if(channels[ch].actuator==ACTUATOR_HEATER){
      if(runtime[ch].outputState) return;
    } else {
      if(runtime[ch].valveState!=VALVE_STOPPED || !runtime[ch].positionKnown || runtime[ch].valvePosition!=0) return;
    }
    runtime[ch].reachLearningActive=true; runtime[ch].reachLearningSlot=e.slot; runtime[ch].reachLearningDateKey=dateKey;
    runtime[ch].reachLearningDeadlineMinutes=e.deadlineMinutes;
    runtime[ch].reachLearningStartedAt=millis(); runtime[ch].reachLearningInitialTemp=runtime[ch].temperature;
    runtime[ch].reachLearningOutdoor=outdoorSensorOK?outdoorTemperature:NAN; runtime[ch].reachLearningTarget=target;
    return;
  }

  // The original REACH slot must remain the active calendar event. If a later START/REACH
  // event takes over, the unfinished sample is discarded rather than attributed to the old slot.
  if(!e.valid || e.slot!=runtime[ch].reachLearningSlot || e.transition!=SCHEDULE_REACH || e.mode!=MODE_COMFORT){
    runtime[ch].reachLearningActive=false; runtime[ch].reachLearningSlot=255; runtime[ch].reachLearningDeadlineMinutes=-1;
    return;
  }
  int nowMinutes=nowDt.hour()*60+nowDt.minute();
  if(nowMinutes>runtime[ch].reachLearningDeadlineMinutes && runtime[ch].temperature<runtime[ch].reachLearningTarget){
    runtime[ch].reachLearningActive=false; runtime[ch].reachLearningSlot=255; runtime[ch].reachLearningDeadlineMinutes=-1;
    return;
  }
  if(runtime[ch].temperature>=runtime[ch].reachLearningTarget){
    unsigned long elapsedSec=(millis()-runtime[ch].reachLearningStartedAt)/1000UL;
    float delta=runtime[ch].reachLearningTarget-runtime[ch].reachLearningInitialTemp;
    if(delta>0.25f && elapsedSec>=120 && elapsedSec<=4UL*3600UL){
      float mpd=((float)elapsedSec/60.0f)/delta; mpd=constrain(mpd,1.0f,30.0f);
      ReachLearningStats &st=reachLearning[ch];
      if(st.samples==0) st.globalMinutesPerDegree=mpd; else st.globalMinutesPerDegree=0.75f*st.globalMinutesPerDegree+0.25f*mpd;
      st.samples++;
      int bin=reachOutdoorBin(runtime[ch].reachLearningOutdoor);
      if(bin>=0){ if(st.binSamples[bin]==0)st.binMinutesPerDegree[bin]=mpd; else st.binMinutesPerDegree[bin]=0.75f*st.binMinutesPerDegree[bin]+0.25f*mpd; st.binSamples[bin]++; }
      uint8_t idx=(uint8_t)((st.samples-1)%12);
      st.history[idx].initialIndoor=runtime[ch].reachLearningInitialTemp;
      st.history[idx].target=runtime[ch].reachLearningTarget;
      st.history[idx].outdoor=runtime[ch].reachLearningOutdoor;
      st.history[idx].minutes=((float)elapsedSec)/60.0f;
      saveReachLearningChannel(ch);
    }
    runtime[ch].reachLearningActive=false; runtime[ch].reachLearningSlot=255; runtime[ch].reachLearningDeadlineMinutes=-1;
  }
}

float targetTemperature(uint8_t ch){OperatingMode m=effectiveMode(ch);if(m==MODE_COMFORT)return channels[ch].comfortTemperature;if(m==MODE_ECONOMY)return channels[ch].economyTemperature;return NAN;}

void heaterControl(uint8_t ch){auto&c=channels[ch];auto&r=runtime[ch];if(c.actuator!=ACTUATOR_HEATER)return;if(c.mode==MODE_MANUAL){applyHeaterOutput(ch,r.outputState);return;}if(!r.sensorOK){if(r.outputState){heaterPhysicalOff(ch);r.outputState=false;r.lastOutputChange=millis();}else heaterPhysicalOff(ch);return;}float t=targetTemperature(ch);if(isnan(t)){if(r.outputState){heaterPhysicalOff(ch);r.outputState=false;r.lastOutputChange=millis();}else heaterPhysicalOff(ch);return;}bool desired=r.outputState;if(r.outputState){if(r.temperature>=t+c.hysteresis)desired=false;}else if(r.temperature<=t-c.hysteresis)desired=true;if(desired!=r.outputState&&millis()-r.lastOutputChange>=MIN_SWITCH_INTERVAL_MS){applyHeaterOutput(ch,desired);r.outputState=desired;r.lastOutputChange=millis();}digitalWrite(relayB[ch],RELAY_OFF);}
void valveControl(uint8_t ch){if(servoMaintenanceChannel==(int8_t)ch)return;auto&c=channels[ch];auto&r=runtime[ch];if(c.actuator!=ACTUATOR_VALVE)return;if(c.mode==MODE_MANUAL){updateValvePosition(ch);return;}if(c.valveFeedback==VALVE_FEEDBACK_LIMITS&&!gpioExpanderAvailable){valveStop(ch);r.valveFault=true;return;}updateValvePosition(ch);if(r.calibrating)return;if(r.valveFault){valveStop(ch);return;}if(!r.positionKnown){valveStop(ch);return;}if(effectiveMode(ch)==MODE_OFF){if(r.valvePosition>0)valveMoveToPosition(ch,0);return;}if(!r.sensorOK){valveStop(ch);return;}float t=targetTemperature(ch);if(isnan(t)){valveStop(ch);return;}float e=t-r.temperature;if(servoOperationMode[ch]==SERVO_OPERATION_VALVE){/* VALVE mode is strictly endpoint-only: never leave an intermediate position. */uint8_t endpoint=(e>=0.0f)?100:0;if(r.valvePosition!=endpoint)valveMoveToPosition(ch,endpoint);else valveStop(ch);return;}uint8_t target=regulatorTargetPosition(e);if(fabsf(e)<=c.hysteresis){valveStop(ch);return;}if(target==r.valvePosition){valveStop(ch);return;}valveMoveToPosition(ch,target);}
bool channelNeedsAux(uint8_t ch, String& reason){
  if(ch>=MAX_CHANNELS||!(auxChannelMask&(1u<<ch))||channels[ch].mode!=MODE_AUTO||!runtime[ch].sensorOK||!rtcTimeValid())return false;
  DateTime nowDt=rtc.now(); ActiveScheduleEvent e=activeScheduleEvent(ch,nowDt);
  if(!e.valid||e.transition!=SCHEDULE_REACH||e.mode!=MODE_COMFORT)return false;
  float target=channels[ch].comfortTemperature; float delta=target-runtime[ch].temperature;
  if(delta<=channels[ch].hysteresis)return false;
  uint8_t d=(nowDt.dayOfTheWeek()+6)%7;
  const WeekTemplate& tpl=(channels[ch].dayType[d]==DAY_HOLIDAY)?channels[ch].holiday:channels[ch].working;
  const ScheduleSlot& slot=tpl.days[d].slots[e.slot];
  int lead=transitionLeadMinutes(ch,slot);
  int nowMin=nowDt.hour()*60+nowDt.minute();

  // Before the deadline AUX is predictive assistance: start only when the learned
  // forecast says the channel will miss its COMFORT REACH deadline.
  if(nowMin<e.deadlineMinutes){
    if(nowMin+lead>e.deadlineMinutes){
      char t[16];snprintf(t,sizeof(t),"%02d:%02d",e.deadlineMinutes/60,e.deadlineMinutes%60);
      reason="CH"+String(ch+1)+" не встигає до REACH "+String(t);return true;
    }
    return false;
  }

  // After the deadline the user may explicitly keep AUX helping. The calendar
  // still has priority: activeScheduleEvent() must remain this same REACH event.
  if(auxPostReachPolicy==AUX_POST_OFF)return false;
  int postElapsedMin=nowMin-e.deadlineMinutes;
  if(postElapsedMin<0)return false;
  if((uint32_t)postElapsedMin*60UL>=auxPostReachMaxSeconds)return false;
  if(auxPostReachPolicy==AUX_POST_MINUTES && (uint32_t)postElapsedMin>=auxPostReachMinutes)return false;
  char t[16];snprintf(t,sizeof(t),"%02d:%02d",e.deadlineMinutes/60,e.deadlineMinutes%60);
  if(auxPostReachPolicy==AUX_POST_TO_COMFORT)reason="Post-REACH: CH"+String(ch+1)+" догрів до COMFORT після "+String(t);
  else reason="Post-REACH: CH"+String(ch+1)+" ще "+String(auxPostReachMinutes-postElapsedMin)+" хв після "+String(t);
  return true;
}
void controlAux(){
  if(!auxEnabled||auxChannelMask==0){if(auxOutputState)setAuxOutput(false,"Вимкнено",true);else strncpy(auxReason,"Вимкнено",sizeof(auxReason)-1);auxRunLimited=false;return;}
  if(auxLocationChannel>=MAX_CHANNELS||!runtime[auxLocationChannel].sensorOK){setAuxOutput(false,"Датчик зони AUX недоступний",true);return;}
  if(runtime[auxLocationChannel].temperature>=auxMaxLocationTemp){setAuxOutput(false,"Досягнуто ліміт температури AUX",true);return;}
  String reason; bool needed=false; for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){if(channelNeedsAux(ch,reason)){needed=true;break;}}
  if(!needed){setAuxOutput(false,"REACH встигає / Post-REACH завершено / допомога не потрібна",true);auxRunLimited=false;return;}
  if(auxOutputState&&millis()-auxStartedAt>=auxMaxRuntimeSeconds*1000UL){setAuxOutput(false,"Досягнуто максимальний час AUX",true);auxRunLimited=true;return;}
  if(auxRunLimited){setAuxOutput(false,"Ліміт AUX вичерпано до завершення REACH",true);return;}
  setAuxOutput(true,reason.c_str());
}
void controlChannels(){for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){updateReachLearning(ch);channels[ch].actuator==ACTUATOR_HEATER?heaterControl(ch):valveControl(ch);}controlAux();}

void jsonChannel(JsonObject o,uint8_t ch){auto&c=channels[ch];auto&r=runtime[ch];o["id"]=ch+1;o["name"]=c.name;o["actuator"]=actuatorText(c.actuator);o["actuatorType"]=(c.actuator==ACTUATOR_HEATER?(heaterInverted[ch]?"inverted":"ten"):(c.valveFeedback==VALVE_FEEDBACK_LIMITS?"limit":"timed"));o["servoMode"]=(servoOperationMode[ch]==SERVO_OPERATION_VALVE?"valve":"regulator");o["mode"]=modeText(c.mode);o["baseMode"]=modeText(c.mode);OperatingMode am=scheduledAutoMode(ch);OperatingMode em=effectiveMode(ch);o["autoMode"]=modeText(am);o["effectiveMode"]=modeText(em);o["autoInverted"]=r.autoInverted;o["templateLogic"]=templateLogicText(templateLogic[ch]);o["manualOverrideActive"]=runtime[ch].manualOverrideActive;o["manualOverrideMode"]=runtime[ch].manualOverrideActive?modeText(runtime[ch].manualOverrideMode):"";o["sensorIndex"]=c.sensorIndex;char rom[17]={0};for(int i=0;i<8;i++)sprintf(rom+i*2,"%02X",c.sensorRom[i]);o["sensorRom"]=rom;if(r.sensorOK)o["temperature"]=r.temperature;else o["temperature"]=nullptr;o["sensorOK"]=r.sensorOK;float target=targetTemperature(ch);if(isnan(target))o["target"]=nullptr;else o["target"]=target;o["comfort"]=c.comfortTemperature;o["economy"]=c.economyTemperature;o["hysteresis"]=c.hysteresis;o["openTime"]=c.valveOpenTime;o["closeTime"]=c.valveCloseTime;o["valveFeedback"]=c.valveFeedback==VALVE_FEEDBACK_LIMITS?"limits":"time";o["limitSwitchAvailable"]=gpioExpanderAvailable;o["openLimit"]=r.openLimit;o["closeLimit"]=r.closeLimit;o["valveFault"]=r.valveFault;o["valvePosition"]=r.valvePosition;o["valveFlowPercent"]=(c.actuator==ACTUATOR_VALVE&&r.positionKnown)?ballValveFlowPercent(r.valvePosition):nullptr;o["positionKnown"]=r.positionKnown;o["calibrating"]=r.calibrating;o["calibrationInitial"]=(r.calibrationInitialPosition==0?"closed":"open");o["calibrationDirection"]=r.calibrationDirection==VALVE_OPENING?"open":(r.calibrationDirection==VALVE_CLOSING?"close":"stop");o["calibrationElapsed"]=r.calibrating?(millis()-r.calibrationStartedAt)/1000UL:0;o["calibrationOpenMeasured"]=r.calibrationHasOpen?r.calibrationOpenCandidate:0;o["calibrationCloseMeasured"]=r.calibrationHasClose?r.calibrationCloseCandidate:0;o["valveMoving"]=r.valveState!=VALVE_STOPPED;o["heater"]=r.outputState;o["outputInverted"]=heaterInverted[ch];o["reachLearningSamples"]=reachLearning[ch].samples;o["reachLearningMinutesPerDegree"]=learnedMinutesPerDegree(ch);o["reachLearningActive"]=r.reachLearningActive;o["reachLearningOutdoorUsed"]=outdoorSensorOK;}

void apiReachLearningReset(){
  if(!checkAuth())return;
  if(server.hasArg("ch")){
    int ch=server.arg("ch").toInt()-1;
    if(ch<0||ch>=MAX_CHANNELS){server.send(400,"application/json","{\"ok\":false,\"error\":\"channel\"}");return;}
    resetReachLearningChannel(ch);
    if(!saveReachLearningChannel(ch)){
      server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\",\"message\":\"Не вдалося зберегти скидання Adaptive REACH\"}");
      return;
    }
  }else{
    resetReachLearning();
    bool ok=true; for(uint8_t i=0;i<MAX_CHANNELS;i++) if(!saveReachLearningChannel(i)) ok=false;
    if(!ok){server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\",\"message\":\"Не вдалося повністю зберегти скидання Adaptive REACH\"}");return;}
  }
  JsonDocument d;d["ok"]=true;String out;serializeJson(d,out);server.send(200,"application/json",out);
}
void apiReachLearning(){if(!checkAuth())return;JsonDocument d;d["outdoorAvailable"]=outdoorSensorOK;d["outdoorTemperature"]=outdoorSensorOK?outdoorTemperature:NAN;JsonArray a=d["channels"].to<JsonArray>();for(uint8_t ch=0;ch<MAX_CHANNELS;ch++){JsonObject o=a.add<JsonObject>();o["channel"]=ch+1;o["samples"]=reachLearning[ch].samples;o["minutesPerDegree"]=learnedMinutesPerDegree(ch);o["active"]=runtime[ch].reachLearningActive;o["outdoorUsed"]=outdoorSensorOK;o["historySize"]=12;JsonArray hist=o["history"].to<JsonArray>();uint8_t histCount=(uint8_t)min<uint32_t>(reachLearning[ch].samples,12);for(uint8_t n=0;n<histCount;n++){uint8_t hi=(uint8_t)((reachLearning[ch].samples-1u-n)%12u);const auto &h=reachLearning[ch].history[hi];if(h.minutes<=0.0f)continue;JsonObject hx=hist.add<JsonObject>();hx["initialIndoor"]=h.initialIndoor;hx["target"]=h.target;if(isnan(h.outdoor))hx["outdoor"]=nullptr;else hx["outdoor"]=h.outdoor;hx["minutes"]=h.minutes;float dd=h.target-h.initialIndoor;hx["minutesPerDegree"]=(dd>0.25f)?(h.minutes/dd):NAN;}for(uint8_t b=0;b<REACH_BIN_COUNT;b++){JsonObject x=o["outdoorBins"].to<JsonObject>();const char*names[5]={"below_-10","-10_to_0","0_to_10","10_to_20","above_20"};x[names[b]]=reachLearning[ch].binSamples[b];}}String out;serializeJson(d,out);server.send(200,"application/json",out);}

void apiSelfTest(){if(!checkAuth())return;JsonDocument d;bool storage=storageSelfTest();bool fs=LittleFS.exists("/index.html");d["ok"]=true;d["testMode"]=HEATCONTROL_TEST_MODE!=0;d["storage"]=storage;d["rtc"]=rtcAvailable;d["rtcTimeValid"]=rtcTimeValid();d["littlefs"]=fs;d["wifiSta"]=(WiFi.status()==WL_CONNECTED);d["sensorCount"]=sensorCount;d["sensorCountOk"]=sensorCount==MAX_SENSORS;d["gpioExpander"]=gpioExpanderAvailable;d["limitSwitchInputs"]=gpioExpanderAvailable;bool configs=true;for(uint8_t ch=0;ch<MAX_CHANNELS;ch++)if(!validateChannel(channels[ch]))configs=false;bool sensorAssignments=validateSensorAssignments();d["config"]=configs;d["sensorAssignments"]=sensorAssignments;d["outputsForcedOff"]=HEATCONTROL_TEST_MODE!=0;if(!d["storage"].as<bool>()||!configs)d["ok"]=false;String out;serializeJson(d,out);server.send(d["ok"]?200:503,"application/json",out);}

void apiServoMaintenanceGet(){if(!checkAuth())return;JsonDocument d;d["enabled"]=servoMaintenanceEnabled;d["hour"]=servoMaintenanceHour;d["minute"]=servoMaintenanceMinute;d["intervalDays"]=7;d["queuePauseSeconds"]=SERVO_MAINT_QUEUE_PAUSE_SECONDS;d["activeChannel"]=servoMaintenanceChannel>=0?servoMaintenanceChannel+1:0;d["state"]=(int)servoMaintenanceState;d["queueCount"]=servoMaintenanceQueueCount;d["queueIndex"]=servoMaintenanceQueueIndex;String out;serializeJson(d,out);server.send(200,"application/json",out);}
void apiServoMaintenancePut(){if(!checkAuth())return;JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"json\"}");return;}if(d["enabled"].is<bool>())servoMaintenanceEnabled=d["enabled"].as<bool>();if(d["hour"].is<int>()){int v=d["hour"];if(v<0||v>23){server.send(400,"application/json","{\"ok\":false,\"error\":\"hour\"}");return;}servoMaintenanceHour=(uint8_t)v;}if(d["minute"].is<int>()){int v=d["minute"];if(v<0||v>59){server.send(400,"application/json","{\"ok\":false,\"error\":\"minute\"}");return;}servoMaintenanceMinute=(uint8_t)v;}if(!saveServoMaintenance()){server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}server.send(200,"application/json","{\"ok\":true}");}

void apiStatus(){if(!checkAuth())return;JsonDocument d;d["aux"]["enabled"]=auxEnabled;d["aux"]["locationChannel"]=auxLocationChannel+1;d["aux"]["channelMask"]=auxChannelMask;d["aux"]["output"]=auxOutputState;d["aux"]["reason"]=auxReason;d["aux"]["maxRuntimeSeconds"]=auxMaxRuntimeSeconds;d["aux"]["maxLocationTemp"]=auxMaxLocationTemp;d["aux"]["postReachPolicy"]=auxPostReachPolicyText(auxPostReachPolicy);d["aux"]["postReachMinutes"]=auxPostReachMinutes;d["aux"]["postReachMaxSeconds"]=auxPostReachMaxSeconds;d["ip"]=(WiFi.status()==WL_CONNECTED?WiFi.localIP():WiFi.softAPIP()).toString();d["netId"]=netId;d["deviceName"]=deviceName;d["staConnected"]=(WiFi.status()==WL_CONNECTED);d["staIP"]=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"";d["staSSID"]=staSSID;d["apPasswordConfigured"]=strlen(apPassword)>=MIN_AP_PASSWORD_LEN;d["sensorCount"]=sensorCount;char orom[17]={0};for(uint8_t b=0;b<8;b++)sprintf(orom+b*2,"%02X",outdoorSensorRom[b]);d["outdoorSensorRom"]=orom;if(outdoorSensorOK)d["outdoorTemperature"]=outdoorTemperature;else d["outdoorTemperature"]=nullptr;d["outdoorSensorOK"]=outdoorSensorOK;d["uptime"]=millis()/1000;d["rtcOK"]=rtcAvailable&&!rtc.lostPower();JsonArray sa=d["sensors"].to<JsonArray>();for(uint8_t i=0;i<sensorCount;i++){JsonObject x=sa.add<JsonObject>();char rom[17]={0};for(int b=0;b<8;b++)sprintf(rom+b*2,"%02X",sensorAddresses[i][b]);x["rom"]=rom;float t=sensors.getTempC(sensorAddresses[i]);if(t==DEVICE_DISCONNECTED_C||t<-55||t>125)x["temperature"]=nullptr;else x["temperature"]=t;}JsonArray a=d["channels"].to<JsonArray>();for(uint8_t i=0;i<MAX_CHANNELS;i++)jsonChannel(a.add<JsonObject>(),i);String out;serializeJson(d,out);server.send(200,"application/json",out);}
void apiChannels(){if(!checkAuth())return;JsonDocument d;JsonArray a=d.to<JsonArray>();for(uint8_t i=0;i<MAX_CHANNELS;i++)jsonChannel(a.add<JsonObject>(),i);String out;serializeJson(d,out);server.send(200,"application/json",out);}

void resetReachRuntime(uint8_t ch){
  if(ch>=MAX_CHANNELS)return;
  runtime[ch].reachCacheDateKey=0;
  runtime[ch].reachCacheValidMask=0;
  runtime[ch].reachActivatedMask=0;
  clearManualOverride(ch);runtime[ch].reachLearningActive=false;runtime[ch].reachLearningSlot=255;
  for(uint8_t i=0;i<12;i++)runtime[ch].reachStartMinutes[i]=0;
}

bool parseChannelUpdate(uint8_t ch,JsonObject doc,ChannelConfig&c,ServoOperationMode&servoMode){
  if(ch>=MAX_CHANNELS)return false;
  if(doc["name"].is<const char*>()){const char*n=doc["name"];if(!n||!strlen(n)||strlen(n)>=sizeof(c.name))return false;strncpy(c.name,n,sizeof(c.name)-1);c.name[sizeof(c.name)-1]=0;}
  if(doc["actuatorType"].is<const char*>()){const char*t=doc["actuatorType"];if(!strcmp(t,"ten")){c.actuator=ACTUATOR_HEATER;heaterInverted[ch]=false;}else if(!strcmp(t,"limit")){c.actuator=ACTUATOR_VALVE;c.valveFeedback=VALVE_FEEDBACK_LIMITS;}else if(!strcmp(t,"timed")){c.actuator=ACTUATOR_VALVE;c.valveFeedback=VALVE_FEEDBACK_TIME;}else if(!strcmp(t,"inverted")){c.actuator=ACTUATOR_HEATER;heaterInverted[ch]=true;}else return false;}
  else if(doc["actuator"].is<const char*>()){const char*a=doc["actuator"];if(!strcmp(a,"valve"))c.actuator=ACTUATOR_VALVE;else if(!strcmp(a,"heater"))c.actuator=ACTUATOR_HEATER;else return false;}
  if(doc["servoMode"].is<const char*>()){const char*sm=doc["servoMode"];if(!strcmp(sm,"regulator"))servoMode=SERVO_OPERATION_REGULATOR;else if(!strcmp(sm,"valve"))servoMode=SERVO_OPERATION_VALVE;else return false;}
  if(doc["mode"].is<const char*>()){OperatingMode parsed;if(!parseModeStrict(doc["mode"],parsed))return false;if(runtime[ch].sensorRecoveryPending && parsed!=c.mode)runtime[ch].sensorRecoveryUserOverride=true;c.mode=parsed;}if(doc["templateLogic"].is<const char*>()){TemplateLogic tl;if(!parseTemplateLogic(doc["templateLogic"],tl))return false;templateLogic[ch]=tl;}
  // A channel's ROM assignment is persistent configuration. If that sensor is temporarily
  // disconnected, unrelated channel settings must still be editable. A missing ROM is
  // therefore accepted only when it is the channel's existing assignment; a new assignment
  // still requires a currently discovered sensor.
  bool sensorRomChanged=false;
  if(doc["sensorRom"].is<const char*>()){
    const char*rs=doc["sensorRom"];
    if(strlen(rs)!=16)return false;
    int found=-1;
    for(uint8_t i=0;i<sensorCount;i++){char rom[17]={0};for(uint8_t b=0;b<8;b++)sprintf(rom+b*2,"%02X",sensorAddresses[i][b]);if(!strcasecmp(rs,rom)){found=i;break;}}
    if(found>=0){c.sensorIndex=found;memcpy(c.sensorRom,sensorAddresses[found],8);sensorRomChanged=true;}
    else {char existing[17]={0};for(uint8_t b=0;b<8;b++)sprintf(existing+b*2,"%02X",c.sensorRom[b]);if(strcasecmp(rs,existing)!=0)return false;}
  }
  if(doc["sensorIndex"].is<int>()&&!sensorRomChanged){
    int i=doc["sensorIndex"];
    if(i<0||i>=sensorCount){if(i!=c.sensorIndex)return false;}
    else {c.sensorIndex=i;memcpy(c.sensorRom,sensorAddresses[i],8);}
  }
  if(doc["comfort"].is<float>()||doc["comfort"].is<int>())c.comfortTemperature=doc["comfort"];if(doc["economy"].is<float>()||doc["economy"].is<int>())c.economyTemperature=doc["economy"];if(doc["hysteresis"].is<float>()||doc["hysteresis"].is<int>())c.hysteresis=doc["hysteresis"];if(doc["outputInverted"].is<bool>())heaterInverted[ch]=doc["outputInverted"].as<bool>();if(doc["openTime"].is<int>())c.valveOpenTime=doc["openTime"];if(doc["closeTime"].is<int>())c.valveCloseTime=doc["closeTime"];if(doc["valveFeedback"].is<const char*>()){const char*f=doc["valveFeedback"];if(!strcmp(f,"time"))c.valveFeedback=VALVE_FEEDBACK_TIME;else if(!strcmp(f,"limits"))c.valveFeedback=VALVE_FEEDBACK_LIMITS;else return false;if(c.valveFeedback==VALVE_FEEDBACK_LIMITS&&!gpioExpanderAvailable)return false;}return validateChannel(c);
}
void apiUpdateChannel(){if(!checkAuth())return;String u=server.uri();int ch=u.substring(u.lastIndexOf('/')+1).toInt()-1;if(ch<0||ch>=MAX_CHANNELS){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid channel\"}");return;}JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid JSON\"}");return;}bool oldInverted=heaterInverted[ch];ServoOperationMode oldServoMode=servoOperationMode[ch];TemplateLogic oldTemplateLogic=templateLogic[ch];bool oldAutoInverted=runtime[ch].autoInverted;bool requestedAutoInverted=d["autoInverted"].is<bool>()?d["autoInverted"].as<bool>():false;bool hasHeater=d["heater"].is<bool>();bool requestedHeater=hasHeater&&d["heater"].as<bool>();ChannelConfig candidate=channels[ch];ServoOperationMode candidateServoMode=servoOperationMode[ch];if(!parseChannelUpdate(ch,d.as<JsonObject>(),candidate,candidateServoMode)){templateLogic[ch]=oldTemplateLogic;heaterInverted[ch]=oldInverted;servoOperationMode[ch]=oldServoMode;server.send(400,"application/json","{\"ok\":false,\"error\":\"Validation failed\"}");return;}ChannelConfig oldConfig=channels[ch];ChannelRuntime oldRuntime=runtime[ch];ServoOperationMode oldServoModeForRuntime=servoOperationMode[ch];bool changed=candidate.actuator!=channels[ch].actuator;bool feedbackChanged=candidate.valveFeedback!=channels[ch].valveFeedback;channels[ch]=candidate;servoOperationMode[ch]=candidateServoMode;applyTemplateLogicToSlots(ch);runtime[ch].autoInverted=(channels[ch].mode==MODE_AUTO)?requestedAutoInverted:false;if(changed||feedbackChanged){valveStop(ch);heaterPhysicalOff(ch);runtime[ch].outputState=false;runtime[ch].valvePosition=0;runtime[ch].positionKnown=false;runtime[ch].calibrating=false;}if(channels[ch].actuator==ACTUATOR_HEATER&&!hasHeater){valveStop(ch);heaterPhysicalOff(ch);runtime[ch].outputState=false;}if(!saveConfiguration()){channels[ch]=oldConfig;servoOperationMode[ch]=oldServoModeForRuntime;templateLogic[ch]=oldTemplateLogic;applyTemplateLogicToSlots(ch);runtime[ch]=oldRuntime;runtime[ch].autoInverted=oldAutoInverted;heaterInverted[ch]=oldInverted;server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}if(channels[ch].actuator==ACTUATOR_HEATER&&channels[ch].mode==MODE_MANUAL&&hasHeater){runtime[ch].outputState=requestedHeater;applyHeaterOutput(ch,requestedHeater);runtime[ch].lastOutputChange=millis();}resetReachRuntime(ch);server.send(200,"application/json","{\"ok\":true}");}
void apiChannelManualOverride(){
  if(!checkAuth())return;
  int ch;if(!parseValveChannel(ch)){server.send(400,"application/json","{\"ok\":false,\"error\":\"channel\"}");return;}
  if(channels[ch].mode!=MODE_AUTO){server.send(409,"application/json","{\"ok\":false,\"error\":\"manual_override_requires_auto\",\"message\":\"Разове ручне перемикання доступне тільки в режимі AUTO\"}");return;}
  JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"json\"}");return;}
  const char* m=d["mode"]|"";
  if(!strcmp(m,"clear")||!strcmp(m,"auto")){clearManualOverride(ch);server.send(200,"application/json","{\"ok\":true,\"active\":false}");return;}
  OperatingMode parsed;if(!parseModeStrict(m,parsed)|| (parsed!=MODE_COMFORT&&parsed!=MODE_ECONOMY)){server.send(400,"application/json","{\"ok\":false,\"error\":\"mode_must_be_comfort_or_economy\"}");return;}
  if(!rtcTimeValid()){server.send(409,"application/json","{\"ok\":false,\"error\":\"rtc_invalid\"}");return;}
  DateTime nowDt=rtc.now();ActiveScheduleEvent e=activeScheduleEvent(ch,nowDt);
  runtime[ch].manualOverrideActive=true;runtime[ch].manualOverrideMode=parsed;runtime[ch].manualOverrideDateKey=reachDateKey(nowDt);runtime[ch].manualOverrideEventSlot=e.valid?e.slot:255;
  server.send(200,"application/json","{\"ok\":true,\"active\":true}");
}

void serializeTemplate(JsonArray out,const WeekTemplate& tpl,uint8_t ch){for(uint8_t d=0;d<7;d++){JsonArray slots=out.add<JsonArray>();for(uint8_t s=0;s<6;s++){const auto&x=tpl.days[d].slots[s];JsonObject q=slots.add<JsonObject>();q["hour"]=x.hour;q["minute"]=x.minute;q["mode"]=x.mode==MODE_COMFORT?"comfort":"economy";q["enabled"]=x.enabled;q["transition"]=((templateLogic[ch]==TEMPLATE_LOGIC_REACH&&x.mode==MODE_COMFORT)?"reach":"start");}}}
bool parseTemplate(JsonArray arr,WeekTemplate& tpl){if(arr.isNull()||arr.size()!=7)return false;for(uint8_t d=0;d<7;d++){JsonArray slots=arr[d].as<JsonArray>();if(slots.isNull()||slots.size()!=6)return false;for(uint8_t s=0;s<6;s++){JsonObject x=slots[s].as<JsonObject>();int h=x["hour"]|0,m=x["minute"]|0;if(h<0||h>23||m<0||m>59)return false;const char*mode=x["mode"]|"economy";if(strcmp(mode,"comfort")&&strcmp(mode,"economy"))return false;tpl.days[d].slots[s].hour=h;tpl.days[d].slots[s].minute=m;tpl.days[d].slots[s].mode=!strcmp(mode,"comfort")?MODE_COMFORT:MODE_ECONOMY;tpl.days[d].slots[s].enabled=x["enabled"]|false;tpl.days[d].slots[s].transitionType=SCHEDULE_START;}}return validateTemplate(tpl);}
void apiGetCalendar(){if(!checkAuth())return;int ch=server.uri().substring(server.uri().lastIndexOf('/')+1).toInt()-1;if(ch<0||ch>=MAX_CHANNELS){server.send(400,"application/json","{\"error\":\"channel\"}");return;}JsonDocument d;d["channel"]=ch+1;JsonArray types=d["dayTypes"].to<JsonArray>();for(uint8_t i=0;i<7;i++)types.add(channels[ch].dayType[i]==DAY_HOLIDAY?"holiday":"working");JsonArray w=d["working"].to<JsonArray>();JsonArray h=d["holiday"].to<JsonArray>();serializeTemplate(w,channels[ch].working,ch);serializeTemplate(h,channels[ch].holiday,ch);String out;serializeJson(d,out);server.send(200,"application/json",out);}
void apiUpdateCalendar(){if(!checkAuth())return;int ch=server.uri().substring(server.uri().lastIndexOf('/')+1).toInt()-1;if(ch<0||ch>=MAX_CHANNELS){server.send(400,"application/json","{\"error\":\"channel\"}");return;}JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"error\":\"json\"}");return;}TemplateLogic requestedLogic=templateLogic[ch];if(d["logic"].is<const char*>()){if(!parseTemplateLogic(d["logic"],requestedLogic)){server.send(400,"application/json","{\"error\":\"bad logic\"}");return;}}JsonArray types=d["dayTypes"].as<JsonArray>();if(types.isNull()||types.size()!=7){server.send(400,"application/json","{\"error\":\"7 day types required\"}");return;}ChannelConfig candidate=channels[ch];for(uint8_t i=0;i<7;i++){const char*t=types[i]|"working";if(strcmp(t,"working")&&strcmp(t,"holiday")){server.send(400,"application/json","{\"error\":\"bad day type\"}");return;}candidate.dayType[i]=!strcmp(t,"holiday")?DAY_HOLIDAY:DAY_WORKING;}if(!parseTemplate(d["working"].as<JsonArray>(),candidate.working)||!parseTemplate(d["holiday"].as<JsonArray>(),candidate.holiday)){server.send(400,"application/json","{\"error\":\"invalid template\"}");return;}if(!validateChannel(candidate)){server.send(400,"application/json","{\"error\":\"validation\"}");return;}ChannelConfig old=channels[ch];TemplateLogic oldLogic=templateLogic[ch];channels[ch]=candidate;templateLogic[ch]=requestedLogic;applyTemplateLogicToSlots(ch);if(!saveConfiguration()){channels[ch]=old;templateLogic[ch]=oldLogic;applyTemplateLogicToSlots(ch);server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}resetReachRuntime(ch);server.send(200,"application/json","{\"ok\":true}");}
bool parseValveChannel(int &ch){
  String v=server.hasArg("ch")?server.arg("ch"):server.arg("channel");
  if(v.isEmpty())return false;char*endp=nullptr;long n=strtol(v.c_str(),&endp,10);if(!endp||*endp||n<1||n>MAX_CHANNELS)return false;ch=(int)n-1;return true;
}
void apiValve(const char*cmd){
  if(!checkAuth())return;int ch;if(!parseValveChannel(ch)||channels[ch].actuator!=ACTUATOR_VALVE){server.send(409,"application/json","{\"ok\":false,\"error\":\"Invalid channel or actuator\"}");return;}
  if(channels[ch].valveFeedback==VALVE_FEEDBACK_LIMITS&&!gpioExpanderAvailable&&strcmp(cmd,"stop")!=0){server.send(409,"application/json","{\"ok\":false,\"error\":\"limit_switch_unavailable\"}");return;}
  if((!strcmp(cmd,"open")||!strcmp(cmd,"close"))&&channels[ch].valveFeedback==VALVE_FEEDBACK_TIME&&!runtime[ch].positionKnown){
    server.send(409,"application/json","{\"ok\":false,\"error\":\"position_unknown\",\"message\":\"Необхідна калібровка клапана перед ручним рухом\"}");return;
  }
  if(!strcmp(cmd,"open")){valveOpen(ch);}else if(!strcmp(cmd,"close")){valveClose(ch);}else if(!strcmp(cmd,"stop")){if(runtime[ch].calibrating){runtime[ch].calibrating=false;runtime[ch].calibrationDirection=VALVE_STOPPED;runtime[ch].positionKnown=false;}valveStop(ch);}else{server.send(400,"application/json","{\"error\":\"command\"}");return;}
  if(runtime[ch].valveFault&&strcmp(cmd,"stop")){server.send(409,"application/json","{\"ok\":false,\"error\":\"valve_fault\",\"message\":\"Необхідна калібровка клапана або усунення помилки\"}");return;}server.send(200,"application/json","{\"ok\":true}");
}
void apiValveOpen(){apiValve("open");}void apiValveClose(){apiValve("close");}void apiValveStop(){apiValve("stop");}
void apiValveCalibration(){
  if(!checkAuth())return;int ch;if(!parseValveChannel(ch)||channels[ch].actuator!=ACTUATOR_VALVE){server.send(409,"application/json","{\"ok\":false,\"error\":\"Invalid channel or actuator\"}");return;}
  if(channels[ch].valveFeedback!=VALVE_FEEDBACK_TIME){server.send(409,"application/json","{\"ok\":false,\"error\":\"calibration_not_required\",\"message\":\"Для клапана з кінцевими вимикачами калібровка не потрібна\"}");return;}
  JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid JSON\"}");return;}
  const char* action=d["action"]|"";
  if(!strcmp(action,"start")){const char* initial=d["initial"]|"";if(strcmp(initial,"closed")&&strcmp(initial,"open")){server.send(400,"application/json","{\"ok\":false,\"error\":\"initial must be closed or open\"}");return;}startCalibration(ch,!strcmp(initial,"open")?1:0);}
  else if(!strcmp(action,"stop")){stopCalibration(ch);}
  else{server.send(400,"application/json","{\"ok\":false,\"error\":\"action must be start or stop\"}");return;}
  if(runtime[ch].valveFault){server.send(409,"application/json","{\"ok\":false,\"error\":\"valve_fault\"}");return;}server.send(200,"application/json","{\"ok\":true}");
}
void apiOutdoorSensor(){
  if(!checkAuth())return;JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"json\"}");return;}
  const char* rs=d["sensorRom"]|"";
  if(!strlen(rs)){uint8_t old[8];memcpy(old,outdoorSensorRom,8);memset(outdoorSensorRom,0,8);if(!saveConfiguration()){memcpy(outdoorSensorRom,old,8);server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}server.send(200,"application/json","{\"ok\":true}");return;}
  if(strlen(rs)!=16){server.send(400,"application/json","{\"ok\":false,\"error\":\"sensorRom\"}");return;}
  int found=-1;for(uint8_t i=0;i<sensorCount;i++){char rom[17]={0};for(uint8_t b=0;b<8;b++)sprintf(rom+b*2,"%02X",sensorAddresses[i][b]);if(!strcasecmp(rs,rom)){found=i;break;}}
  if(found<0){server.send(400,"application/json","{\"ok\":false,\"error\":\"sensor_not_found\"}");return;}
  if(!memcmp(sensorAddresses[found],channels[0].sensorRom,8)||!memcmp(sensorAddresses[found],channels[1].sensorRom,8)||!memcmp(sensorAddresses[found],channels[2].sensorRom,8)||!memcmp(sensorAddresses[found],channels[3].sensorRom,8)||!memcmp(sensorAddresses[found],channels[4].sensorRom,8)||!memcmp(sensorAddresses[found],channels[5].sensorRom,8)){server.send(409,"application/json","{\"ok\":false,\"error\":\"outdoor_sensor_must_be_dedicated\"}");return;}
  uint8_t old[8];memcpy(old,outdoorSensorRom,8);memcpy(outdoorSensorRom,sensorAddresses[found],8);if(!saveConfiguration()){memcpy(outdoorSensorRom,old,8);server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}server.send(200,"application/json","{\"ok\":true}");
}

void apiRTCGet(){if(!checkAuth())return;if(!rtcTimeValid()){server.send(503,"application/json","{\"error\":\"RTC unavailable or time invalid\"}");return;}DateTime n=rtc.now();char date[11],time[9],dt[25];snprintf(date,sizeof(date),"%04d-%02d-%02d",n.year(),n.month(),n.day());snprintf(time,sizeof(time),"%02d:%02d:%02d",n.hour(),n.minute(),n.second());snprintf(dt,sizeof(dt),"%04d-%02d-%02d %02d:%02d:%02d",n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());JsonDocument d;d["date"]=date;d["time"]=time;d["datetime"]=dt;String out;serializeJson(d,out);server.send(200,"application/json",out);}
bool validDateTimeParts(int y,int m,int day,int h,int min){if(y<2020||y>2099||m<1||m>12||day<1||day>31||h<0||h>23||min<0||min>59)return false;DateTime dt(y,m,day,h,min,0);return dt.year()==y&&dt.month()==m&&dt.day()==day&&dt.hour()==h&&dt.minute()==min;}
void apiRTCSet(){if(!checkAuth())return;if(!rtcAvailable){server.send(503,"application/json","{\"error\":\"RTC unavailable\"}");return;}JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"error\":\"json\"}");return;}int y,m,day,h,min;if(sscanf(d["date"]|"","%d-%d-%d",&y,&m,&day)!=3||sscanf(d["time"]|"","%d:%d",&h,&min)!=2||!validDateTimeParts(y,m,day,h,min)){server.send(400,"application/json","{\"error\":\"bad date/time\"}");return;}rtc.adjust(DateTime(y,m,day,h,min,0));server.send(200,"application/json","{\"ok\":true}");}
void apiNTP(){if(!checkAuth())return;configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4","pool.ntp.org","time.nist.gov");struct tm ti;if(!getLocalTime(&ti,10000)){server.send(504,"application/json","{\"error\":\"NTP timeout\"}");return;}if(!rtcAvailable){server.send(503,"application/json","{\"ok\":false,\"error\":\"RTC unavailable\"}");return;}rtc.adjust(DateTime(ti.tm_year+1900,ti.tm_mon+1,ti.tm_mday,ti.tm_hour,ti.tm_min,ti.tm_sec));server.send(200,"application/json","{\"ok\":true}");}
void apiReboot(){if(!checkAuth())return;allRelaysOff();for(uint8_t i=0;i<MAX_CHANNELS;i++){runtime[i].valveState=VALVE_STOPPED;runtime[i].outputState=false;}server.send(200,"application/json","{\"ok\":true}");delay(250);ESP.restart();}
void apiReset(){if(!checkAuth())return;allRelaysOff();for(uint8_t i=0;i<MAX_CHANNELS;i++){runtime[i].valveState=VALVE_STOPPED;runtime[i].outputState=false;}clearCompactStorage();clearV11CompactStorage();clearLegacyStorage();resetReachLearning();server.send(200,"application/json","{\"ok\":true}");delay(250);ESP.restart();}

void apiChangeAuth(){if(!checkAuth())return;JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid JSON\"}");return;}const char*cur=d["currentPassword"]|"";const char*nu=d["username"]|"";const char*np=d["password"]|"";if(strcmp(cur,authPass)){delay(250);server.send(403,"application/json","{\"ok\":false,\"error\":\"Invalid current password\"}");return;}if(!strlen(nu)||strlen(nu)>=sizeof(authUser)){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid username\"}");return;}if(strlen(np)<MIN_PASSWORD_LEN||strlen(np)>=sizeof(authPass)){server.send(400,"application/json","{\"ok\":false,\"error\":\"Password too short\"}");return;}char oldUser[sizeof(authUser)];char oldPass[sizeof(authPass)];strncpy(oldUser,authUser,sizeof(oldUser)-1);oldUser[sizeof(oldUser)-1]=0;strncpy(oldPass,authPass,sizeof(oldPass)-1);oldPass[sizeof(oldPass)-1]=0;strncpy(authUser,nu,sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,np,sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;if(!saveConfiguration()){strncpy(authUser,oldUser,sizeof(authUser)-1);authUser[sizeof(authUser)-1]=0;strncpy(authPass,oldPass,sizeof(authPass)-1);authPass[sizeof(authPass)-1]=0;server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}server.send(200,"application/json","{\"ok\":true}");}

void apiDiscover(){JsonDocument d;d["netId"]=netId;d["name"]=deviceName;d["staConnected"]=(WiFi.status()==WL_CONNECTED);String out;serializeJson(d,out);server.send(200,"application/json",out);}
void apiAPConfig(){if(!checkAuth())return;JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid JSON\"}");return;}const char*np=d["password"]|"";if(strlen(np)<MIN_AP_PASSWORD_LEN||strlen(np)>=sizeof(apPassword)){server.send(400,"application/json","{\"ok\":false,\"error\":\"AP password must be 8-63 characters\"}");return;}char old[sizeof(apPassword)];strncpy(old,apPassword,sizeof(old)-1);old[sizeof(old)-1]=0;strncpy(apPassword,np,sizeof(apPassword)-1);apPassword[sizeof(apPassword)-1]=0;if(!saveConfiguration()){strncpy(apPassword,old,sizeof(apPassword)-1);apPassword[sizeof(apPassword)-1]=0;server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}server.send(200,"application/json","{\"ok\":true,\"rebooting\":true}");delay(300);ESP.restart();}

void apiWifiConfig(){if(!checkAuth())return;JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid JSON\"}");return;}bool netChanged=false;char oldDeviceName[sizeof(deviceName)];char oldSsid[sizeof(staSSID)];char oldPass[sizeof(staPassword)];bool oldStaConfigured=staConfigured;strncpy(oldDeviceName,deviceName,sizeof(oldDeviceName)-1);oldDeviceName[sizeof(oldDeviceName)-1]=0;strncpy(oldSsid,staSSID,sizeof(oldSsid)-1);oldSsid[sizeof(oldSsid)-1]=0;strncpy(oldPass,staPassword,sizeof(oldPass)-1);oldPass[sizeof(oldPass)-1]=0;if(d["deviceName"].is<const char*>()){const char*name=d["deviceName"];if(!strlen(name)||strlen(name)>=sizeof(deviceName)){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid device name\"}");return;}strncpy(deviceName,name,sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;}if(d["ssid"].is<const char*>()){const char*ssid=d["ssid"];if(strlen(ssid)>=sizeof(staSSID)){server.send(400,"application/json","{\"ok\":false,\"error\":\"SSID too long\"}");return;}if(!strlen(ssid)){staSSID[0]=0;staPassword[0]=0;staConfigured=false;}else{strncpy(staSSID,ssid,sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;if(d["password"].is<const char*>()){const char*pass=d["password"];if(strlen(pass)>=sizeof(staPassword)||(strlen(pass)>0&&strlen(pass)<8)){server.send(400,"application/json","{\"ok\":false,\"error\":\"Password must be empty (open network) or 8+ chars\"}");return;}strncpy(staPassword,pass,sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;}staConfigured=true;}netChanged=true;}if(!saveConfiguration()){strncpy(deviceName,oldDeviceName,sizeof(deviceName)-1);deviceName[sizeof(deviceName)-1]=0;strncpy(staSSID,oldSsid,sizeof(staSSID)-1);staSSID[sizeof(staSSID)-1]=0;strncpy(staPassword,oldPass,sizeof(staPassword)-1);staPassword[sizeof(staPassword)-1]=0;staConfigured=oldStaConfigured;server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}if(netChanged){server.send(200,"application/json","{\"ok\":true,\"rebooting\":true}");delay(300);ESP.restart();}else server.send(200,"application/json","{\"ok\":true,\"rebooting\":false}");}

void apiAuxGet(){if(!checkAuth())return;JsonDocument d;d["enabled"]=auxEnabled;d["locationChannel"]=auxLocationChannel+1;d["channelMask"]=auxChannelMask;d["output"]=auxOutputState;d["reason"]=auxReason;d["maxRuntimeSeconds"]=auxMaxRuntimeSeconds;d["maxLocationTemp"]=auxMaxLocationTemp;d["postReachPolicy"]=auxPostReachPolicyText(auxPostReachPolicy);d["postReachMinutes"]=auxPostReachMinutes;d["postReachMaxSeconds"]=auxPostReachMaxSeconds;String out;serializeJson(d,out);server.send(200,"application/json",out);}
void apiAuxUpdate(){if(!checkAuth())return;JsonDocument d;if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid JSON\"}");return;}bool oldEn=auxEnabled;uint8_t oldLoc=auxLocationChannel,oldMask=auxChannelMask;uint32_t oldRun=auxMaxRuntimeSeconds;float oldTemp=auxMaxLocationTemp;AuxPostReachPolicy oldPostPol=auxPostReachPolicy;uint32_t oldPostMin=auxPostReachMinutes,oldPostMax=auxPostReachMaxSeconds;if(d["enabled"].is<bool>())auxEnabled=d["enabled"].as<bool>();if(d["locationChannel"].is<int>()){int v=d["locationChannel"];if(v<1||v>MAX_CHANNELS){auxEnabled=oldEn;auxLocationChannel=oldLoc;auxChannelMask=oldMask;auxMaxRuntimeSeconds=oldRun;auxMaxLocationTemp=oldTemp;auxPostReachPolicy=oldPostPol;auxPostReachMinutes=oldPostMin;auxPostReachMaxSeconds=oldPostMax;server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid AUX location\"}");return;}auxLocationChannel=(uint8_t)(v-1);}if(d["channelMask"].is<int>()){int v=d["channelMask"];if(v<0||v>((1<<MAX_CHANNELS)-1)){auxEnabled=oldEn;auxLocationChannel=oldLoc;auxChannelMask=oldMask;auxMaxRuntimeSeconds=oldRun;auxMaxLocationTemp=oldTemp;auxPostReachPolicy=oldPostPol;auxPostReachMinutes=oldPostMin;auxPostReachMaxSeconds=oldPostMax;server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid AUX channel mask\"}");return;}auxChannelMask=(uint8_t)v;}if(d["maxRuntimeSeconds"].is<int>()){int v=d["maxRuntimeSeconds"];if(v<60||v>7200){auxEnabled=oldEn;auxLocationChannel=oldLoc;auxChannelMask=oldMask;auxMaxRuntimeSeconds=oldRun;auxMaxLocationTemp=oldTemp;auxPostReachPolicy=oldPostPol;auxPostReachMinutes=oldPostMin;auxPostReachMaxSeconds=oldPostMax;server.send(400,"application/json","{\"ok\":false,\"error\":\"AUX max runtime must be 60..7200 s\"}");return;}auxMaxRuntimeSeconds=(uint32_t)v;}if(d["maxLocationTemp"].is<float>()||d["maxLocationTemp"].is<int>()){float v=d["maxLocationTemp"];if(v<5||v>35){auxEnabled=oldEn;auxLocationChannel=oldLoc;auxChannelMask=oldMask;auxMaxRuntimeSeconds=oldRun;auxMaxLocationTemp=oldTemp;auxPostReachPolicy=oldPostPol;auxPostReachMinutes=oldPostMin;auxPostReachMaxSeconds=oldPostMax;server.send(400,"application/json","{\"ok\":false,\"error\":\"AUX max location temperature must be 5..35 C\"}");return;}auxMaxLocationTemp=v;}if(d["postReachPolicy"].is<const char*>()){AuxPostReachPolicy v;if(!parseAuxPostReachPolicy(d["postReachPolicy"].as<const char*>(),v)){auxEnabled=oldEn;auxLocationChannel=oldLoc;auxChannelMask=oldMask;auxMaxRuntimeSeconds=oldRun;auxMaxLocationTemp=oldTemp;auxPostReachPolicy=oldPostPol;auxPostReachMinutes=oldPostMin;auxPostReachMaxSeconds=oldPostMax;server.send(400,"application/json","{\"ok\":false,\"error\":\"Invalid AUX post-REACH policy\"}");return;}auxPostReachPolicy=v;}if(d["postReachMinutes"].is<int>()){int v=d["postReachMinutes"];if(v<5||v>240){auxEnabled=oldEn;auxLocationChannel=oldLoc;auxChannelMask=oldMask;auxMaxRuntimeSeconds=oldRun;auxMaxLocationTemp=oldTemp;auxPostReachPolicy=oldPostPol;auxPostReachMinutes=oldPostMin;auxPostReachMaxSeconds=oldPostMax;server.send(400,"application/json","{\"ok\":false,\"error\":\"AUX post-REACH minutes must be 5..240\"}");return;}auxPostReachMinutes=(uint32_t)v;}if(d["postReachMaxSeconds"].is<int>()){int v=d["postReachMaxSeconds"];if(v<60||v>7200){auxEnabled=oldEn;auxLocationChannel=oldLoc;auxChannelMask=oldMask;auxMaxRuntimeSeconds=oldRun;auxMaxLocationTemp=oldTemp;auxPostReachPolicy=oldPostPol;auxPostReachMinutes=oldPostMin;auxPostReachMaxSeconds=oldPostMax;server.send(400,"application/json","{\"ok\":false,\"error\":\"AUX post-REACH max runtime must be 60..7200 s\"}");return;}auxPostReachMaxSeconds=(uint32_t)v;}if(!saveConfiguration()){auxEnabled=oldEn;auxLocationChannel=oldLoc;auxChannelMask=oldMask;auxMaxRuntimeSeconds=oldRun;auxMaxLocationTemp=oldTemp;auxPostReachPolicy=oldPostPol;auxPostReachMinutes=oldPostMin;auxPostReachMaxSeconds=oldPostMax;server.send(500,"application/json","{\"ok\":false,\"error\":\"storage\"}");return;}if(!auxEnabled)auxPhysicalOff();server.send(200,"application/json","{\"ok\":true}");}
void serveIndex(){if(!checkAuth())return;File f=LittleFS.open("/index.html","r");if(!f){server.send(500,"text/plain","index.html not found");return;}server.streamFile(f,"text/html; charset=utf-8");f.close();}
void setupAPI(){server.on("/",HTTP_GET,serveIndex);server.on("/index.html",HTTP_GET,serveIndex);server.on("/api/discover",HTTP_GET,apiDiscover);server.on("/api/status",HTTP_GET,apiStatus);server.on("/api/channels",HTTP_GET,apiChannels);server.on("/api/channels/1",HTTP_PUT,apiUpdateChannel);server.on("/api/channels/2",HTTP_PUT,apiUpdateChannel);server.on("/api/channels/3",HTTP_PUT,apiUpdateChannel);server.on("/api/channels/4",HTTP_PUT,apiUpdateChannel);server.on("/api/channels/5",HTTP_PUT,apiUpdateChannel);server.on("/api/channels/6",HTTP_PUT,apiUpdateChannel);server.on("/api/channels/1/override",HTTP_POST,apiChannelManualOverride);server.on("/api/channels/2/override",HTTP_POST,apiChannelManualOverride);server.on("/api/channels/3/override",HTTP_POST,apiChannelManualOverride);server.on("/api/channels/4/override",HTTP_POST,apiChannelManualOverride);server.on("/api/channels/5/override",HTTP_POST,apiChannelManualOverride);server.on("/api/channels/6/override",HTTP_POST,apiChannelManualOverride);server.on("/api/calendar/1",HTTP_GET,apiGetCalendar);server.on("/api/calendar/2",HTTP_GET,apiGetCalendar);server.on("/api/calendar/3",HTTP_GET,apiGetCalendar);server.on("/api/calendar/4",HTTP_GET,apiGetCalendar);server.on("/api/calendar/5",HTTP_GET,apiGetCalendar);server.on("/api/calendar/6",HTTP_GET,apiGetCalendar);server.on("/api/calendar/1",HTTP_PUT,apiUpdateCalendar);server.on("/api/calendar/2",HTTP_PUT,apiUpdateCalendar);server.on("/api/calendar/3",HTTP_PUT,apiUpdateCalendar);server.on("/api/calendar/4",HTTP_PUT,apiUpdateCalendar);server.on("/api/calendar/5",HTTP_PUT,apiUpdateCalendar);server.on("/api/calendar/6",HTTP_PUT,apiUpdateCalendar);server.on("/api/valve/open",HTTP_POST,apiValveOpen);server.on("/api/valve/close",HTTP_POST,apiValveClose);server.on("/api/valve/stop",HTTP_POST,apiValveStop);server.on("/api/valve/calibration",HTTP_POST,apiValveCalibration);server.on("/api/valve/calibrate",HTTP_POST,apiValveCalibration);server.on("/api/rtc",HTTP_GET,apiRTCGet);server.on("/api/rtc",HTTP_PUT,apiRTCSet);server.on("/api/rtc/ntp",HTTP_POST,apiNTP);server.on("/api/system/reboot",HTTP_POST,apiReboot);server.on("/api/system/reset",HTTP_POST,apiReset);server.on("/api/system/auth",HTTP_PUT,apiChangeAuth);server.on("/api/system/ap",HTTP_PUT,apiAPConfig);server.on("/api/system/wifi",HTTP_PUT,apiWifiConfig);server.on("/api/system/outdoor",HTTP_PUT,apiOutdoorSensor);server.on("/api/system/selftest",HTTP_GET,apiSelfTest);server.on("/api/reach-learning",HTTP_GET,apiReachLearning);server.on("/api/reach-learning/reset",HTTP_POST,apiReachLearningReset);server.on("/api/aux",HTTP_GET,apiAuxGet);server.on("/api/aux",HTTP_PUT,apiAuxUpdate);server.on("/api/servo-maintenance",HTTP_GET,apiServoMaintenanceGet);server.on("/api/servo-maintenance",HTTP_PUT,apiServoMaintenancePut);server.onNotFound([](){if(!checkAuth())return;server.send(404,"text/plain","Not found");});server.begin();}
void maintainMDNS();

void setupWiFi(){WiFi.mode(WIFI_AP_STA);WiFi.softAP(netId,apPassword);Serial.printf("AP %s / %s\n",netId,WiFi.softAPIP().toString().c_str());if(staConfigured&&strlen(staSSID)){WiFi.begin(staSSID,staPassword);Serial.printf("Connecting to home Wi-Fi '%s'",staSSID);unsigned long start=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-start<20000){delay(300);Serial.print(".");}Serial.println();if(WiFi.status()==WL_CONNECTED)maintainMDNS();else Serial.println("STA connect FAILED - still reachable via own AP");}}

void maintainMDNS(){static unsigned long last=0;if(millis()-last<2000)return;last=millis();bool connected=WiFi.status()==WL_CONNECTED;if(connected&&!mdnsStarted){if(MDNS.begin(netId)){MDNS.addService("heatcontrol","tcp",80);mdnsStarted=true;Serial.printf("mDNS: %s.local\n",netId);}else Serial.println("mDNS: ERROR");}else if(!connected&&mdnsStarted){MDNS.end();mdnsStarted=false;Serial.println("mDNS stopped: STA disconnected");}else if(mdnsStarted){/* ESP32 mDNS service is maintained by the stack. */}}
void initializeRuntime(){for(uint8_t i=0;i<MAX_CHANNELS;i++){runtime[i].temperature=NAN;runtime[i].sensorOK=false;runtime[i].outputState=false;runtime[i].valvePosition=0;runtime[i].positionKnown=false;runtime[i].calibrating=false;runtime[i].calibrationInitialPosition=0;runtime[i].calibrationDirection=VALVE_STOPPED;runtime[i].calibrationStartedAt=0;runtime[i].calibrationOpenCandidate=0;runtime[i].calibrationCloseCandidate=0;runtime[i].calibrationHasOpen=false;runtime[i].calibrationHasClose=false;runtime[i].valveState=VALVE_STOPPED;runtime[i].valveTargetPosition=0;runtime[i].valveTargetActive=false;runtime[i].movementStarted=0;runtime[i].movementStartPosition=0;runtime[i].lastOutputChange=millis()-MIN_SWITCH_INTERVAL_MS;runtime[i].limitHardwareFault=false;runtime[i].reachCacheDateKey=0;runtime[i].reachCacheValidMask=0;runtime[i].reachActivatedMask=0;for(uint8_t j=0;j<12;j++)runtime[i].reachStartMinutes[j]=0;runtime[i].autoInverted=false;runtime[i].sensorRecoveryPending=false;runtime[i].sensorRecoveryMode=MODE_MANUAL;runtime[i].sensorRecoveryAutoInverted=false;runtime[i].sensorRecoveryUserOverride=false;runtime[i].sensorRecoveryGoodReads=0;runtime[i].reachLearningActive=false;runtime[i].reachLearningSlot=255;runtime[i].reachLearningDateKey=0;runtime[i].reachLearningStartedAt=0;runtime[i].reachLearningInitialTemp=NAN;runtime[i].reachLearningOutdoor=NAN;runtime[i].reachLearningTarget=NAN;}}

void setup(){Serial.begin(115200);delay(300);generateNetId();for(uint8_t i=0;i<MAX_CHANNELS;i++){pinMode(relayA[i],OUTPUT);pinMode(relayB[i],OUTPUT);}pinMode(AUX_TEN_RELAY_PIN,OUTPUT);allRelaysOff();auxPhysicalOff();initializeRuntime();Wire.begin(RTC_SDA,RTC_SCL);gpioExpanderAvailable=detectAndInitGpioExpander();lastGpioExpanderProbe=millis();Serial.printf("GPIO expander MCP23017: %s\n",gpioExpanderAvailable?"OK":"NOT FOUND (limit-switch mode unavailable)");rtcAvailable=rtc.begin();if(rtcAvailable)Serial.println("DS3231: OK");else Serial.println("DS3231: ERROR");sensors.begin();discoverSensors();loadConfiguration();loadValvePositions();loadReachLearning();loadServoMaintenance();if(servoMaintenanceInterrupted){int8_t interruptedCh=servoMaintenanceChannel;if(interruptedCh>=0&&interruptedCh<MAX_CHANNELS&&channels[(uint8_t)interruptedCh].actuator==ACTUATOR_VALVE){uint8_t ch=(uint8_t)interruptedCh;runtime[ch].positionKnown=false;runtime[ch].valveFault=false;saveValvePosition(ch);Serial.printf("Servo maintenance interrupted by reboot: CH%u requires position revalidation\n",ch+1);}servoMaintenanceInterrupted=false;servoMaintenanceChannel=-1;servoMaintenanceState=SERVO_MAINT_IDLE;servoMaintenanceQueueCount=0;servoMaintenanceQueueIndex=0;saveServoMaintenance();}if(!LittleFS.begin(true))Serial.println("LittleFS: ERROR");else Serial.println("LittleFS: OK");setupWiFi();setupAPI();Serial.println("HTTP server started. SAFE START: outputs OFF");Serial.printf("TEST MODE: %s\n",HEATCONTROL_TEST_MODE?"ENABLED (physical outputs forced OFF)":"disabled");}
void loop(){server.handleClient();maintainMDNS();maintainGpioExpander();updateLimitInputs();static unsigned long last=0;if(millis()-last>=SENSOR_READ_INTERVAL_MS){last=millis();readTemperatures();serviceServoMaintenance();controlChannels();}for(uint8_t ch=0;ch<MAX_CHANNELS;ch++)if(channels[ch].actuator==ACTUATOR_VALVE)updateValvePosition(ch);maybePersistValvePositions();delay(2);}

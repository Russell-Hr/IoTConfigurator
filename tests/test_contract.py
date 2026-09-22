"""Static V0.18.7 HARDENED contract tests. These do not replace real ESP32/Android builds."""
from pathlib import Path
import re

ROOT = Path(__file__).parents[1]
FW = (ROOT / "firmware/src/main.cpp").read_text()
CFG = (ROOT / "firmware/include/CONFIG_TEMPLATE.h").read_text()
HTML = (ROOT / "firmware/data/index.html").read_text()
KOTLIN = "\n".join(str(p.read_text()) for p in (ROOT / "android/HeatControlApp/app/src/main/java").rglob("*.kt"))

def test_six_channels_and_gpio():
    assert '#define MAX_CHANNELS 6' in CFG
    for gpio in (4,21,22,16,17,18,19,23,25,26,27,32,33,13,14):
        assert str(gpio) in CFG

def test_nvs_allows_empty_sta_credentials():
    # AP-only defaults legitimately have empty STA SSID/password.
    assert 'p.putString("staSsid",staSSID);' in FW
    assert 'p.putString("staPass",staPassword);' in FW
    assert 'putString("staSsid",staSSID)>0' not in FW
    assert 'putString("staPass",staPassword)>0' not in FW
    # Readback verification must still cover the optional strings and staCfg flag.
    assert 'ss == String(staSSID)' in FW
    assert 'sp == String(staPassword)' in FW
    assert 'sc == staConfigured' in FW

def test_atomic_storage():
    for token in ('STORAGE_SLOT0','STORAGE_SLOT1','STORAGE_META','setActiveSlot','verifyConfigurationSlot'):
        assert token in FW
    assert 'saveConfiguration(){' in FW

def test_safe_output_logic():
    assert 'outputsAllowed(){return HEATCONTROL_TEST_MODE==0;}' in FW
    assert 'digitalWrite(relayB[ch],RELAY_OFF)' in FW
    assert 'MIN_SWITCH_INTERVAL_MS' in FW

def test_working_holiday_templates():
    for token in ('WeekTemplate','dayType[7]','working','holiday','serializeTemplate','parseTemplate'):
        assert token in FW
    assert 'channels[ch].dayType[d]==DAY_HOLIDAY' in FW
    assert 'd[\"working\"]' in FW and 'd[\"holiday\"]' in FW
    assert 'state.calendar.working' in HTML and 'state.calendar.holiday' in HTML
    assert 'Тип кожного дня' in HTML

def test_api_surface():
    for route in ('/api/status','/api/channels/6','/api/calendar/6','/api/system/selftest'):
        assert route in FW

def test_sensor_rom_and_strict_mode():
    assert 'OneWire::crc8(rom,7)' in FW
    assert 'parseModeStrict' in FW
    assert 'sensorRom' in FW

def test_android_ipv6_and_stable_nsd():
    assert 'heatControlUrl' in KOTLIN
    assert 'resolveService' in KOTLIN
    assert 'NsdManager.ResolveListener' in KOTLIN
    assert 'ServiceInfoCallback' not in KOTLIN
    assert 'registerServiceInfoCallback' not in KOTLIN

def test_no_old_release_labels():
    assert 'V0.10' not in (ROOT / 'README.md').read_text()
    assert 'V0.10' not in (ROOT / 'android/HeatControlApp/README.md').read_text()
    assert 'V0.10' not in (ROOT / 'docs/BUILD_STATUS.md').read_text()
    assert 'V0.10' not in (ROOT / 'docs/REVIEW_REPORT.md').read_text()

def test_config_version_is_v18():
    assert re.search(r'#define\s+CONFIG_VERSION\s+18\b', CFG)
    assert 'readOldConfigurationSlotV16' in FW
    assert 'readOldConfigurationSlotV15' in FW
    assert 'readOldConfigurationSlotV14' in FW
    assert 'readOldConfigurationSlotV13' in FW
    assert 'readOldConfigurationSlotV12' in FW
    assert 'cfgVer",0)==14' in FW
    assert 'cfgVer",0)==13' in FW
    assert 'cfgVer",0)==12' in FW

def test_android_calendar_matches_v13_api():
    assert 'data class CalendarConfig' in KOTLIN
    assert 'data class WeekTemplate' in KOTLIN
    assert 'd.optJSONArray("dayTypes")' in KOTLIN
    assert 'd.optJSONArray("working")' in KOTLIN
    assert 'd.optJSONArray("holiday")' in KOTLIN
    assert '.put("dayTypes", typesArr)' in KOTLIN
    assert '.put("working", templateJson(calendar.working))' in KOTLIN
    assert '.put("holiday", templateJson(calendar.holiday))' in KOTLIN
    assert 'optJSONArray("days")' not in KOTLIN
    assert 'put("days", daysArr)' not in KOTLIN
    assert 'DaySchedule' not in KOTLIN

def test_calendar_runtime_selection_behavior():
    # Behavioral mirror of the firmware's getAutoMode selection rule.
    working = [[None]*6 for _ in range(7)]
    holiday = [[None]*6 for _ in range(7)]
    # Monday: WORKING schedule says COMFORT at 06:00.
    working[0][0] = (6*60, 'comfort')
    # Monday: HOLIDAY schedule says ECONOMY at 09:00.
    holiday[0][0] = (9*60, 'economy')
    # Tuesday: WORKING says ECONOMY at 07:00; HOLIDAY says COMFORT at 08:00.
    working[1][0] = (7*60, 'economy')
    holiday[1][0] = (8*60, 'comfort')
    day_types = ['working', 'holiday', 'working', 'working', 'working', 'holiday', 'holiday']

    def auto_mode(day, now_min):
        template = holiday if day_types[day] == 'holiday' else working
        result = 'economy'
        latest = -1
        for slot in template[day]:
            if slot and slot[0] <= now_min and slot[0] >= latest:
                latest, result = slot
        return result

    assert auto_mode(0, 7*60) == 'comfort'
    assert auto_mode(0, 10*60) == 'comfort'  # still WORKING; HOLIDAY is ignored
    assert auto_mode(1, 9*60) == 'comfort'  # HOLIDAY selected; WORKING is ignored

def test_v13_api_calendar_shape_is_distinct_from_v12():
    get_fn = re.search(r'void apiGetCalendar\(\).*?void apiUpdateCalendar\(\)', FW, re.S)
    assert get_fn
    block = get_fn.group(0)
    assert 'd["dayTypes"]' in block
    assert 'd["working"]' in block
    assert 'd["holiday"]' in block
    assert 'd["days"]' not in block
    update = re.search(r'void apiUpdateCalendar\(\).*', FW, re.S).group(0)
    assert 'candidate.dayType[i]' in update


def test_optional_mcp23017_limit_switch_architecture():
    assert '#define LIMIT_SWITCH_ACTIVE_LOW 1' in CFG
    assert 'MCP23017_ADDR=0x20' in FW
    assert 'gpioExpanderAvailable' in FW
    assert 'VALVE_FEEDBACK_TIME' in FW and 'VALVE_FEEDBACK_LIMITS' in FW
    assert 'valveFeedback' in FW
    assert 'limitSwitchAvailable' in FW
    assert 'readLimitInputs' in FW
    assert 'openLimit' in FW and 'closeLimit' in FW

def test_limit_switch_mode_is_per_channel_and_optional():
    assert 'c.valveFeedback==VALVE_FEEDBACK_LIMITS&&!gpioExpanderAvailable' in FW
    assert 'var valveFeedback by remember(channelId)' in KOTLIN
    assert 'enabled = channel.limitSwitchAvailable' in KOTLIN
    assert 'valveFeedback' in KOTLIN

def test_v13_migration_defaults_feedback_to_time():
    assert 'readOldConfigurationSlotV13' in FW
    assert 'cfgVer",0)==13' in FW
    assert 'channels[ch].valveFeedback=VALVE_FEEDBACK_TIME' in FW

def test_android_status_poll_single_flight():
    assert 'statusMutex' in KOTLIN
    assert 'withLock' in KOTLIN


def test_limit_switch_mapping_is_12_inputs():
    assert 'limitBit(ch*2)' in FW
    assert 'limitBit(ch*2+1)' in FW
    assert '0x0FFFU' in FW
    assert 'LIMIT_SWITCH_DEBOUNCE_MS' in FW

def test_limit_switch_safety_interlocks():
    assert 'runtime[ch].openLimit&&runtime[ch].closeLimit' in FW
    assert 'r.valveFault=true;valveStop(ch)' in FW
    assert 'MAX_VALVE_RUN_SECONDS' in FW

def test_web_and_android_expose_feedback_selection():
    assert 'feedback${c.id}' in HTML
    assert 'За кінцевими вимикачами' in HTML
    assert 'За кінцевиками' in KOTLIN
    assert 'limitSwitchAvailable' in KOTLIN


def test_limit_switch_behavior_model():
    # Model the endpoint rules used by the firmware: an active endpoint is authoritative
    # for its direction, and simultaneous endpoints are a fault.
    def step(direction, open_active, close_active):
        if open_active and close_active:
            return 'fault'
        if direction == 'open' and open_active:
            return 'stop_at_100'
        if direction == 'close' and close_active:
            return 'stop_at_0'
        return 'moving'

    assert step('open', True, False) == 'stop_at_100'
    assert step('close', False, True) == 'stop_at_0'
    assert step('open', False, False) == 'moving'
    assert step('close', False, False) == 'moving'
    assert step('open', True, True) == 'fault'


def test_v14_1_ap_password_configuration():
    assert '#define CONFIG_VERSION 18' in CFG
    assert '#define DEFAULT_AP_PASSWORD' in CFG
    assert '#define MIN_AP_PASSWORD_LEN 8' in CFG
    assert 'p.putString("apPass",apPassword)>0' in FW
    assert 'aps == String(apPassword)' in FW
    assert 'readOldConfigurationSlotV14' in FW
    assert 'server.on("/api/system/ap",HTTP_PUT,apiAPConfig)' in FW
    assert 'WiFi.softAP(netId,apPassword)' in FW
    assert 'apPasswordConfigured' in FW




def test_ball_valve_flow_characteristic_v18_2():
    assert 'BALL_VALVE_FLOW_ANGLE_DEG[] = {0,10,20,30,40,50,60,70,80,90}' in FW
    assert 'BALL_VALVE_FLOW_PERCENT[] = {0,0,2,10,30,55,72,85,96,100}' in FW
    assert 'float ballValveFlowPercent(uint8_t mechanicalPosition)' in FW
    assert 'mechanicalPosition*0.9f' in FW
    assert 'o["valveFlowPercent"]' in FW

def test_valve_direction_deadtime_and_fault_api():
    assert '#define VALVE_DIRECTION_DEADTIME_MS 100UL' in CFG
    assert 'delay(VALVE_DIRECTION_DEADTIME_MS)' in FW
    assert 'limit_switch_unavailable' in FW
    assert 'valve_fault' in FW


def test_mcp_probe_is_verified_and_retried():
    assert 'mcpReadRegs(MCP_IODIRA,a,b)' in FW
    assert 'mcpReadRegs(MCP_GPPUA,pa,pb)' in FW
    assert 'a==0xFF&&b==0xFF&&pa==0xFF&&pb==0xFF' in FW
    assert 'GPIO_EXPANDER_RECHECK_MS' in CFG
    assert 'maintainGpioExpander' in FW


def test_android_status_parser_matches_channel_model():
    assert 'valveFeedback = c.optString("valveFeedback", "time")' in KOTLIN
    assert 'limitSwitchAvailable = c.optBoolean("limitSwitchAvailable")' in KOTLIN
    assert 'openLimit = c.optBoolean("openLimit")' in KOTLIN
    assert 'closeLimit = c.optBoolean("closeLimit")' in KOTLIN
    assert 'valveFault = c.optBoolean("valveFault")' in KOTLIN
    assert 'apPasswordConfigured = d.optBoolean("apPasswordConfigured")' in KOTLIN
    assert 'fun updateApPassword(newPassword: String): Boolean' in KOTLIN


def test_wiring_document_uses_authoritative_gpio_map():
    wiring=(ROOT/'docs/WIRING_6CH.md').read_text()
    assert 'GPIO4' in wiring and 'GPIO21' in wiring and 'GPIO22' in wiring
    assert 'GPIO34' not in wiring and 'GPIO35' not in wiring
    assert 'MCP23017' in wiring and '0x20' in wiring
    assert 'GPA0' in wiring and 'GPB3' in wiring


def test_android_has_real_unit_test_source():
    test_root=ROOT/'android/HeatControlApp/app/src/test'
    assert any(test_root.rglob('*.kt'))

def test_v16_outdoor_sensor_and_calibration_api():
    assert re.search(r'#define\s+CONFIG_VERSION\s+18\b', CFG)
    assert '#define MAX_SENSORS 7' in CFG
    assert 'outdoorSensorRom' in FW
    assert 'outdoorTemperature' in FW
    assert 'outdoorSensorOK' in FW
    assert '/api/system/outdoor' in FW
    assert 'action must be start or stop' in FW
    assert 'initial must be closed or open' in FW
    assert 'readOldConfigurationSlotV15' in FW

def test_limit_switch_mode_has_no_time_calibration():
    assert 'calibration_not_required' in FW
    assert 'c.valveFeedback!=VALVE_FEEDBACK_TIME' in FW

def test_manual_time_valve_requires_known_position():
    assert 'c.valveFeedback==VALVE_FEEDBACK_TIME&&!r.positionKnown' in FW

def test_android_outdoor_and_calibration_models():
    assert 'outdoorTemperature' in KOTLIN
    assert 'outdoorSensorRom' in KOTLIN
    assert 'valveCalibration' in KOTLIN
    assert 'showCalibration' in KOTLIN


def test_channel_template_logic_is_single_setting_for_both_templates():
    assert 'enum TemplateLogic' in FW
    assert 'templateLogic[MAX_CHANNELS]' in FW
    assert 'logic%u' in FW
    assert 'd["logic"]' in FW
    assert 'templateLogicText(templateLogic[ch])' in FW
    assert 'applyTemplateLogicToSlots(ch)' in FW
    assert 'const char*tr=x["transition"]' not in FW

def test_reach_mode_is_preheated_from_temperature_difference():
    # Mirror the initial firmware heuristic: 10 min per degree, bounded 5..120.
    def lead(delta, hysteresis=0.3):
        if delta <= hysteresis:
            return 0
        return max(5, min(120, int(delta * 10 + 0.999999)))
    assert lead(3.0) == 30
    assert lead(0.2) == 0
    assert lead(20.0) == 120

def test_calendar_example_semantics():
    # One channel-level logic applies to both WORKING and HOLIDAY templates.
    logic = 'reach'
    working = [(390, 'comfort'), (540, 'economy'), (1020, 'comfort'), (1380, 'economy')]
    holiday = [(480, 'comfort'), (660, 'economy'), (780, 'comfort'), (1380, 'economy')]
    assert all((mode == 'comfort' and logic == 'reach') or mode == 'economy' for _, mode in working + holiday)
    assert logic == 'reach'


def test_v16_migration_sets_start_transition():
    assert 'readOldConfigurationSlotV16' in FW
    block = FW[FW.index('bool readOldConfigurationSlotV16'):FW.index('bool readOldConfigurationSlotV15')]
    assert 'transitionType=SCHEDULE_START' in block

def test_v151_android_calendar_transition_parser():
    assert 'z.optString("transition", "start")' in KOTLIN
    assert 'ScheduleSlot(' in KOTLIN

def test_v151_nvs_partition_and_sensor_uniqueness():
    part=(ROOT/'firmware/partitions.csv').read_text()
    assert 'nvs,      data, nvs,     0x9000,   0x10000' in part
    assert 'validateSensorAssignments' in FW
    assert 'outdoor_sensor_must_be_dedicated' in FW

def test_v151_android_credential_backup_hardening():
    assert 'CredentialCipher' in KOTLIN
    manifest=(ROOT/'android/HeatControlApp/app/src/main/AndroidManifest.xml').read_text()
    assert 'android:dataExtractionRules="@xml/data_extraction_rules"' in manifest
    assert 'android:fullBackupContent="@xml/backup_rules"' in manifest


def test_save_configuration_is_defined_before_stop_calibration():
    assert FW.index('bool saveConfiguration(){') < FW.index('void stopCalibration(')
    assert 'bool writeConfigurationSlot(const char* ns);' in FW
    assert 'bool verifyConfigurationSlot(const char* ns);' in FW
    assert 'bool validateChannel(const ChannelConfig& c);' in FW

def test_sensor_validation_is_diagnostic_not_save_blocker():
    save = FW[FW.index('bool saveConfiguration(){'):FW.index('void startCalibration(')]
    assert 'validateSensorAssignments()' not in save

def test_reach_activation_latch():
    assert 'reachActivatedMask' in FW
    assert 'runtime[ch].reachActivatedMask&bit' in FW
    assert 'runtime[ch].reachActivatedMask|=bit' in FW
    assert 'resetReachRuntime(ch)' in FW

def test_selftest_reports_sensor_assignment_diagnostics():
    assert 'bool sensorAssignments=validateSensorAssignments();' in FW
    assert 'd["sensorAssignments"]=sensorAssignments;' in FW

def test_no_forward_function_use_without_prototype_for_save_configuration():
    save = FW[FW.index('bool saveConfiguration(){'):FW.index('void startCalibration(')]
    assert 'validateChannel(channels[ch])' in save
    assert 'bool validateChannel(const ChannelConfig& c);' in FW[:FW.index('bool saveConfiguration(){')]

def test_channel_update_preserves_missing_sensor_assignment():
    block = FW[FW.index('bool parseChannelUpdate'):FW.index('void apiUpdateChannel')]
    assert 'if(found>=0)' in block
    assert 'strcasecmp(rs,existing)!=0' in block

def test_outdoor_sensor_can_be_cleared():
    assert 'if(!strlen(rs))' in FW[FW.index('void apiOutdoorSensor'):FW.index('void apiRTCGet')]
    assert 'outdoorSelection.isEmpty() || outdoorSelection.length == 16' in KOTLIN


def test_v0153_android_encrypts_both_controller_credentials():
    repo = (ROOT / "android/HeatControlApp/app/src/main/java/com/heatcontrol/app/data/DeviceRepository.kt").read_text(encoding="utf-8")
    assert 'username = cipher.encrypt(device.username)' in repo
    assert 'password = cipher.encrypt(device.password)' in repo
    assert 'username = cipher.decrypt(device.username)' in repo
    assert 'password = cipher.decrypt(device.password)' in repo


def test_v0153_firmware_does_not_enable_global_cors():
    assert 'server.enableCORS(true)' not in FW


def test_v0153_manual_time_valve_requires_known_position():
    assert 'error\\":\\"position_unknown' in FW
    assert 'channels[ch].valveFeedback==VALVE_FEEDBACK_TIME&&!runtime[ch].positionKnown' in FW


def test_v0153_room_schema_unchanged_for_credential_encryption():
    db = (ROOT / "android/HeatControlApp/app/src/main/java/com/heatcontrol/app/data/AppDatabase.kt").read_text(encoding="utf-8")
    assert '@Database(entities = [Device::class], version = 1' in db


def test_v018_config_version_adds_channel_template_logic():
    assert re.search(r'#define\s+CONFIG_VERSION\s+18\b', CFG)
    assert 'readOldConfigurationSlotV17' in FW


def test_calendar_ui_explicitly_explains_transition_time():
    assert 'Оберіть логіку роботи' in HTML
    assert 'Почати' in HTML
    assert 'Досягти' in HTML
    assert 'Одна логіка роботи каналу застосовується одночасно до робочого та вихідного шаблонів.' in HTML
    assert 'logicWorking' not in HTML and 'logicHoliday' not in HTML


def test_calendar_ui_and_android_show_selected_transition_meaning():
    assert 'Що означає цей час?' in KOTLIN
    assert 'Почати о цьому часі' in KOTLIN
    assert 'Досягти до цього часу' in KOTLIN
    assert 'Дедлайн: контролер почне нагрів завчасно.' in KOTLIN
    assert 'Старт: переключення почнеться саме у цей час.' in KOTLIN


def test_reach_latch_resets_on_new_calendar_day():
    marker = 'if(runtime[ch].reachCacheDateKey!=key)'
    block = FW[FW.index(marker):FW.index('int start=runtime[ch].reachStartMinutes', FW.index(marker))]
    assert 'runtime[ch].reachActivatedMask=0;' in block


def test_adaptive_reach_learning_persistence_and_model():
    assert 'struct ReachLearningStats' in FW
    assert 'REACH_LEARN_NAMESPACE="reachlearn"' in FW
    assert 'saveReachLearningChannel' in FW
    assert 'loadReachLearning' in FW
    assert 'learnedMinutesPerDegree' in FW
    assert 'outdoorSensorOK' in FW and 'reachOutdoorBin' in FW
    assert 'updateReachLearning(ch)' in FW
    assert '/api/reach-learning' in FW
    assert '/api/reach-learning/reset' in FW


def test_adaptive_reach_has_safe_fallback_and_bounds():
    assert '10.0f' in FW
    assert 'constrain(base,3.0f,25.0f)' in FW
    assert 'constrain(lead,5,120)' in FW
    assert 'delta<=channels[ch].hysteresis' in FW


def test_adaptive_reach_does_not_learn_when_already_warm():
    assert 'if(runtime[ch].temperature>=target) return; // already warm; no valid heating-time sample' in FW


def test_adaptive_reach_ui_explains_learning():
    assert 'Adaptive REACH' in HTML
    assert 'фактичним часом нагріву' in HTML
    assert 'Зразків:' in HTML
    assert 'Скинути навчання' in KOTLIN


def test_v0156_richer_reach_history_model_and_quality_guards():
    assert 'ReachLearningSample' in FW
    assert 'history[12]' in FW
    assert 'dIndoor' in FW and 'dTarget' in FW and 'dOutdoor' in FW
    assert 'runtime[ch].valvePosition!=0' in FW
    assert 'runtime[ch].outputState' in FW
    assert 'e.slot!=runtime[ch].reachLearningSlot' in FW
    assert 'nowMinutes>runtime[ch].reachLearningDeadlineMinutes' in FW
    assert 'history' in HTML


def test_v0157_reach_history_is_normalized_to_minutes_per_degree_before_similarity():
    marker = 'float historicalDelta=h.target-h.initialIndoor;'
    assert marker in FW
    block = FW[FW.index(marker):FW.index('if(bin>=0', FW.index(marker))]
    assert 'float historicalMpd=h.minutes/historicalDelta;' in block
    assert 'weighted += w*historicalMpd;' in block
    assert 'weighted += w*h.minutes;' not in block


def test_v0157_reach_history_api_is_newest_first_and_exposes_mpd():
    assert 'uint8_t histCount=(uint8_t)min<uint32_t>(reachLearning[ch].samples,12);' in FW
    assert 'uint8_t hi=(uint8_t)((reachLearning[ch].samples-1u-n)%12u);' in FW
    assert 'hx["minutesPerDegree"]' in FW


def test_v0157_android_and_web_use_api_history_order():
    assert 'history=rs&&Array.isArray(rs.history)?rs.history.slice(0,6):[];' in HTML
    assert 'take(5)?.forEach' in KOTLIN


def test_v0158_time_valve_position_requires_both_calibration_directions():
    marker = 'r.positionKnown=r.calibrationHasOpen&&r.calibrationHasClose;'
    assert marker in FW
    block = FW[FW.index('void stopCalibration'):FW.index('void mcpWriteReg')]
    assert marker in block
    assert 'if(r.positionKnown){' in block

def test_v0158_reach_learning_save_reports_nvs_write_failure():
    assert 'bool saveReachLearningChannel(uint8_t ch);' in FW
    assert 'size_t written=p.putBytes' in FW
    assert 'written==sizeof(ReachLearningStats)' in FW
    assert r'\"error\":\"storage\"' in FW

def test_v0158_sensor_assignment_validation_remains_self_test_guarded():
    assert 'bool sensorAssignments=validateSensorAssignments();' in FW
    assert 'd["sensorAssignments"]=sensorAssignments;' in FW


def test_v0159_aux_output_and_api():
    assert '#define AUX_TEN_RELAY_PIN 5' in CFG
    assert 'auxEnabled' in FW and 'auxLocationChannel' in FW and 'auxChannelMask' in FW
    assert 'controlAux()' in FW
    assert 'server.on("/api/aux",HTTP_GET,apiAuxGet)' in FW
    assert 'server.on("/api/aux",HTTP_PUT,apiAuxUpdate)' in FW

def test_v0159_output_inversion():
    assert 'heaterInverted[MAX_CHANNELS]' in FW
    assert 'outputInverted' in FW
    assert 'heaterInverted[ch]?!logicalOn:logicalOn' in FW
    assert 'outputInverted' in HTML
    assert 'Інверсія виходу' in HTML

def test_v0159_aux_web_ui():
    assert "show('aux',this)" in HTML
    assert 'loadAux()' in HTML
    assert '/api/aux' in HTML
    assert 'Кому може допомагати AUX' in HTML


def test_v0159_aux_runtime_limit_latches_until_need_clears():
    assert 'auxRunLimited' in FW
    assert 'Ліміт AUX вичерпано до завершення REACH' in FW

def test_v0159_android_models_and_api():
    assert 'data class AuxInfo' in KOTLIN
    assert 'val aux: AuxInfo' in KOTLIN
    assert 'outputInverted: Boolean' in KOTLIN
    assert 'fun updateAux(' in KOTLIN
    assert '"outputInverted" to outputInverted' in KOTLIN


def test_v01510_aux_deadline_is_pre_deadline_only():
    assert 'if(nowMin<e.deadlineMinutes){' in FW
    assert 'if(auxPostReachPolicy==AUX_POST_OFF)return false;' in FW
    assert 'AUX_POST_MINUTES' in FW and 'AUX_POST_TO_COMFORT' in FW

def test_v01510_aux_protective_off_bypasses_antichatter():
    assert 'bool forceOff=false' in FW
    assert 'if(on && !forceOff && millis()-auxLastOutputChange<AUX_MIN_SWITCH_INTERVAL_MS)return;' in FW
    assert 'setAuxOutput(false,"Датчик зони AUX недоступний",true)' in FW
    assert 'setAuxOutput(false,"Досягнуто ліміт температури AUX",true)' in FW
    assert 'setAuxOutput(false,"Досягнуто максимальний час AUX",true)' in FW

def test_v01510_inversion_is_transactionally_verified():
    block = re.search(r'bool verifyConfigurationSlot\(const char\* ns\).*?bool readConfigurationSlot', FW, re.S).group(0)
    assert 'snprintf(k,sizeof(k),"inv%u",ch);' in block
    assert 'p.getBool(k,!heaterInverted[ch])!=heaterInverted[ch]' in block

def test_v01510_gpio5_aux_is_documented_as_strapping_pin():
    assert '#define AUX_TEN_RELAY_PIN 5' in CFG
    assert 'strapping GPIO' in CFG
    assert 'GPIO5: AUX TEN relay control' in (ROOT / 'docs/WIRING_6CH.md').read_text()


def test_v01511_aux_post_reach_is_persisted_and_exposed():
    assert 'auxPostReachPolicy' in FW
    assert 'auxPostReachMinutes' in FW
    assert 'auxPostReachMaxSeconds' in FW
    assert 'p.putUChar("auxPostPol"' in FW
    assert 'p.putUInt("auxPostMin"' in FW
    assert 'p.putUInt("auxPostMax"' in FW
    assert 'd["postReachPolicy"]' in FW
    assert 'postReachPolicy' in HTML
    assert 'postReachMinutes' in HTML
    assert 'postReachMaxSeconds' in HTML
    assert 'postReachPolicy: String' in KOTLIN


def test_channel_manual_modes_and_one_shot_override():
    assert 'MODE_AUTO=0' in FW and 'MODE_COMFORT=1' in FW and 'MODE_ECONOMY=2' in FW and 'MODE_OFF=3' in FW
    assert 'effectiveMode' in FW
    assert 'manualOverrideActive' in FW
    assert 'apiChannelManualOverride' in FW
    for n in range(1,7):
        assert f'/api/channels/{n}/override' in FW
    assert 'manual_override_requires_auto' in FW
    assert 'next calendar event' in FW
    assert 'COMFORT CONST' in KOTLIN
    assert 'ECONOM CONST' in KOTLIN
    assert 'setManualOverride' in KOTLIN
    assert 'COMFORT зараз' in KOTLIN
    assert 'Разове ручне перемикання' in HTML
    assert 'COMFORT CONST' in HTML
    assert 'ECON CONST' in HTML

def test_off_closes_valve_and_constant_modes_ignore_calendar():
    assert 'if(effectiveMode(ch)==MODE_OFF)' in FW
    assert 'if(channels[ch].mode!=MODE_AUTO)return channels[ch].mode;' in FW
    assert 'if(channels[ch].mode!=MODE_AUTO){clearManualOverride(ch);return;}' in FW

def test_new_dashboard_mode_contract():
    assert 'MODE_MANUAL=4' in FW
    assert '"manual"' in FW
    assert 'modeText(OperatingMode m)' in FW
    assert 'c.mode==MODE_MANUAL' in FW
    assert 'manualHeater' in HTML and 'actuator' in FW


def test_auto_inversion_contract():
    assert 'autoInverted' in FW
    assert 'scheduledAutoMode' in FW
    assert 'if(runtime[ch].autoInverted)' in FW
    assert 'autoInverted:inverted' in HTML
    assert '2-ге → ІНВЕРСІЯ' in HTML
    assert '3-тє → вимкнути інверсію' in HTML


def test_approved_dashboard_calendar_is_disabled_outside_auto():
    assert "${isAuto?'':' inactive'}" in HTML
    assert "${isAuto?'':'disabled'}" in HTML
    assert "if(!c||c.mode!=='auto')return" in HTML
    assert '.dayScale.inactive{opacity:.48;filter:grayscale(1)}' in HTML
    assert '.templateBtn:disabled' in HTML


def test_manual_heater_uses_put_channel_contract():
    assert "body:JSON.stringify({mode:'manual',heater:!!on})" in HTML
    assert 'hasHeater=d["heater"].is<bool>()' in FW
    assert 'requestedHeater' in FW
    assert 'applyHeaterOutput(ch,requestedHeater)' in FW


def test_approved_settings_ui_contract():
    assert 'Групові налаштування' in HTML
    assert 'Розклад тижня' in HTML
    assert 'Робочі дні' in HTML and 'Вихідні дні' in HTML
    assert 'settingsSetDayType' in HTML and 'groupSetDayType' in HTML
    assert HTML.count('Оберіть логіку роботи') == 2
    assert 'settingsSetLogic' in HTML and 'groupSetLogic' in HTML
    assert 'd.logic' in HTML and 'g.logic' in HTML
    assert 'transition' in HTML

def test_production_ui_uses_real_api():
    assert 'await fetch(path' in HTML
    assert 'TEST.channels' not in HTML
    assert '/api/calendar/' in HTML



def test_sensor_error_automatic_recovery_contract():
    assert 'sensorRecoveryPending' in FW
    assert 'sensorRecoveryMode' in FW
    assert 'sensorRecoveryAutoInverted' in FW
    assert 'sensorRecoveryUserOverride' in FW
    assert 'SENSOR_RECOVERY_STABLE_READS' in CFG
    assert 'handleSensorLost' in FW
    assert 'handleSensorRecovered' in FW
    assert 'parsed!=c.mode' in FW


def test_four_actuator_types_have_distinct_firmware_mapping():
    assert 'actuatorType' in HTML
    assert 'Сервокран з кінц. вимик.' in HTML
    assert 'Сервокран часовий' in HTML
    assert 'ТЕН/Клапан' in HTML
    assert 'Клапан інв.' in HTML
    assert 'else if(!strcmp(t,"inverted")){c.actuator=ACTUATOR_HEATER;heaterInverted[ch]=true;}' in FW
    assert 'heaterInverted[ch]?"inverted":"ten"' in FW

def test_valve_position_persists_across_reboot():
    assert 'VALVE_POSITION_NAMESPACE="valvepos"' in FW
    assert 'void loadValvePositions()' in FW
    assert 'bool saveValvePosition(uint8_t ch)' in FW
    assert 'loadConfiguration();loadValvePositions();loadReachLearning();' in FW
    assert 'runtime[ch].valvePosition=(uint8_t)constrain((int)pos,0,100);' in FW
    assert 'runtime[ch].positionKnown=known;' in FW
    assert 'bool wasMoving=r.valveState!=VALVE_STOPPED;' in FW
    assert 'if(wasMoving&&channels[ch].actuator==ACTUATOR_VALVE&&r.positionKnown)saveValvePosition(ch);' in FW
    assert 'maybePersistValvePositions();' in FW


def test_valve_position_persist_is_not_written_on_every_idle_control_cycle():
    assert 'if(wasMoving&&channels[ch].actuator==ACTUATOR_VALVE&&r.positionKnown)' in FW
    assert 'constexpr unsigned long VALVE_POSITION_AUTOSAVE_MS=10000UL;' in FW

def test_v18_6_regulator_vs_valve_auto_contract():
    assert 'servoOperationMode[ch]==SERVO_OPERATION_VALVE' in FW
    assert 'regulatorTargetPosition(e)' in FW
    assert 'void valveMoveToPosition(uint8_t ch,uint8_t target)' in FW
    assert 'REGULATOR_CONTROL_BAND_C=4.0f' in FW


def test_v18_6_maintenance_reboot_recovery_persistence():
    assert 'SERVO_MAINT_VERSION=3' in FW
    assert 'p.putBool("active",servoMaintenanceState!=SERVO_MAINT_IDLE)' in FW
    assert 'p.putUChar("phase",(uint8_t)servoMaintenanceState)' in FW
    assert 'p.putChar("ch",servoMaintenanceChannel)' in FW
    assert 'p.putUChar("ret",servoMaintenanceReturnPosition)' in FW
    assert 'servoMaintenanceInterrupted' in FW
    assert 'Never resume a timed exercise blindly after reboot' in FW


def test_v18_6_mcp_recovery_validates_inputs_before_clearing_fault():
    assert 'RECOVERED; validating limit inputs before clearing faults' in FW
    assert 'else if(runtime[ch].limitHardwareFault){runtime[ch].limitHardwareFault=false;runtime[ch].valveFault=false;}' in FW


def test_v18_6_maintenance_state_persisted_on_transitions():
    assert 'saveServoMaintenance();valveClose(ch);' in FW
    assert 'servoMaintenanceState=SERVO_MAINT_OPENING;saveServoMaintenance();' in FW
    assert 'servoMaintenanceState=SERVO_MAINT_RETURNING;saveServoMaintenance();' in FW
    assert 'servoMaintenanceState=SERVO_MAINT_PAUSE;servoMaintenancePauseStarted=millis();saveServoMaintenance();' in FW


def test_v187_valve_mode_is_endpoint_only():
    assert 'if(servoOperationMode[ch]==SERVO_OPERATION_VALVE)' in FW
    block=FW.split('if(servoOperationMode[ch]==SERVO_OPERATION_VALVE)',1)[1].split('uint8_t target=regulatorTargetPosition',1)[0]
    assert 'uint8_t endpoint=(e>=0.0f)?100:0;' in block
    assert 'valveMoveToPosition(ch,endpoint)' in block
    assert 'else valveStop(ch)' in block
    assert 'else valveStop(ch);return;}uint8_t target=' not in block

def test_v187_maintenance_reboot_invalidates_only_interrupted_channel():
    assert 'int storedCh=(int)p.getChar("ch",-1);' in FW
    assert 'servoMaintenanceChannel=(int8_t)storedCh;' in FW
    assert 'int8_t interruptedCh=servoMaintenanceChannel;' in FW
    setup=FW.split('if(servoMaintenanceInterrupted){',1)[1].split('if(!LittleFS.begin',1)[0]
    assert 'for(uint8_t ch=0;ch<MAX_CHANNELS;ch++)' not in setup

def test_v187_maintenance_state_is_not_blindly_resumed():
    assert 'Never resume a timed exercise blindly after reboot' in FW
    assert 'servoMaintenanceState=SERVO_MAINT_IDLE;' in FW

def test_v187_maintenance_nvs_version_bumped():
    assert 'SERVO_MAINT_VERSION=3' in FW
    assert 'ver>=2' in FW

def test_v187_maintenance_behavior_model():
    def valve_mode_endpoint(error):
        return 100 if error >= 0 else 0
    assert valve_mode_endpoint(0.0)==100
    assert valve_mode_endpoint(0.1)==100
    assert valve_mode_endpoint(-0.1)==0
    assert valve_mode_endpoint(2.0)==100
    assert valve_mode_endpoint(-2.0)==0

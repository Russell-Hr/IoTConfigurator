#pragma once

#define CONFIG_VERSION 18

#define DEFAULT_CH1_NAME "Канал 1"
#define DEFAULT_CH2_NAME "Канал 2"
#define DEFAULT_CH3_NAME "Канал 3"
#define DEFAULT_CH4_NAME "Канал 4"
#define DEFAULT_CH5_NAME "Канал 5"
#define DEFAULT_CH6_NAME "Канал 6"

#define RTC_SDA 21
#define RTC_SCL 22
#define ONE_WIRE_PIN 4

#define CH1_OUT_A 16
#define CH1_OUT_B 17
#define CH2_OUT_A 18
#define CH2_OUT_B 19
#define CH3_OUT_A 23
#define CH3_OUT_B 25
#define CH4_OUT_A 26
#define CH4_OUT_B 27
#define CH5_OUT_A 32
#define CH5_OUT_B 33
#define CH6_OUT_A 13
#define CH6_OUT_B 14
#define AUX_TEN_RELAY_PIN 5 // ESP32 strapping GPIO; relay input must not disturb boot sampling

#define MAX_CHANNELS 6
#define MAX_SENSORS 7

// One dedicated AUX heater output. It is NOT CH7.
#define AUX_DEFAULT_MAX_RUNTIME_SECONDS 1800UL
#define AUX_DEFAULT_MAX_LOCATION_TEMP_C 28.0f
#define AUX_DEFAULT_POST_REACH_MINUTES 30UL
#define AUX_DEFAULT_POST_REACH_MAX_SECONDS 7200UL
#define AUX_MIN_SWITCH_INTERVAL_MS 30000UL

// Optional MCP23017 GPIO expander at I2C address 0x20.
// Inputs 0..11 are CH1..CH6 OPEN/CLOSE limit switches.
#define LIMIT_SWITCH_ACTIVE_LOW 1
#define LIMIT_SWITCH_POLL_MS 50UL
#define LIMIT_SWITCH_DEBOUNCE_MS 30UL
#define GPIO_EXPANDER_RECHECK_MS 5000UL

// Set to 1 for a bench/software test build. All physical relay outputs remain OFF.
#define HEATCONTROL_TEST_MODE 0

// IMPORTANT: verify this with the actual relay board before connecting any load.
#define RELAY_ON HIGH
#define RELAY_OFF LOW

#define SENSOR_READ_INTERVAL_MS 5000UL
#define SENSOR_RECOVERY_STABLE_READS 2U
#define MIN_SWITCH_INTERVAL_MS 30000UL
// Software dead-time between opposite valve directions. Hardware interlocking is still recommended.
#define VALVE_DIRECTION_DEADTIME_MS 100UL
#define MAX_VALVE_RUN_SECONDS 300UL
#define MIN_SETPOINT_C 5.0f
#define MAX_SETPOINT_C 35.0f
#define MIN_HYSTERESIS_C 0.1f
#define MAX_HYSTERESIS_C 5.0f

// HTTP Basic Auth for the web UI and the entire /api/* surface.
// IMPORTANT: change these before real deployment.
#define DEFAULT_AUTH_USER "admin"
#define DEFAULT_AUTH_PASS "changeme123"
#define DEFAULT_AP_PASSWORD "hc-change-me"
#define MIN_PASSWORD_LEN 6
#define MIN_AP_PASSWORD_LEN 8

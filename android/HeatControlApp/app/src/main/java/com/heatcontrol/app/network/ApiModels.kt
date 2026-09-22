package com.heatcontrol.app.network

data class SensorReading(
    val rom: String,
    val temperature: Double?
)

data class ChannelInfo(
    val id: Int,
    val name: String,
    val actuator: String, // "valve" | "heater"
    val mode: String,     // persistent base: "auto" | "comfort" | "economy" | "off"
    val baseMode: String,
    val autoMode: String = "economy",
    val autoInverted: Boolean = false,
    val effectiveMode: String,
    val manualOverrideActive: Boolean,
    val manualOverrideMode: String,
    val sensorIndex: Int,
    val sensorRom: String,
    val temperature: Double?,
    val sensorOK: Boolean,
    val target: Double?,
    val comfort: Double,
    val economy: Double,
    val hysteresis: Double,
    val openTime: Int,
    val closeTime: Int,
    val valveFeedback: String,
    val limitSwitchAvailable: Boolean,
    val openLimit: Boolean,
    val closeLimit: Boolean,
    val valveFault: Boolean,
    val valvePosition: Int,
    val positionKnown: Boolean,
    val calibrating: Boolean,
    val valveMoving: Boolean,
    val heater: Boolean,
    val calibrationInitial: String,
    val calibrationDirection: String,
    val calibrationElapsed: Int,
    val calibrationOpenMeasured: Int,
    val calibrationCloseMeasured: Int,
    val outputInverted: Boolean
)

data class AuxInfo(
    val enabled: Boolean,
    val locationChannel: Int,
    val channelMask: Int,
    val output: Boolean,
    val reason: String,
    val maxRuntimeSeconds: Int,
    val maxLocationTemp: Double,
    val postReachPolicy: String, // "off" | "minutes" | "comfort"
    val postReachMinutes: Int,
    val postReachMaxSeconds: Int
)

data class DeviceStatus(
    val ip: String,
    val netId: String,
    val deviceName: String,
    val staConnected: Boolean,
    val staIP: String,
    val staSSID: String,
    val sensorCount: Int,
    val uptime: Long,
    val rtcOK: Boolean,
    val apPasswordConfigured: Boolean,
    val sensors: List<SensorReading>,
    val outdoorSensorRom: String,
    val outdoorTemperature: Double?,
    val outdoorSensorOK: Boolean,
    val channels: List<ChannelInfo>,
    val aux: AuxInfo
)

data class ScheduleSlot(
    val hour: Int,
    val minute: Int,
    val mode: String, // "comfort" | "economy"
    val enabled: Boolean,
    val transition: String // "start" | "reach"
)

data class WeekTemplate(
    val days: List<List<ScheduleSlot>> // 7 days, 6 slots per day
)

data class CalendarConfig(
    val dayTypes: List<String>, // 7 entries: "working" | "holiday"
    val working: WeekTemplate,
    val holiday: WeekTemplate
)

data class RtcInfo(
    val date: String,
    val time: String,
    val datetime: String
)

data class DiscoverResult(
    val netId: String,
    val name: String,
    val staConnected: Boolean
)


data class ReachLearningSample(
    val initialIndoor: Double,
    val target: Double,
    val outdoor: Double?,
    val minutes: Double,
    val minutesPerDegree: Double? = null
)

data class ReachLearningChannel(
    val channel: Int,
    val samples: Int,
    val minutesPerDegree: Double,
    val active: Boolean,
    val outdoorUsed: Boolean,
    val history: List<ReachLearningSample> = emptyList()
)

data class ReachLearningInfo(
    val outdoorAvailable: Boolean,
    val outdoorTemperature: Double?,
    val channels: List<ReachLearningChannel>
)

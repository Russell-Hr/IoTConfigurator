package com.heatcontrol.app.network

import android.util.Base64
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit

private val JSON = "application/json".toMediaType()

/**
 * Talks to exactly one HeatControl unit over its local HTTP API. One instance is created
 * per [Device][com.heatcontrol.app.data.Device] and cached by the view layer, since each
 * unit has its own host + credentials. All methods are blocking (plain OkHttp calls) -
 * callers must invoke them from a background dispatcher (e.g. Dispatchers.IO).
 */
class HeatControlApiClient(
    private val host: String,
    private val username: String,
    private val password: String
) {
    private val client = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(8, TimeUnit.SECONDS)
        .writeTimeout(8, TimeUnit.SECONDS)
        .build()

    private fun authHeader(): String {
        val raw = "$username:$password"
        return "Basic " + Base64.encodeToString(raw.toByteArray(), Base64.NO_WRAP)
    }

    private fun url(path: String) = heatControlUrl(host, path)

    private fun request(path: String, method: String, body: JSONObject? = null): JSONObject? {
        val builder = Request.Builder().url(url(path)).header("Authorization", authHeader())
        val reqBody = body?.toString()?.toRequestBody(JSON)
        when (method) {
            "GET" -> builder.get()
            "POST" -> builder.post(reqBody ?: "{}".toRequestBody(JSON))
            "PUT" -> builder.put(reqBody ?: "{}".toRequestBody(JSON))
            else -> throw IllegalArgumentException("Unsupported method $method")
        }
        client.newCall(builder.build()).execute().use { resp ->
            val text = resp.body?.string().orEmpty()
            if (!resp.isSuccessful) {
                val msg = runCatching { JSONObject(text).optString("error", text) }.getOrDefault(text)
                throw ApiException(resp.code, msg.ifBlank { "HTTP ${resp.code}" })
            }
            if (text.isBlank()) return null
            return runCatching { JSONObject(text) }.getOrNull()
        }
    }

    private fun requestArray(path: String): JSONArray {
        val builder = Request.Builder().url(url(path)).header("Authorization", authHeader()).get()
        client.newCall(builder.build()).execute().use { resp ->
            val text = resp.body?.string().orEmpty()
            if (!resp.isSuccessful) throw ApiException(resp.code, "HTTP ${resp.code}")
            return JSONArray(text)
        }
    }

    fun getStatus(): DeviceStatus {
        val d = request("/api/status", "GET") ?: throw ApiException(-1, "Empty response")
        val sensors = d.optJSONArray("sensors")?.let { arr ->
            (0 until arr.length()).map { i ->
                val s = arr.getJSONObject(i)
                SensorReading(s.optString("rom"), s.optDouble("temperature").takeIf { !s.isNull("temperature") })
            }
        } ?: emptyList()
        val channels = d.optJSONArray("channels")?.let { parseChannels(it) } ?: emptyList()
        return DeviceStatus(
            ip = d.optString("ip"),
            netId = d.optString("netId"),
            deviceName = d.optString("deviceName"),
            staConnected = d.optBoolean("staConnected"),
            staIP = d.optString("staIP"),
            staSSID = d.optString("staSSID"),
            sensorCount = d.optInt("sensorCount"),
            uptime = d.optLong("uptime"),
            rtcOK = d.optBoolean("rtcOK"),
            apPasswordConfigured = d.optBoolean("apPasswordConfigured"),
            outdoorSensorRom = d.optString("outdoorSensorRom"),
            outdoorTemperature = d.optDouble("outdoorTemperature").takeIf { !d.isNull("outdoorTemperature") },
            outdoorSensorOK = d.optBoolean("outdoorSensorOK"),
            sensors = sensors,
            channels = channels,
            aux = parseAux(d.optJSONObject("aux"))
        )
    }

    fun getChannels(): List<ChannelInfo> = parseChannels(requestArray("/api/channels"))

    private fun parseAux(o: JSONObject?): AuxInfo = AuxInfo(
        enabled = o?.optBoolean("enabled", false) ?: false,
        locationChannel = o?.optInt("locationChannel", 5) ?: 5,
        channelMask = o?.optInt("channelMask", 0) ?: 0,
        output = o?.optBoolean("output", false) ?: false,
        reason = o?.optString("reason", "Вимкнено") ?: "Вимкнено",
        maxRuntimeSeconds = o?.optInt("maxRuntimeSeconds", 1800) ?: 1800,
        maxLocationTemp = o?.optDouble("maxLocationTemp", 28.0) ?: 28.0,
        postReachPolicy = o?.optString("postReachPolicy", "off") ?: "off",
        postReachMinutes = o?.optInt("postReachMinutes", 30) ?: 30,
        postReachMaxSeconds = o?.optInt("postReachMaxSeconds", 7200) ?: 7200
    )

    fun getAux(): AuxInfo {
        val o = request("/api/aux", "GET") ?: throw ApiException(-1, "Empty response")
        return parseAux(o)
    }

    fun updateAux(enabled: Boolean, locationChannel: Int, channelMask: Int, maxRuntimeSeconds: Int, maxLocationTemp: Double, postReachPolicy: String, postReachMinutes: Int, postReachMaxSeconds: Int) {
        request("/api/aux", "PUT", JSONObject()
            .put("enabled", enabled)
            .put("locationChannel", locationChannel)
            .put("channelMask", channelMask)
            .put("maxRuntimeSeconds", maxRuntimeSeconds)
            .put("maxLocationTemp", maxLocationTemp)
            .put("postReachPolicy", postReachPolicy)
            .put("postReachMinutes", postReachMinutes)
            .put("postReachMaxSeconds", postReachMaxSeconds))
    }

    private fun parseChannels(arr: JSONArray): List<ChannelInfo> = (0 until arr.length()).map { i ->
        val c = arr.getJSONObject(i)
        ChannelInfo(
            id = c.optInt("id"),
            name = c.optString("name"),
            actuator = c.optString("actuator"),
            mode = c.optString("mode"),
            baseMode = c.optString("baseMode", c.optString("mode")),
            autoMode = c.optString("autoMode", "economy"),
            autoInverted = c.optBoolean("autoInverted", false),
            effectiveMode = c.optString("effectiveMode", c.optString("mode")),
            manualOverrideActive = c.optBoolean("manualOverrideActive", false),
            manualOverrideMode = c.optString("manualOverrideMode", ""),
            sensorIndex = c.optInt("sensorIndex"),
            sensorRom = c.optString("sensorRom"),
            temperature = c.optDouble("temperature").takeIf { !c.isNull("temperature") },
            sensorOK = c.optBoolean("sensorOK"),
            target = c.optDouble("target").takeIf { !c.isNull("target") },
            comfort = c.optDouble("comfort"),
            economy = c.optDouble("economy"),
            hysteresis = c.optDouble("hysteresis"),
            openTime = c.optInt("openTime"),
            closeTime = c.optInt("closeTime"),
            valveFeedback = c.optString("valveFeedback", "time"),
            limitSwitchAvailable = c.optBoolean("limitSwitchAvailable"),
            openLimit = c.optBoolean("openLimit"),
            closeLimit = c.optBoolean("closeLimit"),
            valveFault = c.optBoolean("valveFault"),
            valvePosition = c.optInt("valvePosition"),
            positionKnown = c.optBoolean("positionKnown"),
            calibrating = c.optBoolean("calibrating"),
            valveMoving = c.optBoolean("valveMoving"),
            heater = c.optBoolean("heater"),
            calibrationInitial = c.optString("calibrationInitial", "closed"),
            calibrationDirection = c.optString("calibrationDirection", "stop"),
            calibrationElapsed = c.optInt("calibrationElapsed"),
            calibrationOpenMeasured = c.optInt("calibrationOpenMeasured"),
            calibrationCloseMeasured = c.optInt("calibrationCloseMeasured"),
            outputInverted = c.optBoolean("outputInverted", false)
        )
    }


    fun setManualOverride(channelId: Int, mode: String) {
        request("/api/channels/$channelId/override", "POST", JSONObject().put("mode", mode))
    }

    /** [fields] keys match the firmware's PUT /api/channels/{id} body: name, actuator,
     *  mode, sensorIndex, sensorRom, comfort, economy, hysteresis, openTime, closeTime, valveFeedback. Omit any
     *  key you don't want to change. */
    fun updateChannel(channelId: Int, fields: Map<String, Any?>) {
        val body = JSONObject()
        for ((k, v) in fields) if (v != null) body.put(k, v)
        request("/api/channels/$channelId", "PUT", body)
    }

    fun getCalendar(channelId: Int): CalendarConfig {
        val d = request("/api/calendar/$channelId", "GET") ?: throw ApiException(-1, "Empty response")
        val types = d.optJSONArray("dayTypes") ?: throw ApiException(-1, "Invalid calendar: dayTypes")
        if (types.length() != 7) throw ApiException(-1, "Invalid calendar: 7 day types required")

        fun parseTemplate(arr: JSONArray): WeekTemplate {
            if (arr.length() != 7) throw ApiException(-1, "Invalid calendar template: 7 days required")
            val days = (0 until 7).map { dayIndex ->
                val slots = arr.getJSONArray(dayIndex)
                if (slots.length() != 6) throw ApiException(-1, "Invalid calendar template: 6 slots required")
                (0 until 6).map { slotIndex ->
                    val z = slots.getJSONObject(slotIndex)
                    ScheduleSlot(
                        z.optInt("hour"), z.optInt("minute"),
                        z.optString("mode", "economy"), z.optBoolean("enabled"),
                        z.optString("transition", "start")
                    )
                }
            }
            return WeekTemplate(days)
        }

        return CalendarConfig(
            dayTypes = (0 until 7).map { types.optString(it, "working") },
            working = parseTemplate(d.optJSONArray("working") ?: throw ApiException(-1, "Invalid calendar: working")),
            holiday = parseTemplate(d.optJSONArray("holiday") ?: throw ApiException(-1, "Invalid calendar: holiday"))
        )
    }

    fun updateCalendar(channelId: Int, calendar: CalendarConfig) {
        if (calendar.dayTypes.size != 7) throw IllegalArgumentException("7 day types required")
        fun templateJson(template: WeekTemplate): JSONArray {
            if (template.days.size != 7 || template.days.any { it.size != 6 }) {
                throw IllegalArgumentException("Calendar template must be 7x6")
            }
            val out = JSONArray()
            template.days.forEach { slots ->
                val slotsArr = JSONArray()
                slots.forEach { slot ->
                    slotsArr.put(
                        JSONObject()
                            .put("hour", slot.hour)
                            .put("minute", slot.minute)
                            .put("mode", slot.mode)
                            .put("enabled", slot.enabled)
                            .put("transition", slot.transition)
                    )
                }
                out.put(slotsArr)
            }
            return out
        }

        val typesArr = JSONArray()
        calendar.dayTypes.forEach { type -> typesArr.put(type) }
        val body = JSONObject()
            .put("dayTypes", typesArr)
            .put("working", templateJson(calendar.working))
            .put("holiday", templateJson(calendar.holiday))
        request("/api/calendar/$channelId", "PUT", body)
    }

    fun valveCommand(cmd: String, channelId: Int) {
        val builder = Request.Builder().url(url("/api/valve/$cmd?ch=$channelId")).header("Authorization", authHeader()).post("{}".toRequestBody(JSON))
        client.newCall(builder.build()).execute().use { resp -> if (!resp.isSuccessful) throw ApiException(resp.code, resp.body?.string().orEmpty()) }
    }

    fun valveCalibration(action: String, channelId: Int, initial: String? = null) {
        val body = JSONObject().put("action", action)
        if (initial != null) body.put("initial", initial)
        request("/api/valve/calibration?ch=$channelId", "POST", body)
    }

    fun updateOutdoorSensor(sensorRom: String) {
        request("/api/system/outdoor", "PUT", JSONObject().put("sensorRom", sensorRom))
    }

    fun getRtc(): RtcInfo {
        val d = request("/api/rtc", "GET") ?: throw ApiException(-1, "Empty response")
        return RtcInfo(d.optString("date"), d.optString("time"), d.optString("datetime"))
    }

    fun setRtc(date: String, time: String) {
        request("/api/rtc", "PUT", JSONObject().put("date", date).put("time", time))
    }

    fun syncNtp() {
        request("/api/rtc/ntp", "POST")
    }

    fun reboot() {
        request("/api/system/reboot", "POST")
    }

    fun factoryReset() {
        request("/api/system/reset", "POST")
    }

    fun changeAuth(newUsername: String, currentPassword: String, newPassword: String) {
        request(
            "/api/system/auth", "PUT",
            JSONObject()
                .put("username", newUsername)
                .put("currentPassword", currentPassword)
                .put("password", newPassword)
        )
    }

    /** Changes the controller's own AP password. The unit reboots to apply it. */
    fun updateApPassword(newPassword: String): Boolean {
        if (newPassword.length < 8 || newPassword.length > 63) {
            throw IllegalArgumentException("AP password must be 8-63 characters")
        }
        val resp = request("/api/system/ap", "PUT", JSONObject().put("password", newPassword))
        return resp?.optBoolean("rebooting", false) ?: false
    }

    fun updateNetwork(deviceName: String?, ssid: String?, password: String?): Boolean {
        val body = JSONObject()
        if (deviceName != null) body.put("deviceName", deviceName)
        if (ssid != null) {
            body.put("ssid", ssid)
            if (password != null) body.put("password", password)
        }
        val resp = request("/api/system/wifi", "PUT", body)
        return resp?.optBoolean("rebooting", false) ?: false
    }

    companion object {
        /** Unauthenticated - only for identifying a device before its credentials are
         *  known (e.g. while adding it in the app). */
        fun discover(host: String): DiscoverResult {
            val client = OkHttpClient.Builder()
                .connectTimeout(3, TimeUnit.SECONDS)
                .readTimeout(3, TimeUnit.SECONDS)
                .build()
            val req = Request.Builder().url(heatControlUrl(host, "/api/discover")).get().build()
            client.newCall(req).execute().use { resp ->
                if (!resp.isSuccessful) throw ApiException(resp.code, "HTTP ${resp.code}")
                val d = JSONObject(resp.body?.string().orEmpty())
                return DiscoverResult(d.optString("netId"), d.optString("name"), d.optBoolean("staConnected"))
            }
        }
    }
    suspend fun getReachLearning(): ReachLearningInfo {
        val o = request("/api/reach-learning", "GET") ?: throw ApiException(-1, "Empty response")
        val arr = o.optJSONArray("channels")
        val list = mutableListOf<ReachLearningChannel>()
        if (arr != null) for (i in 0 until arr.length()) {
            val x = arr.getJSONObject(i)
            val history = mutableListOf<ReachLearningSample>()
            val h = x.optJSONArray("history")
            if (h != null) for (j in 0 until h.length()) {
                val hs = h.getJSONObject(j)
                history += ReachLearningSample(
                    hs.optDouble("initialIndoor"),
                    hs.optDouble("target"),
                    if (hs.isNull("outdoor")) null else hs.optDouble("outdoor"),
                    hs.optDouble("minutes"),
                    if (hs.isNull("minutesPerDegree")) null else hs.optDouble("minutesPerDegree")
                )
            }
            list += ReachLearningChannel(x.optInt("channel"), x.optInt("samples"), x.optDouble("minutesPerDegree", 10.0), x.optBoolean("active"), x.optBoolean("outdoorUsed"), history)
        }
        return ReachLearningInfo(o.optBoolean("outdoorAvailable"), if (o.isNull("outdoorTemperature")) null else o.optDouble("outdoorTemperature"), list)
    }

    fun resetReachLearning(channelId: Int) { request("/api/reach-learning/reset?ch=$channelId", "POST", JSONObject()) }

}

package com.heatcontrol.app.ui.device

import com.heatcontrol.app.network.CalendarConfig

import com.heatcontrol.app.data.Device
import com.heatcontrol.app.data.DeviceRepository
import com.heatcontrol.app.network.DeviceStatus
import com.heatcontrol.app.network.HeatControlApiClient
import com.heatcontrol.app.network.RtcInfo
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext

/**
 * Owns the connection to one HeatControl unit: loads its saved [Device] record, polls
 * /api/status every 5s, and exposes every write action the UI needs. One instance is
 * created per device id and cached by [com.heatcontrol.app.HeatControlApplication] for
 * the life of the app process (or until the device is deleted from the list) - it is
 * intentionally *not* an AndroidX ViewModel, so its lifetime doesn't depend on which
 * Compose screen is currently on top, only on whether the device is still in the list.
 */
class DeviceController(
    private val deviceId: Long,
    private val repository: DeviceRepository
) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    private val _device = MutableStateFlow<Device?>(null)
    val device: StateFlow<Device?> = _device.asStateFlow()

    private val _status = MutableStateFlow<DeviceStatus?>(null)
    val status: StateFlow<DeviceStatus?> = _status.asStateFlow()

    private val _error = MutableStateFlow<String?>(null)
    val error: StateFlow<String?> = _error.asStateFlow()

    private val _loading = MutableStateFlow(true)
    val loading: StateFlow<Boolean> = _loading.asStateFlow()

    private var client: HeatControlApiClient? = null
    private val statusMutex = Mutex()

    init {
        scope.launch {
            val d = repository.getById(deviceId)
            _device.value = d
            _loading.value = false
            if (d != null) {
                client = HeatControlApiClient(d.host, d.username, d.password)
                pollStatusLoop()
            } else {
                _error.value = "Пристрій не знайдено в списку"
            }
        }
    }

    private suspend fun refreshStatusOnce() {
        statusMutex.withLock {
            val c = client ?: return
            try {
                val s = withContext(Dispatchers.IO) { c.getStatus() }
                _status.value = s
                _error.value = null
            } catch (e: Exception) {
                _error.value = e.message ?: "Помилка з'єднання"
            }
        }
    }

    private suspend fun pollStatusLoop() {
        while (scope.isActive) {
            refreshStatusOnce()
            delay(5000)
        }
    }

    fun refreshNow() {
        scope.launch { refreshStatusOnce() }
    }

    private fun <T> runAction(block: (HeatControlApiClient) -> T, onSuccess: (T) -> Unit = {}) {
        val c = client ?: run { _error.value = "Клієнт не готовий"; return }
        scope.launch {
            try {
                val result = withContext(Dispatchers.IO) { block(c) }
                _error.value = null
                onSuccess(result)
                refreshStatusOnce()
            } catch (e: Exception) {
                _error.value = e.message ?: "Помилка запиту"
            }
        }
    }

    fun updateChannel(channelId: Int, fields: Map<String, Any?>) =
        runAction({ it.updateChannel(channelId, fields) })

    fun setManualOverride(channelId: Int, mode: String) =
        runAction({ it.setManualOverride(channelId, mode) })

    fun valveCommand(cmd: String, channelId: Int) = runAction({ it.valveCommand(cmd, channelId) })

    fun valveCalibration(action: String, channelId: Int, initial: String? = null) = runAction({ it.valveCalibration(action, channelId, initial) })

    fun updateOutdoorSensor(sensorRom: String, onDone: (Boolean) -> Unit = {}) = runAction({ it.updateOutdoorSensor(sensorRom); true }, onDone)

    fun updateAux(enabled: Boolean, locationChannel: Int, channelMask: Int, maxRuntimeSeconds: Int, maxLocationTemp: Double, postReachPolicy: String, postReachMinutes: Int, postReachMaxSeconds: Int, onDone: (Boolean) -> Unit = {}) = runAction({ it.updateAux(enabled, locationChannel, channelMask, maxRuntimeSeconds, maxLocationTemp, postReachPolicy, postReachMinutes, postReachMaxSeconds); true }, onDone)

    fun reboot() = runAction({ it.reboot() })

    fun factoryReset(onDone: (Boolean) -> Unit = {}) = runAction({ it.factoryReset(); true }, onDone)

    fun syncNtp() = runAction({ it.syncNtp() })

    fun setRtc(date: String, time: String) = runAction({ it.setRtc(date, time) })

    suspend fun setRtcAndWait(date: String, time: String) {
        val c = client ?: throw IllegalStateException("Клієнт не готовий")
        withContext(Dispatchers.IO) { c.setRtc(date, time) }
    }

    suspend fun syncNtpAndWait() {
        val c = client ?: throw IllegalStateException("Клієнт не готовий")
        withContext(Dispatchers.IO) { c.syncNtp() }
    }

    fun changeAuth(username: String, currentPassword: String, newPassword: String, onDone: (Boolean) -> Unit) {
        val c = client ?: return
        scope.launch {
            try {
                withContext(Dispatchers.IO) { c.changeAuth(username, currentPassword, newPassword) }
                // The saved Device record's credentials are now stale - update them so
                // future calls from this app keep working without re-prompting the user.
                _device.value?.let { d ->
                    val updated = d.copy(username = username, password = newPassword)
                    repository.update(updated)
                    _device.value = updated
                    client = HeatControlApiClient(updated.host, updated.username, updated.password)
                }
                _error.value = null
                onDone(true)
            } catch (e: Exception) {
                _error.value = e.message ?: "Помилка зміни пароля"
                onDone(false)
            }
        }
    }

    fun updateApPassword(newPassword: String, onDone: (rebooting: Boolean) -> Unit) {
        val c = client ?: run { _error.value = "Клієнт не готовий"; onDone(false); return }
        scope.launch {
            try {
                val rebooting = withContext(Dispatchers.IO) { c.updateApPassword(newPassword) }
                _error.value = null
                onDone(rebooting)
            } catch (e: Exception) {
                _error.value = e.message ?: "Помилка зміни пароля AP"
                onDone(false)
            }
        }
    }

    fun updateNetwork(newDeviceName: String?, ssid: String?, password: String?, onDone: (rebooting: Boolean) -> Unit) {
        val c = client ?: return
        scope.launch {
            try {
                val rebooting = withContext(Dispatchers.IO) { c.updateNetwork(newDeviceName, ssid, password) }
                _error.value = null
                onDone(rebooting)
            } catch (e: Exception) {
                _error.value = e.message ?: "Помилка зміни мережі"
                onDone(false)
            }
        }
    }

    suspend fun getCalendar(channelId: Int): CalendarConfig {
        val c = client ?: throw IllegalStateException("Клієнт не готовий")
        return withContext(Dispatchers.IO) { c.getCalendar(channelId) }
    }

    suspend fun updateCalendar(channelId: Int, calendar: CalendarConfig) {
        val c = client ?: throw IllegalStateException("Клієнт не готовий")
        withContext(Dispatchers.IO) { c.updateCalendar(channelId, calendar) }
    }

    suspend fun getRtc(): RtcInfo {
        val c = client ?: throw IllegalStateException("Клієнт не готовий")
        return withContext(Dispatchers.IO) { c.getRtc() }
    }

    suspend fun getReachLearning(): com.heatcontrol.app.network.ReachLearningInfo {
        val c = client ?: throw IllegalStateException("Клієнт не готовий")
        return withContext(Dispatchers.IO) { c.getReachLearning() }
    }

    suspend fun resetReachLearning(channelId: Int) {
        val c = client ?: throw IllegalStateException("Клієнт не готовий")
        withContext(Dispatchers.IO) { c.resetReachLearning(channelId) }
    }

    /** Stops the polling loop. Called by HeatControlApplication when this device is
     *  removed from the list - never by the UI layer. */
    fun close() {
        scope.cancel()
    }
}

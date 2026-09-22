package com.heatcontrol.app.ui.devicelist

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.heatcontrol.app.HeatControlApplication
import com.heatcontrol.app.data.Device
import com.heatcontrol.app.discovery.DiscoveredService
import com.heatcontrol.app.discovery.NsdDiscoveryHelper
import com.heatcontrol.app.network.HeatControlApiClient
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

sealed class TestState {
    object Idle : TestState()
    object Testing : TestState()
    data class Success(val deviceName: String, val netId: String) : TestState()
    data class Failed(val message: String) : TestState()
}

class AddDeviceViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = (application as HeatControlApplication).repository
    private val nsd = NsdDiscoveryHelper(application)

    private val _discovered = MutableStateFlow<List<DiscoveredService>>(emptyList())
    val discovered: StateFlow<List<DiscoveredService>> = _discovered.asStateFlow()

    private val _scanning = MutableStateFlow(false)
    val scanning: StateFlow<Boolean> = _scanning.asStateFlow()

    private val _testState = MutableStateFlow<TestState>(TestState.Idle)
    val testState: StateFlow<TestState> = _testState.asStateFlow()

    fun startScan() {
        _discovered.value = emptyList()
        _scanning.value = true
        nsd.startDiscovery(
            onFound = { svc ->
                if (_discovered.value.none { it.netId == svc.netId }) {
                    _discovered.value = _discovered.value + svc
                }
            },
            onError = { _scanning.value = false }
        )
    }

    fun stopScan() {
        nsd.stopDiscovery()
        _scanning.value = false
    }

    fun testConnection(host: String, username: String, password: String) {
        _testState.value = TestState.Testing
        viewModelScope.launch {
            try {
                val status = withContext(Dispatchers.IO) {
                    HeatControlApiClient(host, username, password).getStatus()
                }
                _testState.value = TestState.Success(status.deviceName, status.netId)
            } catch (e: Exception) {
                _testState.value = TestState.Failed(e.message ?: "Не вдалося з'єднатися")
            }
        }
    }

    fun resetTest() {
        _testState.value = TestState.Idle
    }

    /** Returns null if the device was saved, or an error message. */
    suspend fun save(label: String, host: String, netId: String, username: String, password: String): String? {
        if (!repository.canAddMore()) return "Досягнуто ліміту 10 пристроїв"
        val id = repository.add(
            Device(label = label, host = host, netId = netId, username = username, password = password)
        )
        return if (id == null) "Не вдалося зберегти пристрій (ліміт 10)" else null
    }

    override fun onCleared() {
        nsd.close()
        super.onCleared()
    }
}

package com.heatcontrol.app.ui.devicelist

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.heatcontrol.app.HeatControlApplication
import com.heatcontrol.app.data.Device
import com.heatcontrol.app.data.MAX_DEVICES
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class DeviceListViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = (application as HeatControlApplication).repository

    val devices: StateFlow<List<Device>> = repository.observeAll()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    fun delete(device: Device) {
        viewModelScope.launch {
            repository.delete(device)
            (getApplication<Application>() as HeatControlApplication).releaseDeviceController(device.id)
        }
    }
}

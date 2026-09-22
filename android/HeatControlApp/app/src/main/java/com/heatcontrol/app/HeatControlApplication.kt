package com.heatcontrol.app

import android.app.Application
import com.heatcontrol.app.data.AppDatabase
import com.heatcontrol.app.data.DeviceRepository
import com.heatcontrol.app.ui.device.DeviceController

class HeatControlApplication : Application() {

    val database: AppDatabase by lazy { AppDatabase.getInstance(this) }
    val repository: DeviceRepository by lazy { DeviceRepository(database.deviceDao(), com.heatcontrol.app.data.CredentialCipher(this)) }

    private val deviceControllers = mutableMapOf<Long, DeviceController>()

    /** Returns the cached controller for this device, creating one on first access. */
    @Synchronized
    fun getDeviceController(deviceId: Long): DeviceController =
        deviceControllers.getOrPut(deviceId) { DeviceController(deviceId, repository) }

    /** Stops polling and drops the cached controller. Call when a device is removed
     *  from the list - never on ordinary navigation away from its screens. */
    @Synchronized
    fun releaseDeviceController(deviceId: Long) {
        deviceControllers.remove(deviceId)?.close()
    }
}

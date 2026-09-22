package com.heatcontrol.app.data

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

class DeviceRepository(private val dao: DeviceDao, private val cipher: CredentialCipher) {

    fun observeAll(): Flow<List<Device>> = dao.observeAll().map { list -> list.map(::decrypt) }

    suspend fun getById(id: Long): Device? = dao.getById(id)?.let(::decrypt)

    /** Returns the new device's id, or null if the [MAX_DEVICES] cap was reached. */
    suspend fun add(device: Device): Long? {
        if (dao.count() >= MAX_DEVICES) return null
        return dao.insert(encrypt(device))
    }

    suspend fun update(device: Device) = dao.update(encrypt(device))

    private fun encrypt(device: Device): Device = device.copy(
        username = cipher.encrypt(device.username),
        password = cipher.encrypt(device.password)
    )

    private fun decrypt(device: Device): Device = device.copy(
        username = cipher.decrypt(device.username),
        password = cipher.decrypt(device.password)
    )

    suspend fun delete(device: Device) = dao.delete(device)

    suspend fun canAddMore(): Boolean = dao.count() < MAX_DEVICES
}

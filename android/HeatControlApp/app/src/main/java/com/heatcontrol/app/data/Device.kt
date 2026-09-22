package com.heatcontrol.app.data

import androidx.room.Entity
import androidx.room.PrimaryKey
import com.heatcontrol.app.network.heatControlUrl

/**
 * One managed HeatControl unit, as saved locally on the phone.
 *
 * [host] is whatever the user (or discovery) supplied to reach the device: an mDNS
 * name like "hc-a1b2c3.local", a plain IP like "192.168.1.42", or the device's own
 * AP IP "192.168.4.1". [username]/[password] are the unit's HTTP Basic Auth
 * credentials. The repository encrypts both fields with an Android Keystore-backed
 * AES/GCM key before writing them to Room; callers receive the decrypted values.
 */
@Entity(tableName = "devices")
data class Device(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val label: String,
    val host: String,
    val netId: String = "",
    val username: String,
    val password: String
) {
    /** Base URL for this device's HTTP API. */
    fun baseUrl(): String = heatControlUrl(host)
}

/** Hard cap requested for this app: at most this many managed units. */
const val MAX_DEVICES = 10

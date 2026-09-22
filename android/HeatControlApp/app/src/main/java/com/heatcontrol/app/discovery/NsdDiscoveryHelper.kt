package com.heatcontrol.app.discovery

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.util.Log
import com.heatcontrol.app.network.HeatControlApiClient
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.util.Collections
import java.util.concurrent.Executors

data class DiscoveredService(val netId: String, val host: String, val port: Int)

/** HeatControl mDNS discovery using the stable resolveService() path on all Android versions. */
class NsdDiscoveryHelper(context: Context) {
    private val nsdManager = context.applicationContext.getSystemService(Context.NSD_SERVICE) as NsdManager
    private val executorService = Executors.newSingleThreadExecutor()
    private var discoveryListener: NsdManager.DiscoveryListener? = null
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val resolving = Collections.synchronizedSet(mutableSetOf<String>())

    companion object {
        private const val SERVICE_TYPE = "_heatcontrol._tcp."
        private const val TAG = "NsdDiscoveryHelper"
    }

    private fun key(info: NsdServiceInfo) = "${info.serviceName}|${info.serviceType}"

    private fun processResolved(info: NsdServiceInfo, onFound: (DiscoveredService) -> Unit) {
        val host = info.host?.hostAddress ?: return
        scope.launch {
            val discovered = runCatching { HeatControlApiClient.discover(host) }.getOrNull()
            if (discovered != null && discovered.netId.isNotBlank()) {
                onFound(DiscoveredService(discovered.netId, host, info.port))
            } else {
                onFound(DiscoveredService(info.serviceName, host, info.port))
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun resolve(serviceInfo: NsdServiceInfo, onFound: (DiscoveredService) -> Unit) {
        val serviceKey = key(serviceInfo)
        if (!resolving.add(serviceKey)) return
        val listener = object : NsdManager.ResolveListener {
            override fun onResolveFailed(info: NsdServiceInfo, errorCode: Int) {
                resolving.remove(serviceKey)
                Log.w(TAG, "Resolve failed: $errorCode")
            }
            override fun onServiceResolved(info: NsdServiceInfo) {
                resolving.remove(serviceKey)
                processResolved(info, onFound)
            }
        }
        runCatching { nsdManager.resolveService(serviceInfo, listener) }
            .onFailure {
                resolving.remove(serviceKey)
                Log.w(TAG, "Resolve request failed", it)
            }
    }

    fun startDiscovery(onFound: (DiscoveredService) -> Unit, onError: (String) -> Unit) {
        stopDiscovery()
        val listener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(serviceType: String) {}
            override fun onServiceFound(serviceInfo: NsdServiceInfo) { resolve(serviceInfo, onFound) }
            override fun onServiceLost(serviceInfo: NsdServiceInfo) { resolving.remove(key(serviceInfo)) }
            override fun onDiscoveryStopped(serviceType: String) {}
            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
                discoveryListener = null
                onError("Discovery start failed: $errorCode")
            }
            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
                Log.w(TAG, "Discovery stop failed: $errorCode")
            }
        }
        discoveryListener = listener
        runCatching { nsdManager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, listener) }
            .onFailure {
                discoveryListener = null
                onError(it.message ?: "Discovery failed to start")
            }
    }

    fun stopDiscovery() {
        discoveryListener?.let { runCatching { nsdManager.stopServiceDiscovery(it) } }
        discoveryListener = null
        resolving.clear()
    }

    fun close() {
        stopDiscovery()
        scope.cancel()
        executorService.shutdownNow()
    }
}

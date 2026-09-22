package com.heatcontrol.app.ui.device

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AccessTime
import androidx.compose.material.icons.filled.Dashboard
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*

private enum class DeviceTab(val label: String) { DASHBOARD("Дашборд"), RTC("Дата і час"), SYSTEM("Система") }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DeviceHomeScreen(
    controller: DeviceController,
    onOpenChannel: (Int) -> Unit,
    onDeviceReset: () -> Unit
) {
    var tab by remember { mutableStateOf(DeviceTab.DASHBOARD) }
    val device by controller.device.collectAsState()

    Scaffold(
        topBar = {
            TopAppBar(title = { Text(device?.label ?: "HeatControl") })
        },
        bottomBar = {
            NavigationBar {
                NavigationBarItem(
                    selected = tab == DeviceTab.DASHBOARD,
                    onClick = { tab = DeviceTab.DASHBOARD },
                    icon = { Icon(Icons.Default.Dashboard, contentDescription = null) },
                    label = { Text(DeviceTab.DASHBOARD.label) }
                )
                NavigationBarItem(
                    selected = tab == DeviceTab.RTC,
                    onClick = { tab = DeviceTab.RTC },
                    icon = { Icon(Icons.Default.AccessTime, contentDescription = null) },
                    label = { Text(DeviceTab.RTC.label) }
                )
                NavigationBarItem(
                    selected = tab == DeviceTab.SYSTEM,
                    onClick = { tab = DeviceTab.SYSTEM },
                    icon = { Icon(Icons.Default.Settings, contentDescription = null) },
                    label = { Text(DeviceTab.SYSTEM.label) }
                )
            }
        }
    ) { padding ->
        Box(modifier = androidx.compose.ui.Modifier.padding(padding)) {
            when (tab) {
                DeviceTab.DASHBOARD -> DashboardScreen(controller, onOpenChannel)
                DeviceTab.RTC -> RtcScreen(controller)
                DeviceTab.SYSTEM -> SystemScreen(controller, onDeviceReset)
            }
        }
    }
}

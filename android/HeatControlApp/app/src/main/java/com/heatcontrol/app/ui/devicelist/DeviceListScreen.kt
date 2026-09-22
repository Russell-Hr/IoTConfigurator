package com.heatcontrol.app.ui.devicelist

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.heatcontrol.app.data.Device
import com.heatcontrol.app.data.MAX_DEVICES

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DeviceListScreen(
    onOpenDevice: (Long) -> Unit,
    onAddDevice: () -> Unit,
    viewModel: DeviceListViewModel = viewModel()
) {
    val devices by viewModel.devices.collectAsState()
    var pendingDelete by remember { mutableStateOf<Device?>(null) }

    Scaffold(
        topBar = {
            TopAppBar(title = { Text("HeatControl (${devices.size}/$MAX_DEVICES)") })
        },
        floatingActionButton = {
            if (devices.size < MAX_DEVICES) {
                FloatingActionButton(onClick = onAddDevice) {
                    Icon(Icons.Default.Add, contentDescription = "Додати пристрій")
                }
            }
        }
    ) { padding ->
        if (devices.isEmpty()) {
            Box(Modifier.fillMaxSize().padding(padding), contentAlignment = Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Icon(Icons.Default.Thermostat, contentDescription = null, modifier = Modifier.size(48.dp))
                    Spacer(Modifier.height(12.dp))
                    Text("Ще немає жодного пристрою")
                    Spacer(Modifier.height(4.dp))
                    Text("Натисніть \"+\", щоб додати перший HeatControl", style = MaterialTheme.typography.bodySmall)
                }
            }
        } else {
            LazyColumn(Modifier.fillMaxSize().padding(padding)) {
                items(devices, key = { it.id }) { device ->
                    ListItem(
                        headlineContent = { Text(device.label, fontWeight = FontWeight.Bold) },
                        supportingContent = { Text(device.host) },
                        leadingContent = { Icon(Icons.Default.Thermostat, contentDescription = null) },
                        trailingContent = {
                            IconButton(onClick = { pendingDelete = device }) {
                                Icon(Icons.Default.Delete, contentDescription = "Видалити")
                            }
                        },
                        modifier = Modifier.clickable { onOpenDevice(device.id) }
                    )
                    HorizontalDivider()
                }
            }
        }
    }

    pendingDelete?.let { device ->
        AlertDialog(
            onDismissRequest = { pendingDelete = null },
            title = { Text("Видалити пристрій?") },
            text = { Text("\"${device.label}\" буде прибрано зі списку на цьому телефоні. Сам пристрій і його налаштування не зміняться.") },
            confirmButton = {
                TextButton(onClick = { viewModel.delete(device); pendingDelete = null }) { Text("Видалити") }
            },
            dismissButton = {
                TextButton(onClick = { pendingDelete = null }) { Text("Скасувати") }
            }
        )
    }
}

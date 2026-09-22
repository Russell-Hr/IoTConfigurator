package com.heatcontrol.app.ui.device

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.heatcontrol.app.network.ChannelInfo
import com.heatcontrol.app.network.DeviceStatus

@Composable
fun DashboardScreen(controller: DeviceController, onOpenChannel: (Int) -> Unit) {
    val status by controller.status.collectAsState()
    val error by controller.error.collectAsState()
    val loading by controller.loading.collectAsState()

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        error?.let {
            Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.errorContainer)) {
                Text("Немає з'єднання: $it", Modifier.padding(12.dp))
            }
            Spacer(Modifier.height(12.dp))
        }

        if (loading && status == null) {
            Box(Modifier.fillMaxSize(), contentAlignment = androidx.compose.ui.Alignment.Center) {
                CircularProgressIndicator()
            }
        } else {
            status?.let { s ->
                StatusHeader(s)
                Spacer(Modifier.height(10.dp))
                OutdoorCard(s)
            }

            Spacer(Modifier.height(12.dp))

            val channels = status?.channels.orEmpty()
            LazyColumn(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                items(channels, key = { it.id }) { ch ->
                    ChannelCard(ch, onClick = { onOpenChannel(ch.id) })
                }
            }
        }
    }
}

@Composable
private fun StatusHeader(s: DeviceStatus) {
    Card {
        Column(Modifier.padding(14.dp)) {
            Text(s.deviceName.ifBlank { s.netId }, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(4.dp))
            Text(
                if (s.staConnected) "Wi-Fi: ${s.staSSID} (${s.staIP})" else "Wi-Fi: тільки власна точка доступу (${s.ip})",
                style = MaterialTheme.typography.bodySmall
            )
            Text("DS3231: ${if (s.rtcOK) "OK" else "ПОМИЛКА"} · Датчиків: ${s.sensorCount} · Uptime: ${s.uptime}с", style = MaterialTheme.typography.bodySmall)
        }
    }
}

@Composable
private fun ChannelCard(ch: ChannelInfo, onClick: () -> Unit) {
    Card(modifier = Modifier.fillMaxWidth().clickable(onClick = onClick)) {
        Column(Modifier.padding(14.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(ch.name, fontWeight = FontWeight.Bold)
                AssistChip(onClick = {}, label = { Text(modeLabel(ch.mode)) })
            }
            Spacer(Modifier.height(6.dp))
            Text(
                if (ch.sensorOK && ch.temperature != null) "%.1f °C".format(ch.temperature) else "Датчик: помилка",
                style = MaterialTheme.typography.headlineSmall
            )
            ch.target?.let { Text("Ціль: %.1f °C".format(it), style = MaterialTheme.typography.bodySmall) }
            Spacer(Modifier.height(6.dp))
            if (ch.actuator == "valve") {
                Text(
                    "Клапан: ${ch.valvePosition}%${if (ch.valveMoving) " (рухається)" else ""}${if (!ch.positionKnown) " · позиція невідома" else ""}",
                    style = MaterialTheme.typography.bodySmall
                )
            } else {
                Text("ТЕН: ${if (ch.heater) "УВІМК" else "вимк"}", style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}

fun modeLabel(mode: String): String = when (mode) {
    "auto" -> "Авто (розклад)"
    "comfort" -> "Комфорт"
    "economy" -> "Економія"
    else -> "Вимкнено"
}

@Composable
private fun OutdoorCard(s: DeviceStatus) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(14.dp)) {
            Text("🌡 Зовнішня температура", fontWeight = FontWeight.Bold)
            Text("Температура навколишнього середовища / вулиці", style = MaterialTheme.typography.bodySmall)
            Spacer(Modifier.height(4.dp))
            Text(if (s.outdoorSensorOK && s.outdoorTemperature != null) "%.1f °C".format(s.outdoorTemperature) else "--", style = MaterialTheme.typography.headlineMedium)
            Text(if (s.outdoorSensorOK) "Датчик ONLINE" else "Датчик OFFLINE", style = MaterialTheme.typography.bodySmall)
        }
    }
}

package com.heatcontrol.app.ui.device

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch

@Composable
fun RtcScreen(controller: DeviceController) {
    val scope = rememberCoroutineScope()
    var deviceDatetime by remember { mutableStateOf<String?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var dateField by remember { mutableStateOf("") }
    var timeField by remember { mutableStateOf("") }

    suspend fun load() {
        try {
            val rtc = controller.getRtc()
            deviceDatetime = rtc.datetime
            dateField = rtc.date
            timeField = rtc.time
            error = null
        } catch (e: Exception) {
            error = e.message ?: "Не вдалося отримати час"
        }
    }

    LaunchedEffect(Unit) { load() }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("Дата і час на пристрої", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(8.dp))
        error?.let { Text("Помилка: $it", color = MaterialTheme.colorScheme.error); Spacer(Modifier.height(8.dp)) }
        deviceDatetime?.let { Text("Зараз на пристрої: $it") }
        Spacer(Modifier.height(16.dp))

        OutlinedTextField(
            value = dateField, onValueChange = { dateField = it },
            label = { Text("Дата (РРРР-ММ-ДД)") }, modifier = Modifier.fillMaxWidth()
        )
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            value = timeField, onValueChange = { timeField = it },
            label = { Text("Час (ГГ:ХХ)") }, modifier = Modifier.fillMaxWidth()
        )
        Spacer(Modifier.height(16.dp))
        Button(
            onClick = {
                scope.launch {
                    try {
                        controller.setRtcAndWait(dateField, timeField)
                        load()
                    } catch (e: Exception) {
                        error = e.message ?: "Не вдалося встановити час"
                    }
                }
            },
            modifier = Modifier.fillMaxWidth()
        ) { Text("Встановити вручну") }
        Spacer(Modifier.height(8.dp))
        OutlinedButton(
            onClick = {
                scope.launch {
                    try {
                        controller.syncNtpAndWait()
                        load()
                    } catch (e: Exception) {
                        error = e.message ?: "Не вдалося синхронізувати час"
                    }
                }
            },
            modifier = Modifier.fillMaxWidth()
        ) { Text("Синхронізувати через NTP") }
    }
}

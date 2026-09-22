package com.heatcontrol.app.ui.device

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp

@Composable
fun SystemScreen(controller: DeviceController, onDeviceReset: () -> Unit) {
    val status by controller.status.collectAsState()
    val error by controller.error.collectAsState()

    var netDeviceName by remember { mutableStateOf("") }
    var netSsid by remember { mutableStateOf("") }
    var netPassword by remember { mutableStateOf("") }
    var netOpenNetwork by remember { mutableStateOf(false) }
    var netInitialized by remember { mutableStateOf(false) }
    var netMessage by remember { mutableStateOf<String?>(null) }
    var apPassword by remember { mutableStateOf("") }
    var apMessage by remember { mutableStateOf<String?>(null) }
    var outdoorMessage by remember { mutableStateOf<String?>(null) }
    var auxEnabled by remember { mutableStateOf(false) }
    var auxLocation by remember { mutableStateOf(5) }
    var auxMask by remember { mutableStateOf(0) }
    var auxMaxRuntime by remember { mutableStateOf("1800") }
    var auxMaxTemp by remember { mutableStateOf("28.0") }
    var auxPostPolicy by remember { mutableStateOf("off") }
    var auxPostMinutes by remember { mutableStateOf("30") }
    var auxPostMaxSeconds by remember { mutableStateOf("7200") }
    var auxMessage by remember { mutableStateOf<String?>(null) }
    var auxInitialized by remember { mutableStateOf(false) }

    var authUsername by remember { mutableStateOf("") }
    var authCurrentPassword by remember { mutableStateOf("") }
    var authNewPassword by remember { mutableStateOf("") }
    var authMessage by remember { mutableStateOf<String?>(null) }

    var showResetConfirm by remember { mutableStateOf(false) }
    var showRebootConfirm by remember { mutableStateOf(false) }
    var showForgetWifiConfirm by remember { mutableStateOf(false) }

    LaunchedEffect(status) {
        val s = status ?: return@LaunchedEffect
        if (!auxInitialized && s.aux != null) {
            auxEnabled = s.aux.enabled
            auxLocation = s.aux.locationChannel
            auxMask = s.aux.channelMask
            auxMaxRuntime = s.aux.maxRuntimeSeconds.toString()
            auxMaxTemp = s.aux.maxLocationTemp.toString()
            auxPostPolicy = s.aux.postReachPolicy
            auxPostMinutes = s.aux.postReachMinutes.toString()
            auxPostMaxSeconds = s.aux.postReachMaxSeconds.toString()
            auxInitialized = true
        }
        if (!netInitialized) {
            netDeviceName = s.deviceName
            netSsid = s.staSSID
            netInitialized = true
        }
    }

    Column(
        Modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(rememberScrollState())
    ) {
        error?.let { Text("Помилка: $it", color = MaterialTheme.colorScheme.error); Spacer(Modifier.height(12.dp)) }

        Text("📶 Мережа", style = MaterialTheme.typography.titleMedium)
        Text(
            "Мережевий ID: ${status?.netId ?: "..."}  ·  mDNS: ${status?.netId ?: "..."}.local",
            style = MaterialTheme.typography.bodySmall
        )
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(value = netDeviceName, onValueChange = { netDeviceName = it }, label = { Text("Назва пристрою") }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(value = netSsid, onValueChange = { netSsid = it }, label = { Text("SSID домашньої мережі") }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            value = netPassword, onValueChange = { netPassword = it },
            label = { Text("Пароль домашньої мережі (порожній = не міняти)") },
            enabled = !netOpenNetwork,
            visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth()
        )
        Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
            Checkbox(checked = netOpenNetwork, onCheckedChange = { netOpenNetwork = it; if (it) netPassword = "" })
            Text("Відкрита Wi-Fi мережа (без пароля)")
        }
        Spacer(Modifier.height(8.dp))
        netMessage?.let { Text(it, style = MaterialTheme.typography.bodySmall); Spacer(Modifier.height(6.dp)) }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = {
                    controller.updateNetwork(
                        netDeviceName.ifBlank { null },
                        netSsid.ifBlank { null },
                        if (netOpenNetwork) "" else netPassword.ifBlank { null }
                    ) { rebooting ->
                        netMessage = if (rebooting) "Збережено, пристрій перезавантажується..." else "Збережено"
                        netPassword = ""
                    }
                },
                modifier = Modifier.weight(1f)
            ) { Text("Зберегти мережу") }
            OutlinedButton(onClick = { showForgetWifiConfirm = true }, modifier = Modifier.weight(1f)) { Text("Забути Wi-Fi") }
        }

        Spacer(Modifier.height(24.dp))
        HorizontalDivider()
        Spacer(Modifier.height(24.dp))

        Text("📡 Власна точка доступу", style = MaterialTheme.typography.titleMedium)
        Text(
            if (status?.apPasswordConfigured == true) "Пароль AP налаштований" else "Потрібно встановити пароль AP",
            style = MaterialTheme.typography.bodySmall
        )
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            value = apPassword, onValueChange = { apPassword = it },
            label = { Text("Новий пароль AP (8–63 символи)") },
            visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth()
        )
        Spacer(Modifier.height(8.dp))
        apMessage?.let { Text(it, style = MaterialTheme.typography.bodySmall); Spacer(Modifier.height(6.dp)) }
        Button(
            onClick = {
                controller.updateApPassword(apPassword) { rebooting ->
                    apMessage = if (rebooting) "Пароль збережено. AP перезапускається разом із ESP32." else "Не вдалося змінити пароль"
                    if (rebooting) apPassword = ""
                }
            },
            enabled = apPassword.length in 8..63, modifier = Modifier.fillMaxWidth()
        ) { Text("Змінити пароль AP") }

        Spacer(Modifier.height(24.dp))
        HorizontalDivider()
        Spacer(Modifier.height(24.dp))

        Text("🌡 Зовнішній датчик", style = MaterialTheme.typography.titleMedium)
        Text(
            if (status?.outdoorSensorOK == true && status?.outdoorTemperature != null) "Температура вулиці: %.1f °C".format(status!!.outdoorTemperature) else "Зовнішній датчик OFFLINE",
            style = MaterialTheme.typography.bodySmall
        )
        Spacer(Modifier.height(8.dp))
        var outdoorExpanded by remember { mutableStateOf(false) }
        var outdoorSelection by remember(status?.outdoorSensorRom) { mutableStateOf(status?.outdoorSensorRom.orEmpty()) }
        Box {
            OutlinedButton(onClick = { outdoorExpanded = true }, enabled = !status?.sensors.isNullOrEmpty()) {
                Text(if (outdoorSelection.isBlank() || outdoorSelection == "0000000000000000") "Вибрати DS18B20" else outdoorSelection)
            }
            DropdownMenu(expanded = outdoorExpanded, onDismissRequest = { outdoorExpanded = false }) {
                DropdownMenuItem(text = { Text("Не призначено") }, onClick = { outdoorSelection = ""; outdoorExpanded = false })
                status?.sensors.orEmpty().forEach { sensor ->
                    DropdownMenuItem(text = { Text(sensor.rom) }, onClick = { outdoorSelection = sensor.rom; outdoorExpanded = false })
                }
            }
        }
        Spacer(Modifier.height(6.dp))
        outdoorMessage?.let { Text(it, style = MaterialTheme.typography.bodySmall) }
        Spacer(Modifier.height(6.dp))
        Button(enabled = outdoorSelection.isEmpty() || outdoorSelection.length == 16, onClick = {
            controller.updateOutdoorSensor(outdoorSelection) { success ->
                outdoorMessage = if (success) "Зовнішній датчик збережено" else "Не вдалося зберегти зовнішній датчик"
            }
        }, modifier = Modifier.fillMaxWidth()) { Text("Зберегти зовнішній датчик") }

        Spacer(Modifier.height(24.dp))
        HorizontalDivider()
        Spacer(Modifier.height(24.dp))

        Text("⚡ Додатковий ТЕН (AUX)", style = MaterialTheme.typography.titleMedium)
        Text("Один загальний ТЕН. Працює тільки як допомога під час COMFORT REACH, коли прогноз показує запізнення.", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
            Checkbox(checked = auxEnabled, onCheckedChange = { auxEnabled = it })
            Text("AUX увімкнено")
        }
        var auxLocationExpanded by remember { mutableStateOf(false) }
        OutlinedButton(onClick = { auxLocationExpanded = true }) { Text("Розташування: CH$auxLocation") }
        DropdownMenu(expanded = auxLocationExpanded, onDismissRequest = { auxLocationExpanded = false }) {
            (1..6).forEach { ch -> DropdownMenuItem(text = { Text("CH$ch — ${status?.channels?.find { it.id == ch }?.name ?: "Канал $ch"}") }, onClick = { auxLocation = ch; auxLocationExpanded = false }) }
        }
        Spacer(Modifier.height(8.dp))
        Text("Канали, яким дозволена допомога", style = MaterialTheme.typography.labelLarge)
        (status?.channels.orEmpty()).forEach { ch ->
            Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
                Checkbox(checked = (auxMask and (1 shl (ch.id - 1))) != 0, onCheckedChange = { checked -> auxMask = if (checked) auxMask or (1 shl (ch.id - 1)) else auxMask and (1 shl (ch.id - 1)).inv() })
                Text("CH${ch.id} — ${ch.name}")
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedTextField(value = auxMaxRuntime, onValueChange = { auxMaxRuntime = it }, label = { Text("Макс. час, с") }, modifier = Modifier.weight(1f))
            OutlinedTextField(value = auxMaxTemp, onValueChange = { auxMaxTemp = it }, label = { Text("Макс. зона, °C") }, modifier = Modifier.weight(1f))
        }
        Spacer(Modifier.height(12.dp))
        Text("Поведінка після дедлайну REACH", style = MaterialTheme.typography.labelLarge)
        var auxPostExpanded by remember { mutableStateOf(false) }
        OutlinedButton(onClick = { auxPostExpanded = true }) {
            Text(when (auxPostPolicy) { "minutes" -> "Продовжити заданий час"; "comfort" -> "Продовжувати до COMFORT"; else -> "Вимкнути після дедлайну" })
        }
        DropdownMenu(expanded = auxPostExpanded, onDismissRequest = { auxPostExpanded = false }) {
            listOf("off" to "Вимкнути після дедлайну", "minutes" to "Продовжити заданий час", "comfort" to "Продовжувати до COMFORT").forEach { (value, label) ->
                DropdownMenuItem(text = { Text(label) }, onClick = { auxPostPolicy = value; auxPostExpanded = false })
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedTextField(value = auxPostMinutes, onValueChange = { auxPostMinutes = it }, label = { Text("Post-REACH, хв") }, modifier = Modifier.weight(1f))
            OutlinedTextField(value = auxPostMaxSeconds, onValueChange = { auxPostMaxSeconds = it }, label = { Text("Post max, с") }, modifier = Modifier.weight(1f))
        }
        Text("«Заданий час» діє після дедлайну. «До COMFORT» — до досягнення комфортної температури, але в межах Post max. Наступна календарна подія зупиняє Post-REACH.", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(6.dp))
        Text("Стан: ${if (status?.aux?.output == true) "АКТИВНИЙ" else "очікування"} · ${status?.aux?.reason ?: "—"}", style = MaterialTheme.typography.bodySmall)
        auxMessage?.let { Text(it, style = MaterialTheme.typography.bodySmall) }
        Spacer(Modifier.height(6.dp))
        Button(onClick = {
            controller.updateAux(auxEnabled, auxLocation, auxMask, auxMaxRuntime.toIntOrNull() ?: 1800, auxMaxTemp.toDoubleOrNull() ?: 28.0, auxPostPolicy, auxPostMinutes.toIntOrNull() ?: 30, auxPostMaxSeconds.toIntOrNull() ?: 7200) { ok -> auxMessage = if (ok) "AUX налаштування збережено" else "Не вдалося зберегти AUX" }
        }, modifier = Modifier.fillMaxWidth()) { Text("Зберегти AUX") }

        Spacer(Modifier.height(24.dp))
        HorizontalDivider()
        Spacer(Modifier.height(24.dp))

        Text("🔒 Безпека", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(value = authUsername, onValueChange = { authUsername = it }, label = { Text("Логін") }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            value = authCurrentPassword, onValueChange = { authCurrentPassword = it },
            label = { Text("Поточний пароль") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth()
        )
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            value = authNewPassword, onValueChange = { authNewPassword = it },
            label = { Text("Новий пароль (мін. 6 символів)") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth()
        )
        Spacer(Modifier.height(8.dp))
        authMessage?.let { Text(it, style = MaterialTheme.typography.bodySmall); Spacer(Modifier.height(6.dp)) }
        Button(
            onClick = {
                controller.changeAuth(authUsername, authCurrentPassword, authNewPassword) { success ->
                    authMessage = if (success) "Облікові дані оновлено (і збережено в цьому додатку)" else "Не вдалося змінити"
                    if (success) { authCurrentPassword = ""; authNewPassword = "" }
                }
            },
            enabled = authUsername.isNotBlank() && authNewPassword.length >= 6,
            modifier = Modifier.fillMaxWidth()
        ) { Text("Змінити облікові дані") }

        Spacer(Modifier.height(24.dp))
        HorizontalDivider()
        Spacer(Modifier.height(24.dp))

        Text("⚙️ Обслуговування", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { showRebootConfirm = true }, modifier = Modifier.weight(1f)) { Text("Перезавантажити") }
            OutlinedButton(onClick = { showResetConfirm = true }, modifier = Modifier.weight(1f)) { Text("Скинути налаштування") }
        }
    }

    if (showRebootConfirm) {
        AlertDialog(
            onDismissRequest = { showRebootConfirm = false },
            title = { Text("Перезавантажити пристрій?") },
            confirmButton = { TextButton(onClick = { controller.reboot(); showRebootConfirm = false }) { Text("Перезавантажити") } },
            dismissButton = { TextButton(onClick = { showRebootConfirm = false }) { Text("Скасувати") } }
        )
    }

    if (showForgetWifiConfirm) {
        AlertDialog(
            onDismissRequest = { showForgetWifiConfirm = false },
            title = { Text("Забути домашню Wi-Fi мережу?") },
            text = { Text("Пристрій залишиться доступним лише через власну точку доступу (${status?.netId ?: ""}).") },
            confirmButton = {
                TextButton(onClick = {
                    controller.updateNetwork(null, "", null) { netMessage = "Мережу забуто, перезавантаження..." }
                    showForgetWifiConfirm = false
                }) { Text("Забути") }
            },
            dismissButton = { TextButton(onClick = { showForgetWifiConfirm = false }) { Text("Скасувати") } }
        )
    }

    if (showResetConfirm) {
        AlertDialog(
            onDismissRequest = { showResetConfirm = false },
            title = { Text("Скинути ВСІ налаштування?") },
            text = { Text("Канали, календарі, мережа та логін/пароль повернуться до заводських значень. Цей пристрій, ймовірно, доведеться видалити зі списку і додати заново з дефолтним логіном/паролем.") },
            confirmButton = {
                TextButton(onClick = {
                    controller.factoryReset { success ->
                        if (success) onDeviceReset()
                    }
                    showResetConfirm = false
                }) { Text("Скинути") }
            },
            dismissButton = { TextButton(onClick = { showResetConfirm = false }) { Text("Скасувати") } }
        )
    }
}

package com.heatcontrol.app.ui.device

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.CalendarMonth
import androidx.compose.material3.*
import androidx.compose.ui.window.Dialog
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

private val ACTUATORS = listOf("valve" to "Сервокран", "heater" to "ТЕН ON/OFF")
private val MODES = listOf("auto" to "AUTO", "comfort" to "COMFORT CONST", "economy" to "ECONOM CONST", "off" to "OFF", "manual" to "MANUAL")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ChannelDetailScreen(
    channelId: Int,
    controller: DeviceController,
    onBack: () -> Unit,
    onOpenCalendar: (Int) -> Unit
) {
    val status by controller.status.collectAsState()
    val channel = status?.channels?.find { it.id == channelId }

    var initialized by remember(channelId) { mutableStateOf(false) }
    var name by remember(channelId) { mutableStateOf("") }
    var actuator by remember(channelId) { mutableStateOf("valve") }
    var mode by remember(channelId) { mutableStateOf("auto") }
    var sensorIndex by remember(channelId) { mutableStateOf(0) }
    var sensorRom by remember(channelId) { mutableStateOf("") }
    var sensorMenuExpanded by remember(channelId) { mutableStateOf(false) }
    var comfort by remember(channelId) { mutableStateOf("22") }
    var economy by remember(channelId) { mutableStateOf("19") }
    var hysteresis by remember(channelId) { mutableStateOf("0.3") }
    var openTime by remember(channelId) { mutableStateOf("90") }
    var closeTime by remember(channelId) { mutableStateOf("90") }
    var valveFeedback by remember(channelId) { mutableStateOf("time") }
    var outputInverted by remember(channelId) { mutableStateOf(false) }
    var showCalibration by remember(channelId) { mutableStateOf(false) }
    var calibrationInitial by remember(channelId) { mutableStateOf("closed") }

    LaunchedEffect(channel) {
        if (channel != null && !initialized) {
            name = channel.name
            actuator = channel.actuator
            mode = channel.mode
            sensorIndex = channel.sensorIndex
            sensorRom = channel.sensorRom
            comfort = channel.comfort.toString()
            economy = channel.economy.toString()
            hysteresis = channel.hysteresis.toString()
            openTime = channel.openTime.toString()
            closeTime = channel.closeTime.toString()
            valveFeedback = channel.valveFeedback
            outputInverted = channel.outputInverted
            initialized = true
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(channel?.name ?: "Канал $channelId") },
                navigationIcon = { IconButton(onClick = onBack) { Icon(Icons.Default.ArrowBack, contentDescription = "Назад") } },
                actions = {
                    IconButton(onClick = { onOpenCalendar(channelId) }) {
                        Icon(Icons.Default.CalendarMonth, contentDescription = "Календар")
                    }
                }
            )
        }
    ) { padding ->
        if (channel == null) {
            Box(Modifier.fillMaxSize().padding(padding), contentAlignment = androidx.compose.ui.Alignment.Center) {
                CircularProgressIndicator()
            }
            return@Scaffold
        }

        Column(
            Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp)
                .verticalScroll(rememberScrollState())
        ) {
            Text(
                if (channel.sensorOK && channel.temperature != null) "Поточна: %.1f °C".format(channel.temperature) else "Датчик: помилка",
                style = MaterialTheme.typography.headlineSmall
            )
            Text("Робочий стан: ${when (channel.effectiveMode) { "comfort" -> "COMFORT"; "economy" -> "ECONOMY"; "off" -> "OFF"; "manual" -> "MANUAL"; else -> "AUTO" }}${if (channel.manualOverrideActive) " · разове ручне перемикання" else ""}", style = MaterialTheme.typography.bodyMedium)
            Spacer(Modifier.height(16.dp))

            OutlinedTextField(value = name, onValueChange = { name = it }, label = { Text("Назва каналу") }, modifier = Modifier.fillMaxWidth())
            Spacer(Modifier.height(12.dp))

            Text("Тип виконавчого механізму", style = MaterialTheme.typography.labelLarge)
            SingleChoiceRow(ACTUATORS, actuator) { actuator = it }
            Spacer(Modifier.height(12.dp))

            Text("Режим", style = MaterialTheme.typography.labelLarge)
            SingleChoiceRow(MODES, mode) { mode = it }
            Text(
                when (mode) {
                    "off" -> "Клапан повністю закривається. Режим діє до ручної зміни."
                    "manual" -> "Ручне керування виходом/клапаном. Календар не керує каналом."
                    "economy" -> "Постійна економічна температура, календар ігнорується."
                    "comfort" -> "Постійна комфортна температура, календар ігнорується."
                    else -> "Календар активний. Нижче можна виконати разове ручне перемикання до наступної події."
                }, style = MaterialTheme.typography.bodySmall
            )
            if (mode == "auto") {
                Spacer(Modifier.height(10.dp))
                Text("Разове ручне перемикання", style = MaterialTheme.typography.labelLarge)
                Text("Діє до наступної календарної події або зміни базового режиму.", style = MaterialTheme.typography.bodySmall)
                Spacer(Modifier.height(6.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { controller.setManualOverride(channelId, "comfort") }, modifier = Modifier.weight(1f)) { Text("COMFORT зараз") }
                    OutlinedButton(onClick = { controller.setManualOverride(channelId, "economy") }, modifier = Modifier.weight(1f)) { Text("ECONOMY зараз") }
                    OutlinedButton(onClick = { controller.setManualOverride(channelId, "clear") }, modifier = Modifier.weight(1f)) { Text("AUTO") }
                }
                if (channel.manualOverrideActive) Text("Разовий override: ${if (channel.manualOverrideMode == "comfort") "COMFORT" else "ECONOMY"}", style = MaterialTheme.typography.bodySmall)
            }
            Spacer(Modifier.height(12.dp))

            Text("Датчик DS18B20", style = MaterialTheme.typography.labelLarge)
            val sensors = status?.sensors.orEmpty()
            Box {
                OutlinedButton(
                    onClick = { sensorMenuExpanded = true },
                    enabled = sensors.isNotEmpty()
                ) {
                    val selectedIndex = sensors.indexOfFirst { it.rom.equals(sensorRom, ignoreCase = true) }.takeIf { it >= 0 } ?: sensorIndex
                    val selected = sensors.getOrNull(selectedIndex)
                    Text(selected?.let { "#${selectedIndex + 1} ${it.rom}" } ?: "Датчик недоступний")
                }
                DropdownMenu(
                    expanded = sensorMenuExpanded,
                    onDismissRequest = { sensorMenuExpanded = false }
                ) {
                    sensors.forEachIndexed { index, sensor ->
                        DropdownMenuItem(
                            text = { Text("#${index + 1} ${sensor.rom}") },
                            onClick = { sensorIndex = index; sensorRom = sensor.rom; sensorMenuExpanded = false }
                        )
                    }
                }
            }
            Spacer(Modifier.height(12.dp))

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(value = comfort, onValueChange = { comfort = it }, label = { Text("Комфорт, °C") }, modifier = Modifier.weight(1f))
                OutlinedTextField(value = economy, onValueChange = { economy = it }, label = { Text("Економія, °C") }, modifier = Modifier.weight(1f))
            }
            Spacer(Modifier.height(8.dp))
            OutlinedTextField(value = hysteresis, onValueChange = { hysteresis = it }, label = { Text("Гістерезис, °C") }, modifier = Modifier.fillMaxWidth())

            if (actuator == "heater") {
                Spacer(Modifier.height(12.dp))
                Text("ТЕН / ON-OFF вихід", style = MaterialTheme.typography.labelLarge)
                Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
                    Checkbox(checked = outputInverted, onCheckedChange = { outputInverted = it })
                    Text("Інверсія виходу")
                }
                Text("Звичайний ТЕН: вимкнено. Для приводу з протилежною логікою увімкніть інверсію.", style = MaterialTheme.typography.bodySmall)
            }

            if (actuator == "valve") {
                Spacer(Modifier.height(12.dp))
                Text("Метод визначення положення", style = MaterialTheme.typography.labelLarge)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(selected = valveFeedback == "time", onClick = { valveFeedback = "time" }, label = { Text("За часом") })
                    FilterChip(selected = valveFeedback == "limits", onClick = { valveFeedback = "limits" }, enabled = channel.limitSwitchAvailable, label = { Text("За кінцевиками") })
                }
                Text(if (channel.limitSwitchAvailable) "Плата кінцевиків: виявлена" else "Плата кінцевиків: не виявлена — режим кінцевиків недоступний", style = MaterialTheme.typography.bodySmall)
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(value = openTime, onValueChange = { openTime = it }, label = { Text("Час відкриття, с") }, modifier = Modifier.weight(1f))
                    OutlinedTextField(value = closeTime, onValueChange = { closeTime = it }, label = { Text("Час закриття, с") }, modifier = Modifier.weight(1f))
                }
                Spacer(Modifier.height(12.dp))
                Text("Ручне керування клапаном", style = MaterialTheme.typography.labelLarge)
                Text("OPEN limit: ${if (channel.openLimit) "ACTIVE" else "OFF"} · CLOSE limit: ${if (channel.closeLimit) "ACTIVE" else "OFF"}", style = MaterialTheme.typography.bodySmall)
                Text(
                    if (channel.positionKnown) "Позиція: ${channel.valvePosition}% · ${if (channel.valveFeedback == "limits") "кінцевики" else "за часом"}"
                    else "⚠ Необхідна калібровка клапана",
                    style = MaterialTheme.typography.bodySmall
                )
                Spacer(Modifier.height(6.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(enabled = channel.valveFeedback == "limits" || channel.positionKnown, onClick = { controller.valveCommand("open", channelId) }, modifier = Modifier.weight(1f)) { Text("Відкрити") }
                    OutlinedButton(enabled = channel.valveFeedback == "limits" || channel.positionKnown, onClick = { controller.valveCommand("close", channelId) }, modifier = Modifier.weight(1f)) { Text("Закрити") }
                    OutlinedButton(onClick = { controller.valveCommand("stop", channelId) }, modifier = Modifier.weight(1f)) { Text("Стоп") }
                }
                if (channel.valveFeedback == "time") {
                    Spacer(Modifier.height(8.dp))
                    Button(onClick = { showCalibration = true }, modifier = Modifier.fillMaxWidth()) { Text(if (channel.positionKnown) "Калібровка часу клапана" else "⚠ Виконати калібровку клапана") }
                }
            }

            if (showCalibration && channel.valveFeedback == "time") {
                Dialog(onDismissRequest = { if (!channel.calibrating) showCalibration = false }) {
                    Card(Modifier.fillMaxWidth().padding(16.dp)) {
                        Column(Modifier.padding(18.dp)) {
                            Text("Калібровка клапана", style = MaterialTheme.typography.titleLarge)
                            Spacer(Modifier.height(8.dp))
                            Text("Встановіть клапан у початкове положення. Натисніть СТАРТ, дочекайтеся повного ходу та натисніть СТОП.")
                            Spacer(Modifier.height(12.dp))
                            Text("Початкове положення", style = MaterialTheme.typography.labelLarge)
                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                FilterChip(selected = calibrationInitial == "closed", onClick = { calibrationInitial = "closed" }, enabled = !channel.calibrating, label = { Text("Закрито") })
                                FilterChip(selected = calibrationInitial == "open", onClick = { calibrationInitial = "open" }, enabled = !channel.calibrating, label = { Text("Відкрито") })
                            }
                            Spacer(Modifier.height(8.dp))
                            Text("Напрямок: ${if (channel.calibrating) if (channel.calibrationDirection == "open") "ВІДКРИТТЯ" else "ЗАКРИТТЯ" else "—"} · ${channel.calibrationElapsed} с")
                            Text("Відкриття: ${channel.calibrationOpenMeasured.takeIf { it > 0 } ?: channel.openTime} с · Закриття: ${channel.calibrationCloseMeasured.takeIf { it > 0 } ?: channel.closeTime} с", style = MaterialTheme.typography.bodySmall)
                            Spacer(Modifier.height(12.dp))
                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                Button(enabled = !channel.calibrating, onClick = { controller.valveCalibration("start", channelId, calibrationInitial) }, modifier = Modifier.weight(1f)) { Text("▶ СТАРТ") }
                                Button(enabled = channel.calibrating, onClick = { controller.valveCalibration("stop", channelId) }, modifier = Modifier.weight(1f)) { Text("■ СТОП") }
                            }
                            Spacer(Modifier.height(8.dp))
                            Text("Після СТОП виміряне значення автоматично встановлюється як час відповідного напрямку та зберігається в налаштуваннях.", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            }

            Spacer(Modifier.height(20.dp))
            Button(
                onClick = {
                    controller.updateChannel(
                        channelId,
                        mapOf(
                            "name" to name,
                            "actuator" to actuator,
                            "mode" to mode,
                            "sensorIndex" to sensorIndex,
                            "sensorRom" to sensorRom,
                            "comfort" to comfort.toDoubleOrNull(),
                            "economy" to economy.toDoubleOrNull(),
                            "hysteresis" to hysteresis.toDoubleOrNull(),
                            "openTime" to openTime.toIntOrNull(),
                            "closeTime" to closeTime.toIntOrNull(),
                            "valveFeedback" to valveFeedback,
                            "outputInverted" to outputInverted
                        )
                    )
                },
                modifier = Modifier.fillMaxWidth()
            ) { Text("Зберегти") }
        }
    }
}

@Composable
private fun SingleChoiceRow(options: List<Pair<String, String>>, selected: String, onSelect: (String) -> Unit) {
    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        options.forEach { (value, label) ->
            FilterChip(selected = selected == value, onClick = { onSelect(value) }, label = { Text(label) })
        }
    }
}

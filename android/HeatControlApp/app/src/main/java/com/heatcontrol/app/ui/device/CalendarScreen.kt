package com.heatcontrol.app.ui.device

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.heatcontrol.app.network.CalendarConfig
import com.heatcontrol.app.network.ScheduleSlot
import com.heatcontrol.app.network.WeekTemplate
import kotlinx.coroutines.launch

private val DAY_NAMES = listOf("Понеділок", "Вівторок", "Середа", "Четвер", "П'ятниця", "Субота", "Неділя")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CalendarScreen(channelId: Int, controller: DeviceController, onBack: () -> Unit) {
    val scope = rememberCoroutineScope()
    var calendar by remember(channelId) { mutableStateOf<CalendarConfig?>(null) }
    var error by remember(channelId) { mutableStateOf<String?>(null) }
    var saving by remember { mutableStateOf(false) }
    var reachInfo by remember(channelId) { mutableStateOf<com.heatcontrol.app.network.ReachLearningInfo?>(null) }

    LaunchedEffect(channelId) {
        try { calendar = controller.getCalendar(channelId); reachInfo = controller.getReachLearning() }
        catch (e: Exception) { error = e.message ?: "Не вдалося завантажити календар" }
    }

    fun updateDayType(day: Int, type: String) {
        calendar = calendar?.let { it.copy(dayTypes = it.dayTypes.mapIndexed { i, v -> if (i == day) type else v }) }
    }

    fun updateSlot(template: String, day: Int, slot: Int, transform: (ScheduleSlot) -> ScheduleSlot) {
        calendar = calendar?.let { c ->
            fun change(t: WeekTemplate): WeekTemplate = t.copy(days = t.days.mapIndexed { di, slots ->
                if (di != day) slots else slots.mapIndexed { si, s -> if (si == slot) transform(s) else s }
            })
            if (template == "working") c.copy(working = change(c.working)) else c.copy(holiday = change(c.holiday))
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Календар — канал $channelId") },
                navigationIcon = { IconButton(onClick = onBack) { Icon(Icons.Default.ArrowBack, contentDescription = "Назад") } }
            )
        }
    ) { padding ->
        val c = calendar
        when {
            error != null -> Box(Modifier.fillMaxSize().padding(padding), contentAlignment = Alignment.Center) { Text("Помилка: $error") }
            c == null -> Box(Modifier.fillMaxSize().padding(padding), contentAlignment = Alignment.Center) { CircularProgressIndicator() }
            else -> Column(Modifier.fillMaxSize().padding(padding)) {
                LazyColumn(Modifier.weight(1f).padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    item {
                        Text("Тип кожного дня", fontWeight = FontWeight.Bold, style = MaterialTheme.typography.titleMedium)
                        Text("У кожному слоті ви самі задаєте час і обираєте його значення: початок переключення або дедлайн досягнення температури.")
                    }
                    item {
                        val r = reachInfo?.channels?.firstOrNull { it.channel == channelId }
                        Card { Column(Modifier.padding(12.dp)) {
                            Text("Adaptive REACH", fontWeight = FontWeight.Bold, style = MaterialTheme.typography.titleMedium)
                            Text("Контролер навчається за фактичним часом нагріву та враховує різницю температур. Зовнішня температура використовується, якщо датчик доступний.")
                            Spacer(Modifier.height(6.dp))
                            Text("Зразків: ${r?.samples ?: 0} · Базовий коефіцієнт: ${"%.1f".format(r?.minutesPerDegree ?: 10.0)} хв/°C")
                            Text("Модель підбирає схожі історичні випадки за початковою, цільовою та зовнішньою температурою.")
                            Text(if (reachInfo?.outdoorAvailable == true) "Зовнішня температура враховується" else "Зовнішній датчик недоступний")
                            r?.history?.take(5)?.forEach { h ->
                                Text("${"%.1f".format(h.initialIndoor)} → ${"%.1f".format(h.target)} °C · ${"%.1f".format(h.minutes)} хв${h.minutesPerDegree?.let { " · ${"%.1f".format(it)} хв/°C" } ?: ""}${h.outdoor?.let { " · зовн. ${"%.1f".format(it)} °C" } ?: ""}", style = MaterialTheme.typography.bodySmall)
                            }
                            TextButton(onClick = {
                                scope.launch { try { controller.resetReachLearning(channelId); reachInfo = controller.getReachLearning() } catch (e: Exception) { error = e.message ?: "Помилка скидання навчання" } }
                            }) { Text("Скинути навчання") }
                        } }
                    }
                    itemsIndexed(DAY_NAMES) { dayIndex, name ->
                        DayTypeCard(name, c.dayTypes[dayIndex]) { updateDayType(dayIndex, it) }
                    }
                    item { Text("WORKING — шаблон робочих днів", fontWeight = FontWeight.Bold, style = MaterialTheme.typography.titleLarge) }
                    itemsIndexed(DAY_NAMES) { dayIndex, name ->
                        TemplateDayCard("WORKING", name, c.working.days[dayIndex]) { slot, transform -> updateSlot("working", dayIndex, slot, transform) }
                    }
                    item { Text("HOLIDAY — шаблон вихідних/святкових днів", fontWeight = FontWeight.Bold, style = MaterialTheme.typography.titleLarge) }
                    itemsIndexed(DAY_NAMES) { dayIndex, name ->
                        TemplateDayCard("HOLIDAY", name, c.holiday.days[dayIndex]) { slot, transform -> updateSlot("holiday", dayIndex, slot, transform) }
                    }
                    item { Spacer(Modifier.height(8.dp)) }
                }
                Button(
                    onClick = {
                        val current = calendar ?: return@Button
                        saving = true
                        scope.launch {
                            try { controller.updateCalendar(channelId, current); error = null }
                            catch (e: Exception) { error = e.message ?: "Помилка збереження" }
                            finally { saving = false }
                        }
                    }, enabled = !saving, modifier = Modifier.fillMaxWidth().padding(16.dp)
                ) { Text(if (saving) "Збереження..." else "Зберегти календар") }
            }
        }
    }
}

@Composable
private fun DayTypeCard(dayName: String, type: String, onChange: (String) -> Unit) {
    Card { Column(Modifier.padding(12.dp)) {
        Text(dayName, fontWeight = FontWeight.Bold)
        Row(verticalAlignment = Alignment.CenterVertically) {
            FilterChip(selected = type == "working", onClick = { onChange("working") }, label = { Text("WORKING") })
            Spacer(Modifier.width(8.dp))
            FilterChip(selected = type == "holiday", onClick = { onChange("holiday") }, label = { Text("HOLIDAY") })
        }
    } }
}

@Composable
private fun TemplateDayCard(templateName: String, dayName: String, slots: List<ScheduleSlot>, onSlotChange: (Int, (ScheduleSlot) -> ScheduleSlot) -> Unit) {
    Card { Column(Modifier.padding(12.dp)) {
        Text("$dayName — $templateName", fontWeight = FontWeight.Bold)
        slots.forEachIndexed { i, slot -> SlotRow(slot) { transform -> onSlotChange(i, transform) } }
    } }
}

@Composable
private fun SlotRow(slot: ScheduleSlot, onChange: ((ScheduleSlot) -> ScheduleSlot) -> Unit) {
    var hourText by remember(slot.hour) { mutableStateOf(slot.hour.toString()) }
    var minuteText by remember(slot.minute) { mutableStateOf(slot.minute.toString().padStart(2, '0')) }
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Checkbox(checked = slot.enabled, onCheckedChange = { checked -> onChange { it.copy(enabled = checked) } })
        OutlinedTextField(value = hourText, onValueChange = { v -> hourText = v; v.toIntOrNull()?.let { onChange { x -> x.copy(hour = it.coerceIn(0,23)) } } }, label = { Text("Год") }, modifier = Modifier.width(72.dp))
        Text(":")
        OutlinedTextField(value = minuteText, onValueChange = { v -> minuteText = v; v.toIntOrNull()?.let { onChange { x -> x.copy(minute = it.coerceIn(0,59)) } } }, label = { Text("Хв") }, modifier = Modifier.width(72.dp))
        Spacer(Modifier.width(8.dp))
        FilterChip(selected = slot.mode == "comfort", onClick = { onChange { it.copy(mode = if (it.mode == "comfort") "economy" else "comfort") } }, label = { Text(if (slot.mode == "comfort") "Комфорт" else "Економія") })
        Spacer(Modifier.width(8.dp))
        Column(Modifier.widthIn(min = 230.dp).weight(1f)) {
            Text("Що означає цей час?", style = MaterialTheme.typography.labelMedium)
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                FilterChip(
                    selected = slot.transition == "start",
                    onClick = { onChange { it.copy(transition = "start") } },
                    label = { Text("Почати о цьому часі") }
                )
                FilterChip(
                    selected = slot.transition == "reach",
                    onClick = { onChange { it.copy(transition = "reach") } },
                    label = { Text("Досягти до цього часу") }
                )
            }
            Text(
                if (slot.transition == "reach")
                    "Дедлайн: контролер почне нагрів завчасно."
                else
                    "Старт: переключення почнеться саме у цей час.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

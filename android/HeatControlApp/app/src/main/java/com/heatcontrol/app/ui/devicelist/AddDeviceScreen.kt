package com.heatcontrol.app.ui.devicelist

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Router
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.heatcontrol.app.discovery.DiscoveredService
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AddDeviceScreen(
    onDone: () -> Unit,
    onBack: () -> Unit,
    viewModel: AddDeviceViewModel = viewModel()
) {
    val discovered by viewModel.discovered.collectAsState()
    val scanning by viewModel.scanning.collectAsState()
    val testState by viewModel.testState.collectAsState()
    val scope = rememberCoroutineScope()

    var label by remember { mutableStateOf("") }
    var host by remember { mutableStateOf("") }
    var netId by remember { mutableStateOf("") }
    var username by remember { mutableStateOf("admin") }
    var password by remember { mutableStateOf("") }
    var saveError by remember { mutableStateOf<String?>(null) }

    DisposableEffect(Unit) {
        viewModel.startScan()
        onDispose { viewModel.stopScan() }
    }

    fun pick(service: DiscoveredService) {
        host = service.host
        netId = service.netId
        if (label.isBlank()) label = service.netId
        viewModel.resetTest()
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Додати пристрій") },
                navigationIcon = {
                    IconButton(onClick = onBack) { Icon(Icons.Default.ArrowBack, contentDescription = "Назад") }
                }
            )
        }
    ) { padding ->
        Column(
            Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp)
        ) {
            Text("Знайдено в мережі" + if (scanning) " (пошук...)" else "", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.height(8.dp))
            if (discovered.isEmpty()) {
                Text(
                    if (scanning) "Ще нічого не знайдено. Переконайтесь, що пристрій підключено до тієї ж Wi-Fi мережі."
                    else "Нічого не знайдено. Можна ввести дані вручну нижче.",
                    style = MaterialTheme.typography.bodySmall
                )
            } else {
                LazyColumn(Modifier.heightIn(max = 180.dp)) {
                    items(discovered, key = { it.netId }) { svc ->
                        ListItem(
                            headlineContent = { Text(svc.netId) },
                            supportingContent = { Text("${svc.host}:${svc.port}") },
                            leadingContent = { Icon(Icons.Default.Router, contentDescription = null) },
                            modifier = Modifier.clickable { pick(svc) }
                        )
                    }
                }
            }

            Spacer(Modifier.height(20.dp))
            HorizontalDivider()
            Spacer(Modifier.height(20.dp))
            Text("Дані пристрою", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.height(8.dp))

            OutlinedTextField(
                value = label, onValueChange = { label = it },
                label = { Text("Назва в списку (наприклад \"Кухня\")") },
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(8.dp))
            OutlinedTextField(
                value = host, onValueChange = { host = it; viewModel.resetTest() },
                label = { Text("Адреса (hc-xxxxxx.local або IP)") },
                placeholder = { Text("192.168.4.1") },
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(8.dp))
            OutlinedTextField(
                value = username, onValueChange = { username = it; viewModel.resetTest() },
                label = { Text("Логін") },
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(8.dp))
            OutlinedTextField(
                value = password, onValueChange = { password = it; viewModel.resetTest() },
                label = { Text("Пароль") },
                visualTransformation = PasswordVisualTransformation(),
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(Modifier.height(16.dp))

            when (val t = testState) {
                is TestState.Idle -> Button(
                    onClick = { viewModel.testConnection(host.trim(), username.trim(), password) },
                    enabled = host.isNotBlank() && username.isNotBlank(),
                    modifier = Modifier.fillMaxWidth()
                ) { Text("Перевірити з'єднання") }

                is TestState.Testing -> Button(onClick = {}, enabled = false, modifier = Modifier.fillMaxWidth()) {
                    CircularProgressIndicator(modifier = Modifier.size(18.dp), strokeWidth = 2.dp)
                    Spacer(Modifier.width(8.dp))
                    Text("Перевірка...")
                }

                is TestState.Success -> Column {
                    Text("З'єднано: ${t.deviceName} (${t.netId})", color = MaterialTheme.colorScheme.secondary)
                    Spacer(Modifier.height(8.dp))
                    Button(
                        onClick = {
                            scope.launch {
                                val finalLabel = label.ifBlank { t.deviceName }
                                val err = viewModel.save(finalLabel, host.trim(), t.netId, username.trim(), password)
                                if (err != null) saveError = err else onDone()
                            }
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) { Text("Додати пристрій") }
                }

                is TestState.Failed -> Column {
                    Text("Помилка: ${t.message}", color = MaterialTheme.colorScheme.error)
                    Spacer(Modifier.height(8.dp))
                    OutlinedButton(
                        onClick = { viewModel.testConnection(host.trim(), username.trim(), password) },
                        modifier = Modifier.fillMaxWidth()
                    ) { Text("Спробувати ще раз") }
                }
            }

            saveError?.let {
                Spacer(Modifier.height(8.dp))
                Text(it, color = MaterialTheme.colorScheme.error)
            }
        }
    }
}

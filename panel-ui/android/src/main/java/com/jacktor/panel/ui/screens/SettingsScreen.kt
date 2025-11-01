package com.jacktor.panel.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import com.jacktor.panel.ui.data.PanelRepository

@Composable
fun SettingsScreen(repository: PanelRepository, onOpenConsole: () -> Unit) {
    val pin = remember { mutableStateOf("") }
    val confirmPin = remember { mutableStateOf("") }
    val toneMode = remember { mutableStateOf("simple") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(24.dp)
    ) {
        Text("Settings", style = MaterialTheme.typography.titleLarge)
        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Factory Reset", color = MaterialTheme.colorScheme.primary)
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                OutlinedTextField(
                    value = pin.value,
                    onValueChange = { pin.value = it },
                    label = { Text("PIN") },
                    visualTransformation = PasswordVisualTransformation(),
                    modifier = Modifier.weight(1f)
                )
                OutlinedTextField(
                    value = confirmPin.value,
                    onValueChange = { confirmPin.value = it },
                    label = { Text("Konfirmasi PIN") },
                    visualTransformation = PasswordVisualTransformation(),
                    modifier = Modifier.weight(1f)
                )
            }
            Button(onClick = {
                if (pin.value == confirmPin.value && pin.value.length in 4..6) {
                    repository.sendCli("reset nvs --force --pin ${pin.value}")
                }
            }) { Text("Reset NVS") }
        }

        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("RTC Sync", color = MaterialTheme.colorScheme.primary)
            Button(onClick = { repository.sendCli("rtc sync") }) { Text("Sync Now") }
        }

        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Tone Lab", color = MaterialTheme.colorScheme.primary)
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                listOf("simple", "sequence", "musical", "randomizer").forEach { mode ->
                    OutlinedButton(onClick = { toneMode.value = mode }) {
                        Text(mode.uppercase())
                    }
                }
            }
            Button(onClick = { repository.sendCli("tone {\"type\":\"${toneMode.value}\"}") }) { Text("Send Tone") }
        }

        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Diagnostics", color = MaterialTheme.colorScheme.primary)
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = { repository.refreshTelemetry() }) { Text("Refresh Cache") }
                Button(onClick = onOpenConsole) { Text("Open Console") }
            }
        }
    }
}

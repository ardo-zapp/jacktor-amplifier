package com.jacktor.panel.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.jacktor.panel.ui.data.PanelRepository
import com.jacktor.panel.ui.data.TelemetryState

@Composable
fun ControlsPanel(repository: PanelRepository, telemetry: TelemetryState, modifier: Modifier = Modifier) {
    Column(modifier = modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            val powerLabel = if (telemetry.state == "ON") "Power ON" else "Power STBY"
            Button(onClick = { repository.sendCli("power ${if (telemetry.state == "ON") "off" else "on"}") }) {
                Text(powerLabel)
            }
            Button(onClick = { repository.sendCli("bt ${if (telemetry.input == "BT") "disable" else "enable"}") }) {
                Text(if (telemetry.input == "BT") "BT Enabled" else "BT Disabled")
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            listOf("play", "pause", "next", "prev").forEach { action ->
                OutlinedButton(onClick = { repository.sendCli("bt $action") }) {
                    Text(action.uppercase())
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            OutlinedButton(onClick = { repository.sendCli("set speaker-selector ${if (telemetry.selector == "BIG") "small" else "big"}") }) {
                Text("Speaker ${telemetry.selector}")
            }
            OutlinedButton(onClick = { repository.sendCli("smps protect toggle") }) {
                Text("SMPS Protect")
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            listOf("AUTO", "CUSTOM", "FAILSAFE").forEach { mode ->
                val active = telemetry.fanMode == mode
                Button(
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surface
                    ),
                    onClick = { repository.sendCli("fan mode ${mode.lowercase()}") }
                ) {
                    Text("Fan $mode")
                }
            }
        }
        if (telemetry.fanMode == "CUSTOM") {
            val fanDuty = remember { mutableStateOf(telemetry.fanDuty.toFloat()) }
            Column(modifier = Modifier.padding(horizontal = 8.dp)) {
                Slider(
                    value = fanDuty.value,
                    onValueChange = { fanDuty.value = it },
                    valueRange = 0f..100f,
                    onValueChangeFinished = {
                        repository.sendCli("fan duty ${fanDuty.value.toInt()}")
                    }
                )
                Text("Duty ${fanDuty.value.toInt()}%", color = MaterialTheme.colorScheme.primary)
            }
        }
    }
}

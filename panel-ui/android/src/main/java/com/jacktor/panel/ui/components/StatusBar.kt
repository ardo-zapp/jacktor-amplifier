package com.jacktor.panel.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.jacktor.panel.ui.data.PanelRepository
import com.jacktor.panel.ui.data.TelemetryState
import kotlinx.coroutines.delay
import java.time.LocalTime
import java.time.format.DateTimeFormatter

@Composable
fun StatusBar(repository: PanelRepository, telemetry: TelemetryState, modifier: Modifier = Modifier) {
    val clock = remember { mutableStateOf(LocalTime.now()) }
    LaunchedEffect(Unit) {
        while (true) {
            clock.value = LocalTime.now()
            delay(1000)
        }
    }
    Column(
        modifier = modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.5f))
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
            Text("RTC ${telemetry.rtc}")
            Text("Fan ${telemetry.fanMode} ${if (telemetry.fanMode == "CUSTOM") "${telemetry.fanDuty}%" else ""}")
        }
        Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
            Text("BT Auto-Off ${telemetry.btAutoOffSeconds}s")
            Text("Clock ${clock.value.format(DateTimeFormatter.ISO_LOCAL_TIME)}")
        }
        Button(onClick = { repository.sendCli("ota status") }) {
            Text("OTA Status")
        }
    }
}

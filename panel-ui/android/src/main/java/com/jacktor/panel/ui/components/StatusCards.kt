package com.jacktor.panel.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.jacktor.panel.ui.data.TelemetryState

@Composable
fun StatusCards(frame: TelemetryState, modifier: Modifier = Modifier) {
    val cards = listOf(
        "Heat" to "${frame.heatC}°C",
        "RTC" to frame.rtc,
        "SMPS" to "${frame.smpsV} V",
        "Input" to frame.input,
        "State" to frame.state
    )
    Column(modifier = modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        LazyRow(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            items(cards) { card ->
                Column(
                    modifier = Modifier
                        .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.6f))
                        .padding(16.dp)
                ) {
                    Text(card.first.uppercase(), color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                    Spacer(Modifier.height(4.dp))
                    Text(card.second, style = MaterialTheme.typography.headlineMedium)
                }
            }
        }
        Column(
            modifier = Modifier
                .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.6f))
                .padding(16.dp)
        ) {
            Text("Errors", fontWeight = FontWeight.Bold)
            if (frame.errorFlags.isEmpty()) {
                Text("No faults", color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.4f))
            } else {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    frame.errorFlags.forEach { flag ->
                        Text(flag.uppercase(), color = MaterialTheme.colorScheme.error)
                    }
                }
            }
        }
    }
}

package com.jacktor.panel.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.jacktor.panel.ui.components.AnalyzerVisualizer
import com.jacktor.panel.ui.components.ControlsPanel
import com.jacktor.panel.ui.components.LinkIndicators
import com.jacktor.panel.ui.components.LinkState
import com.jacktor.panel.ui.components.StatusBar
import com.jacktor.panel.ui.components.StatusCards
import com.jacktor.panel.ui.data.PanelRepository

@Composable
fun HomeScreen(repository: PanelRepository, onOpenConsole: () -> Unit) {
    val telemetry by repository.telemetry.collectAsState()
    val linkState = LinkState(link = true, rx = true, tx = false)

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(24.dp)
    ) {
        Text("Jacktor Amplifier", style = MaterialTheme.typography.titleLarge, color = MaterialTheme.colorScheme.primary)
        Row(horizontalArrangement = Arrangement.spacedBy(24.dp), modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(2f)) {
                AnalyzerVisualizer(telemetry.bars)
                StatusCards(telemetry, modifier = Modifier.padding(top = 16.dp))
            }
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                ControlsPanel(repository = repository, telemetry = telemetry)
                LinkIndicators(state = linkState)
            }
        }
        StatusBar(repository = repository, telemetry = telemetry, modifier = Modifier.fillMaxWidth())
    }
}

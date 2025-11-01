package com.jacktor.panel.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.jacktor.panel.ui.components.LinkIndicators
import com.jacktor.panel.ui.components.LinkState
import com.jacktor.panel.ui.data.PanelRepository

@Composable
fun ConsoleScreen(repository: PanelRepository, onBack: () -> Unit) {
    val console = repository.console.collectAsState()
    val input = remember { mutableStateOf("") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
            Text("Diagnostics Console")
            LinkIndicators(state = LinkState(link = true, rx = true, tx = false))
        }
        LazyColumn(modifier = Modifier.weight(1f)) {
            items(console.value) { line ->
                Text(line)
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
            OutlinedTextField(
                value = input.value,
                onValueChange = { input.value = it },
                modifier = Modifier.weight(1f),
                label = { Text("CLI Command") }
            )
            Button(onClick = {
                repository.sendCli(input.value)
                input.value = ""
            }) { Text("Send") }
        }
        Button(onClick = onBack) { Text("Back") }
    }
}

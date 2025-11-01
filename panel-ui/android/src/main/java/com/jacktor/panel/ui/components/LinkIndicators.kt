package com.jacktor.panel.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp

data class LinkState(
    val link: Boolean,
    val rx: Boolean,
    val tx: Boolean
)

@Composable
fun LinkIndicators(state: LinkState, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        IndicatorDot("Link", state.link)
        IndicatorDot("RX", state.rx)
        IndicatorDot("TX", state.tx)
    }
}

@Composable
private fun IndicatorDot(label: String, active: Boolean) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        androidx.compose.foundation.layout.Box(
            modifier = Modifier
                .clip(CircleShape)
                .background(if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surface)
                .padding(8.dp)
        )
        Text(label.uppercase(), color = MaterialTheme.colorScheme.onSurface.copy(alpha = if (active) 1f else 0.5f))
    }
}

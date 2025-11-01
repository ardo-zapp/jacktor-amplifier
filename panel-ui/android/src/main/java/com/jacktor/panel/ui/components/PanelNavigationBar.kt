package com.jacktor.panel.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.jacktor.panel.ui.PanelPage

@Composable
fun PanelNavigationBar(
    currentPage: PanelPage,
    onNavigate: (PanelPage) -> Unit
) {
    val items = listOf(
        PanelPage.HOME to "Home",
        PanelPage.SETTINGS to "Settings",
        PanelPage.CONSOLE to "Console"
    )
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.3f))
            .padding(horizontal = 24.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceEvenly
    ) {
        items.forEach { (page, label) ->
            val isActive = page == currentPage
            Text(
                text = label.uppercase(),
                color = if (isActive) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onBackground.copy(alpha = 0.6f),
                modifier = Modifier.clickable { onNavigate(page) },
                fontWeight = if (isActive) FontWeight.Bold else FontWeight.Normal
            )
        }
    }
}

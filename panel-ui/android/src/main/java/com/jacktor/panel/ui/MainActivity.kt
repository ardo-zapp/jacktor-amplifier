package com.jacktor.panel.ui

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import com.jacktor.panel.ui.components.PanelNavigationBar
import com.jacktor.panel.ui.data.PanelRepository
import com.jacktor.panel.ui.screens.ConsoleScreen
import com.jacktor.panel.ui.screens.HomeScreen
import com.jacktor.panel.ui.screens.SettingsScreen
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val repository = PanelRepository(this)
        setContent {
            PanelApp(repository)
        }
    }
}

enum class PanelPage { HOME, SETTINGS, CONSOLE }

@Composable
fun PanelApp(repository: PanelRepository) {
    var page by remember { mutableStateOf(PanelPage.HOME) }
    val scope = rememberCoroutineScope()

    MaterialTheme(colorScheme = repository.themeColorScheme) {
        Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
            when (page) {
                PanelPage.HOME -> HomeScreen(
                    repository = repository,
                    onOpenConsole = { page = PanelPage.CONSOLE }
                )
                PanelPage.SETTINGS -> SettingsScreen(
                    repository = repository,
                    onOpenConsole = { page = PanelPage.CONSOLE }
                )
                PanelPage.CONSOLE -> ConsoleScreen(
                    repository = repository,
                    onBack = { page = PanelPage.HOME }
                )
            }
            PanelNavigationBar(
                currentPage = page,
                onNavigate = { next ->
                    if (next == PanelPage.HOME) {
                        scope.launch { repository.refreshTelemetry() }
                    }
                    page = next
                }
            )
        }
    }
}

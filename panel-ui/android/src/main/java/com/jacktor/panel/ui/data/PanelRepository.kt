package com.jacktor.panel.ui.data

import android.content.Context
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.ui.graphics.Color
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.parseToJsonElement
import org.json.JSONObject
import java.io.File
import java.util.concurrent.atomic.AtomicLong

@Serializable
data class TelemetryState(
    val heatC: Float = 0f,
    val rtc: String = "--:--",
    val smpsV: Float = 0f,
    val input: String = "AUX",
    val state: String = "STANDBY",
    val errorFlags: List<String> = emptyList(),
    val fanMode: String = "AUTO",
    val fanDuty: Int = 0,
    val btAutoOffSeconds: Int = 0,
    val selector: String = "SMALL",
    val bars: List<Int> = List(16) { 0 }
)

class PanelRepository(private val context: Context) {
    private val scope = CoroutineScope(Dispatchers.IO)
    private val cacheFile: File = File(context.filesDir, "nv-cache.json")
    private val json = Json { ignoreUnknownKeys = true }
    private val digest = AtomicLong(0)

    private val _telemetry = MutableStateFlow(TelemetryState())
    val telemetry: StateFlow<TelemetryState> = _telemetry

    private val _console = MutableStateFlow<List<String>>(emptyList())
    val console: StateFlow<List<String>> = _console

    val themeColorScheme: ColorScheme = darkColorScheme(
        primary = Color(0xFF00CFFF),
        secondary = Color(0xFF00E6FF),
        background = Color(0xFF080B0E),
        surface = Color(0xFF10151B),
        error = Color(0xFFFF3355),
        onBackground = Color(0xFFE5F9FF),
        onSurface = Color(0xFFE5F9FF)
    )

    init {
        scope.launch { restoreNvCache() }
    }

    fun refreshTelemetry() {
        scope.launch {
            // TODO: fetch telemetry dari serial service
        }
    }

    fun appendConsole(line: String) {
        _console.value = (_console.value + line).takeLast(500)
    }

    fun sendCli(command: String) {
        appendConsole("> $command")
        // TODO: connect to serial service
    }

    fun updateNv(key: String, value: Any?) {
        digest.incrementAndGet()
        persistNvCache(mapOf(key to value))
    }

    private fun persistNvCache(data: Map<String, Any?>) {
        scope.launch {
            val payload = JSONObject().apply {
                put("etag", digest.get().toString())
                put("data", JSONObject(data))
            }
            cacheFile.writeText(payload.toString())
        }
    }

    private suspend fun restoreNvCache() {
        if (!cacheFile.exists()) return
        runCatching {
            val raw = cacheFile.readText()
            val payload = json.parseToJsonElement(raw).jsonObject
            payload["etag"]?.let { digest.set(it.jsonPrimitive.long) }
        }
    }
}

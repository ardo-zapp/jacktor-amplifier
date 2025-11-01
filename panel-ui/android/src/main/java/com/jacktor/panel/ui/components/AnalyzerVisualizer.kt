package com.jacktor.panel.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.unit.dp

@Composable
fun AnalyzerVisualizer(bars: List<Int>, modifier: Modifier = Modifier) {
    val brush = Brush.verticalGradient(listOf(Color(0xFF00E6FF), Color(0x3300CFFF)))
    Canvas(modifier = modifier.fillMaxWidth().height(180.dp)) {
        val barWidth = size.width / bars.size
        bars.forEachIndexed { index, value ->
            val normalized = (value.coerceIn(0, 100) / 100f) * size.height
            val left = index * barWidth
            val path = Path().apply {
                moveTo(left + barWidth * 0.2f, size.height)
                lineTo(left + barWidth * 0.2f, size.height - normalized)
                lineTo(left + barWidth * 0.8f, size.height - normalized)
                lineTo(left + barWidth * 0.8f, size.height)
                close()
            }
            drawPath(path = path, brush = brush)
        }
    }
}

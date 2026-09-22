package com.heatcontrol.app.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val Background = Color(0xFF0B1020)
val Surface = Color(0xFF121A2D)
val SurfaceVariant = Color(0xFF18233B)
val OnSurface = Color(0xFFEEF3FF)
val Muted = Color(0xFF93A0BA)
val Accent = Color(0xFF4DA3FF)
val Good = Color(0xFF38D39F)
val Bad = Color(0xFFFF6464)

private val HeatControlColors = darkColorScheme(
    primary = Accent,
    secondary = Good,
    error = Bad,
    background = Background,
    surface = Surface,
    surfaceVariant = SurfaceVariant,
    onBackground = OnSurface,
    onSurface = OnSurface,
    onPrimary = Color(0xFF06101E)
)

@Composable
fun HeatControlTheme(content: @Composable () -> Unit) {
    // Dark-only palette, matching the web UI. isSystemInDarkTheme() intentionally
    // unused for now - keeping this app visually consistent across devices.
    MaterialTheme(colorScheme = HeatControlColors, content = content)
}

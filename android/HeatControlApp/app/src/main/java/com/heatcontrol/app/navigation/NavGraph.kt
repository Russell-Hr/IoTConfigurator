package com.heatcontrol.app.navigation

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.heatcontrol.app.HeatControlApplication
import com.heatcontrol.app.ui.device.CalendarScreen
import com.heatcontrol.app.ui.device.ChannelDetailScreen
import com.heatcontrol.app.ui.device.DeviceHomeScreen
import com.heatcontrol.app.ui.devicelist.AddDeviceScreen
import com.heatcontrol.app.ui.devicelist.DeviceListScreen

@Composable
fun HeatControlNavGraph() {
    val navController = rememberNavController()

    NavHost(navController = navController, startDestination = "device_list") {
        composable("device_list") {
            DeviceListScreen(
                onOpenDevice = { id -> navController.navigate("device/$id") },
                onAddDevice = { navController.navigate("add_device") }
            )
        }

        composable("add_device") {
            AddDeviceScreen(
                onDone = { navController.popBackStack() },
                onBack = { navController.popBackStack() }
            )
        }

        composable(
            "device/{deviceId}",
            arguments = listOf(navArgument("deviceId") { type = NavType.LongType })
        ) { entry ->
            val deviceId = entry.arguments?.getLong("deviceId") ?: return@composable
            val application = LocalContext.current.applicationContext as HeatControlApplication
            val controller = remember(deviceId) { application.getDeviceController(deviceId) }
            DeviceHomeScreen(
                controller = controller,
                onOpenChannel = { chId -> navController.navigate("device/$deviceId/channel/$chId") },
                onDeviceReset = {
                    // Credentials on the device just reverted to factory defaults; this
                    // app's saved entry is now stale. Send the user back to the list
                    // rather than pretend the connection still works.
                    navController.popBackStack("device_list", inclusive = false)
                }
            )
        }

        composable(
            "device/{deviceId}/channel/{channelId}",
            arguments = listOf(
                navArgument("deviceId") { type = NavType.LongType },
                navArgument("channelId") { type = NavType.IntType }
            )
        ) { entry ->
            val deviceId = entry.arguments?.getLong("deviceId") ?: return@composable
            val channelId = entry.arguments?.getInt("channelId") ?: return@composable
            val application = LocalContext.current.applicationContext as HeatControlApplication
            val controller = remember(deviceId) { application.getDeviceController(deviceId) }
            ChannelDetailScreen(
                channelId = channelId,
                controller = controller,
                onBack = { navController.popBackStack() },
                onOpenCalendar = { chId -> navController.navigate("device/$deviceId/calendar/$chId") }
            )
        }

        composable(
            "device/{deviceId}/calendar/{channelId}",
            arguments = listOf(
                navArgument("deviceId") { type = NavType.LongType },
                navArgument("channelId") { type = NavType.IntType }
            )
        ) { entry ->
            val deviceId = entry.arguments?.getLong("deviceId") ?: return@composable
            val channelId = entry.arguments?.getInt("channelId") ?: return@composable
            val application = LocalContext.current.applicationContext as HeatControlApplication
            val controller = remember(deviceId) { application.getDeviceController(deviceId) }
            CalendarScreen(
                channelId = channelId,
                controller = controller,
                onBack = { navController.popBackStack() }
            )
        }
    }
}

// Copyright 2026 Azahar Thor fork
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.content.Context
import androidx.preference.PreferenceManager
import org.citra.citra_emu.CitraApplication
import java.io.IOException

/**
 * Installs the GPU driver package that ships inside the APK under assets/gpu_drivers.
 *
 * The first start after an install or an update copies the package into the user's
 * gpu_drivers folder, extracts it as the active driver, and records the package key. A later
 * manual driver choice, including the system driver, is kept until the bundled package changes.
 */
object BundledGpuDriver {
    private const val ASSET_DIR = "gpu_drivers"
    private const val PREF_KEY = "bundled_gpu_driver_key"

    fun installIfNeeded(context: Context) {
        if (!GpuDriverHelper.supportsCustomDriverLoading()) {
            return
        }
        val assetName = try {
            context.assets.list(ASSET_DIR)?.firstOrNull { it.endsWith(".zip", ignoreCase = true) }
        } catch (e: IOException) {
            null
        } ?: return

        val metadata = try {
            context.assets.open("$ASSET_DIR/$assetName").use { GpuDriverHelper.getMetadataFromZip(it) }
        } catch (e: IOException) {
            Log.error("[BundledGpuDriver] Cannot read $assetName: ${e.message}")
            return
        }
        val name = metadata.name ?: return
        val key = "$assetName|$name|${metadata.version}"
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        if (prefs.getString(PREF_KEY, null) == key) {
            return
        }

        val storage = GpuDriverHelper.driverStoragePath
        storage.findFile(assetName)?.delete()
        val target = storage.createFile("application/zip", assetName)
        if (target == null) {
            Log.error("[BundledGpuDriver] Cannot create $assetName in gpu_drivers")
            return
        }
        try {
            CitraApplication.appContext.contentResolver.openOutputStream(target.uri)?.use { out ->
                context.assets.open("$ASSET_DIR/$assetName").use { it.copyTo(out) }
            } ?: throw IOException("no output stream")
        } catch (e: IOException) {
            Log.error("[BundledGpuDriver] Copy of $assetName failed: ${e.message}")
            target.delete()
            return
        }

        val installed = GpuDriverHelper.installCustomDriverPartial(target.uri)
        Log.info("[BundledGpuDriver] $name installed=$installed")
        if (installed) {
            prefs.edit().putString(PREF_KEY, key).apply()
        }
    }
}

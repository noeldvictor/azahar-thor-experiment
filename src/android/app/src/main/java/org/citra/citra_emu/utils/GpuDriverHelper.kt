// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.net.Uri
import android.os.Build
import androidx.documentfile.provider.DocumentFile
import java.io.BufferedInputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.lang.IllegalStateException
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipEntry
import java.util.zip.ZipException
import java.util.zip.ZipInputStream
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.utils.FileUtil.asDocumentFile
import org.citra.citra_emu.utils.FileUtil.inputStream
import org.citra.citra_emu.utils.FileUtil.outputStream
import org.json.JSONArray
import org.json.JSONObject

object GpuDriverHelper {
    private const val META_JSON_FILENAME = "meta.json"
    private const val DRIVER_RELEASES_URL =
        "https://api.github.com/repos/K11MCH1/AdrenoToolsDrivers/releases?per_page=30"
    private const val FALLBACK_TURNIP_DRIVER_NAME = "Turnip_v26.0.0_R8.zip"
    private const val FALLBACK_TURNIP_DRIVER_URL =
        "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/Turnip_v26.0.0_R8.zip"
    private const val MAX_NORMAL_TURNIP_OPTIONS = 8
    private var fileRedirectionPath: String? = null
    var driverInstallationPath: String? = null
    private var hookLibPath: String? = null

    data class DriverPackage(val file: DocumentFile, val metadata: GpuDriverMetadata)

    data class RecommendedDriverOption(
        val title: String,
        val note: String,
        val assetName: String,
        val downloadUrl: String
    )

    private data class RemoteDriverAsset(val name: String, val downloadUrl: String)

    val driverStoragePath: DocumentFile
        get() {
            // Bypass directory initialization checks
            val root = DocumentFile.fromTreeUri(
                CitraApplication.appContext,
                Uri.parse(DirectoryInitialization.userPath)
            )!!
            var driverDirectory = root.findFile("gpu_drivers")
            if (driverDirectory == null) {
                driverDirectory = FileUtil.createDir(root.uri.toString(), "gpu_drivers")
            }
            return driverDirectory!!
        }

    fun initializeDriverParameters() {
        try {
            // Initialize the file redirection directory.
            fileRedirectionPath =
                DirectoryInitialization.internalUserPath + "/gpu/vk_file_redirect/"

            // Initialize the driver installation directory.
            driverInstallationPath = CitraApplication.appContext
                .filesDir.canonicalPath + "/gpu_driver/"
        } catch (e: IOException) {
            throw RuntimeException(e)
        }

        // Initialize directories.
        initializeDirectories()

        // Initialize hook libraries directory.
        hookLibPath = CitraApplication.appContext.applicationInfo.nativeLibraryDir + "/"

        applyDriverEnvironment()

        val activeDriver = customDriverData
        val activeDriverMetadata = JSONObject()
            .put("name", activeDriver.name ?: "System GPU driver")
            .put("version", activeDriver.version ?: "")
            .put("libraryName", activeDriver.libraryName ?: "")
        Log.info("[GpuDriverHelper] Active Vulkan driver metadata: $activeDriverMetadata")

        // Initialize GPU driver.
        NativeLibrary.initializeGpuDriver(
            hookLibPath,
            driverInstallationPath,
            activeDriver.libraryName,
            fileRedirectionPath
        )
    }

    /**
     * Thor MCP: `thor_driver_env.txt` in the user directory holds KEY=VALUE lines that are set in
     * the process environment before the driver loads, for example TU_DEBUG=nobin. The Turnip
     * driver reads them when the Vulkan instance is created. No file means no change.
     */
    /**
     * Turnip chooses between tiled rendering and rendering straight to memory for each render
     * pass. A 3DS frame is about ninety passes over small targets, and the chooser picks tiled
     * rendering for them, which pays a binning and tile cost that the small targets never earn
     * back. Forcing the direct path took the E.X. Troopers engine scene from 75.6 to 96.1
     * percent speed at 2x on the AYN Thor. The setting applies to Mesa drivers only; the
     * system driver ignores it. A `TU_DEBUG` line in thor_driver_env.txt overrides it.
     */
    private const val DEFAULT_MESA_DEBUG = "sysmem"

    private fun applyDriverEnvironment() {
        val assignments = linkedMapOf<String, String>()
        if (customDriverData.vendor == "Mesa") {
            assignments["TU_DEBUG"] = DEFAULT_MESA_DEBUG
        }
        try {
            val userPath = DirectoryInitialization.userPath
            val root = userPath?.let {
                DocumentFile.fromTreeUri(CitraApplication.appContext, Uri.parse(it))
            }
            val file = root?.findFile("thor_driver_env.txt")
            val text = file?.let {
                CitraApplication.appContext.contentResolver.openInputStream(it.uri)
                    ?.use { stream -> String(stream.readBytes()) }
            }
            text?.lines()?.map(String::trim)
                ?.filter { it.contains('=') && !it.startsWith("#") }
                ?.forEach { line ->
                    assignments[line.substringBefore('=').trim()] =
                        line.substringAfter('=').trim()
                }
        } catch (e: Exception) {
            Log.error("[GpuDriverHelper] Driver environment file: ${e.message}")
        }
        assignments.forEach { (key, value) ->
            try {
                if (value.isEmpty()) {
                    android.system.Os.unsetenv(key)
                    Log.info("[GpuDriverHelper] Driver environment: $key cleared")
                } else {
                    android.system.Os.setenv(key, value, true)
                    Log.info("[GpuDriverHelper] Driver environment: $key=$value")
                }
            } catch (e: Exception) {
                Log.error("[GpuDriverHelper] Driver environment $key: ${e.message}")
            }
        }
    }

    fun getDrivers(): MutableList<Pair<Uri, GpuDriverMetadata>> {
        val driverZips = driverStoragePath.listFiles()
        val drivers: MutableList<Pair<Uri, GpuDriverMetadata>> =
            driverZips
                .mapNotNull {
                    val metadata = getMetadataFromZip(it.inputStream())
                    metadata.name?.let { _ -> Pair(it.uri, metadata) }
                }
                .sortedByDescending { it: Pair<Uri, GpuDriverMetadata> -> it.second.name }
                .distinct()
                .toMutableList()

        // TODO: Get system driver information
        drivers.add(0, Pair(Uri.EMPTY, GpuDriverMetadata()))
        return drivers
    }

    fun installDefaultDriver() {
        // Removing the installed driver will result in the backend using the default system driver.
        File(driverInstallationPath!!).deleteRecursively()
        initializeDriverParameters()
    }

    fun copyDriverToExternalStorage(driverUri: Uri): DocumentFile? {
        // Ensure we have directories.
        initializeDirectories()

        // Copy the zip file URI to user data
        val copiedFile =
            FileUtil.copyToExternalStorage(driverUri, driverStoragePath) ?: return null

        if (getSupportedMetadata(copiedFile) == null) {
            copiedFile.delete()
            return null
        }
        return copiedFile
    }

    fun downloadRecommendedTurnipDriver(): DriverPackage? {
        val asset = fetchRecommendedDriverAsset()

        return downloadDriverAssetPackage(asset)
    }

    fun getRecommendedDriverOptions(): List<RecommendedDriverOption> {
        val assets = fetchReleaseAssets()
        val options = mutableListOf<RecommendedDriverOption>()
        val normalTurnipAssets = assets
            .filter { isRecommendedTurnipAsset(it.name) }
            .distinctBy { it.name }

        val recommendedTurnip = normalTurnipAssets.firstOrNull()
            ?: RemoteDriverAsset(FALLBACK_TURNIP_DRIVER_NAME, FALLBACK_TURNIP_DRIVER_URL)
        options.add(
            RecommendedDriverOption(
                title = "${driverDisplayName(recommendedTurnip.name)} - Recommended",
                note = "Best first try for Thor Base/Pro/Max and Adreno 740. " +
                    "Use this unless a game has new graphics bugs or crashes.",
                assetName = recommendedTurnip.name,
                downloadUrl = recommendedTurnip.downloadUrl
            )
        )

        normalTurnipAssets
            .drop(1)
            .take(MAX_NORMAL_TURNIP_OPTIONS - 1)
            .forEach {
                options.add(
                    RecommendedDriverOption(
                        title = driverDisplayName(it.name),
                        note = "Older Turnip build. Try this for one stubborn game if the " +
                            "recommended driver crashes, black screens, or regresses graphics.",
                        assetName = it.name,
                        downloadUrl = it.downloadUrl
                    )
                )
            }

        assets.firstOrNull { isTurnipVariantAsset(it.name, "sysmem") }?.let {
            options.add(
                RecommendedDriverOption(
                    title = driverDisplayName(it.name),
                    note = "Turnip alternate memory path. Try per-game if recommended Turnip " +
                        "has rendering glitches.",
                    assetName = it.name,
                    downloadUrl = it.downloadUrl
                )
            )
        }

        assets.firstOrNull { isTurnipVariantAsset(it.name, "gmem") }?.let {
            options.add(
                RecommendedDriverOption(
                    title = driverDisplayName(it.name),
                    note = "Turnip GMEM variant. Experimental on Thor; keep it as a " +
                        "troubleshooting option, not the default.",
                    assetName = it.name,
                    downloadUrl = it.downloadUrl
                )
            )
        }

        assets.firstOrNull { isQualcommDriverAsset(it.name) }?.let {
            options.add(
                RecommendedDriverOption(
                    title = driverDisplayName(it.name),
                    note = "Qualcomm user-mode package. Try this when Turnip breaks a " +
                        "specific game or you want a stock-like fallback.",
                    assetName = it.name,
                    downloadUrl = it.downloadUrl
                )
            )
        }

        return options.distinctBy { it.assetName }
    }

    fun downloadDriverOption(option: RecommendedDriverOption): DriverPackage? =
        downloadDriverAssetPackage(RemoteDriverAsset(option.assetName, option.downloadUrl))

    private fun downloadDriverAssetPackage(asset: RemoteDriverAsset): DriverPackage? {
        val existingDriver = driverStoragePath.findFile(asset.name)
        if (existingDriver != null) {
            val metadata = getSupportedMetadata(existingDriver)
            if (metadata != null) {
                return DriverPackage(existingDriver, metadata)
            }
            existingDriver.delete()
        }

        val downloadedDriver = downloadDriverAsset(asset) ?: return null
        val metadata = getSupportedMetadata(downloadedDriver)
        if (metadata == null) {
            downloadedDriver.delete()
            return null
        }
        return DriverPackage(downloadedDriver, metadata)
    }

    /**
     * Copies driver zip into user data directory so that it can be exported along with
     * other user data and also unzipped into the installation directory
     */
    fun installCustomDriverComplete(driverUri: Uri): Boolean {
        // Revert to system default in the event the specified driver is bad.
        installDefaultDriver()

        // Ensure we have directories.
        initializeDirectories()

        // Copy the zip file URI to user data
        val copiedFile =
            FileUtil.copyToExternalStorage(driverUri, driverStoragePath) ?: return false

        if (getSupportedMetadata(copiedFile) == null) {
            copiedFile.delete()
            return false
        }

        // Unzip the driver.
        try {
            FileUtil.unzipToInternalStorage(
                BufferedInputStream(copiedFile.inputStream()),
                File(driverInstallationPath!!)
            )
        } catch (e: SecurityException) {
            return false
        }

        // Initialize the driver parameters.
        initializeDriverParameters()

        return true
    }

    /**
     * Unzips driver into private installation directory
     */
    fun installCustomDriverPartial(driver: Uri): Boolean {
        // Revert to system default in the event the specified driver is bad.
        installDefaultDriver()

        // Ensure we have directories.
        initializeDirectories()

        // Validate driver
        val metadata = getMetadataFromZip(driver.inputStream())
        if (!isSupportedMetadata(metadata)) {
            driver.asDocumentFile()?.delete()
            return false
        }

        // Unzip the driver to the private installation directory
        try {
            FileUtil.unzipToInternalStorage(
                BufferedInputStream(driver.inputStream()),
                File(driverInstallationPath!!)
            )
        } catch (e: SecurityException) {
            return false
        }

        // Initialize the driver parameters.
        initializeDriverParameters()

        return true
    }

    /**
     * Takes in a zip file and reads the meta.json file for presentation to the UI
     *
     * @param driver Zip containing driver and meta.json file
     * @return A non-null [GpuDriverMetadata] instance that may have null members
     */
    fun getMetadataFromZip(driver: InputStream): GpuDriverMetadata {
        try {
            ZipInputStream(driver).use { zis ->
                var entry: ZipEntry? = zis.nextEntry
                while (entry != null) {
                    if (!entry.isDirectory && entry.name.lowercase().contains(".json")) {
                        val size = if (entry.size == -1L) 0L else entry.size
                        return GpuDriverMetadata(zis, size)
                    }
                    entry = zis.nextEntry
                }
            }
        } catch (_: ZipException) {
        }
        return GpuDriverMetadata()
    }

    private fun fetchRecommendedDriverAsset(): RemoteDriverAsset =
        fetchReleaseAssets().firstOrNull {
            isRecommendedTurnipAsset(it.name)
        }
            ?: RemoteDriverAsset(FALLBACK_TURNIP_DRIVER_NAME, FALLBACK_TURNIP_DRIVER_URL)

    private fun fetchReleaseAssets(): List<RemoteDriverAsset> {
        try {
            val connection = (
                URL(
                    DRIVER_RELEASES_URL
                ).openConnection() as HttpURLConnection
                ).apply {
                connectTimeout = 15000
                readTimeout = 30000
                setRequestProperty("Accept", "application/vnd.github+json")
                setRequestProperty("User-Agent", "AzaharThorExperiment")
            }

            connection.inputStream.use { input ->
                val releases = JSONArray(FileUtil.getStringFromInputStream(input))
                val assets = mutableListOf<RemoteDriverAsset>()
                for (releaseIndex in 0 until releases.length()) {
                    val release = releases.getJSONObject(releaseIndex)
                    if (release.optBoolean("draft", false)) {
                        continue
                    }

                    val releaseAssets = release.optJSONArray("assets") ?: continue
                    for (assetIndex in 0 until releaseAssets.length()) {
                        val asset = releaseAssets.getJSONObject(assetIndex)
                        val name = asset.optString("name")
                        val downloadUrl = asset.optString("browser_download_url")
                        if (downloadUrl.isNotBlank()) {
                            assets.add(RemoteDriverAsset(name, downloadUrl))
                        }
                    }
                }
                return assets
            }
        } catch (e: Exception) {
            Log.warning("[GpuDriverHelper] Failed to fetch driver release list: ${e.message}")
        }

        return emptyList()
    }

    private fun isRecommendedTurnipAsset(name: String): Boolean {
        val normalized = name.lowercase()
        return normalized.startsWith("turnip_v") &&
            normalized.endsWith(".zip") &&
            !normalized.contains("a8xx") &&
            !normalized.contains("gmem") &&
            !normalized.contains("sysmem") &&
            !normalized.contains("magisk") &&
            !normalized.contains("winlator")
    }

    private fun isTurnipVariantAsset(name: String, variant: String): Boolean {
        val normalized = name.lowercase()
        return normalized.startsWith("turnip_v") &&
            normalized.endsWith(".zip") &&
            normalized.contains(variant) &&
            !normalized.contains("a8xx") &&
            !normalized.contains("magisk") &&
            !normalized.contains("winlator")
    }

    private fun isQualcommDriverAsset(name: String): Boolean {
        val normalized = name.lowercase()
        return normalized.startsWith("qualcomm_") &&
            normalized.endsWith("_adpkg.zip")
    }

    private fun driverDisplayName(name: String): String = name.removeSuffix(".zip")
        .removeSuffix("_adpkg")
        .replace('_', ' ')

    private fun downloadDriverAsset(asset: RemoteDriverAsset): DocumentFile? {
        val destinationFile =
            driverStoragePath.createFile("application/zip", asset.name) ?: return null

        try {
            val connection = (URL(asset.downloadUrl).openConnection() as HttpURLConnection).apply {
                connectTimeout = 15000
                readTimeout = 120000
                setRequestProperty("User-Agent", "AzaharThorExperiment")
            }

            connection.inputStream.use { input ->
                destinationFile.outputStream().use { output ->
                    input.copyTo(output)
                }
            }
            return destinationFile
        } catch (e: Exception) {
            Log.error("[GpuDriverHelper] Failed to download Turnip driver: ${e.message}")
            destinationFile.delete()
            return null
        }
    }

    private fun getSupportedMetadata(driverFile: DocumentFile): GpuDriverMetadata? {
        val metadata = getMetadataFromZip(driverFile.inputStream())
        if (!isSupportedMetadata(metadata)) {
            return null
        }
        return metadata
    }

    private fun isSupportedMetadata(metadata: GpuDriverMetadata): Boolean = metadata.name != null &&
        metadata.libraryName != null &&
        metadata.minApi <= Build.VERSION.SDK_INT

    external fun supportsCustomDriverLoading(): Boolean

    // Parse the custom driver metadata to retrieve the name.
    val customDriverData: GpuDriverMetadata
        get() = GpuDriverMetadata(File(driverInstallationPath + META_JSON_FILENAME))

    fun initializeDirectories() {
        // Ensure the file redirection directory exists.
        val fileRedirectionDir = File(fileRedirectionPath!!)
        if (!fileRedirectionDir.exists()) {
            fileRedirectionDir.mkdirs()
        }
        // Ensure the driver installation directory exists.
        val driverInstallationDir = File(driverInstallationPath!!)
        if (!driverInstallationDir.exists()) {
            driverInstallationDir.mkdirs()
        }
        // Ensure the driver storage directory exists
        if (!driverStoragePath.exists()) {
            throw IllegalStateException("Driver storage directory couldn't be created!")
        }
    }
}

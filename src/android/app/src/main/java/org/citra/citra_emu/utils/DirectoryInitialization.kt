// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.content.Context
import android.net.Uri
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.concurrent.atomic.AtomicBoolean
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.utils.PermissionsHandler.hasWriteAccess

/**
 * A service that spawns its own thread in order to copy several binary and shader files
 * from the Citra APK to the external file system.
 */
object DirectoryInitialization {
    private const val BUNDLED_CHEATS_DIR = "cheats"
    private const val BUNDLED_GAME_SETTINGS_DIR = "game_profiles"
    private const val GAME_SETTINGS_DIR = "GameSettings"
    private const val BUNDLED_SHADER_RULES_DIR = "shader_rules"
    private const val SHADER_RULES_DIR = "config/ShaderRules"
    private const val BUNDLED_GAME_NOTES_DIR = "game_notes"
    private const val GAME_NOTES_DIR = "config/GameNotes"
    private const val SYS_DIR_VERSION = "sysDirectoryVersion"
    private val REPLACEABLE_BUNDLED_CHEAT_SIZES = mapOf(
        "0004000000086300.txt" to 5076L,
        "000400000008C300.txt" to 67L
    )

    @Volatile
    private var directoryState: DirectoryInitializationState? = null
    var userPath: String? = null
    val internalUserPath: String
        get() = CitraApplication.appContext.filesDir.canonicalPath
    private val isCitraDirectoryInitializationRunning = AtomicBoolean(false)

    val context: Context get() = CitraApplication.appContext

    @JvmStatic
    fun start(): DirectoryInitializationState? {
        if (!isCitraDirectoryInitializationRunning.compareAndSet(false, true)) {
            return null
        }

        if (directoryState != DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED) {
            directoryState = if (hasWriteAccess(context)) {
                if (setCitraUserDirectory()) {
                    CitraApplication.documentsTree.setRoot(Uri.parse(userPath))
                    NativeLibrary.createLogFile()
                    NativeLibrary.logUserDirectory(userPath.toString())
                    NativeLibrary.createConfigFile()
                    installBundledCheats()
                    installBundledGameSettings()
                    installBundledShaderRules()
                    installBundledGameNotes()
                    GpuDriverHelper.initializeDriverParameters()
                    BundledGpuDriver.installIfNeeded(context)
                    DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED
                } else {
                    DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE
                }
            } else {
                DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED
            }
        }
        isCitraDirectoryInitializationRunning.set(false)
        return directoryState
    }

    private fun deleteDirectoryRecursively(file: File) {
        if (file.isDirectory) {
            for (child in file.listFiles()!!) {
                deleteDirectoryRecursively(child)
            }
        }
        file.delete()
    }

    @JvmStatic
    fun areCitraDirectoriesReady(): Boolean =
        directoryState == DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED

    fun resetCitraDirectoryState() {
        directoryState = null
        isCitraDirectoryInitializationRunning.compareAndSet(true, false)
    }

    val userDirectory: String?
        get() {
            checkNotNull(directoryState) {
                "DirectoryInitialization has to run at least once!"
            }
            check(!isCitraDirectoryInitializationRunning.get()) {
                "DirectoryInitialization has to finish running first!"
            }
            return userPath
        }

    fun setCitraUserDirectory(): Boolean {
        val dataPath = PermissionsHandler.citraDirectory
        if (dataPath.toString().isNotEmpty()) {
            userPath = dataPath.toString()
            android.util.Log.d("[Azahar Frontend]", "[DirectoryInitialization] User Dir: $userPath")
            return true
        }
        return false
    }

    private fun installBundledCheats() {
        val cheatFiles = try {
            context.assets.list(BUNDLED_CHEATS_DIR) ?: return
        } catch (e: IOException) {
            Log.error(
                "[DirectoryInitialization] Failed to list bundled cheats: " +
                    e.message
            )
            return
        }

        if (cheatFiles.isEmpty()) {
            return
        }

        if (CitraApplication.documentsTree.folderUriHelper("/cheats/", true) == null) {
            Log.warning("[DirectoryInitialization] Failed to create bundled cheats directory")
            return
        }

        for (filename in cheatFiles) {
            if (!filename.endsWith(".txt", ignoreCase = true)) {
                continue
            }

            val destinationPath = "/cheats/$filename"
            try {
                val existingSize = CitraApplication.documentsTree.getFileSize(destinationPath)
                val shouldReplace = REPLACEABLE_BUNDLED_CHEAT_SIZES[filename] == existingSize
                if (existingSize > 0L && !shouldReplace) {
                    continue
                }

                if (!CitraApplication.documentsTree.createFile("/cheats/", filename)) {
                    Log.warning(
                        "[DirectoryInitialization] Failed to create bundled cheat $filename"
                    )
                    continue
                }

                val destinationUri = CitraApplication.documentsTree.getUri(destinationPath)
                context.assets.open("$BUNDLED_CHEATS_DIR/$filename").use { input ->
                    context.contentResolver.openOutputStream(destinationUri, "wt").use { output ->
                        if (output == null) {
                            Log.warning(
                                "[DirectoryInitialization] Failed to open bundled cheat $filename"
                            )
                        } else {
                            copyFile(input, output)
                        }
                    }
                }
            } catch (e: Exception) {
                Log.error(
                    "[DirectoryInitialization] Failed to install bundled cheat $filename: " +
                        e.message
                )
            }
        }
    }

    /**
     * Seeds GameSettings/ with the per-title profiles shipped in assets. A file the user already
     * has is never touched, so in-app edits always win over bundled defaults.
     */
    /**
     * Seed the per-title draw rule files the renderer reads, the same way game profiles are
     * seeded: a file the user already has is never overwritten, so an edited rule set wins over
     * the bundled one and deleting a file restores the untouched picture.
     */
    /**
     * Seed the per-title notes shown from the game's bottom sheet. Same contract as the other
     * bundled assets: a file the user already has is never overwritten, so an edited note wins.
     */
    private fun installBundledGameNotes() {
        val notes = try {
            context.assets.list(BUNDLED_GAME_NOTES_DIR) ?: return
        } catch (e: IOException) {
            Log.error("[DirectoryInitialization] Failed to list bundled game notes: ${e.message}")
            return
        }
        val noteFiles = notes.filter { it.endsWith(".md", ignoreCase = true) }
        if (noteFiles.isEmpty()) {
            return
        }
        val tree = CitraApplication.documentsTree
        if (tree.folderUriHelper("/$GAME_NOTES_DIR/", true) == null) {
            Log.warning("[DirectoryInitialization] Failed to create $GAME_NOTES_DIR directory")
            return
        }
        for (filename in noteFiles) {
            val destinationPath = "/$GAME_NOTES_DIR/$filename"
            try {
                if (tree.exists(destinationPath)) {
                    continue
                }
                if (!tree.createFile("/$GAME_NOTES_DIR/", filename)) {
                    Log.warning("[DirectoryInitialization] Failed to create $destinationPath")
                    continue
                }
                context.assets.open("$BUNDLED_GAME_NOTES_DIR/$filename").use { input ->
                    context.contentResolver.openOutputStream(tree.getUri(destinationPath), "wt")
                        .use { output ->
                            if (output == null) {
                                Log.warning("[DirectoryInitialization] Failed to open $destinationPath")
                            } else {
                                copyFile(input, output)
                            }
                        }
                }
            } catch (e: Exception) {
                Log.error(
                    "[DirectoryInitialization] Failed to install bundled game notes $filename: " +
                        e.message
                )
            }
        }
    }

    private fun installBundledShaderRules() {
        val rules = try {
            context.assets.list(BUNDLED_SHADER_RULES_DIR) ?: return
        } catch (e: IOException) {
            Log.error("[DirectoryInitialization] Failed to list bundled shader rules: ${e.message}")
            return
        }
        val ruleFiles = rules.filter { it.endsWith(".txt", ignoreCase = true) }
        if (ruleFiles.isEmpty()) {
            return
        }
        val tree = CitraApplication.documentsTree
        if (tree.folderUriHelper("/$SHADER_RULES_DIR/", true) == null) {
            Log.warning("[DirectoryInitialization] Failed to create $SHADER_RULES_DIR directory")
            return
        }
        for (filename in ruleFiles) {
            val destinationPath = "/$SHADER_RULES_DIR/$filename"
            try {
                if (tree.exists(destinationPath)) {
                    continue
                }
                if (!tree.createFile("/$SHADER_RULES_DIR/", filename)) {
                    Log.warning("[DirectoryInitialization] Failed to create $destinationPath")
                    continue
                }
                context.assets.open("$BUNDLED_SHADER_RULES_DIR/$filename").use { input ->
                    context.contentResolver.openOutputStream(tree.getUri(destinationPath), "wt")
                        .use { output ->
                            if (output == null) {
                                Log.warning("[DirectoryInitialization] Failed to open $destinationPath")
                            } else {
                                copyFile(input, output)
                            }
                        }
                }
            } catch (e: Exception) {
                Log.error(
                    "[DirectoryInitialization] Failed to install bundled shader rules $filename: " +
                        e.message
                )
            }
        }
    }

    private fun installBundledGameSettings() {
        val profiles = try {
            context.assets.list(BUNDLED_GAME_SETTINGS_DIR) ?: return
        } catch (e: IOException) {
            Log.error("[DirectoryInitialization] Failed to list bundled game settings: ${e.message}")
            return
        }
        val iniFiles = profiles.filter { it.endsWith(".ini", ignoreCase = true) }
        if (iniFiles.isEmpty()) {
            return
        }
        val tree = CitraApplication.documentsTree
        if (tree.folderUriHelper("/$GAME_SETTINGS_DIR/", true) == null) {
            Log.warning("[DirectoryInitialization] Failed to create $GAME_SETTINGS_DIR directory")
            return
        }
        for (filename in iniFiles) {
            val destinationPath = "/$GAME_SETTINGS_DIR/$filename"
            try {
                if (tree.exists(destinationPath)) {
                    continue
                }
                if (!tree.createFile("/$GAME_SETTINGS_DIR/", filename)) {
                    Log.warning("[DirectoryInitialization] Failed to create $destinationPath")
                    continue
                }
                context.assets.open("$BUNDLED_GAME_SETTINGS_DIR/$filename").use { input ->
                    context.contentResolver.openOutputStream(tree.getUri(destinationPath), "wt")
                        .use { output ->
                            if (output == null) {
                                Log.warning("[DirectoryInitialization] Failed to open $destinationPath")
                            } else {
                                copyFile(input, output)
                            }
                        }
                }
            } catch (e: Exception) {
                Log.error(
                    "[DirectoryInitialization] Failed to install bundled game settings $filename: " +
                        e.message
                )
            }
        }
    }

    private fun copyAsset(asset: String, output: File, overwrite: Boolean, context: Context) {
        Log.debug("[DirectoryInitialization] Copying File $asset to $output")
        try {
            if (!output.exists() || overwrite) {
                val inputStream = context.assets.open(asset)
                val outputStream = FileOutputStream(output)
                copyFile(inputStream, outputStream)
                inputStream.close()
                outputStream.close()
            }
        } catch (e: IOException) {
            Log.error("[DirectoryInitialization] Failed to copy asset file: $asset" + e.message)
        }
    }

    private fun copyAssetFolder(
        assetFolder: String,
        outputFolder: File,
        overwrite: Boolean,
        context: Context
    ) {
        Log.debug("[DirectoryInitialization] Copying Folder $assetFolder to $outputFolder")
        try {
            var createdFolder = false
            for (file in context.assets.list(assetFolder)!!) {
                if (!createdFolder) {
                    outputFolder.mkdir()
                    createdFolder = true
                }
                copyAssetFolder(
                    assetFolder + File.separator + file,
                    File(outputFolder, file),
                    overwrite,
                    context
                )
                copyAsset(
                    assetFolder + File.separator + file,
                    File(outputFolder, file),
                    overwrite,
                    context
                )
            }
        } catch (e: IOException) {
            Log.error(
                "[DirectoryInitialization] Failed to copy asset folder: $assetFolder" +
                    e.message
            )
        }
    }

    @Throws(IOException::class)
    private fun copyFile(inputStream: InputStream, outputStream: OutputStream) {
        val buffer = ByteArray(1024)
        var read: Int
        while (inputStream.read(buffer).also { read = it } != -1) {
            outputStream.write(buffer, 0, read)
        }
    }

    enum class DirectoryInitializationState {
        CITRA_DIRECTORIES_INITIALIZED,
        EXTERNAL_STORAGE_PERMISSION_NEEDED,
        CANT_FIND_EXTERNAL_STORAGE
    }
}

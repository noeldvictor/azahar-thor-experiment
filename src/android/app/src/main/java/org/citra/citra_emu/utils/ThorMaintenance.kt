// Copyright 2026 Azahar Thor fork
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.io.InputStream
import java.security.MessageDigest
import java.util.zip.ZipInputStream
import org.citra.citra_emu.CitraApplication
import org.json.JSONArray
import org.json.JSONObject

/**
 * Startup maintenance for the Thor MCP server. The app's private files directory is not readable
 * over ADB on a release build. At startup the app looks for `thor_maintenance.txt` in the user
 * directory. The file holds one operation name and an optional zip name. The app runs the
 * operation on its own driver directories, writes `log/thor_maintenance.json`, and deletes the
 * request. There is no exported component; only the user directory can trigger this.
 *
 * Request file: line 1 = op, line 2 = zip name (optional).
 * Ops: list, verify, clear_redirect, reinstall_driver, system_driver.
 */
object ThorMaintenance {
    private const val REQUEST_NAME = "thor_maintenance.txt"
    private const val RESULT_NAME = "thor_maintenance.json"

    fun runPendingRequest(context: Context) {
        val userPath = DirectoryInitialization.userPath ?: return
        val root = DocumentFile.fromTreeUri(context, Uri.parse(userPath)) ?: return
        val request = root.findFile(REQUEST_NAME) ?: return
        val lines = try {
            context.contentResolver.openInputStream(request.uri)?.use {
                String(it.readBytes()).lines().map(String::trim).filter(String::isNotEmpty)
            } ?: emptyList()
        } catch (e: Exception) {
            emptyList()
        }
        request.delete()
        val op = lines.getOrNull(0) ?: "list"
        val zipName = lines.getOrNull(1)
        val result = run(context, op, zipName)
        Log.info("[ThorMaintenance] $result")
        writeResult(context, root, result.toString())
    }

    private fun run(context: Context, op: String, zipName: String?): JSONObject {
        val result = JSONObject().put("op", op)
        try {
            val files = context.filesDir
            val driverDir = File(files, "gpu_driver")
            val redirectDir = File(files, "gpu/vk_file_redirect")
            when (op) {
                "list" -> {
                    // The whole private data directory: files, shared_prefs, cache, databases.
                    val dataDir = files.parentFile ?: files
                    result.put("data_dir", dataDir.canonicalPath)
                    result.put("entries", listTree(dataDir, 800))
                    val prefs = JSONObject()
                    File(dataDir, "shared_prefs").listFiles()?.forEach {
                        if (it.isFile && it.length() < 65536) {
                            prefs.put(it.name, it.readText())
                        }
                    }
                    result.put("shared_prefs", prefs)
                }
                "verify" -> result.put("driver", verifyDriver(driverDir))
                "clear_redirect" -> {
                    result.put("redirect_before", listTree(redirectDir, 200))
                    result.put("deleted", redirectDir.deleteRecursively())
                    redirectDir.mkdirs()
                }
                "reinstall_driver" -> {
                    val wanted = zipName
                        ?: GpuDriverHelper.customDriverData.name
                        ?: throw IllegalStateException("no driver is selected and no zip given")
                    val zip = findDriverZip(wanted)
                        ?: throw IllegalStateException("no zip matches '$wanted' in gpu_drivers")
                    result.put("zip", zip.first)
                    result.put("installed", GpuDriverHelper.installCustomDriverPartial(zip.second))
                    result.put("driver", verifyDriver(driverDir))
                }
                "system_driver" -> {
                    GpuDriverHelper.installDefaultDriver()
                    result.put("driver", verifyDriver(driverDir))
                }
                else -> throw IllegalArgumentException("unknown op '$op'")
            }
            result.put("ok", true)
        } catch (e: Exception) {
            result.put("ok", false)
            result.put("error", e.toString())
        }
        return result
    }

    private fun writeResult(context: Context, root: DocumentFile, text: String) {
        val logDir = root.findFile("log") ?: root.createDirectory("log") ?: return
        logDir.findFile(RESULT_NAME)?.delete()
        val file = logDir.createFile("application/json", RESULT_NAME) ?: return
        context.contentResolver.openOutputStream(file.uri)?.use { it.write(text.toByteArray()) }
    }

    private fun listTree(root: File, limit: Int): JSONArray {
        val out = JSONArray()
        if (!root.exists()) {
            return out
        }
        root.walkTopDown().filter { it.isFile }.take(limit).forEach {
            out.put(
                JSONObject()
                    .put("path", it.relativeTo(root).path)
                    .put("size", it.length())
                    .put("mtime", it.lastModified())
            )
        }
        return out
    }

    /** Hashes every extracted driver file and the matching entry of the selected zip. */
    private fun verifyDriver(driverDir: File): JSONObject {
        val out = JSONObject()
        val meta = GpuDriverHelper.customDriverData
        out.put("name", meta.name ?: "System GPU driver")
        out.put("library", meta.libraryName ?: "")
        val extracted = JSONObject()
        if (driverDir.exists()) {
            driverDir.walkTopDown().filter { it.isFile }.forEach {
                extracted.put(
                    it.name,
                    JSONObject().put("size", it.length()).put("sha256", sha256(it.inputStream()))
                )
            }
        }
        out.put("extracted", extracted)
        val name = meta.name ?: return out
        val zip = findDriverZip(name) ?: return out.put("zip", "not found")
        out.put("zip", zip.first)
        val inZip = JSONObject()
        val stream = CitraApplication.appContext.contentResolver.openInputStream(zip.second)
            ?: return out.put("zip_error", "cannot open")
        ZipInputStream(stream).use { zis ->
            var entry = zis.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val digest = MessageDigest.getInstance("SHA-256")
                    val buffer = ByteArray(65536)
                    var total = 0L
                    while (true) {
                        val read = zis.read(buffer)
                        if (read < 0) break
                        digest.update(buffer, 0, read)
                        total += read
                    }
                    inZip.put(
                        File(entry.name).name,
                        JSONObject().put("size", total).put("sha256", hex(digest.digest()))
                    )
                }
                entry = zis.nextEntry
            }
        }
        out.put("in_zip", inZip)
        var match = true
        inZip.keys().forEach { key ->
            val a = inZip.getJSONObject(key).getString("sha256")
            val b = extracted.optJSONObject(key)?.optString("sha256")
            if (a != b) match = false
        }
        out.put("match", match)
        return out
    }

    /** Finds a driver zip in the user directory by file name or by metadata name. */
    private fun findDriverZip(wanted: String): Pair<String, Uri>? {
        val zips = GpuDriverHelper.driverStoragePath.listFiles()
        zips.firstOrNull { it.name == wanted }?.let { return Pair(it.name ?: wanted, it.uri) }
        val resolver = CitraApplication.appContext.contentResolver
        for (zip in zips) {
            val stream = resolver.openInputStream(zip.uri) ?: continue
            val meta = GpuDriverHelper.getMetadataFromZip(stream)
            if (meta.name == wanted) {
                return Pair(zip.name ?: wanted, zip.uri)
            }
        }
        return null
    }

    private fun sha256(stream: InputStream): String {
        val digest = MessageDigest.getInstance("SHA-256")
        stream.use {
            val buffer = ByteArray(65536)
            while (true) {
                val read = it.read(buffer)
                if (read < 0) break
                digest.update(buffer, 0, read)
            }
        }
        return hex(digest.digest())
    }

    private fun hex(bytes: ByteArray): String = bytes.joinToString("") { "%02x".format(it) }
}

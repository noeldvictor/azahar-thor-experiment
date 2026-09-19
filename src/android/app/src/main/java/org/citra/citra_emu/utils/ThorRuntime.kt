// Copyright 2026 Azahar Thor fork
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.net.Uri
import android.os.Handler
import android.os.Looper
import androidx.documentfile.provider.DocumentFile
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary
import org.json.JSONObject

/**
 * Runtime command channel for the Thor MCP server.
 *
 * A measurement that needs a particular scene used to cost a seven minute replay. While a game
 * runs, the fragment polls `thor_command.txt` in the user directory once per second. The file
 * holds one command and an optional argument. The app runs it, writes `log/thor_command.json`,
 * and deletes the request. There is no exported component; only the user directory can trigger
 * this, the same channel [ThorMaintenance] uses at startup.
 *
 * Commands: save_state <slot>, load_state <slot>, perf, perf_log on|off, speed <percent>.
 */
object ThorRuntime {
    private const val REQUEST_NAME = "thor_command.txt"
    private const val RESULT_NAME = "thor_command.json"
    private const val POLL_MS = 1000L

    private val handler = Handler(Looper.getMainLooper())
    private var poller: Runnable? = null
    private var logPerf = false

    fun start() {
        if (poller != null) {
            return
        }
        val task = object : Runnable {
            override fun run() {
                try {
                    poll()
                } catch (e: Exception) {
                    Log.error("[ThorRuntime] poll failed: ${e.message}")
                }
                handler.postDelayed(this, POLL_MS)
            }
        }
        poller = task
        handler.post(task)
    }

    fun stop() {
        poller?.let { handler.removeCallbacks(it) }
        poller = null
    }

    private fun root(): DocumentFile? {
        val userPath = DirectoryInitialization.userPath ?: return null
        return DocumentFile.fromTreeUri(CitraApplication.appContext, Uri.parse(userPath))
    }

    private fun poll() {
        if (logPerf) {
            Log.info("[ThorRuntime] ${perfStats()}")
        }
        val root = root() ?: return
        val request = root.findFile(REQUEST_NAME) ?: return
        val context = CitraApplication.appContext
        val lines = try {
            context.contentResolver.openInputStream(request.uri)?.use {
                // Keep empty lines: the argument line is often blank and the id follows it.
                String(it.readBytes()).lines().map(String::trim)
            } ?: emptyList()
        } catch (e: Exception) {
            emptyList()
        }
        request.delete()
        val command = lines.getOrNull(0)?.takeIf { it.isNotEmpty() } ?: return
        val argument = lines.getOrNull(1)
        val requestId = lines.getOrNull(2) ?: ""
        val result = run(command, argument).put("id", requestId)
        Log.info("[ThorRuntime] $result")
        writeResult(root, result.toString())
    }

    private fun run(command: String, argument: String?): JSONObject {
        val result = JSONObject().put("command", command).put("argument", argument ?: "")
        try {
            when (command) {
                "save_state" -> {
                    val slot = argument?.toIntOrNull() ?: 1
                    NativeLibrary.saveState(slot)
                    result.put("slot", slot)
                }
                "load_state" -> {
                    val slot = argument?.toIntOrNull() ?: 1
                    result.put("slot", slot)
                    // Load directly rather than through loadStateIfAvailable, which hides a
                    // state written by a different build of the emulator. Measuring a change
                    // means loading the same scene before and after a rebuild, and the core
                    // decides whether to accept it through allow_savestate_mismatch.
                    val listed = NativeLibrary.getSavestateInfo()?.any { it.slot == slot } ?: false
                    result.put("listed", listed)
                    NativeLibrary.loadState(slot)
                    // listed is the honest signal. A state written by another build is not
                    // listed, and the load is dropped even when the core is told to accept a
                    // version mismatch, so reporting success unconditionally hid real failures.
                    result.put("requested", true)
                    result.put("loaded", listed)
                }
                "states" -> {
                    val info = NativeLibrary.getSavestateInfo()
                    result.put("count", info?.size ?: 0)
                    result.put("slots", info?.joinToString(",") { it.slot.toString() } ?: "")
                }
                "perf" -> {}
                "perf_log" -> logPerf = argument?.lowercase() != "off"
                else -> throw IllegalArgumentException("unknown command '$command'")
            }
            result.put("perf", perfStats())
            result.put("ok", true)
        } catch (e: Exception) {
            result.put("ok", false)
            result.put("error", e.toString())
        }
        return result
    }

    /** The native perf stat array, named. Index order matches NativeLibrary.getPerfStats(). */
    private fun perfStats(): JSONObject {
        val stats = NativeLibrary.getPerfStats()
        val names = arrayOf(
            "system_fps", "game_fps", "speed_percent", "frame_time_s", "svc_s", "ipc_s",
            "gpu_cmd_s", "swap_s", "remaining_s"
        )
        val out = JSONObject()
        for (i in names.indices) {
            if (i >= stats.size) {
                continue
            }
            // Frame rates come through as they are, the speed ratio becomes a percent, and every
            // time is reported in milliseconds.
            val value = when {
                i == 2 -> stats[i] * 100.0
                i >= 3 -> stats[i] * 1000.0
                else -> stats[i]
            }
            out.put(names[i], Math.round(value * 1000.0) / 1000.0)
        }
        return out
    }

    private fun writeResult(root: DocumentFile, text: String) {
        val logDir = root.findFile("log") ?: root.createDirectory("log") ?: return
        // Overwrite in place. Deleting and recreating leaves a stale entry behind when the host
        // removed the file underneath the storage provider.
        val file = logDir.findFile(RESULT_NAME)
            ?: logDir.createFile("application/json", RESULT_NAME)
            ?: return
        CitraApplication.appContext.contentResolver.openOutputStream(file.uri, "wt")?.use {
            it.write(text.toByteArray())
        }
    }
}

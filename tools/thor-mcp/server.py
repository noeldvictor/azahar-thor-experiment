#!/usr/bin/env python3
# Copyright 2026 Azahar Thor Experiment
# Licensed under GPLv2 or any later version
# Refer to the license.txt file included.
"""MCP server that controls an AYN Thor over ADB for the Azahar Thor fork.

Transport: stdio. Claude Code starts it from `.mcp.json`. Start it by hand with:

    python tools/thor-mcp/server.py
    python tools/thor-mcp/server.py --list-tools

Environment:
    THOR_SERIAL       ADB serial. If unset, the single attached device is used.
    THOR_PACKAGE      App package. Default: org.azahar_emu.azahar.debug
    THOR_USER_DIR     Azahar user directory on the device.
                      Default: /storage/emulated/0/Azaharuser
    THOR_ROM_TREE     Storage tree the app holds a persisted grant for, written as
                      "<volume>:<path>". Default: 2664-21DE:Roms/n3ds
    THOR_CAPTURE_DIR  Local folder for screenshots and pulled files.
                      Default: <repo>/thor-captures (ignored by git).
    ADB               adb executable. Default: adb
"""
from __future__ import annotations

import json
import asyncio
import os
import re
import statistics
import subprocess
import sys
import time
import urllib.parse
from pathlib import Path

from mcp.server.fastmcp import FastMCP

REPO_ROOT = Path(__file__).resolve().parents[2]
ADB = os.environ.get("ADB", "adb")
PACKAGE = os.environ.get("THOR_PACKAGE", "org.azahar_emu.azahar.debug")
ACTIVITY = "org.citra.citra_emu.activities.EmulationActivity"
USER_DIR = os.environ.get("THOR_USER_DIR", "/storage/emulated/0/Azaharuser")
ROM_TREE = os.environ.get("THOR_ROM_TREE", "2664-21DE:Roms/n3ds")
CAPTURE_DIR = Path(os.environ.get("THOR_CAPTURE_DIR", str(REPO_ROOT / "thor-captures")))
DOCS_AUTHORITY = "com.android.externalstorage.documents"
KGSL = "/sys/class/kgsl/kgsl-3d0"

BUTTON_KEYCODES = {
    "A": "KEYCODE_BUTTON_A",
    "B": "KEYCODE_BUTTON_B",
    "X": "KEYCODE_BUTTON_X",
    "Y": "KEYCODE_BUTTON_Y",
    "L": "KEYCODE_BUTTON_L1",
    "R": "KEYCODE_BUTTON_R1",
    "ZL": "KEYCODE_BUTTON_L2",
    "ZR": "KEYCODE_BUTTON_R2",
    "START": "KEYCODE_BUTTON_START",
    "SELECT": "KEYCODE_BUTTON_SELECT",
    "UP": "KEYCODE_DPAD_UP",
    "DOWN": "KEYCODE_DPAD_DOWN",
    "LEFT": "KEYCODE_DPAD_LEFT",
    "RIGHT": "KEYCODE_DPAD_RIGHT",
    "BACK": "KEYCODE_BACK",
    "HOME": "KEYCODE_HOME",
}

mcp = FastMCP("thor")


# ----------------------------------------------------------------------------
# ADB helpers
# ----------------------------------------------------------------------------

def _adb(args: list[str], timeout: int = 120, binary: bool = False):
    proc = subprocess.run([ADB, *args], capture_output=True, timeout=timeout)
    if proc.returncode != 0:
        err = proc.stderr.decode("utf-8", "replace").strip()
        out = proc.stdout.decode("utf-8", "replace").strip()
        raise RuntimeError(f"adb {' '.join(args)} failed ({proc.returncode}): {err or out}")
    return proc.stdout if binary else proc.stdout.decode("utf-8", "replace")


def _device_list() -> list[dict]:
    devs = []
    for line in _adb(["devices", "-l"]).splitlines()[1:]:
        parts = line.split()
        if len(parts) >= 2 and parts[1] == "device":
            info = {"serial": parts[0]}
            for part in parts[2:]:
                if ":" in part:
                    key, value = part.split(":", 1)
                    info[key] = value
            devs.append(info)
    return devs


def _serial() -> str:
    serial = os.environ.get("THOR_SERIAL", "").strip()
    if serial:
        return serial
    devs = _device_list()
    if len(devs) == 1:
        return devs[0]["serial"]
    raise RuntimeError(f"Set THOR_SERIAL. Attached devices: {devs}")


def _sh(command: str, timeout: int = 120) -> str:
    return _adb(["-s", _serial(), "shell", command], timeout=timeout)


def _pid() -> int | None:
    out = _sh(f"pidof {PACKAGE} 2>/dev/null; true").strip()
    return int(out.split()[0]) if out else None


def _setting(name: str, namespace: str = "system") -> str:
    return _sh(f"settings get {namespace} {name}").strip()


def _gpubusy_percent(text: str) -> float:
    busy, total = [int(x) for x in text.split()[:2]]
    return 100.0 * busy / total if total else 0.0


def _display_ids() -> dict:
    """Physical display ids. The HWC display 0 is the primary panel."""
    ids = {}
    for line in _sh("dumpsys SurfaceFlinger --display-id").splitlines():
        match = re.match(r"Display (\d+) \(HWC display (\d+)\)", line.strip())
        if match:
            ids["primary" if match.group(2) == "0" else "secondary"] = match.group(1)
    return ids


def _blast_layer() -> str:
    pattern = re.compile(
        rf"^SurfaceView\[{re.escape(PACKAGE)}/{re.escape(ACTIVITY)}\]\(BLAST\)#\d+$"
    )
    for line in _sh("dumpsys SurfaceFlinger --list").splitlines():
        if pattern.match(line.strip()):
            return line.strip()
    raise RuntimeError("No Azahar BLAST layer is on screen. Is a game running?")


def _capture_path(name: str, suffix: str) -> Path:
    CAPTURE_DIR.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    stem = re.sub(r"[^A-Za-z0-9_.-]+", "-", name) if name else "capture"
    return CAPTURE_DIR / f"{stamp}-{stem}{suffix}"


def _apply_assignments(text: str, assignments: dict[str, str]) -> str:
    """Set `Section.key = value` pairs in ini text. Missing sections and keys are added."""
    lines = text.splitlines()
    for full_key, value in assignments.items():
        if "." not in full_key:
            raise ValueError(f"Use 'Section.key' form, got {full_key!r}")
        section, key = full_key.split(".", 1)
        header = f"[{section}]"
        starts = [i for i, line in enumerate(lines) if line.strip() == header]
        if not starts:
            if lines and lines[-1].strip():
                lines.append("")
            lines.extend([header, f"{key} = {value}"])
            continue
        start = starts[0]
        end = next(
            (i for i in range(start + 1, len(lines)) if lines[i].startswith("[")), len(lines)
        )
        key_pattern = re.compile(rf"^\s*{re.escape(key)}\s*=")
        for i in range(start + 1, end):
            if key_pattern.match(lines[i]):
                lines[i] = f"{key} = {value}"
                break
        else:
            insert = end
            while insert > start + 1 and not lines[insert - 1].strip():
                insert -= 1
            lines.insert(insert, f"{key} = {value}")
    return "\n".join(lines) + "\n"


def _read_remote_text(remote: str) -> str:
    # exec-out is binary safe. A plain shell cat inserts CR before every LF on Windows hosts.
    return _adb(["-s", _serial(), "exec-out", "cat", remote], binary=True).decode("utf-8", "replace")


def _write_remote_text(remote: str, text: str) -> None:
    local = _capture_path("push", ".ini")
    local.write_text(text, encoding="utf-8", newline="\n")
    _adb(["-s", _serial(), "push", str(local), remote])
    local.unlink(missing_ok=True)


def _require_stopped(allow_running: bool) -> None:
    pid = _pid()
    if pid and not allow_running:
        raise RuntimeError(
            f"{PACKAGE} is running (pid {pid}). It reads config at launch and may overwrite "
            "edits on exit. Call stop() first, or pass allow_running=True."
        )


# ----------------------------------------------------------------------------
# Tools: device state and settings
# ----------------------------------------------------------------------------

@mcp.tool()
def devices() -> list[dict]:
    """List attached ADB devices with their model and transport."""
    return _device_list()


@mcp.tool()
def shell(command: str, timeout: int = 120) -> str:
    """Run one shell command on the Thor and return its output."""
    return _sh(command, timeout)


@mcp.tool()
def state() -> dict:
    """Snapshot the device: model, app version, pid, performance and fan mode, brightness,
    battery, GPU clock and busy percent, and the active Vulkan driver from the log."""
    pid = _pid()
    battery = _sh("dumpsys battery")
    level = re.search(r"level: (\d+)", battery)
    version = re.search(r"versionName=(\S+)", _sh(f"dumpsys package {PACKAGE}"))
    driver = re.findall(
        r"Active Vulkan driver metadata: (\{[^\n]+\})", _sh("logcat -d -t 4000 2>/dev/null")
    )
    busy = _sh(f"cat {KGSL}/gpubusy")
    return {
        "serial": _serial(),
        "model": _sh("getprop ro.product.model").strip(),
        "android": _sh("getprop ro.build.version.release").strip(),
        "package": PACKAGE,
        "version": version.group(1) if version else None,
        "pid": pid,
        "performance_mode": _setting("performance_mode"),
        "fan_mode": _setting("fan_mode"),
        "screen_brightness": _setting("screen_brightness"),
        "screen_brightness_mode": _setting("screen_brightness_mode"),
        "battery_level": int(level.group(1)) if level else None,
        "usb_powered": "USB powered: true" in battery,
        "ac_powered": "AC powered: true" in battery,
        "gpu_clock_mhz": _sh(f"cat {KGSL}/clock_mhz").strip(),
        "gpu_busy_percent": round(_gpubusy_percent(busy), 1),
        "vulkan_driver": driver[-1] if driver else None,
        "wakefulness": _wakefulness(),
        "displays": _display_ids(),
    }


@mcp.tool()
def setting_get(name: str, namespace: str = "system") -> str:
    """Read an Android settings value, for example performance_mode or fan_mode."""
    return _setting(name, namespace)


@mcp.tool()
def setting_put(name: str, value: str, namespace: str = "system") -> dict:
    """Write an Android settings value and return the previous value so it can be restored.
    Read and restore performance_mode, fan_mode and screen_brightness after every experiment."""
    previous = _setting(name, namespace)
    _sh(f"settings put {namespace} {name} {value}")
    return {"name": name, "previous": previous, "current": _setting(name, namespace)}


@mcp.tool()
def gpu(seconds: float = 2.0) -> dict:
    """GPU clock and busy percent over a sampling window read from the KGSL sysfs nodes."""
    out = _sh(
        f"cat {KGSL}/gpubusy; sleep {seconds}; cat {KGSL}/gpubusy; cat {KGSL}/clock_mhz",
        timeout=int(seconds) + 30,
    ).splitlines()
    return {
        "seconds": seconds,
        "busy_percent_start": round(_gpubusy_percent(out[0]), 1),
        "busy_percent_end": round(_gpubusy_percent(out[1]), 1),
        "clock_mhz": out[2].strip(),
    }


@mcp.tool()
def threads(seconds: int = 3) -> str:
    """Per-thread CPU use of the app over a window (top -H). NativeEmulation is the guest CPU
    thread; VulkanWorker and VulkanPresent are the renderer threads."""
    pid = _pid()
    if not pid:
        return f"{PACKAGE} is not running"
    out = _sh(
        f"top -H -p {pid} -n 2 -d {seconds} -b -o TID,%CPU,CMD 2>/dev/null", timeout=seconds + 30
    )
    rows = [line for line in out.splitlines() if re.match(r"\s*\d+\s+[\d.]+\s+", line)]
    half = len(rows) // 2
    return "\n".join(rows[half:]) if half else out


@mcp.tool()
def emulation_thread_time(seconds: int = 3) -> dict:
    """User and system CPU time, page faults and context switches of the NativeEmulation
    thread over a window. High system time with few context switches means long kernel calls."""
    pid = _pid()
    if not pid:
        raise RuntimeError(f"{PACKAGE} is not running")
    # Several threads carry the NativeEmulation name. The guest CPU thread is the one with
    # the most accumulated CPU time.
    listing = _sh(
        f"for t in /proc/{pid}/task/*; do n=$(cat $t/comm); "
        f"if [ \"$n\" = NativeEmulation ]; then cat $t/stat; fi; done; true"
    )
    best = None
    for line in listing.splitlines():
        if "(" not in line:
            continue
        tail = line[line.rfind(")") + 2:].split()
        total = int(tail[11]) + int(tail[12])
        if best is None or total > best[1]:
            best = (line.split()[0], total)
    if best is None:
        raise RuntimeError("No NativeEmulation thread found")
    tid = best[0]
    script = (
        f"cat /proc/{pid}/task/{tid}/stat; grep ctxt_switches /proc/{pid}/task/{tid}/status; "
        f"sleep {seconds}; "
        f"cat /proc/{pid}/task/{tid}/stat; grep ctxt_switches /proc/{pid}/task/{tid}/status"
    )
    out = _sh(script, timeout=seconds + 30).splitlines()
    stats = [line for line in out if line.startswith(f"{tid} (")]
    ctxt = [int(x) for x in re.findall(r"ctxt_switches:\s+(\d+)", "\n".join(out))]

    def fields(line: str) -> list[str]:
        return line[line.rfind(")") + 2:].split()

    a, b = fields(stats[0]), fields(stats[1])
    return {
        "tid": int(tid),
        "seconds": seconds,
        "user_percent_of_core": round((int(b[11]) - int(a[11])) / seconds, 1),
        "sys_percent_of_core": round((int(b[12]) - int(a[12])) / seconds, 1),
        "minor_faults": int(b[7]) - int(a[7]),
        "major_faults": int(b[9]) - int(a[9]),
        "voluntary_switches": ctxt[2] - ctxt[0],
        "nonvoluntary_switches": ctxt[3] - ctxt[1],
    }


# ----------------------------------------------------------------------------
# Tools: Azahar configuration
# ----------------------------------------------------------------------------

@mcp.tool()
def config_read() -> str:
    """Return the device config.ini text."""
    return _read_remote_text(f"{USER_DIR}/config/config.ini")


@mcp.tool()
def config_set(assignments: dict[str, str], allow_running: bool = False) -> dict:
    """Set config.ini keys. Keys use 'Section.key' form, for example
    {"Renderer.resolution_factor": "4", "Layout.performance_overlay_show_speed": "true"}.
    A backup of the previous file is written to the capture folder and its path is returned."""
    _require_stopped(allow_running)
    remote = f"{USER_DIR}/config/config.ini"
    before = _read_remote_text(remote)
    backup = _capture_path("config-before", ".ini")
    backup.write_text(before, encoding="utf-8", newline="\n")
    after = _apply_assignments(before, assignments)
    _write_remote_text(remote, after)
    return {"backup": str(backup), "changed": assignments}


@mcp.tool()
def game_settings_read(title_id: str) -> str:
    """Return GameSettings/<title id>.ini for a title, or an empty string if none exists."""
    try:
        return _read_remote_text(f"{USER_DIR}/GameSettings/{title_id.upper()}.ini")
    except RuntimeError:
        return ""


@mcp.tool()
def game_settings_set(
    title_id: str, assignments: dict[str, str], allow_running: bool = False
) -> dict:
    """Set per-title override keys in GameSettings/<title id>.ini using 'Section.key' form.
    Only keys present in the file override config.ini for that title. Note: the GPU applies
    Compatibility.skip_texture_copy_fallback from the built-in hack list, not from this file."""
    _require_stopped(allow_running)
    remote = f"{USER_DIR}/GameSettings/{title_id.upper()}.ini"
    before = game_settings_read(title_id)
    after = _apply_assignments(before, assignments)
    _write_remote_text(remote, after)
    return {"file": remote, "previous_text": before, "changed": assignments}


@mcp.tool()
def game_settings_delete(title_id: str) -> str:
    """Delete GameSettings/<title id>.ini for a title."""
    remote = f"{USER_DIR}/GameSettings/{title_id.upper()}.ini"
    _sh(f"rm -f '{remote}'")
    return f"removed {remote}"


# ----------------------------------------------------------------------------
# Tools: launch, input, capture
# ----------------------------------------------------------------------------

def _wakefulness() -> str:
    m = re.search(r"mWakefulness=(\w+)", _sh("dumpsys power"))
    return m.group(1) if m else "unknown"


def _wake() -> dict:
    """Wake the panels and dismiss the keyguard. On a sleeping device the activity starts in the
    stopped state: black panels, no surface, no emulation thread."""
    before = _wakefulness()
    if before != "Awake":
        _sh("input keyevent KEYCODE_WAKEUP")
        for _ in range(10):
            if _wakefulness() == "Awake":
                break
            _sh("sleep 0.5", timeout=10)
    if "isKeyguardShowing=true" in _sh("dumpsys window"):
        _sh("wm dismiss-keyguard")
    return {"before": before, "after": _wakefulness()}


@mcp.tool()
def launch(rom_path: str = "", content_uri: str = "", wait_seconds: int = 0) -> dict:
    """Launch a game. rom_path is relative to THOR_ROM_TREE, for example
    'zcci/Medabots9-KWG-1007 [0004000000174F00] [UNK].zcci'. The URI must use the tree form
    the app holds a persisted grant for; a plain document URI crashes the app with a
    SecurityException. content_uri overrides rom_path. Wakes the panels first. Returns the pid
    after wait_seconds."""
    if not content_uri:
        if not rom_path:
            raise ValueError("Pass rom_path or content_uri")
        volume, tree_path = ROM_TREE.split(":", 1)
        tree = urllib.parse.quote(f"{volume}:{tree_path}", safe="")
        document = urllib.parse.quote(f"{volume}:{tree_path}/{rom_path}", safe="")
        content_uri = f"content://{DOCS_AUTHORITY}/tree/{tree}/document/{document}"
    woke = _wake()
    out = _sh(
        f"am start -W -a android.intent.action.VIEW -d '{content_uri}' "
        f"-t application/octet-stream -n {PACKAGE}/{ACTIVITY}",
        timeout=60,
    )
    if wait_seconds:
        _sh(f"sleep {wait_seconds}", timeout=wait_seconds + 30)
    return {
        "uri": content_uri,
        "wake": woke,
        "am_start": out.strip().splitlines()[-2:],
        "pid": _pid(),
    }


@mcp.tool()
def stop() -> str:
    """Force-stop the app."""
    _sh(f"am force-stop {PACKAGE}")
    return f"stopped {PACKAGE}"


@mcp.tool()
def press(button: str, hold: bool = True) -> str:
    """Press a 3DS button: A B X Y L R ZL ZR START SELECT UP DOWN LEFT RIGHT, or BACK / HOME.
    The guest samples input once per frame, so a plain tap is usually missed. hold=True sends a
    long press, which the guest sees."""
    keycode = BUTTON_KEYCODES.get(button.upper())
    if not keycode:
        raise ValueError(f"Unknown button {button!r}. Use one of {sorted(BUTTON_KEYCODES)}")
    flag = "--longpress " if hold else ""
    _sh(f"input keyevent {flag}{keycode}")
    return f"sent {keycode}{' (held)' if hold else ''}"


@mcp.tool()
def tap(x: int, y: int, display: int = 0) -> str:
    """Tap a screen position. display 0 is the primary panel, display 4 is the secondary panel
    on the Thor, which shows the bottom 3DS screen."""
    _sh(f"input -d {display} tap {x} {y}")
    return f"tapped {x},{y} on display {display}"


@mcp.tool()
def screenshot(display: str = "primary", name: str = "") -> str:
    """Save a PNG of the primary or secondary panel to the capture folder and return its path."""
    ids = _display_ids()
    if display not in ids:
        raise ValueError(f"Unknown display {display!r}; found {ids}")
    path = _capture_path(name or display, ".png")
    data = _adb(["-s", _serial(), "exec-out", "screencap", "-p", "-d", ids[display]], binary=True)
    path.write_bytes(data)
    return str(path)


@mcp.tool()
def fps() -> dict:
    """Frame pacing of the game's primary-panel layer from SurfaceFlinger's last 127 presented
    frames: mean FPS, mean, median, P95 and maximum interval in milliseconds."""
    layer = _blast_layer()
    raw = _sh(f"dumpsys SurfaceFlinger --latency '{layer}'")
    lines = [line.strip() for line in raw.splitlines() if line.strip()]
    stamps = set()
    for line in lines[1:]:
        parts = line.split()
        if len(parts) >= 3 and 0 < int(parts[1]) < (1 << 62):
            stamps.add(int(parts[1]))
    stamps = sorted(stamps)
    intervals = sorted((b - a) / 1e6 for a, b in zip(stamps, stamps[1:]) if 0 < (b - a) < 1e9)
    if not intervals:
        return {"layer": layer, "frames": len(stamps), "note": "no presented frames in the window"}
    mean = statistics.mean(intervals)
    return {
        "layer": layer,
        "refresh_period_ms": round(int(lines[0]) / 1e6, 2),
        "frames": len(stamps),
        "mean_fps": round(1000 / mean, 1),
        "mean_ms": round(mean, 2),
        "median_ms": round(intervals[len(intervals) // 2], 2),
        "p95_ms": round(intervals[max(0, int(len(intervals) * 0.95) - 1)], 2),
        "max_ms": round(intervals[-1], 2),
    }


@mcp.tool()
def logcat(lines: int = 200, pattern: str = "") -> str:
    """Return the last log lines, filtered by a regular expression if given."""
    out = _sh(f"logcat -d -t {lines}", timeout=60)
    if pattern:
        regex = re.compile(pattern)
        out = "\n".join(line for line in out.splitlines() if regex.search(line))
    return out


@mcp.tool()
def frame_profile(lines: int = 4000) -> str:
    """Return the latest whole-frame counter report. It exists only in a build made with
    -PthorFrameProfiling=true, which logs a window every 300 swaps."""
    out = _sh(f"logcat -d -t {lines}", timeout=60)
    rows = [line for line in out.splitlines() if "frame_profile.cpp" in line]
    if not rows:
        return "No frame-profile report in the log. Use a profiling build."
    return "\n".join(rows[-40:])


# ----------------------------------------------------------------------------
# Tools: files and packages
# ----------------------------------------------------------------------------

@mcp.tool()
def install(apk_path: str) -> str:
    """Install an APK over the existing app (adb install -r -d). Downgrades are allowed so that a
    control build can be reinstalled for a before/after measurement."""
    out = _adb(["-s", _serial(), "install", "-r", "-d", apk_path], timeout=600)
    version = re.search(r"versionName=(\S+)", _sh(f"dumpsys package {PACKAGE}"))
    return f"{out.strip()} version={version.group(1) if version else '?'}"


@mcp.tool()
def pull(remote: str, local: str = "") -> str:
    """Copy a file from the device into the capture folder, or to local if given."""
    target = Path(local) if local else _capture_path(Path(remote).name, "")
    target.parent.mkdir(parents=True, exist_ok=True)
    _adb(["-s", _serial(), "pull", remote, str(target)], timeout=600)
    return str(target)


@mcp.tool()
def push(local: str, remote: str) -> str:
    """Copy a local file to the device."""
    _adb(["-s", _serial(), "push", local, remote], timeout=600)
    return f"pushed {local} -> {remote}"


@mcp.tool()
def app_maintenance(op: str = "list", zip_name: str = "", wait_seconds: int = 25) -> str:
    """Run one maintenance operation inside the app on its private driver directories and return
    the JSON result. The app reads thor_maintenance.txt from the user directory at startup, runs
    the operation, writes log/thor_maintenance.json, and deletes the request. Nothing outside the
    app's own storage is touched. Ops: list (private files tree), verify (hash the extracted GPU
    driver against its zip), clear_redirect (delete the driver's file redirect directory),
    reinstall_driver (re-extract the selected or the given zip from gpu_drivers), system_driver
    (switch to the system Vulkan driver). The app is stopped before and after."""
    ops = {"list", "verify", "clear_redirect", "export_redirect", "reinstall_driver", "system_driver"}
    if op not in ops:
        raise ValueError(f"op must be one of {sorted(ops)}")
    stop()
    result_remote = f"{USER_DIR}/log/thor_maintenance.json"
    _sh(f"rm -f '{result_remote}'")
    _write_remote_text(f"{USER_DIR}/thor_maintenance.txt", op + ("\n" + zip_name if zip_name else "") + "\n")
    _sh(f"am start -W -n {PACKAGE}/org.citra.citra_emu.ui.main.MainActivity", timeout=60)
    deadline = time.time() + wait_seconds
    text = ""
    while time.time() < deadline:
        try:
            text = _read_remote_text(result_remote)
            if text.strip():
                break
        except RuntimeError:
            pass
        time.sleep(1)
    stop()
    if not text.strip():
        raise RuntimeError("the app wrote no result; check that the user directory is granted")
    return text


def _dismiss_savestate_dialog() -> bool:
    """A save state records the emulator build that wrote it. After a rebuild the app asks
    whether to load it anyway. Answer once so an experiment is not left sitting on a dialog."""
    _sh("sleep 1.5", timeout=10)
    for _ in range(3):
        try:
            nodes = ui_dump(display=0, max_nodes=60)
        except Exception:
            return False
        labels = {str(node.get("text", "")).strip() for node in nodes}
        if "Continue" in labels and any("avestate" in label for label in labels):
            try:
                ui_tap("Continue")
                return True
            except Exception:
                return False
        if "Continue" not in labels:
            return False
        _sh("sleep 0.5", timeout=10)
    return False


@mcp.tool()
def emu_command(command: str = "perf", argument: str = "", wait_seconds: int = 4) -> str:
    """Send one command to the running game and return the JSON result.

    The app polls thor_command.txt in the user directory once per second while a game runs and
    writes log/thor_command.json. The game must be running; nothing outside the user directory
    is touched. Commands:
      save_state <slot>   write a save state, so an experiment can return to a scene in seconds
      load_state <slot>   load a save state
      states              list the slots that hold a state
      perf                return the performance numbers only
      perf_log on|off     log the performance numbers to logcat once per second
    Every result carries "perf": game FPS, speed percent, and the frame time split in
    milliseconds (frame_time, svc, ipc, gpu_cmd, swap, remaining)."""
    allowed = {"save_state", "load_state", "states", "perf", "perf_log"}
    if command not in allowed:
        raise ValueError(f"command must be one of {sorted(allowed)}")
    if _pid() is None:
        raise RuntimeError("The app is not running. Launch a game first.")
    result_remote = f"{USER_DIR}/log/thor_command.json"
    request_id = f"{time.time():.3f}"
    payload = command + chr(10) + argument + chr(10) + request_id + chr(10)
    _write_remote_text(f"{USER_DIR}/thor_command.txt", payload)
    deadline = time.time() + max(wait_seconds, 3)
    while time.time() < deadline:
        text = _read_remote_text(result_remote).strip()
        if text.startswith("{"):
            try:
                parsed = json.loads(text)
            except ValueError:
                parsed = {}
            # The app echoes the request id, so a result left over from an earlier command is
            # never mistaken for this one.
            if parsed.get("id") == request_id:
                if command == "load_state" and parsed.get("loaded"):
                    _dismiss_savestate_dialog()
                return text
        _sh("sleep 0.5", timeout=10)
    raise RuntimeError(
        "No result. The game must be running with the emulation screen in front; the app polls "
        "once per second."
    )


@mcp.tool()
def perf_stats(samples: int = 3, interval: float = 1.0) -> dict:
    """Read the emulator's own performance numbers from the running game, averaged over samples.

    Returns game FPS, speed percent, and the frame time split in milliseconds. Speed percent is
    the number that says whether a scene runs at full speed; the frame time split says where a
    slow frame goes (gpu_cmd is guest command processing, swap includes waiting for the host
    GPU). Prefer this over reading the on-screen overlay from a screenshot."""
    rows = []
    for index in range(max(samples, 1)):
        if index:
            _sh(f"sleep {interval}", timeout=int(interval) + 10)
        rows.append(json.loads(emu_command("perf"))["perf"])
    keys = sorted({key for row in rows for key in row})
    return {
        "samples": len(rows),
        "mean": {key: round(sum(row.get(key, 0.0) for row in rows) / len(rows), 3) for key in keys},
        "rows": rows,
    }


@mcp.tool()
def driver_env(assignments: dict[str, str] | None = None) -> str:
    """Set environment variables for the GPU driver inside the app, for example
    {"TU_DEBUG": "nobin"} to switch off a Turnip feature. The app reads thor_driver_env.txt from
    the user directory before it loads the driver. An empty dict removes the file. Takes effect
    at the next launch."""
    remote = f"{USER_DIR}/thor_driver_env.txt"
    if not assignments:
        _sh(f"rm -f '{remote}'")
        return "driver environment cleared"
    text = "".join(f"{k}={v}\n" for k, v in assignments.items())
    _write_remote_text(remote, text)
    return f"driver environment set: {assignments}"


@mcp.tool()
def gpu_faults(max_bursts: int = 12) -> dict:
    """Count Adreno command-processor faults ('CP: AHB bus error') in the kernel log and map each
    burst to wall-clock time through the audit lines, which carry both clocks. A burst is a run
    of fault lines separated by more than 20 seconds. A GPU fault stalls the Vulkan swapchain and
    the app spins on 'dequeueBuffer timed out'."""
    now = _sh("date +%s; date +%H:%M:%S").split()
    epoch_now, local_now = float(now[0]), now[1]
    text = _sh("dmesg 2>/dev/null | grep -E 'AHB bus error|audit\(' | tail -6000")
    anchor = None
    for line in text.splitlines():
        m = re.match(r"\[\s*([0-9.]+)\].*audit\(([0-9.]+):", line)
        if m:
            anchor = (float(m.group(1)), float(m.group(2)))
    h, mi, sec = (int(x) for x in local_now.split(":"))
    local_secs = h * 3600 + mi * 60 + sec

    def wall(ts: float) -> str:
        if anchor is None:
            return f"kernel+{ts:.0f}s"
        l = (local_secs + (ts - anchor[0] + anchor[1] - epoch_now)) % 86400
        return "%02d:%02d:%02d" % (l // 3600, (l % 3600) // 60, l % 60)

    total = 0
    prev = None
    bursts: list[dict] = []
    for line in text.splitlines():
        if "AHB bus error" not in line:
            continue
        total += 1
        ts = float(re.match(r"\[\s*([0-9.]+)\]", line).group(1))
        if prev is None or ts - prev > 20:
            bursts.append({"start": wall(ts), "lines": 1})
        else:
            bursts[-1]["lines"] += 1
        prev = ts
    return {"total_lines": total, "bursts": bursts[-max_bursts:], "device_time": local_now}


@mcp.tool()
def ui_dump(display: int = 0, max_nodes: int = 80) -> list[dict]:
    """Dump the visible UI of a display with uiautomator and return the nodes that have text, a
    content description, or a resource id, with their center points. Use ui_tap to press one."""
    remote = "/sdcard/thor_ui_dump.xml"
    _sh(f"uiautomator dump --display-id {display} {remote} >/dev/null 2>&1 || uiautomator dump {remote} >/dev/null 2>&1", timeout=60)
    xml = _read_remote_text(remote)
    nodes = []
    for m in re.finditer(r"<node [^>]*?/?>", xml):
        attrs = dict(re.findall(r'(\w[\w-]*)="([^"]*)"', m.group(0)))
        text = attrs.get("text", "")
        desc = attrs.get("content-desc", "")
        rid = attrs.get("resource-id", "")
        if not (text or desc or rid):
            continue
        b = re.match(r"\[(\d+),(\d+)\]\[(\d+),(\d+)\]", attrs.get("bounds", ""))
        center = [(int(b.group(1)) + int(b.group(3))) // 2, (int(b.group(2)) + int(b.group(4))) // 2] if b else None
        nodes.append({"text": text, "desc": desc, "id": rid.split("/")[-1], "center": center,
                      "clickable": attrs.get("clickable") == "true"})
        if len(nodes) >= max_nodes:
            break
    return nodes


@mcp.tool()
def ui_tap(text: str, display: int = 0) -> str:
    """Find a UI node whose text, description, or resource id contains the given text (case
    insensitive) and tap its center. Raises if no node matches."""
    wanted = text.lower()
    for node in ui_dump(display, max_nodes=400):
        hay = " ".join([node["text"], node["desc"], node["id"]]).lower()
        if wanted in hay and node["center"]:
            x, y = node["center"]
            _sh(f"input -d {display} tap {x} {y}")
            return f"tapped '{node['text'] or node['desc'] or node['id']}' at {x},{y}"
    raise RuntimeError(f"no UI node matches '{text}'")


def _main() -> None:
    if "--list-tools" in sys.argv:
        for tool in asyncio.run(mcp.list_tools()):
            print(tool.name)
        return
    mcp.run()


if __name__ == "__main__":
    _main()

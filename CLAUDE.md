# CLAUDE.md

[AGENTS.md](AGENTS.md) is the engineering ledger for this fork. Read it before you change code.
Update it when behavior changes. It holds every accepted optimization, every rejected experiment
with its reason, and the invariants that must stay in place. This file holds the operating rules.
It does not repeat the ledger.

## Documentation map

- [AGENTS.md](AGENTS.md): engineering rules and invariants. Canonical.
- [docs/thor-optimization-notes.md](docs/thor-optimization-notes.md): dated evidence behind the rules.
- [docs/thor-cheat-gaps.md](docs/thor-cheat-gaps.md): cheat coverage gaps.
- [README.md](README.md): public description of the fork.
- [AI-POLICY.md](AI-POLICY.md): how AI assistance is used here.
- [.claude/commands/goal.md](.claude/commands/goal.md): the current performance goal and its
  procedure. Run it with `/goal`.
- [tools/thor-mcp/server.py](tools/thor-mcp/server.py): the MCP server that controls the Thor.
  See "Device control" below.

When behavior changes, update the rule in AGENTS.md and the evidence in the notes. Do not copy
engineering detail into README.md or this file. Link to AGENTS.md instead.

## Writing standard for all English

This standard applies to every English text written in this workspace: documentation, commit
messages, code comments, ledger entries, and replies to the user.

Base rule: write in ASD-STE100 Simplified Technical English where the vocabulary allows it. Use the
active voice. Give one instruction per sentence. Use simple approved words. Keep a procedural
sentence at or below 20 words. Keep a descriptive sentence at or below 25 words.

The rules below add to the ASD-STE100 rule. They do not replace it.

- Use literal, plain, and direct language. State facts and concepts exactly as they are.
- Do not use metaphors, similes, analogies, or idioms. Examples of banned words: "journey",
  "tapestry", "navigating", "beacon", "dive in", "landscape".
- Do not use AI buzzwords, hype, or flowery adjectives.
- Keep sentences short. Put one idea in each sentence. Order the sentences so that each one
  follows from the one before it.
- Put clarity and precision before style.

## Device control

The Thor is controlled through the `thor` MCP server declared in [.mcp.json](.mcp.json). The server
wraps ADB. List its tools with `python tools/thor-mcp/server.py --list-tools`. Use a tool instead
of a raw `adb` command when a tool exists.

- `state` reads the device settings, the app version, the GPU clock, and the active Vulkan driver.
  Call it first and last. Restore `performance_mode`, `fan_mode`, and `screen_brightness` with
  `setting_put` after an experiment.
- `config_read`, `config_set`, `game_settings_read`, `game_settings_set`, and
  `game_settings_delete` edit the ini files in the user directory. `config_set` refuses to write
  while the app runs, and it keeps a backup.
- `launch` starts a title from the granted ROM tree. `stop` force-stops the app.
- `press` sends a held button press. The guest reads input once per frame, so a plain tap is
  missed.
- `screenshot`, `fps`, `gpu`, `threads`, and `emulation_thread_time` measure a running scene.
  `frame_profile` reads the whole-frame counters of a profiling build.
- `gpu_faults` counts the kernel's `CP: AHB bus error` lines and maps each burst to wall-clock
  time. Call it before and after every Vulkan measurement. A run with a new burst is void.
- `app_maintenance` runs one operation inside the app on its private driver directories: list,
  verify, clear_redirect, reinstall_driver, or system_driver. The app reads the request from
  `thor_maintenance.txt` in the user directory at startup and writes `log/thor_maintenance.json`.
- `ui_dump` and `ui_tap` read the visible UI of a panel and press a node by its text.
- `install`, `pull`, `push`, `shell`, and `logcat` cover the rest.

When a test needs a device capability that no tool provides, add a tool to the server and record
it here. Do not add an exported Android component for that; the request file above is the
channel into the app.

The tools depend on these facts: package `org.azahar_emu.azahar.debug`, user directory
`/storage/emulated/0/Azaharuser`, ROM tree `2664-21DE:Roms/n3ds`, USB serial `c3ca0370`. Change
them in `.mcp.json` when the device changes.

## Finishing a task

Cleanup is part of finishing. Before you hand work back, remove the stale CMake configuration
hashes, Gradle intermediates, and scratch artifacts that you created, in the repository and on the
device. Report the bytes reclaimed. AGENTS.md lists the exact storage rules and the safe paths.
Never run a broad sweep that could reach source, saves, manuals, research copies, or unrelated
user files.

## Working agreements

- Work on `master`. Push to `origin/master` in small verified commits. Use command-line Git over
  SSH. Do not use PR automation or the GitHub CLI unless asked.
- Never commit generated Gradle, CMake, or APK output.
- Build the device APK with
  `.\gradlew.bat :app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` from
  `src/android`. Do not pass `--configuration-cache`.
- Add `-PthorFrameProfiling=true` for a profiling build. Never use a profiling build for an FPS,
  power, or thermal claim.
- Always pass `adb -s <serial>` when you use raw ADB. The Thor can enumerate over USB and Wi-Fi.
- Read and restore the user's performance mode, fan mode, brightness, GPU driver, and resolution
  after any experiment. Do not change a device setting without recording it first.
- Separate what was measured from what it proves. An isolated ratio is not an FPS or battery-watt
  claim.

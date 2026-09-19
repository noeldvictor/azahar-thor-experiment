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
- [docs/goal.md](docs/goal.md): the current performance goal, its premise and its procedure.
  `/goal` is Claude Code's built-in command, not ours: give it a completion condition that
  points at this file. Do not add a `goal` command under `.claude/commands/`; it would shadow
  the built-in.
- [docs/goal_history/](docs/goal_history/): a dated snapshot of each goal we have set, so the
  goals and what they produced can be read back. Add one whenever the goal changes.
- [tools/thor-mcp/server.py](tools/thor-mcp/server.py): the MCP server that controls the Thor.
  See "Device control" below.
- [tools/turnip/](tools/turnip/): the bundled Turnip driver. `build.sh` builds Mesa with the
  patches in `patches/` in WSL Ubuntu and packages the zip that lives under
  `src/android/app/src/main/assets/gpu_drivers/`. Rebuild it when a patch changes.

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
- `launch` starts a title from the granted ROM tree. It wakes the panels first: on a sleeping
  device the activity starts stopped, with black panels and no emulation thread. `state`
  reports `wakefulness`. `stop` force-stops the app.
- `press` sends a held button press. The guest reads input once per frame, so a plain tap is
  missed.
- `screenshot`, `fps`, `gpu`, `threads`, and `emulation_thread_time` measure a running scene.
  `frame_profile` reads the whole-frame counters of a profiling build.
- `gpu_faults` counts the kernel's `CP: AHB bus error` lines and maps each burst to wall-clock
  time. Call it before and after every Vulkan measurement. A run with a new burst is void.
- `app_maintenance` runs one operation inside the app on its private driver directories: list,
  verify, clear_redirect, reinstall_driver, or system_driver. The app reads the request from
  `thor_maintenance.txt` in the user directory at startup and writes `log/thor_maintenance.json`.
- `emu_command` sends one command to the running game: `save_state`, `load_state`, `states`,
  `perf`, or `perf_log`. The app polls `thor_command.txt` in the user directory once per second
  while the emulation screen is in front and writes `log/thor_command.json`. Save a state at the
  scene you measure, then load it; a replay to a late scene costs seven minutes and a load costs
  seconds.
- `allow_savestate_mismatch` in the Utility settings loads a save state written by a different
  build, so a change can be measured against the same scene before and after a rebuild. Set it
  with `config_set`. It is off by default; a state whose format really changed will crash, so it
  is for testing only.
- `perf_stats` returns the emulator's own numbers, averaged over samples: game FPS, speed
  percent, and the frame time split in milliseconds (`gpu_cmd` is guest command processing,
  `swap` includes waiting for the host GPU). Use it instead of reading the overlay from a
  screenshot. Speed percent is the number that decides whether a scene runs at full speed.
- `ui_dump` and `ui_tap` read the visible UI of a panel and press a node by its text.
- `driver_env` sets environment variables for the GPU driver at the next launch through
  `thor_driver_env.txt` in the user directory, for example `TU_DEBUG` flags for Turnip.
- `install`, `pull`, `push`, `shell`, and `logcat` cover the rest.

When a test needs a device capability that no tool provides, add a tool to the server and record
it here. Do not add an exported Android component for that; the request file above is the
channel into the app.

The tools depend on these facts: package `org.azahar_emu.azahar.debug`, user directory
`/storage/emulated/0/Azaharuser`, ROM tree `2664-21DE:Roms/n3ds`, USB serial `c3ca0370`. Change
them in `.mcp.json` when the device changes.

## Per-title settings

`src/android/app/src/main/assets/game_profiles/<title id>.ini` is the per-title settings
database. The app copies each file into `GameSettings/` on first run and never overwrites a file
the user already has, and native code overlays it over `config.ini` at launch for that session.
Overlays are sparse: only the keys in the file change. A user edits the same values in the app by
long-pressing a game and opening **Game Settings**.

When a measurement finds the setting a title wants, record it in that title's file with the scene
and the numbers behind it, and note it in the notes. That is how a finding reaches users instead
of staying in a log.

## Performance expectation on the Thor

The 3DS GPU is a 268 MHz PICA200 that draws 400x240 and 320x240 frames. The Thor's Adreno 740
has hundreds of times that throughput. A 3DS scene at 2x or 3x that keeps the Adreno 740 near
100% busy, or that draws 6 to 7 W, is an emulator inefficiency, never a hardware limit. Treat
GPU busy percent at a fixed frame rate as the primary efficiency number, and treat any scene
that cannot hold full speed at 2x as a bug to find in the render path: pass restarts, tile
loads and stores, needless copies, needless downloads. Do not accept it as the game's cost.

Fast forward is part of that expectation. At 2x resolution every scene must reach 200% speed
with the per-title frame limit at 200. A scene that cannot is a bug with the same causes, and
the GPU busy percent at 100% speed predicts it: 68% busy at 100% speed means the GPU cannot
give 200%. Measure fast forward with the speed overlay on (`Layout.performance_overlay_show_speed`)
and read the guest FPS, the speed, the GPU busy percent, and the emulation thread share. The
panels are pinned at 60 Hz, so the screen shows at most 60 frames per second; fast forward is
game speed, not frame rate.

## Open work

- HD texture mode, not yet evaluated. The pieces exist: `custom_textures`, `preload_textures`,
  `dump_textures`, and `async_custom_loading` in the Utility settings, with
  `CustomTexManager` reading `load/textures/<title id>/` and writing
  `dump/textures/<title id>/`. What is missing is a measurement on the Thor of what a pack
  costs in memory, load time, and frame rate at 2x, and whether preloading or streaming is
  right for this device. Decide that before recommending the mode to users.
- HD texture paths should sit on internal storage. Dumping and loading through the granted
  user directory goes over the storage access framework for every file, which is slow for the
  thousands of small files a pack contains. Measure internal storage against the user
  directory before choosing.
- A hotkey that reloads custom textures during play. The hotkeys are an enum in
  `features/hotkeys/Hotkey.kt` with the next free id after `TOP_SCREEN_STRETCH(10010)`, plus a
  case in `HotkeyUtility`. Reloading in place lets a pack be iterated on without restarting the
  game, which is what makes authoring a pack practical.
- An endpoint and a shape for texture packs, so a pack can be fetched and installed rather than
  copied by hand. Define the manifest first: title id, pack name, version, hash per file, and
  the texture naming scheme `CustomTexManager` already expects. Keep the shape stable before
  anything downloads from it, and never install a pack the user did not ask for.

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

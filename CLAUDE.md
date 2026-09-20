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
- `thermals` reads the device temperatures and the current frequency caps. Call it before and
  after a measurement. A reading only counts when the part was not throttling, and a CPU whose
  `scaling_max_freq` has fallen below its rated peak is throttling even when the GPU clock is
  still at 680 MHz.
- `cpu_profile` records a CPU profile of the running app and returns the hottest symbols. Use it
  to find where frame time goes before you write a NEON or ARM64 change. The event is the
  software clock, because this kernel refuses hardware counters. Symbols come from the
  unstripped library in the build tree, so profile a build you still have output for.
- `bench` is the measurement loop in one call: launch a title, load a save state, take perf
  samples, capture the scene and record the GPU clock range. It reports `throttled` when any
  sample fell below 615 MHz, which voids a run. Prefer it over a throwaway script, so every
  result has the same shape and can be compared with an earlier one. It finds the ROM for a
  title id by reading the `.zcci` headers, so a title id is enough.
- `shader_use` lists the fragment shader fingerprints the frame spends its draws on, with how
  many of each shader's draws got a coarse shading rate. Needs a profiling build.
- `shader_rules` sets the per-title draw rules that target those fingerprints. See the section
  on targeting individual draws below; rules apply at the next launch with no rebuild.
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

The server lives at `azahar/tools/thor-mcp/server.py`. Use that absolute path: the working
directory is sometimes the workspace root and a relative path silently fails there.

Rules for driving the device, learned the hard way on 2026-09-19:

- **Never navigate the game with button presses.** The user plays; the emulator is only
  measured. Blind repeated presses skip scenes and dialogue and land the game somewhere nobody
  intended, and every reading taken that way was void. A single `press` to answer a prompt the
  user asked about is fine.
- **Never wait for the boot videos to play out.** They run for minutes and a save state load is
  ignored while they do. Ask the user to bring the game to the scene, or to load the state from
  the in-game menu, then measure.
- **A save state restores across builds only with `allow_savestate_mismatch` set.** Without it
  the core refuses the state and raises a dialog whose "Continue" button closes the message
  without retrying, so the game stays where it was. `emu_command load_state` reports
  `"loaded": false` and names the dialog when that happens. Never treat a request as a result.
- **A new setting must also be read in `src/android/app/src/main/jni/config.cpp`.** Declaring it
  in `settings.h`, `GenerateSettingKeys.cmake` and `default_ini.h` only makes the key legal. A
  key that is never read keeps its built-in default, the ini value is ignored in silence, and an
  A/B test built on it compares a build against itself.
- **Confirm the scene with a screenshot before recording a number.** A video or a cutscene reads
  as full speed with the GPU near idle and looks like a result.

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

## What resolution the Thor needs

The main panel is 1920x1080 and the emulator draws the 3DS top screen at 1800x1080 inside it,
measured from a screenshot. The 3DS top screen is 400x240, so native 1:1 for this panel is
1080 / 240 = 4.5x. That sets what each setting looks like:

| resolution | rendered | scaling to panel | result |
| --- | --- | --- | --- |
| 2x | 800x480 | upscaled 2.25x | visibly soft |
| 3x | 1200x720 | upscaled 1.5x | good with a sharpening filter |
| 4x | 1600x960 | upscaled 1.125x | near native |
| 5x | 2000x1200 | downscaled 0.9x | supersampled, sharpest |

**Target 3x to 4x for this device, not 2x.** 2x is the one setting that cannot look right on this
panel. Prefer 3x with `screen_filter = 2`, which is Snapdragon Game Super Resolution: it runs
once on the final present and measured free, 64.10% against 63.73% without it in the E.X.
Troopers snow field, and it recovers most of the sharpness 4x would give at 3x cost. Use 4x when
a title has the headroom, and 5x when both panels are in use so each downscales instead of
stretching.

Write any new performance goal against 3x or 4x. A goal written against 2x optimises for a
picture nobody should be looking at on this hardware.

## Performance expectation on the Thor

The 3DS GPU is a 268 MHz PICA200 that draws 400x240 and 320x240 frames. The Thor's Adreno 740
has hundreds of times that throughput. A 3DS scene at 2x or 3x that keeps the Adreno 740 near
100% busy, or that draws 6 to 7 W, is an emulator inefficiency, never a hardware limit. Treat
GPU busy percent at a fixed frame rate as the primary efficiency number, and treat any scene
that cannot hold full speed at 2x as a bug to find in the render path: pass restarts, tile
loads and stores, needless copies, needless downloads. Do not accept it as the game's cost.

Calibrated on 2026-09-20, because that rule was written from one scene and is too strong as
stated. At 3x with GSR and no frame limit, Ocarina of Time 3D reads 1213% and Kirby Triple
Deluxe 1208%, and Ocarina of Time 3D still holds 190% at 8x. So the emulator has about twelve
times the headroom it needs at 3x on a title that draws its scene once or twice, and clears the
panel's native 4.5x comfortably. **Before concluding anything about the emulator from a slow
scene, measure a second title.** A scene can be expensive because the game is expensive: the
E.X. Troopers snow field shades 1800 draws a frame, every one of them blended, at about 58x
overdraw, and that is the blizzard rather than anything the emulator added. Keep the principle
that a shortfall is a named, measured cost rather than a hardware limit; drop the assumption
that the cost is always ours.

Fast forward is part of that expectation. At 2x resolution every scene must reach 200% speed
with the per-title frame limit at 200. A scene that cannot is a bug with the same causes, and
the GPU busy percent at 100% speed predicts it: 68% busy at 100% speed means the GPU cannot
give 200%. Measure fast forward with the speed overlay on (`Layout.performance_overlay_show_speed`)
and read the guest FPS, the speed, the GPU busy percent, and the emulation thread share. The
panels are pinned at 60 Hz, so the screen shows at most 60 frames per second; fast forward is
game speed, not frame rate.

## Targeting individual draws, instead of guessing in the core

When a scene is slow because of what the game draws, do not write a heuristic in the render path
to guess which draws are expendable. It will be wrong: on 2026-09-20 a rule of "blended and does
not write depth" caught this game's ink outlines along with its fog, because an outline and a
sheet of fog have identical render state. **Render state cannot tell a soft effect from a sharp
one. A fingerprint can.** This is how Dolphin does it too, with graphics mods that name a texture
hash rather than a state combination.

The layer is `shader_shading_rules` in a per-title ini: comma separated `<fingerprint>:<rate>`
pairs, where the fingerprint is `PicaFSConfig::Hash()`, the hash of the PICA combiner, lighting,
fog and alpha test setup, and the rate is 2 or 4 for 2x2 or 4x4 coarse shading through
`VK_KHR_fragment_shading_rate`. A draw whose fingerprint is not listed is untouched, so the
default path stays bit-identical. Rules are read at launch, so an experiment needs no rebuild.

The loop, all through the `thor` MCP server:

1. `shader_use` lists the fingerprints the frame spends its draws on. Needs a profiling build.
2. `shader_rules` sets a rule for the title.
3. `bench` launches, loads the save state, samples and screenshots in one call.

Draw count is not cost. A shader with thousands of cheap draws can matter less than one with a
few draws that cover the screen, so rank candidates by measuring with `bench`, never by the
count `shader_use` reports.

## A change that alters what is drawn is checked in more than one scene

Speed can be judged from one save state. Correctness cannot. On 2026-09-20 a set of draw rules was
tuned in the E.X. Troopers snow field, measured carefully, and shipped as that title's default: it
held 99.66% at 3x against 63.79%, with the blizzard, lighting, ink outlines and scene brightness
all intact and verified by measuring mean image brightness rather than by eye. The first time the
game moved past that save state it was missing its weapon icons and some of its text, and showed
the alpha-test checkerboard through surfaces that should blend.

**A fingerprint identifies a shader, not a purpose.** The same fragment shader configuration that
draws a sheet of blowing snow draws an interface element somewhere else, because both are a
blended textured quad with the same combiner setup. Nothing in a `PicaFSConfig` hash says what a
material is for. The bisection was sound, and its conclusion was only ever "safe in this scene".

So: anything that changes what is drawn ships off by default until it has been seen in several
scenes, including a menu and a heads-up display. If only one scene is reachable, it ships off.

## When instrumentation disagrees with a measurement, suspect the instrumentation

Twice on 2026-09-20 a counter told a clean story that was wrong. `ThorShaderUse` recorded the
last shading rate seen for a fingerprint instead of counting coarsened draws, so a shader used by
both a coarse and a sharp draw reported whichever came last, and the log appeared to say that two
shaders carried a twelve point speed gain. Applying rules to exactly those two moved nothing,
which is what exposed it. A `simpleperf` report filtered on `EmuThread` returned zero samples
because the thread is called `NativeEmulation`. Check that a surprising counter agrees with an
independent measurement before building on it.

## Where to go next

Ranked, with the reason rather than just the task.

1. **Check the E.X. Troopers draw rules across scenes.** They are measured, real, and currently
   shipped disabled because they break the interface outside the snow field. Finding which of the
   21 also draw the heads-up display would make them shippable. This needs someone to bring the
   game to a menu, a battle and a dialogue scene; it cannot be done from one save state.
2. **Raise the default resolution.** The panel wants 4.5x for a 1:1 top screen, the global default
   is 3x, and a normal title is nowhere near limited there: Ocarina of Time 3D reads 650% at 4x
   and 190% at 8x. Bumping the default and spot checking a handful of titles is the largest
   whole-library gain available for the least work.
3. **HD texture packs in ASTC, with a repo list.** The format layer already exists,
   `CustomPixelFormat` covers `ASTC4`, `ASTC6`, `ASTC8`, `BC1`, `BC3`, `BC5` and `BC7`, and
   `CustomFileFormat` covers PNG, DDS and KTX. What is unverified is whether the KTX and ASTC path
   loads end to end, and whether `preload_textures` handles a compressed format or assumes RGBA8.
   What is missing is the distribution side ARMSX2 has: a curated manifest so packs are
   discoverable and installable per title id instead of hand copied. ASTC matters on a handheld
   because it is hardware decoded and four to eight times smaller than RGBA8 in both memory and
   bandwidth. This improves how games look rather than how fast they run, and is independent of
   all the performance work.

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

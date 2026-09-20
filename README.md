# Azahar Thor Experiment

<p align="center">
  <img src="docs/media/branding/azahar-thor-experiment-banner.png" alt="Azahar Thor Experiment banner">
</p>

<p align="center">
  <img src="docs/media/branding/no-support-fork-it.svg" alt="No support. Fork it and do stuff yourself.">
</p>

A personal Android fork of [Azahar](https://github.com/azahar-emu/azahar) for the AYN Thor. It is
tuned around one handheld setup, a bundled cheat workflow, and local testing. It is not upstream
Azahar, not a release channel, and not a general-purpose support project.

> [!WARNING]
> This fork is vibe coded with AI assistance. That is intentional and disclosed in
> [AI-POLICY.md](AI-POLICY.md). If AI-assisted code, docs, or generated art bother you, use upstream
> Azahar or another fork.

> [!CAUTION]
> Personal-use experiment. No guarantee of stability, compatibility, correctness, performance,
> support, or future updates. No games, keys, BIOS, firmware, or copyrighted game content are
> included. Use your own legally dumped content.

## Scope

What this is:

- An Android-only Azahar fork built and tested on AYN Thor Base/Pro/Max.
- Branded `Azahar (Cheat Advanced)` on Android, shipped as release-optimized Thor APKs.
- A source-controlled set of bundled cheats and per-title profiles for one person's library.

What this is not:

- Not upstream Azahar, and not a supported emulator distribution.
- Not a compatibility reporting project.
- Not a place to request ROMs, keys, firmware, game files, or piracy help.
- Not a promise that any cheat, profile, or performance tweak works for your copy of a game.

## Target Hardware

All tuning assumes AYN Thor Base/Pro/Max: Snapdragon 8 Gen 2, Adreno 740, active cooling, and
LPDDR5X. AYN's product page and mirrored manual disagree about the UFS generation, so storage
tuning assumes neither until the physical device is verified. Thor Lite is a different Snapdragon
865 / Adreno 650 target and does not drive defaults unless explicitly called out.

For ordinary 30/60 FPS play, start in the Thor's **Standard** performance mode and move up only
when a title cannot hold speed. In a matched title-screen check, Standard produced the same pixels
and frame pacing as High Performance at a lower fixed GPU clock. That is an AC-powered operating
candidate, not a battery-watt result.

Accepted optimizations are recorded as numbered entries in the evidence ledger, and the entries
are not additive percentages. Whole-game FPS or battery watts still require a matched
title/scene/device A/B. Current performance work is selected from opt-in whole-frame counters
rather than further isolated instruction wins; profiling APKs carry timing overhead and are never
used for FPS, power, or thermal comparisons.

## What Changed From Upstream

### Packaging

- Android app label `Azahar (Cheat Advanced)` with a custom launcher icon and README branding.
- Builds are Android `arm64-v8a` only. The Thor target `:app:assembleVanillaRelWithDebInfoLite` is
  release-optimized, debug-signed, uses the `-thor` version suffix, and installs in the `.debug`
  package slot alongside stock Azahar.
- Upstream Azahar Android is integrated through commit `f6a3e3aa5`: Gradle 8.14.5, target SDK 37,
  and display-cutout margins on the emulation surface and in-game menu instead of opting out of
  enforced edge-to-edge behavior.

### Cheats

- Bundled cheats live under `src/android/app/src/main/assets/cheats/` and the game list marks
  titles that have one.
- The Cheats screen has **Find value**, a guarded memory search for offline single-player games.
  See [Cheats](#cheats) below.

### Display And Speed

- **Eco Turbo** defaults on and caps host presentation to 60 FPS during Turbo or fully uncapped
  emulation while the guest runs at the selected speed. Disable it under General for smoother
  fast-forward on the Thor's 120 Hz panel. Keep **Limit Speed** at 100% for normal play; the UI
  warns that disabling it can produce hundreds of FPS and high power draw.
- Emulation requests a 60 Hz refresh and a 60 Hz game-surface frame-rate hint so ordinary 3DS
  presentation is not tied to the panel's 120 Hz maximum. Frontend menus keep their high-refresh
  preference.
- Fast-forward controls use clearer labels, and toggle toasts report the active speed once instead
  of repeating when settings are merely reloaded.
- Graphics has a separate **Screen Filter** selector for the finished 3DS screens: opt-in
  **Anime4K v4 Mobile** (single-pass DoG) and **Snapdragon GSR 1**, vendored verbatim from
  Qualcomm's BSD-3-Clause shader for the Vulkan present path. **Texture Filter** still operates on
  game textures and is unrelated.
- Thor dual-display emulation is fixed to the top screen on the primary panel and the bottom screen
  on the secondary panel. The old hidden virtual-display fallback is removed.
- Long-press a game and choose **Manage Cached Data** to see that title's Vulkan and OpenGL
  shader-cache sizes or delete either one after confirmation. Downloaded custom-texture packs are
  user content and are never presented as disposable cache.

### Per-Game Settings

- Long-press a game and open **Game Settings** to override graphics, layout, audio, and system
  options for that title only. Overrides live in `GameSettings/<title id>.ini`, apply at launch
  without touching the global config, and store only the values you changed.
- Bundled profiles under `src/android/app/src/main/assets/game_profiles/` seed that folder on first
  run and never overwrite your edits. E.X. Troopers ships a compatibility profile; Conception II
  ships a crisp-presentation profile that renders at 5x so both Thor panels downscale instead of
  stretching.

### GPU Drivers

- The **Thor GPU Driver Manager** is a guided picker with visible download buttons and notes:
  generic Turnip first, recent Turnip rollback builds, Qualcomm and Turnip variants for
  troubleshooting, manual ZIP install, and system-driver fallback.

### Device Control

- `tools/thor-mcp/server.py` is an MCP server that controls the Thor over ADB. It reads device
  state, edits config.ini and per-title ini files, launches a title, sends held button presses,
  captures both panels, measures frame pacing from SurfaceFlinger, samples GPU busy and per-thread
  CPU time, and reads the log. It also counts kernel GPU faults (`gpu_faults`), drives the
  visible UI (`ui_dump`, `ui_tap`), and runs driver-directory maintenance inside the app
  (`app_maintenance`). `.mcp.json` starts it for Claude Code.
- `.claude/commands/goal.md` holds the current performance goal and its device procedure.

### Known Issue On The Thor: Turnip GPU Fault

- Since 2026-09-18 the Turnip Vulkan driver on the test unit faults the GPU at every launch
  (kernel line `kgsl-3d0: CP: AHB bus error`, a refused write to `RB_CCU_CNTL`). E.X. Troopers
  then freezes at the Capcom logo. The fault is in the driver, not in this fork: the unmodified
  build faults the same way, and OpenGL and the system Qualcomm Vulkan driver do not.
- Workaround: select the system GPU driver in the app. On it E.X. Troopers holds 60 FPS at the
  save-slot screen where Turnip gave 40 FPS. Expanded geometry draws stay in software on that
  driver because they hang it; they run on Turnip.
- The `gpu_faults` MCP tool counts the kernel lines. AGENTS.md has the evidence and the rules.

### Guards

- Missing or stale ROM entries stop before launch instead of continuing into emulation.
- Game equality compares real fields instead of treating hash collisions as equality.

### Under The Hood

The bulk of the diff is AArch64-specific work that is invisible in the UI: vendored Crypto++ with
working ARM64 ISA probes, AdvSIMD paths for Y2R video conversion, the PICA vertex-shader JIT and
command-list parser, texture decode and encode, Vulkan uploads and submission polling, SoundTouch
and HLE DSP audio, plus scheduler and presentation shortcuts. Each change is a rule in
[AGENTS.md](AGENTS.md) with its dated measurement in
[docs/thor-optimization-notes.md](docs/thor-optimization-notes.md). Isolated microbenchmark ratios
there are never whole-game FPS or battery-watt claims.

## Thor Screenshot

A live AYN Thor screenshot of this fork showing the game library and bundled-cheat labels. It does
not imply games are bundled with this repository.

![Azahar Android library on Thor showing cheat labels](docs/media/screenshots/azahar-library-cheats.png)

## Build

```powershell
cd src/android
.\gradlew.bat :app:assembleVanillaRelWithDebInfoLite --no-configuration-cache
```

APK output:

```text
src/android/app/build/outputs/apk/vanilla/relWithDebInfoLite/app-vanilla-relWithDebInfoLite.apk
```

## Cheats

Bundled cheats are community-style GateShark/CTRPF text files copied into the app assets for
convenience. They may be incomplete, wrong for a region or revision, unstable, or game-breaking.
Treat them as personal presets, not a curated database.

The app does not overwrite a cheat file that already exists on the device. If a bundled cheat was
fixed in git but the device still shows the old version, replace or remove the device copy first.

**Find value** waits for an acknowledged guest pause, searches only mapped guest application
memory, and supports a single verified temporary write with guarded restoration. Slow initial
searches can be cancelled without keeping partial results. Matches stay temporary: a one-session
raw address never becomes a persistent cheat without a stable pointer, signature, or relaunch
validation. Cheat coverage gaps are tracked in [docs/thor-cheat-gaps.md](docs/thor-cheat-gaps.md).

## Documentation

- [AGENTS.md](AGENTS.md) is the engineering ledger: every accepted optimization, every rejected
  experiment and why, and the invariants that must not be "cleaned up". Read it before changing
  code.
- [docs/thor-optimization-notes.md](docs/thor-optimization-notes.md) holds the dated evidence
  behind those rules.
- [docs/thor-cheat-gaps.md](docs/thor-cheat-gaps.md) tracks cheat coverage gaps.
- [CLAUDE.md](CLAUDE.md) carries the working rules for AI agents in this repository, the device
  control tools, and the writing standard for all English in this repository.
- [.claude/commands/goal.md](.claude/commands/goal.md) is the current performance goal with its
  baseline and procedure.
- [AI-POLICY.md](AI-POLICY.md) states how AI assistance is used here.

## Support

Do not open issues expecting support. Fork it and do stuff yourself. If something breaks, patch it
and own the result. Upstream Azahar has its own rules and support expectations; do not send
Thor-experiment problems there.

## Credit And License

Azahar is an open-source Nintendo 3DS emulator based on Citra. This fork exists because upstream
Azahar, PabloMK7's Citra fork, Lime3DS, Citra, and many emulator contributors did the foundational
work.

This repository remains under the upstream license terms. See [license.txt](license.txt).

## GPU driver

The APK ships its own Turnip build under `assets/gpu_drivers/`. It is Mesa 26.2.2 with one patch
(`tools/turnip/patches/`) that keeps two render-backend register writes on the render pipe. On
the AYN Thor the unpatched driver logs a CP AHB bus error on every command buffer. The app
installs the bundled driver at the first start after an install or an update, and it keeps a
later manual driver choice. `tools/turnip/build.sh` rebuilds the package in WSL. Details are in
AGENTS.md.

The fork also tells that driver to render straight to memory rather than tile every render pass.
A 3DS frame is about ninety passes over small targets, and tiling them costs more than it saves.
In the heaviest E.X. Troopers scene this moved 2x from 75.6% to about 96% of full speed. Put a
`TU_DEBUG` line in `thor_driver_env.txt` in the user directory to override it.

## Draw rules

A per-title file can name individual materials and change how they are drawn. A material is named
by its fragment shader fingerprint, the hash of the PICA combiner, lighting, fog and alpha test
setup, which is the same idea as a Dolphin graphics mod naming a texture hash. The actions are a
coarse shading rate of `2` or `4` through `VK_KHR_fragment_shading_rate`, `thinN` to submit one
draw in every N, and `skip` to drop the material.

Rules live in `ShaderRules/<title id>.txt` in the user directory, one per line, `#` for comments.
They are read at launch, so an experiment needs no rebuild. A file you already have is never
overwritten, and deleting it restores the untouched picture.

To find fingerprints, build with `-PthorFrameProfiling=true` and read the `ThorShaderUse` and
`ThorCoarsened` lines in the log. Rank candidates by measuring, not by draw count: a full screen
sheet is a handful of draws and a cheap material can be thousands.

> [!WARNING]
> A fingerprint identifies a shader, not a purpose. The same shader that draws a sheet of blowing
> snow can draw a heads-up display element in another scene, so rules tuned in one place can
> remove interface elements somewhere else. Check every scene you care about. The E.X. Troopers
> rule file ships commented out for exactly this reason: it takes its snow field from 63.79% to
> 99.66% at 3x, and it also removes weapon icons elsewhere in the game.

## Per-title settings

Each game can carry its own settings. The fork ships a profile for a title under
`assets/game_profiles`, the app installs it on first run, and it never overwrites settings you
already changed. To see or change them, long-press a game and open **Game Settings**; only the
values you change are stored for that game. E.X. Troopers ships at 2x with the Snapdragon GSR
screen filter, which holds 99.97% in its heaviest scene on the Thor with the picture untouched
everywhere in the game.

How much resolution a title can take varies enormously, so measure rather than assume. At 3x with
GSR, Ocarina of Time 3D reads 1213% and Kirby Triple Deluxe 1208%, and Ocarina of Time 3D still
holds 190% at 8x. The E.X. Troopers snow field reads 63.79% at the same setting, because it draws
about 1800 alpha blended meshes a frame at roughly 58x overdraw. The panel draws the 3DS top
screen at 1800x1080, so 4.5x is native 1:1 and most titles clear it comfortably.

Measured on 2026-09-18 in the E.X. Troopers engine scene at 2x and 100% speed: 59.4 FPS with
the GPU 64 to 83% busy on the bundled driver, against 51 FPS at 99.9% on the system Qualcomm
driver.

The same night fixed the E.X. Troopers freeze. It was not the driver. After a stalled swapchain
acquire the Android present path waited for a new surface that never comes. The swapchain is now
rebuilt on the current surface, and a stale frame is dropped instead of presented. A stall now
costs about one second instead of a frozen picture.

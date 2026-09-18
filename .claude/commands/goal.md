---
description: Run the Medarot 9 performance goal on the AYN Thor. Full speed at 2x to 4x with fast-forward working, reached through code efficiency work (NEON, Vulkan, rasterizer cache).
---

# Goal: Medarot 9 at 2x to 4x, full speed, fast-forward working

## Target

- Title: Medarot 9 Kuwagata with the English patch. Title id `0004000000174F00`.
- File: `zcci/Medabots9-KWG-1007 [0004000000174F00] [UNK].zcci` under the granted ROM tree.
- The game presents at 20 FPS by design. Full speed means the overlay shows `Speed: 100%` at 20 FPS.

Success has four parts:

1. Speed 100% at resolution factor 2, 3, and 4 in the intro street scene and in a battle.
2. Fast-forward with `frame_limit = 300` reaches at least 280% at 4x. At 3x it reaches 286% today. That is the Eco Turbo 60 FPS presentation cap for a 20 FPS title.
3. The gain comes from code: NEON paths, Vulkan submission, or the rasterizer cache. A settings change does not count.
4. Every change follows AGENTS.md: a correctness argument, an `arm64-v8a` build, and a matched before and after measurement on the Thor.

## Baseline

Measured 2026-09-17 on build `31e455c30-vanilla-thor`, Turnip R8, performance mode 2, fan mode 4,
brightness 255, USB power. Scene: the intro street dialog after New Game.

| Resolution | Limit | Speed | FPS | GSP command time | Emulation thread | GPU busy |
|---|---|---|---|---|---|---|
| 3x | 100% | 100% | 20 | 0.2 ms | 24% of one core | 13% |
| 3x | 300% | 286% | 57 | 0.2 ms | 63% of one core | 38% |
| 4x | 300% | 120% to 125% | 24 to 25 | 5.0 to 5.3 ms | 74% of one core, about half in the kernel | 27% |

Reading of the baseline:

- At 4x the time inside GSP command execution grows about 25x while the GPU stays mostly idle.
- Page faults and context switches on the emulation thread stay low at 4x.
- These two facts point at long kernel calls inside display transfers or texture copies at 4x, for example GPU submission or fence waits.
- The GSP command time is bracketed in `src/core/hle/service/gsp/gsp_gpu.cpp` around `gpu.Execute(command)`.

## Procedure

1. Call `state` on the `thor` MCP server. Record `performance_mode`, `fan_mode`, and `screen_brightness`. Restore them at the end.
2. Back up config.ini with `config_read`. Set `Layout.performance_overlay_show_speed` and `Layout.performance_overlay_show_frame_time` to `true` with `config_set`.
3. Call `launch` with the rom path and `wait_seconds = 16`. Then `press("START")`, wait 3 s, `press("A")`, wait 10 s. The game is then in the intro street dialog scene.
4. Measure with `screenshot`, `fps`, `gpu`, `threads`, and `emulation_thread_time`. Repeat for `Renderer.resolution_factor` 2, 3, and 4, and for `Renderer.frame_limit` 100 and 300. Relaunch after each config change.
5. Build the profiling APK with `-PthorFrameProfiling=true`. Install it with `install`. Repeat the 4x and 300% run. Read `frame_profile`. Rank the counters that grow between 3x and 4x.
6. Change code in the ranked path. Build the production APK. Repeat step 4 in the same scene. Accept only a matched improvement.
7. Reach a battle and repeat step 4 there. Hold A through dialog with `press("A")`.
8. Record the result: the rule in AGENTS.md and the dated measurement in docs/thor-optimization-notes.md.
9. Restore config.ini. Delete temporary GameSettings files. Restore the device settings. Remove scratch files and stale build hashes. Report the bytes reclaimed.

## Known facts

- Send button presses with `hold = true`. The guest reads input once per frame. A short tap is missed.
- The launch URI must use the tree form. The document form crashes the app with a `SecurityException` in `EmulationFragment.onCreate`. That crash is a separate robustness bug.
- `Compatibility.skip_texture_copy_fallback` in a GameSettings file has no effect. `src/video_core/gpu.cpp` sets that value from the hack list with a `false` default.
- A profiling build carries timing overhead. Never use it for FPS, power, or thermal claims.

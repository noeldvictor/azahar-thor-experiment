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

## Status on 2026-09-17

Parts 1 and 2 hold at 2x and 3x. At 4x, normal speed holds 19.8 FPS but with a P95 interval of
84 ms, and fast-forward stops near Speed 150%. The cause is known and it is not a renderer
defect. See "Root cause" below. No accepted code change came out of the first session. The
ledger entries are in AGENTS.md and in the 2026-09-17 entry of docs/thor-optimization-notes.md.

## Baseline

Production build `31e455c30-vanilla-thor`, Turnip R8, performance mode 2, fan mode 4, brightness
255, USB power. Scene: the intro street dialog after New Game. FPS and P95 come from
SurfaceFlinger. GPU busy is the KGSL node at 615 MHz. Thread time is the NativeEmulation thread
as percent of one core.

| Resolution | Limit | FPS | P95 interval | GPU busy | Thread user / sys | Overlay |
|---|---|---|---|---|---|---|
| 2x | 100% | 19.9 | 50.6 ms | 7.8% | 23% / 1% | Speed 100% |
| 2x | 300% | 52.3 | 33.7 ms | 23.0% | 65% / 1% | about 260% |
| 3x | 100% | 19.9 | 50.6 ms | 13.2% | 24% / 1% | Speed 100% |
| 3x | 300% | 57.9 | 16.9 ms | 38.5% | 64% / 0% | Speed 286% |
| 4x | 100% | 19.8 | 84.2 ms | 20.6% | 23% / 21% | Speed 100% |
| 4x | 300% | 30.3 | 50.5 ms | 32.1% | 36% / 30% | Speed 156%, frame 10.8 ms |

## Root cause

- The game copies its rendered 400x240 RGB8 top framebuffer with the CPU every frame. The loop
  runs at guest PC `0x004008C0`. The first 4-byte read of a frame hits the dirty surface.
- `RasterizerCache::FlushRegion` treats a request of 8 bytes or less as a CPU read and flushes
  the whole dirty region: one 384 KiB download and one GPU finish per frame.
- The finish waits for the frame's draws, which the scheduler submits only at that moment. The
  CPU and the GPU never overlap. The frame time is CPU work plus GPU work.
- At 3x the GPU frame is about 6.7 ms. At 4x it is about 11 ms. That is the 4x fast-forward limit.
- The remaining 72,000 reads of the frame take the slow path for cached pages. A fast path that
  skipped the dirty lookup for them changed nothing measurable, so the slow path is not the cost.

## Procedure

1. Call `state` on the `thor` MCP server. Record `performance_mode`, `fan_mode`, and `screen_brightness`. Restore them at the end.
2. Back up config.ini with `config_read`. Set `Layout.performance_overlay_show_speed` and `Layout.performance_overlay_show_frame_time` to `true` with `config_set`.
3. Call `launch` with the rom path and `wait_seconds = 16`. Then `press("START")`, wait 3 s, `press("A")`, wait 10 s. The game is then in the intro street dialog scene.
4. Measure with `screenshot`, `fps`, `gpu`, `threads`, and `emulation_thread_time`. Repeat for `Renderer.resolution_factor` 2, 3, and 4, and for `Renderer.frame_limit` 100 and 300. Relaunch after each config change. A control build and a candidate build must run the same matrix in the same session.
5. For counters, build with `-PthorFrameProfiling=true`, install with `install`, and read `frame_profile`. Never use that build for a speed claim.
6. Change code in the ranked path. Build the production APK. Run the matrix again. Accept only a matched improvement.
7. Reach a battle and repeat step 4 there. Hold A through dialog with `press("A")`.
8. Record the result: the rule in AGENTS.md and the dated measurement in docs/thor-optimization-notes.md.
9. Restore config.ini. Delete temporary GameSettings files. Restore the device settings. Remove scratch files and stale build hashes. Report the bytes reclaimed.

## Next candidates

1. After a small CPU read has flushed a whole GPU surface, unregister that surface so that its
   pages become uncached. This mirrors the CPU-write path in `InvalidateRegion`. The remaining
   reads of the frame then take the fast memory path. Risk: the surface is recreated each frame,
   and a 384 KiB upload follows if the game does not clear the buffer with a memory fill.
2. Submit recorded draws before the read so that the GPU starts earlier. The gain is bounded by
   the 2 ms recording window per frame.
3. Measure a battle scene. The intro street dialog may not be the heaviest scene.

## Known facts

- Send button presses with `hold = true`. The guest reads input once per frame. A short tap is missed.
- The launch URI must use the tree form. The document form crashes the app with a `SecurityException` in `EmulationFragment.onCreate`. That crash is a separate robustness bug.
- `Compatibility.skip_texture_copy_fallback` in a GameSettings file has no effect. `src/video_core/gpu.cpp` sets that value from the hack list with a `false` default.
- `disable_right_eye_render` changes nothing for this title. It does not render a second eye.
- A profiling build carries timing overhead. Never use it for FPS, power, or thermal claims.

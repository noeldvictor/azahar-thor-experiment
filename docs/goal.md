# Goal

**Current goal, set 2026-09-20: E.X. Troopers holds full speed at 3x with the Snapdragon GSR
screen filter, measured in the snow field from save state 5.** The full statement, the reason the
target moved from 2x, and where it stands are in
[goal_history/20260920_goal_3x.md](goal_history/20260920_goal_3x.md).

**Outcome, 2026-09-20, later the same day.** The goal is met for the emulator and not for this
one title, and the two were separated by measuring a second and third game for the first time.
At 3x with GSR and no frame limit, Ocarina of Time 3D runs at 1213% and Kirby Triple Deluxe at
1208%, against 26.19 ms a frame for the snow field. Sweeping resolution on Ocarina of Time 3D
gives 650% at 4x, 343% at 6x and 190% at 8x, inverse square in resolution to within the spread.
So a normal 3D title has about twelve times the headroom it needs at 3x and clears the panel's
native 4.5x with room to spare; **4x to 6x is the honest recommendation for this panel, not 3x.**

E.X. Troopers is the exception and the reason is its own renderer, not ours: 30 to 33 full screen
blended passes a frame, which cost nine times as much at 3x as at 1x. Its per-title profile is
set to 2x with GSR, measured at 99.97% mean and 99.87% minimum over eight samples, 59.74 FPS at
13.21 ms against a 16.67 ms budget, GPU at 680 MHz and 88% busy, from save state 5 in the snow
field. Reaching 3x on this title would mean shading fewer fragments, which means changing what
the game draws.

The next goal should be set against a title that is representative rather than pathological, and
should aim at 4x or higher now that the headroom is known.

The 2x goal below is superseded and kept for its procedure and for the list of everything that was
ruled out with numbers. Do not restart that search; read its outcome section first.

---

# Performance goal: E.X. Troopers on the AYN Thor

This is a document, not a slash command. `/goal` is Claude Code's built-in command: it takes a
completion condition and keeps working until an evaluator model judges the condition met. Point
it at this file, for example:

```
/goal Read docs/goal.md and follow it. Done when E.X. Troopers holds Speed 100% at 60 FPS in
the snow field from save state 5 at 2x, 3x and 4x, and reaches 200% speed at 2x with the frame
limit at 200, each measured with the thor MCP perf_stats tool and each confirmed by a
screenshot of the snow scene. Or stop after 40 turns.
```

The evaluator reads the conversation rather than running commands itself, so every claim has to
appear in the transcript: the numbers from `perf_stats` and the screenshot that proves the scene.

# Goal: E.X. Troopers at 60 FPS from 2x to 4x resolutions, and 120 FPS at 2x in fast forward

## The target

1. E.X. Troopers holds **Speed 100% at 60 FPS** in gameplay at every resolution from **2x to
   4x**, that is 2x, 3x and 4x.
2. With fast forward, the same scenes reach **200% speed**, which is 120 game frames per second.
   The panels are pinned at 60 Hz, so this shows on screen as 60 FPS with the game running at
   double rate. Fast forward is game speed, not frame rate.
3. **Every measurement for this goal is taken in the snow field, and nowhere else.** It is the
   heaviest scene in the game and the one the target is judged on. A menu, a title screen, a
   cutscene or a video proves nothing: they run at a fraction of the load and have repeatedly
   produced readings that looked like progress and were not. Confirm the scene with a
   screenshot before recording any number, and say which scene every recorded number came from.

## The premise, which is not negotiable

The 3DS GPU is a 268 MHz PICA200 drawing 400x240 and 320x240. The Adreno 740 has roughly 36
times its fill rate and hundreds of times its arithmetic throughput. 2x needs 4 times the 3DS
pixel rate and 4x needs 16 times, so the hardware has room for both. **Any shortfall is emulator
or driver cost, not a hardware limit.** Do not close this goal by declaring a scene too heavy.
Find the cost and name it.

## What this goal is for

The frame rate target is the measuring stick, not the whole purpose. E.X. Troopers is the hardest
case we have, so the work of reaching it is how we find inefficiency anywhere in the emulator.
Every cost this goal uncovers is written down and fixed for every title, not special cased for
one game.

Once a cost is found, the fix should use the hardware the Thor already has:

- **ARM64 and NEON.** Guest emulation, texture decode and swizzle, vertex conversion and audio
  are candidates for vector code. A scalar loop over pixels or vertices that runs every frame is
  a finding.
- **The GPU.** Anything still done on the CPU that the GPU could do is a finding, including any
  path that falls back to software while the GPU sits idle.
- **Turnip.** We build the driver ourselves (`tools/turnip/`), so a driver cost is ours to patch
  and ship rather than work around.
- **The rest of the device.** Fixed-function blocks, the second CPU cluster, and anything else
  the Thor offers that the emulator currently ignores.

## How to work on it

- Measure before changing anything, in the snow field, and confirm the scene with a screenshot.
  Three plausible theories died against measurement on 2026-09-19 alone, and several readings
  were void because the automated run had drifted into a video without noticing.
- Reaching the snow field takes about six minutes of play from launch, so keep a save state of
  it. `allow_savestate_mismatch` in the Utility settings lets a state written by one build load
  into another, which is what makes a before and after comparison possible across a rebuild. It
  is off by default and is for testing only.
- Research is in scope: upstream Azahar, other forks, Mesa and Turnip issues and merge requests.
  We build our own Turnip (`tools/turnip/`), so a driver fix is ours to make and ship.
- Move work onto hardware that is sitting idle: the GPU, NEON on the CPU, and anything else the
  Thor offers. A software fallback that runs every frame is a finding, not an excuse.
- Extend the `thor` MCP server whenever an experiment needs a capability it lacks, and record the
  tool in `azahar/CLAUDE.md`.
- Snapshot this goal into `docs/goal_history/<date>_goal<n>.md` whenever the target changes, so
  the goals we have set and what they produced can be read back later.
- Keep the condition given to `/goal` short and checkable from the transcript. The evaluator
  cannot run commands; it only reads what this session has already shown.

## Titles

| Title | Title id | File under the granted ROM tree | Per-title profile |
|---|---|---|---|
| Medarot 9 Kuwagata, English patch | `0004000000174F00` | `zcci/Medabots9-KWG-1007 [0004000000174F00] [UNK].zcci` | none |
| E.X. Troopers, English patch v1.0.2 | `0004000000053700` | `zcci/E.X. Troopers (Japan) [T-En by Fan Translators International v1.0.2].zcci` | `GameSettings/0004000000053700.ini` forces 2x and a 100% limit |

## Success

1. The target above is met, or the cost that prevents it is named with numbers and a failed
   attempt to remove it is recorded.
2. For each title, the native frame rate at full speed is known per scene: title, menu, cutscene, gameplay. Full speed means the overlay shows `Speed: 100%`.
3. A title that waits three vsyncs per frame by its own design gets a 30 FPS patch code as a bundled cheat under `src/android/app/src/main/assets/cheats/<title id>.txt`, verified on the Thor for speed and for game pacing. Medarot 9 has both codes and they pass the pacing check; see its status.
4. Every emulator change follows AGENTS.md: a correctness argument, an `arm64-v8a` build, and a matched before and after measurement on the Thor. A patch code is guest-code patching and is recorded as a cheat, not as an optimization.

## Status on 2026-09-18

### Medarot 9

- Presents at 20 FPS by design in the title screen, the intro, and the street dialog. Speed 100% at 2x and 3x. At 4x, normal speed holds 19.8 FPS with a P95 interval of 84 ms and fast-forward stops near Speed 150%.
- Root cause of the 4x limit: the game copies its rendered 400x240 RGB8 top framebuffer with the CPU every frame from a loop at guest PC `0x004008C0`. The first 4-byte read flushes the whole dirty surface: one 384 KiB download and one GPU finish per frame. CPU and GPU never overlap. At 4x the GPU frame is about 11 ms.
- A dirty-free-span fast path for the remaining reads was measured and rejected. See AGENTS.md.
- 30 FPS code: finished. The vsync target is a 16-bit field of the game object at heap address
  `0x0801DA48 + 0x4e`, read by `ldrh r1, [r4, #0x4e]` at `0x003D8268`. Patching the load alone
  gave 30 FPS with the logic 1.5 times too fast. Patching the field itself (`1801DA96 00000002`)
  gives 30 FPS at Speed 100% with the intro pacing identical to 20 FPS second for second, so the
  game also uses that field as its logic step. The bundled cheats `30 FPS - Thor Experiment` and
  `60 FPS - Thor Experiment` write 2 or 1 there. The heap address was stable across every launch
  in this session; if a launch ever loads it elsewhere the cheat has no effect and does no harm.

### E.X. Troopers

- Videos present at 30 FPS at Speed 100% with the GPU near idle.
- The save-slot and episode screens present at 60 FPS at Speed 100%, but the GPU is at 99.9% busy at 2x and presentation drops to a mix of 60 and 30 FPS frames. The GSP command time is 4.7 ms per frame there. This is the efficiency target for this title; the frame profiler counters decide what the GPU is doing.
- Engine scenes (the launch scene after the intro videos, about six minutes in) present at 60 FPS
  at Speed 100%, frame 10.1 ms, GSP command time 4.7 ms, GPU busy 62.6% at 2x. This title needs
  no frame-rate patch. Its target is GPU efficiency: at 3x the same scene would exceed the GPU.
- Bug: opening the game's pause menu during a video freezes emulation. The VulkanWorker thread then spins on `dequeueBuffer timed out`. Reproduced twice. Avoid START during videos until fixed. Record it in the ledger.
- The hack list already forces `SKIP_TEXTURE_COPY_FALLBACK` for this title. The same key in the per-title ini is redundant.
- Geometry shaders: program 0x2E now runs inside the host vertex shader; 0x58 and 0x3C stay in
  software because they keep state between invocations. Accelerated draws rose from 33% to 95%
  on the save-slot screen with no frame rate change: the pass restarts hold the GPU there.
- Pass restarts per swap: 121 to 140 color target switches, 28 depth toggles, 39 to 45 render
  area changes. The full-render-area and depth-retention changes are on and measured: GPU
  busy 99.9% to 91.5% at a steady 60 FPS on the save-slot screen (system driver). The color
  target switches remain the next target.
- Driver: the APK bundles a patched Turnip (`tools/turnip/`) that does not fault on this unit;
  it is the bench from 2026-09-18 night. The system Qualcomm driver held 51 FPS at 99.9% GPU
  in the engine scene at 2x; Turnip R8 held 60 FPS at 62.6% there. Keep `gpu_faults` before
  and after every Vulkan run as a guard.
- Open: the E.X. Troopers present-path freeze (AGENTS.md, 2026-09-18 night).

## Where this stands on 2026-09-19

Shipped today, each measured on the device: the patched Turnip that removes the GPU fault, the
swapchain rebuild and acquire ordering that removed the freeze, the stream ring sizes that
stopped the emulation thread waiting on the GPU, and the direct render path on the bundled
driver. Together these took the engine scene from 48% to about 96% speed at 2x.

Standing numbers in the heaviest scene reached so far, at 2x, five samples:

| | value |
|---|---|
| Speed | 83% at 50 FPS, frame 20.0 ms |
| GPU | 98.4% busy at 680 MHz, about 19.7 ms of work |
| Command processing | 8.0 ms, of which the draw path is 3.2 ms |
| Guest emulation | 6.3 ms |
| Structure | 250 render passes, 164 colour switches, 1638 draws per frame |

3x reaches about 51% and 4x about 26%, and GPU time is linear in pixels, so the per-pixel cost
is what blocks 3x and 4x.

Ruled out by measurement, with numbers in the notes, so none of these is retried: texture
downloads, the uniform ring alone, bandwidth compression being disabled by image usage,
low-resolution Z, the end-of-pass barrier, display transfers, and `disable_right_eye_render`.

Ranked next steps:

1. The per-pixel cost is unexplained and is the whole of 3x and 4x. Measure the fragment shaders
   we generate: instruction counts and register pressure for a typical draw. A PICA texture
   combiner emulated badly is the most likely place for a ten times per-pixel penalty.
2. Pick tiled or direct rendering per pass by render-area size in our own Turnip build. The
   direct path won overall because roughly 240 of the 250 passes are tiny, but it takes the tile
   cache away from the few large blended passes. Turnip's existing flag keys on draw count,
   which is the wrong signal, and measured worse.
3. Re-test the end-of-pass barrier on a heavy scene. The test that cleared it ran on a light
   scene and is not trustworthy.
4. The geometry programs that still fall back to software, and the 33 pass restarts per frame
   that come from render-area changes rather than the game switching targets.

## Procedure

1. Call `state` on the `thor` MCP server. Record `performance_mode`, `fan_mode`, and `screen_brightness`. Restore them at the end. Keep the screen on with `svc power stayon usb` and set it back to `false` at the end. A screen timeout pauses the app and leaves it in a bad state.
2. Back up config.ini with `config_read`. Set `Layout.performance_overlay_show_speed` and `Layout.performance_overlay_show_frame_time` to `true` with `config_set`. For E.X. Troopers, the per-title ini overrides the resolution and the limit; edit it with `game_settings_set` for the matrix and restore it after.
3. Launch with `launch` and `wait_seconds = 30`. Advance with held `press("A")`. Use `press("START")` only where the title needs it, and never during an E.X. Troopers video.
4. Measure each scene with `screenshot`, `fps`, `gpu`, `threads`, and `emulation_thread_time`. Run the six-config matrix (2x, 3x, 4x at 100% and 300%) for gameplay. A control build and a candidate build must run the same matrix in the same session.
5. For counters, build with `-PthorFrameProfiling=true`, install with `install`, read `frame_profile`. Never use that build for a speed claim.
6. Emulator change: change code in the ranked path, build the production APK, run the matrix again, accept only a matched improvement.
7. 30 FPS patch, per title that waits three vsyncs per frame:
   1. Read the wait trace from `logcat` (`THORDIAG gsp wait`). Note the link registers that recur once per frame and the dumped code words.
   2. Disassemble the dump with capstone (`python -c "import capstone"` works on this PC) in ARM mode, and in Thumb mode if the link register is odd. Find the compare or the constant that holds the vsync count.
   3. Write a Gateshark code that writes the new constant to that address. Put it in the bundled cheats file for the title with a clear name such as `30 FPS`.
   4. Enable the cheat, relaunch, and measure. Check pacing: a battle timer, an animation, or the play-time clock against a stopwatch. If the game runs 1.5 times too fast, find the logic timer and patch it too, or drop the code.
8. Record results: rules in AGENTS.md, dated measurements in docs/thor-optimization-notes.md, cheats under the assets folder.
9. Restore config.ini and the per-title ini. Restore the device settings and the screen-on state. Remove scratch files and stale build hashes. Report the bytes reclaimed.

## Known facts

- Send button presses with `hold = true`. The guest reads input once per frame. A short tap is missed.
- The launch URI must use the tree form. The document form crashes the app with a `SecurityException` in `EmulationFragment.onCreate`.
- `Compatibility.skip_texture_copy_fallback` in a GameSettings file has no effect. `src/video_core/gpu.cpp` sets that value from the hack list with a `false` default.
- `disable_right_eye_render` changes nothing for Medarot 9.
- A profiling build carries timing overhead. Never use it for FPS, power, or thermal claims.
- A wake sequence with the MENU key while the screen is off brings the secondary-display launcher over the app and the present loop does not recover. Keep the screen on instead.

## Outcome, 2026-09-19

Three of the four targets are not reachable by emulator work on this scene, and the reason is
measured rather than argued. Record kept here so the goal is not restarted from scratch.

What the session reached in the snow field from save state 5, GPU at 680 MHz in every sample:

| resolution | frame limit | at start | at end | target |
| --- | --- | --- | --- | --- |
| 2x | 100 | 100%, GPU 99.8% busy | 99.97%, GPU 88.6% busy | 100% |
| 2x | 200 | 103.40% | 119.39% | 200% |
| 3x | 100 | 52.36% | 63.73% | 100% |
| 4x | 100 | 31.07% | 39.44% | 100% |

The end column is a final back to back run of all four on the shipping build, eight samples each,
GPU 680 MHz throughout, with a screenshot of the snow field from each run. The 2x row at limit 100
is the frame limiter on its target rather than a hardware limit: the GPU has 11% spare there and
the same scene reaches 119% as soon as the limit is raised.

The frame is fully accounted for. Fragment arithmetic is 13% of it, measured by removing the
per-stage combiner rounding. Framebuffer compression is already saving 12%, measured by turning
it off. All 97 render passes together are 0.53 ms, priced at 5.5 us each with a switch that
restarts the pass every N draws. Pass barriers are 0.49 ms. Texture filtering is under 1%.
Shader occupancy is at its maximum of 16 waves with six registers.

What is left is the fragment count. A pipeline statistics query counts 33.7 million fragment
shader invocations per frame at 2x against 691,200 pixels on screen, which is 49x overdraw, and
it is the game's own drawing: five rounds of about 163 draws into one target with a downsample
chain between them. At 72 frames per second that is 2.4 gigapixels per second. Reaching 200% at
2x would need 4.0, and reaching 100% at 4x would need the same work at four times the pixels.

The premise in this goal, that the Adreno 740 has about 36 times the 3DS fill rate and so 4x
needs 16, does not hold, because a PICA fill was one fixed-function pass and an emulated fill is
a shader. That ratio consumes the headroom before resolution scaling starts.

The honest target for this renderer and this scene is full speed at 2x with headroom to spare,
and about 120% in fast forward. Anything beyond that needs the scene to shade fewer fragments,
which means changing what the game draws.

### What was ruled out, so none of it is retried

Every line below is a measurement from the snow field, not an argument. The rules behind them
live in AGENTS.md and the dated evidence in docs/thor-optimization-notes.md.

| Candidate | Result |
| --- | --- |
| Fragment ALU | 13% of the frame; removing 36 of ~100 instructions bought 4.8% |
| Half precision in the combiner | would follow from the above at roughly 6%; not worth the rewrite |
| Render passes | 5.5 us each, 95 per frame, 0.53 ms in total |
| Pass barriers | 0.49 ms per frame |
| Framebuffer compression | already active and already saving 12% |
| Tiled rendering | 55.64% against 63.58% at 3x; the driver choosing per pass gives 56.75% |
| Turnip debug flags | `noconform` 64.07%, `noconform,nouboopt` 64.31%, both inside the spread |
| Texture filtering | under 1% |
| Shader occupancy and registers | already at the maximum of 16 waves with 6 registers |
| GPU clock | 680 MHz is the top of the device's table; no cap was ever applied |
| Storage usage flag on RGBA8 | no effect; UBWC is not being disabled by it |
| Draws sampling their own target | 10 per frame out of 1,843 |
| Screen buffer padding | never shaded; viewport and scissor are the used region |
| Effect and blur buffers | only 4.5% of fragments; not scaling them cannot pay |
| Stereoscopic rendering | already single eye |
| The second Thor panel | free |
| 16-bit render targets | none are being widened to 32-bit |
| Cached upload memory | 4% slower than write-combined |
| Android performance hint interface | 3% slower at 1x |
| Texture descriptor set reuse | 1% slower |
| Direct vertex attribute field reads | neutral; the compiler already folded it |
| Unified memory for vertex data | worth about 3% of the CPU, and nothing above 1x |

Fragment count scales cleanly with resolution, 32.88 million per frame at 2x and 137.92 million
at 4x, so nothing degrades as the scale rises. The part looks 30% more efficient at 4x only
because a partly covered 2x2 quad costs its full width and the counter excludes helper lanes.

The two changes that did pay, and both help every game: the PICA depth transform moved out of the
fragment shader into the viewport, which restored the early depth test and the low resolution Z
pass, and the per-stage combiner quantisation made optional. Two frame limiter bugs, a NEON table
conversion and an emulation thread priority also landed but show only where the frame is CPU
bound.

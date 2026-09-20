# Goal

**Current goal, set 2026-09-20: E.X. Troopers holds full speed at 3x with the Snapdragon GSR
screen filter, by decoupling the cost of full screen post-processing from the resolution scale.**

Earlier goals and what they produced are in [goal_history/](goal_history/). Read
[20260920_goal_3x.md](goal_history/20260920_goal_3x.md) for the twenty-two candidates already
ruled out with numbers, so none of them is retried.

## The target

E.X. Troopers holds **Speed 100% or better at 60 FPS** in the **snow field loaded from save state
5**, at **resolution 3x with `screen_filter = 2`** and the frame limit at 100.

The result must appear in the conversation as a `perf_stats` reading over at least five samples
beside a screenshot of the snow scene from the same run, with the resolution and the filter
stated. A reading from a menu, a title screen, a cutscene or a video does not count. A run whose
GPU clock fell below 615 MHz is void; let it cool and repeat it.

**Stop after 30 turns** even if the target is not met, and report what was learned. The previous
goal had no turn cap and looped indefinitely once its target turned out to be unreachable.

## Why this is the goal, and why it is reachable when the last one was not

The last goal asked for 3x and 4x and 200%, and three of its four rows were unreachable. This one
asks for the row that the measurements say is in range, and it names the mechanism.

The emulator scales **every** render target by `resolution_factor`. For a normal 3DS game that is
fine: the scene is drawn once or twice, so 3x costs nine times on two passes. Ocarina of Time 3D
reads 1213% at 3x and 650% at 4x; Kirby Triple Deluxe reads 1208%.

E.X. Troopers builds its comic-book look out of full screen image processing: halftone screening,
ink outlines, colour grading, bloom and the blizzard. That is 30 to 33 full screen passes a frame,
with 82 of its 97 passes reading an earlier pass's output. So 3x costs nine times on **thirty**
passes, which is 72 million fragments a frame against 0.89 million pixels on screen, about 81
fragments per output pixel, most of them blended. 3x reads 63.79% and 4x reads 39.07%.

None of that work is buying sharpness. Sharpness comes from the scene pass. A halftone screen or
an ink outline at 3x costs nine times what it costs at 1x and looks, if anything, less like the
console, where those effects ran at 400x240 by design. **The fix is to stop multiplying the
post-processing chain by the resolution scale.** If the chain runs near native while the scene
stays at 3x, most of that 95.5% of the fragment load goes away, which is the difference between
26.2 ms and the 16.67 ms budget.

## The trap, which is why this has not already been done

The passes ping-pong on the **same** big target that holds the final image. Allocating "the post
target" smaller therefore downscales the picture. Any mechanism has to separate the effect work
from the image that is finally displayed.

## Candidate mechanisms, with what each one can and cannot reach

Judge these by measurement, not by argument, and record whichever fails with its numbers.

1. **Scratch-target redirect.** A post pass renders into a scratch target at the reduced scale,
   then one cheap unblended blit puts the result back into the full size target. This is the only
   candidate that removes **blend** traffic as well as shading, and blend bandwidth is the measured
   wall: 72 million blended fragments a frame is about 576 MB of colour read and write before
   textures or depth. Most likely to reach the target; most work.
2. **Variable rate shading.** `VK_KHR_fragment_shading_rate` at 2x2 or 4x4 on full screen post
   draws. The target stays at 3x, so the final image is untouched and there is no picture risk
   beyond the effects themselves. It cuts fragment shader invocations and texture fetches but
   **not** the per-pixel blend, so on its own it attacks roughly the 13% that is ALU plus whatever
   texture fetch costs. A good complement and a safe fallback. The bundled Turnip
   (`Turnip-Thor-26.2.2-r1`) carries the extension string; confirm at runtime that it is actually
   advertised on this a740 before building on it.
3. **Per-surface `res_scale` on post targets.** Simplest, but breaks on exactly the ping-pong case
   above. Try it only if the heuristic below turns out to separate targets cleanly.

**The heuristic** for "this is a post-process pass", for any of the three: a draw that covers the
whole render target, samples another render target, and has the depth test disabled. That is a
full screen post quad by construction. `DrawReadsRenderTarget` and `PassReadsRenderTarget` in the
profiling build already count these, so the heuristic can be validated against known counts before
any of it is wired to rendering.

## The user-facing option

This ships as a setting the user can try, not as a silent change.

- **Default off.** With it off the picture is bit-identical to today, and the existing rule that an
  accepted change must not alter the picture continues to apply to the default path.
- Enabled in the E.X. Troopers per-title profile once it is measured, the way
  `accurate_tev_rounding` and `screen_filter` already are.
- An Android setting needs **four** declarations or it silently does nothing: `settings.h`,
  `GenerateSettingKeys.cmake`, `default_ini.h`, and a `ReadSetting` call in `jni/config.cpp`.
- **Help text is part of the deliverable.** It must say, in plain language, which games it helps
  and why: titles that build their look out of full screen post-processing rather than drawing
  their scene once. Name examples. Say that it leaves geometry sharpness alone and only changes
  the resolution the effects are computed at, that it does nothing for a game that draws its scene
  once, and that on such a game it costs nothing and gains nothing.

## How to work on it

- Measure before changing anything, in the snow field, and confirm the scene with a screenshot.
- Every accepted change needs a before and after from save state 5 plus screenshots **with the
  option on and off**, so the picture difference is visible and can be judged rather than assumed.
- Record results in AGENTS.md and dated evidence in docs/thor-optimization-notes.md. Record
  rejected experiments with their numbers so they are not retried.
- Prefer ARM64 and NEON, moving work to the GPU, or patching our own Turnip in `tools/turnip`.
- Commit and push to origin master.
- Snapshot this file into `docs/goal_history/<date>_goal<n>.md` when the target changes.

## Known state at the time this goal was set

- E.X. Troopers, snow field, save state 5, eight samples, GPU 680 MHz: 2x limit 100 is 99.96%,
  3x limit 100 is 63.79%, 4x limit 100 is 39.07%, 2x limit 200 is 118.15%.
- Its shipping profile is 2x with GSR, which holds 99.97%.
- 3x and 4x are GPU bound (`SWP 12.0ms` and `SWP 25.3ms`). 2x at limit 200 is CPU bound
  (`SWP 0.0ms`, `CMD 8.8ms`) and is **not** a fragment problem.
- The Thor panel draws the 3DS top screen at 1800x1080, so native 1:1 is 4.5x. Normal titles
  clear that: Ocarina of Time 3D reads 650% at 4x and 190% at 8x.
- The emulation thread is called `NativeEmulation`, not `EmuThread`.
- Save states written by upstream build 2125.0.1 cannot be loaded; boost rejects the class version
  for `shared_ptr<Memory::PageTable>` and `allow_savestate_mismatch` does not cover that.

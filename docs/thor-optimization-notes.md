# Thor Optimization Notes

These notes are for AYN Thor Base/Pro/Max only. The assumed target is Snapdragon 8 Gen 2 with Adreno 740, active cooling, LPDDR5X memory, and UFS 3.1 storage per AYN's current product page. The mirrored device manual instead reports UFS 4.0, so storage generation remains unverified until checked on the physical device. Thor Lite uses Snapdragon 865 / Adreno 650 and should be treated as a separate target.

## Current Baseline

- Android builds are `arm64-v8a` only.
- Android defaults prefer Vulkan when Vulkan is enabled (`src/common/settings.h`).
- CPU JIT, hardware shaders, shader JIT, disk shader cache, async filesystem operations, and async custom texture loading are already enabled by default.
- Internal resolution defaults to 1x. Game profiles may cap or override this for stability.
- Adreno custom driver loading is already wired through `libadrenotools` and `GpuDriverHelper`.
- E.X. Troopers (`0004000000053700`) currently has a hardcoded Android launch profile and matching manifest.
- Thor dual-display mode is fixed to 3DS top screen on the primary panel and 3DS bottom screen on the secondary panel. The old hidden virtual secondary display fallback is removed, so secondary rendering only starts when Android exposes a real second display.
- The Thor GPU Driver Manager now has a guided picker. It queries K11MCH1 AdrenoToolsDrivers releases, presents the newest generic Turnip ZIP as the recommended first pick, lists recent generic Turnip rollback builds with visible download buttons, also lists Qualcomm and Turnip variant troubleshooting choices when available, validates driver metadata, stores the ZIP under `gpu_drivers`, installs immediately, and still keeps manual ZIP and system-driver fallback paths.

## 2026-08-19 Whole-Frame Profiling Pivot

- The 142-entry ledger is deliberately overlapping and path-local. It proves that many individual
  pieces became cheaper; it does not establish their aggregate frame share or a 142-change
  whole-emulator speedup. Further micro-optimization acceptance now waits for subsystem-level
  evidence instead of treating another favorable instruction loop as the next priority.
- A dedicated profiler is compiled only when the Android package is built with
  `-PthorFrameProfiling=true`. Ordinary builds define `THOR_FRAME_PROFILING=0`, making counter adds,
  reports, and scoped timers inline no-ops. Profiling builds emit a `ThorFrameProfile` window every
  300 `SwapBuffers()` calls and log a warning that their atomic counters and timers make them
  unsuitable for FPS, watt, or thermal A/B comparisons.
- The windows count swap calls and actual presented images; presentation blits/copies and pixels;
  presentation queue waits and combined submissions; scheduler flushes, finishes, submissions,
  timeline waits, and worker drains; render-pass starts, reuse, ends, end-of-render-pass image
  barriers, and Mali-only flushes; texture upload/download/custom-upload bytes plus copy/blit pixels;
  and PICA draw batches,
  immediate vertices, backend-handled draws, software draws, and categorized hardware-shader
  fallback reasons.
  `RenderPassImageBarriers` is intentionally scoped to barriers emitted by
  `RenderManager::EndRendering()` and is not a global Vulkan barrier count.
- The profile-enabled JDK 17 ARM64 package build passed, and its final native library contained the
  warning plus all six `ThorFrameProfile` log groups, including the immediate-vertex field. A
  separate ordinary package build then passed with `ENABLE_THOR_FRAME_PROFILING:BOOL=OFF`. Its
  `libvideo_core.a` contained zero frame-profile symbols and its final `libcitra-android.so`
  contained zero profiler or warning strings, confirming that the normal path compiles the
  instrumentation out. The retained normal APK is 29,010,268 bytes with SHA-256
  `7FD223F97A0F3A40D619C7C018F8525CCFAF2D07C5ECCAD8599F5C5A7356FCA6`.
- Exact bounded cleanup removed the profile cache, the obsolete pre-option cache, and reproducible
  Gradle/JNI/R8/symbol/mapping staging: 9,015,641,394 logical bytes removed and 8,140,775,424
  physical bytes recovered. C: has 52,450,258,944 bytes free. The retained repo output is the
  3,241,799,767-byte active profile-disabled ARM64 CMake/Ninja cache plus the 29,010,268-byte APK
  and 476-byte metadata. No PDF, APK, native binary, profiler log, or scratch note is tracked.
- Static inspection makes final-presentation copy/synchronization, render-pass churn, forced texture
  readback finishes, CPU vertex fallback frequency, and queue wait/submission cadence the main
  forest-level suspects. The counters rank those suspects; none is called the bottleneck without a
  matched game capture.
- Build a diagnostic APK with
  `.\gradlew.bat :app:assembleVanillaRelWithDebInfoLite -PthorFrameProfiling=true --no-configuration-cache`
  and filter a future allowed capture for `ThorFrameProfile`. The current no-launch restriction
  means no ADB command, install, app launch, game launch, or capture was performed in this change.
  The profiler is diagnostic infrastructure, does not increment the optimization ledger, and does
  not support a new speed or watt claim.

## 2026-08-19 Exact-Size Vulkan Presentation Copy

- Entry 143 addresses one of the profiler pivot's whole-frame suspects rather than another guest
  instruction. Vulkan presentation first renders the complete host layout into an intermediate
  image, then transfers that image into the acquired swapchain image. The intermediate image is
  created with the swapchain's exact format, but the prior path used a filtered `vkCmdBlitImage`
  whenever the destination advertised blit support, including a 1:1 transfer with equal extents.
- Equal frame and acquired-swapchain extents now use `vkCmdCopyImage`. With identical formats and
  extents this preserves each texel bit-for-bit and requires no scaling or filtering. A genuine
  extent mismatch still uses the existing linear blit when supported; a device without blit
  support retains the existing overlapping-copy fallback. Selection uses the acquired swapchain
  extent rather than assuming that the requested Android surface dimensions always match it.
- Compile-time checks cover equal-size copy, supported scaling blit, and unsupported scaling copy.
  The existing opt-in whole-frame counters distinguish `PresentCopies` from `PresentBlits`, so a
  future permitted capture can verify route frequency without adding instrumentation to a normal
  APK. Qualcomm's Adreno guidance to avoid unnecessary resolves/filter and external-memory traffic
  makes this a plausible every-presented-frame efficiency improvement, but the driver may already
  optimize some 1:1 blits internally. No FPS, frametime, power, or watt improvement is claimed
  before the matched Thor A/B.
- The latest fetched `upstream/master` is `f6a3e3aa5` (2026-08-19) and is already an ancestor of
  this fork. Upstream still selects blit solely from format support, so this is a current fork-side
  change rather than a duplicate of a newer Azahar fix.
- The first complete JDK 17 `arm64-v8a` package build passed in 2 minutes 6 seconds, compiling the
  selector assertions and linking both the native test executable and production
  `libcitra-android.so`. Linked AArch64 inspection shows the final presentation function loading
  the blit-capability byte, taking the copy route immediately when it is false, and otherwise
  comparing both frame dimensions with the acquired extent before reaching the blit route only on
  a mismatch. The post-commit package rebuild passed in 1 minute 38 seconds.
- The retained post-commit APK contains only `arm64-v8a`, reports version
  `5513c61a5-vanilla-thor`, is 29,010,804 bytes, and has SHA-256
  `74E56BD6824FEA506B03953233B8CE317C65C70244602D66A9817819D73CB8BC`. Its CMake cache records
  `ENABLE_THOR_FRAME_PROFILING=OFF`, and the final normal library contains zero profiler log or
  warning strings. Source commit `5513c61a5` was pushed directly to `origin/master` with
  command-line Git over SSH.
- Exact bounded cleanup removed 2,023,457,379 logical bytes of reproducible Gradle, JNI, native
  test, R8, symbol, and mapping staging. R8 held one generated `classes.dex` open until the Gradle
  daemon stopped; the validated second pass removed the remaining 5,181,700 bytes. Reported C:
  free space increased by 1,579,343,872 physical bytes to 52,209,246,208. The retained build output
  is only the APK plus its 476-byte metadata and the 3,247,645,239-byte active profile-disabled
  ARM64 CMake/Ninja cache. No ADB command, install, app/game launch, or runtime capture was used.
- This is optimization/candidate entry 143 and raises the overlapping ledger count to 143. It
  removes an API-level filtered transfer from the equal-size route; it does not remove the
  intermediate presentation image, the render-ready submission, swapchain acquisition, the final
  transfer command, or queue presentation.

## 2026-08-19 Android 60 Hz Game-Surface Request

- Optimization/candidate 144 addresses the panel/compositor side of the whole-frame power budget.
  The Thor primary panel can run at 120 Hz while normal 3DS presentation targets roughly 60 FPS.
  Eco Turbo limits excess host presents during fast-forward, but it does not by itself tell Android
  that the game surface has 60 Hz content.
- Azahar already attempted to select 60 Hz for `EmulationActivity`, but it required exact floating-
  point equality with `60f` and then set `preferredDisplayModeId`. The exact comparison can miss a
  59.94 Hz mode, while a mode ID expresses both refresh rate and physical resolution. The updated
  window path filters modes to the current physical resolution, accepts the closest rate within
  1 Hz of 60 for emulation, clears the mode ID, and uses the refresh-only
  `preferredRefreshRate`. Main and settings activities retain the highest same-resolution refresh
  preference rather than being globally capped to 60 Hz.
- `EmulationFragment.surfaceChanged()` now also calls
  `Surface.setFrameRate(60, FRAME_RATE_COMPATIBILITY_DEFAULT)` on the valid render surface before
  handing it to native emulation. Android's guidance explicitly uses this game-capped-at-60 case
  as a way to avoid needless high-refresh power, and the `Surface` reference specifies `DEFAULT`
  for games and other non-video content. `FIXED_SOURCE` remains intentionally unused because it is
  intended for video. These are compositor/window preferences, not a forced display switch, and
  the platform or OEM may override them.
- Official references: Android's
  [`WindowManager.LayoutParams`](https://developer.android.com/reference/android/view/WindowManager.LayoutParams),
  [`Surface.setFrameRate`](https://developer.android.com/reference/android/view/Surface),
  [frame-rate and power guide](https://developer.android.com/media/optimize/performance/frame-rate),
  and [game refresh-rate guide](https://developer.android.com/games/optimize/display-refresh-rate-change).
- Five JVM unit tests passed in 1 minute 30 seconds. They cover 59.94 Hz acceptance, closest-to-60
  selection, refusal to force unrelated 90/120 Hz modes, highest-valid frontend selection, and an
  empty mode list. The complete JDK 17 `arm64-v8a` APK build passed before commit in 2 minutes 6
  seconds and the post-commit build passed in 1 minute 46 seconds. Bytecode inspection found
  `preferredDisplayModeId = 0`, `preferredRefreshRate`, and the two-argument `Surface.setFrameRate`
  call; the final minified DEX retains both refresh APIs.
- Source commit `477243df7` was pushed directly to `origin/master` using command-line Git over SSH.
  The retained APK contains only `arm64-v8a`, reports version `477243df7-vanilla-thor`, is
  29,013,940 bytes, and has SHA-256
  `FF81DFBCDCE9D5B90A12A4120936ACEA70E8E331A871EE3B6EE388ADF15CACFA`.
- Exact bounded cleanup removed 2,033,110,980 logical bytes of reproducible Gradle, JNI, unit-test,
  R8, native-symbol, and mapping staging. Reported C: free space increased by 1,589,264,384 physical
  bytes to 51,726,405,632. The retained build output is only the APK plus its 476-byte metadata and
  the 3,247,767,013-byte active ARM64 CMake/Ninja cache. No ADB command, install, app/game launch,
  or runtime capture was used.
- This is a plausible system-level power improvement, not a measured watt or FPS result. A future
  allowed A/B must verify the active display mode and compare compositor/panel power, FPS,
  frametimes, temperature, and thermal slope with title, scene, caches, renderer, resolution,
  driver, layout, brightness, performance/fan mode, power source, and duration fixed.

## 2026-08-19 Targeted Vulkan Presentation Synchronization

- Optimization/candidate 145 addresses another every-presented-frame cost identified by the
  whole-frame pivot. `PresentWindow::CopyToSwapchain()` previously emitted a same-layout barrier for
  the intermediate frame even though the submit waits for the render submission's `render_ready`
  semaphore and the image remains `TransferSrcOptimal`. The semaphore already supplies the required
  availability and visibility for that unchanged-layout producer/consumer handoff, so the duplicate
  image barrier is removed.
- The acquired swapchain image still receives its required `Undefined` to `TransferDstOptimal`
  transition. Both `image_acquired` and `render_ready` waits now target `Transfer`, the stage that
  first consumes either image, instead of `ColorAttachmentOutput` and `AllGraphics`. The completed
  transfer's present transition is narrowed from an `AllCommands`-to-`AllCommands` barrier with
  `TransferWrite`-to-`MemoryRead` access into `Transfer`-to-`BottomOfPipe` with no destination access
  mask.
  The present-ready semaphore continues to order queue presentation. Frame reuse fences and the
  queue-idle paths used when recreating or destroying presentation resources are unchanged.
- This follows Khronos' [synchronization examples](https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples),
  which explain that a semaphore is sufficient when no image layout transition is needed and that
  wait stages should match the consumer, plus the official [pipeline-barrier performance
  sample](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html),
  which warns that broad `ALL_COMMANDS` barriers can drain the pipeline and are particularly costly
  on tile-based GPUs. Current [RPCS3 Vulkan presentation
  code](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/RSX/VK/VKPresent.cpp) likewise uses
  transfer-specific present synchronization; no RPCS3 code was copied.
- The complete pre-commit JDK 17 ARM64 package build passed in 3 minutes 17 seconds, compiling the
  changed object and linking the native tests and production `libcitra-android.so`. Source commit
  `9bb4444bb` was pushed directly to `origin/master` with command-line Git over SSH. The post-commit
  package rebuild passed in 1 minute 51 seconds and embeds that source revision.
- The retained APK contains only `arm64-v8a`, reports version `9bb4444bb-vanilla-thor`, is
  29,013,532 bytes, and has SHA-256
  `F4A57FD9E4E1DE0D884009CBBA7715C26752741CBA4C24D11066A76CE58DE6B7`. Its active CMake cache
  records `ENABLE_THOR_FRAME_PROFILING=OFF`.
- Exact bounded cleanup removed 2,023,510,839 logical bytes of reproducible Gradle, JNI, R8,
  native-symbol, mapping, and packaging staging. Reported C: free space increased by 1,579,917,312
  physical bytes to 51,477,413,888. The retained build output is only the APK plus its 476-byte
  metadata and the 3,247,883,063-byte active ARM64 CMake/Ninja cache. No ADB command, APK install,
  app/game launch, or runtime capture was used.
- One image barrier and one broad full-pipeline synchronization point are removed per presented
  frame. That is a real recurring-work reduction and therefore passes the forest/trees acceptance
  gate, but its driver-dependent FPS, frametime, and power effect is unmeasured. It may be negligible
  if the driver had already optimized the old dependency. A future allowed matched Thor A/B remains
  mandatory before claiming speed or watt gains.

## 2026-08-19 Eco Turbo FIFO and Empty-Submit Elision

- Optimization/candidate 146 removes two sources of recurring Android Vulkan queue work. Azahar's
  swapchain policy selected MAILBOX whenever the frame limit exceeded 100%, but Android Eco Turbo
  already limits host presentation/composition to 60 FPS on that path. A swapchain created or
  recreated while both conditions are active now keeps FIFO, letting the presentation queue apply
  back-pressure instead of continuously replacing frames that cannot be displayed. Eco Turbo off,
  the 0/unthrottled limit, VSync off, and the existing low-refresh override retain their prior
  behavior. Compile-time assertions cover all five policy cases.
- Present mode is chosen when the swapchain is created, not dynamically on every hotkey change.
  Therefore, a typical session that starts at 100% may already retain FIFO when Turbo is toggled.
  The direct policy change most clearly affects startup/per-game limits above 100% and swapchain
  recreation while Eco Turbo is active; no broader frequency claim is made without a runtime trace.
- On an Eco Turbo frame whose final screen is not rendered, `RendererVulkan::SwapBuffers()` now calls
  `Scheduler::FlushIfPending()`. A truly empty `CommandChunk` returns without allocating a timeline
  tick, dispatching the worker, or submitting an empty Vulkan command buffer. Any recorded emulation
  command still follows the normal `Flush()` path. This helper is used only on the no-signal/no-wait
  skipped-presentation path: render-ready/present-ready signaling, explicit readback and finish
  waits, frame fences, and resource-reuse ordering are unchanged. Delaying timeline progress after
  an empty chunk is conservative because no unfinished resource can become prematurely reusable.
- Khronos' mobile [swapchain-image and present-mode guidance](https://docs.vulkan.org/samples/latest/samples/performance/swapchain_images/README.html)
  says MAILBOX lets the CPU and GPU continue submitting and is generally not optimal for mobile,
  while FIFO reduces CPU/GPU load. The Vulkan [present-mode specification](https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VkPresentModeKHR.html)
  defines MAILBOX as replacing the single pending presentation and FIFO as appending to a queue
  consumed at vertical blank. Android's [Frame Pacing library guidance](https://developer.android.com/games/sdk/frame-pacing)
  likewise warns that submitting as quickly as possible can stuff the presentation queue. Azahar
  retains explicit semaphores, fences, and barriers as required by Android's [Vulkan native-engine
  guidance](https://developer.android.com/games/develop/vulkan/native-engine-support); FIFO is used
  for output pacing/back-pressure, not as a substitute for unrelated synchronization.
- A pre-commit normal JDK 17 package build passed in 3 minutes 35 seconds. A separate clean
  profiler-enabled ARM64 native build compiled and linked all 2,203 objects in 12 minutes 18 seconds;
  its cache recorded `ENABLE_THOR_FRAME_PROFILING=ON`, and its production shared library contained
  the new `empty_flushes_skipped` field. The normal library recorded profiling OFF and contained
  zero `ThorFrameProfile`, `empty_flushes_skipped`, or profiler-warning strings.
- Source commit `614ae8c0c` was pushed directly to `origin/master` using command-line Git over SSH.
  The exact post-commit normal package rebuild passed in 1 minute 53 seconds. The retained APK is
  ARM64-only, reports `614ae8c0c-vanilla-thor`, is 29,013,808 bytes, and has SHA-256
  `81444DFD71D5DECE0544CC78F9D88E6060ADC9F3331734C9634C62DD36673AA6`.
- Exact bounded cleanup removed 6,213,338,824 logical bytes of the temporary profiler configuration
  and reproducible Gradle/JNI/R8/native-symbol/mapping staging. Reported C: free space increased by
  4,699,185,152 physical bytes to 56,501,727,232. The retained build output is only the APK plus its
  476-byte metadata and the 3,242,035,281-byte normal profiler-OFF ARM64 CMake/Ninja cache. No ADB
  command, install, app/game launch, or runtime capture was used.
- The code removes an empty queue submission whenever the skipped-frame chunk is empty and avoids
  non-FIFO queue stuffing in the bounded swapchain cases above. That is a real whole-frame/driver-
  work reduction, not proof of a visible FPS or battery-watt percentage. A future allowed matched
  Thor A/B must hold title, scene, save, caches, renderer, resolution, driver, layout, brightness,
  performance/fan mode, power source, and duration fixed before any speed, watt, or thermal claim.

## 2026-08-19 Duplicate-Frame Preparation Elision

- Optimization/candidate 147 moves default-on duplicate-frame suppression ahead of guest display
  preparation. Previously both renderers prepared the guest screen textures before their final
  mailbox/window duplicate guard. Vulkan therefore performed two `GetSurfaceSubRect` display-
  surface cache lookups in a normal mono layout, or up to three when the right-eye surface was
  required, on a frame it would not present. Android OpenGL additionally captured the
  rasterizer state, applied presentation state, and restored the prior state even when no host
  output would be drawn.
- The shared compile-time decision table makes an explicit screenshot override suppression. A
  nonduplicate presentation or video-dump frame still prepares normally, while a duplicate dump
  retains the old inner-mailbox behavior and remains suppressed. The inner renderer guards remain
  in place. Right-eye skipped state is consumed only inside actual preparation, so a suppressed
  duplicate cannot steal the fact from the next visible frame.
- Android OpenGL now exits before `OpenGLState::Apply()` only when there is no presentation,
  screenshot, or required dump output. That exit still polls the secondary window, calls
  `EndSwap()`, runs `EndFrame()` so primary event polling and frame limiting remain intact, and
  ticks the rasterizer. Vulkan still reaches `Scheduler::FlushIfPending()`: a real emulation command
  chunk submits, while only an actually empty chunk is elided by optimization 146.
- Compile-time assertions cover ordinary presentation, suppressed presentation, screenshot
  override, duplicate/nonduplicate dumping, and a swap with no output. The normal pre-commit ARM64
  APK build passed in 3 minutes 42 seconds. Linked AArch64 inspection of
  `RendererOpenGL::SwapBuffers()` shows the no-output branch reaching `EndFrame()`/`TickFrame()`
  before the later presentation-state `OpenGLState::Apply()` call.
- A separate profiler-enabled native build compiled and linked all 2,203 steps in 11 minutes 51
  seconds. Its cache recorded `ENABLE_THOR_FRAME_PROFILING=ON`, and its final shared library
  contained the new `duplicate_prepare_skipped` log field. The post-commit production package build
  passed in 2 minutes 33 seconds with profiling OFF and zero profiler strings.
- Source commit `1d15baafb` was pushed directly to `origin/master` using command-line Git over SSH.
  The retained APK contains only `arm64-v8a`, reports `1d15baafb-vanilla-thor`, is 29,015,052 bytes,
  and has SHA-256
  `6769A549B36CAEC70CE74BE1EA6CF76288E80D5C67E6E8078DE2EB636BD3087E`.
- Exact validated cleanup removed 6,213,355,496 logical bytes of the disposable profiler cache and
  reproducible Gradle/JNI/R8/native-symbol/mapping staging. Reported C: free space increased by
  about 5.32 GB to 56,325,787,648 bytes. The retained output is the 3,242,355,745-byte normal
  profiler-OFF ARM64 CMake/Ninja cache, the APK, and its 476-byte metadata. No ADB command, install,
  app/game launch, or runtime capture was used.
- The latest fetched `upstream/master` remains `f6a3e3aa5` and is already an ancestor of this fork.
  This change removes real recurring host work on duplicate frames, but its frequency depends on
  the title and pacing. No FPS, frametime, watt, temperature, or battery-life gain is claimed until
  a future permitted matched Thor A/B holds the full test matrix fixed.

## 2026-08-19 Targeted Vulkan Resolution-Scale Synchronization

- Optimization/candidate 148 addresses the remaining broad synchronization in Vulkan's recurring
  texture-scale route. `Surface::BlitScale()` previously surrounded each Base-to-Scaled or
  Scaled-to-Base `vkCmdBlitImage` with two image-barrier calls whose outer stage was
  `AllCommands`. The dependency could therefore include unrelated pipeline work even though the
  operation's actual producer and consumer stages are known from the surface's permitted uses.
- The two barrier calls remain, but their broad stage and generic access scopes do not. They now use
  the same `PipelineStageFlags()` and `AccessFlags()` model as `TextureRuntime::BlitTextures()`, with
  `TransferRead`/`TransferWrite` at the blit boundary. Base and Scaled images belong to the same
  `Surface` and share those usage flags. The source and destination images, filter, aspect, general
  and transfer layouts, mip/layer ranges, `eByRegion` dependency, and D24S8 unsupported-hardware
  fallback are unchanged.
- This route runs after scaled texture uploads, before scaled downloads, and once per mip during
  `ScaleUp()`. It is not used when internal resolution is the default 1x, and uploads handled by the
  specialized color-filter helper may bypass it. The change therefore removes two
  `ALL_COMMANDS` stage scopes per scale blit only when this path actually executes; it is not a
  universal per-frame or default-configuration gain.
- The synchronization model follows Khronos' [synchronization
  examples](https://github.com/KhronosGroup/Vulkan-Docs/wiki/synchronization-examples) and official
  [pipeline-barrier performance
  sample](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html),
  which recommend matching dependencies to the real producer and consumer rather than draining
  unrelated stages. Current [RPCS3 Vulkan presentation
  code](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/RSX/VK/VKPresent.cpp) likewise uses
  transfer-specific barriers; no RPCS3 code was copied.
- An ordinary profiling-off ARM64 native build passed in 1 minute 50 seconds. Its linked AArch64
  `BlitScale` command uses the captured surface stage mask on each side of literal Transfer and no
  longer materializes an `ALL_COMMANDS` stage. A separate profiler-enabled build compiled and
  linked all 2,203 actions in 12 minutes 3 seconds; its cache recorded
  `ENABLE_THOR_FRAME_PROFILING=ON`, and the final shared library contained the new `scale_blits` and
  `scale_blit_mpix` diagnostic fields.
- Source commit `bea82a722` was pushed directly to `origin/master` using command-line Git over SSH.
  The post-commit production package build passed in 3 minutes 22 seconds. The retained APK contains
  only `arm64-v8a`, reports `bea82a722-vanilla-thor`, is 29,014,128 bytes, and has SHA-256
  `139E9696E1DEE47AF26031A10793494F6F7DEBEDD29116BB35F170A8E7122D96`. Its normal cache records
  profiling OFF, and its linked native library contains zero Thor profiler strings.
- Exact bounded cleanup removed 6,213,332,852 logical bytes of the disposable profile cache and
  reproducible Gradle/JNI/R8/native-symbol/mapping staging, recovering 6,045,159,424 physical bytes
  on C:. The retained output is the 3,242,525,598-byte normal profiling-off ARM64 CMake/Ninja cache,
  the APK, and its 476-byte metadata; C: reports 55,614,570,496 bytes free. No ADB command, install,
  app/game launch, or runtime capture was used.
- The latest fetched `upstream/master` remains `f6a3e3aa5` and is already an ancestor of this fork.
  The static recurring-work reduction is real for affected scale blits, but driver scheduling,
  whole-frame FPS, frametime, watts, thermals, and battery life remain unmeasured. A future permitted
  matched Thor A/B must hold title, scene, save, caches, renderer, resolution, driver, layout,
  brightness, performance/fan mode, power source, and duration fixed before making those claims.

## 2026-08-19 Combined Vulkan Presentation Submission

- Optimization/candidate 149 addresses the largest structurally recurring host-side synchronization
  sequence left in Vulkan's final presentation path after entries 143 and 145. The old path
  submitted composition through the scheduler, signaled a per-frame `render_ready` binary
  semaphore, acquired a swapchain image,
  recorded a second presentation command buffer, and called `vkQueueSubmit` again with a per-frame
  `present_done` fence. Frame reuse later performed a host `waitForFences`/`resetFences` cycle.
- Final composition and swapchain transfer now occupy one scheduler command buffer and one queue
  submission. A worker-time prepare callback acquires and captures the exact swapchain image,
  image-acquired semaphore, and image-specific present-ready semaphore; records the exact image
  barriers plus copy/blit; and supplies an `image_acquired` wait scoped to `Transfer`. The scheduler
  submission signals both its ordinary completion primitive and `present_ready`. Its post-submit
  callback then queues the frame, and the present thread only calls `vkQueuePresentKHR`.
- The recurring separate presentation command-buffer begin/end/cycle, second `vkQueueSubmit`,
  `render_ready` signal/wait handoff, and `present_done` fence signal/wait/reset are removed. At 60
  displayed frames per second, render plus present therefore changes from up to 120 graphics-queue
  submissions to 60. This is a structural driver-call count, not a 2x emulator-speed claim: the
  guest CPU/GPU work, final layout composition, swapchain acquisition/present, and full-frame
  copy/blit all remain.
- Correctness remains explicit. Final color-attachment writes become transfer reads in the same
  command buffer; the acquired image transitions from `Undefined` through `TransferDstOptimal` to
  `PresentSrcKHR`; and the intermediate image returns from `TransferSrcOptimal` to `General` with
  a `TransferRead` to future `ColorAttachmentWrite` dependency. Vulkan queue order makes that
  barrier apply to later submissions on the same queue, so frame reuse no longer needs a host
  fence. Acquisition failure records a restore-only barrier before swapchain recreation, and
  `submit_mutex` externally synchronizes graphics submit and present calls.
- This design follows current [RPCS3 Vulkan presentation
  structure](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/RSX/VK/VKPresent.cpp), which
  records presentation transfer work in its frame context and submits once with acquire/present
  semaphore handoff. Khronos' [`vkCmdPipelineBarrier`
  reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPipelineBarrier.html)
  defines the same-queue memory dependency across commands and later submissions, while the
  official [synchronization
  examples](https://github.com/KhronosGroup/Vulkan-Docs/wiki/synchronization-examples) and
  [pipeline-barrier performance
  sample](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html)
  support matching waits/barriers to the real transfer consumer. No RPCS3 code was copied.
- The profiling-off ARM64 native rebuild passed in 2 minutes 27 seconds. Its cache records
  `ENABLE_THOR_FRAME_PROFILING=OFF`; linked symbols include the combined scheduler command,
  `PrepareForPresent`, `FinishPresent`, and the exact-stage master-semaphore submission API, while
  the normal library contains zero `ThorFrameProfile` strings. A separate profiling-on cache built
  and linked all 2,203 actions in 13 minutes 27 seconds and contains the new
  `combined_submissions` proof field.
- Source commit `e35688c03d` was pushed directly to `origin/master` with command-line Git over SSH.
  The post-commit normal APK build passed in 3 minutes 43 seconds. The retained APK contains only
  `arm64-v8a`, reports `e35688c03-vanilla-thor`, is 29,004,056 bytes, and has SHA-256
  `9740416CF9B6F8A4C24B3D409BB29C6BF61987DE6325F9600ECB01A4C18285CF`; its active cache keeps
  profiling off.
- Exact bounded cleanup removed 6,212,291,027 logical bytes: the disposable profiling cache plus
  reproducible Gradle/JNI/R8/native-symbol/mapping staging. The retained output is the
  3,242,354,720-byte normal ARM64 CMake/Ninja cache, the APK, and its 476-byte metadata; C: reports
  55,263,846,400 bytes free. No ADB command, install, app launch, game launch, or runtime capture
  was used.
- The latest fetched `upstream/master` remains `f6a3e3aa5` and is already an ancestor of this fork.
  Static proof guarantees one fewer queue submission and host fence cycle per successfully
  presented frame, but the effect on FPS, frametime variance, watts, thermals, and battery life is
  workload- and driver-dependent. Expect the strongest benefit in driver/CPU-bound or lightly
  loaded scenes and a smaller result when Adreno shader/fill work dominates. A matched Thor A/B is
  still required before attaching a numeric speed or power claim.

## 2026-08-19 Procedural Vulkan Presentation Quads

- Optimization/candidate entry 150 removes recurring CPU buffer work from Vulkan's final layout
  pass. The prior renderer built a four-vertex `ScreenRectVertex` array for every visible 3DS
  screen, mapped a host stream-buffer slice, copied and committed 64 bytes, and bound that vertex
  buffer before each draw. A normal two-screen frame paid that sequence twice; stereo and
  additional-screen layouts paid it for every extra draw.
- The present vertex shader now derives the quad corner from `gl_VertexIndex`. Push constants carry
  the screen rectangle, texture-coordinate rectangle, framebuffer transform, input/output
  resolutions, screen ID, layer, reverse flag, and orientation. Landscape, portrait, and both
  flipped orientations generate the same four clip-space positions and texture coordinates as the
  former CPU arrays. The final-present pipelines have no vertex-input binding. The retained
  128 KiB stream buffer remains available only for the optional software cursor path.
- NDK 27 `glslc` compiled the normal, anaglyph, interlaced, and Anime4K fragment variants plus the
  shared vertex shader for Vulkan 1.1. A table-driven equivalence check covered all four vertices
  in all four orientations. The profiling-off ARM64 release APK build passed in 3 minutes 53
  seconds, and source commit `4dba0e934` was pushed directly to `origin/master` over SSH.
- On-device testing initially exposed an unrelated earlier Dynarmic page-table regression before
  any presentation result could be trusted. A clean build without this entry reproduced that same
  JIT abort, excluding the quad change. After the page-table fix, builds containing entry 150 kept
  Art Academy and 7th Dragon alive and visibly rendered both title screens on the Thor. No Vulkan
  shader, pipeline, or layout error was logged.
- This guarantees fewer host maps, tiny CPU copies, stream-buffer commits, and vertex-buffer bind
  commands per presented frame. Vertex work remains four trivial vertices per screen draw, and the
  full composition/render-target/swapchain transfer still occurs. FPS, frametime, watts, thermals,
  and battery life remain unmeasured. Because the absolute-offset page-table entry was withdrawn,
  this is numbered ledger entry 150 but leaves 149 active accepted entries rather than hiding the
  regression by inflating the total.

## 2026-08-19 Vulkan Acquire Recovery and Uncapped Power Guard

- The on-device power investigation found `Renderer_FrameLimit: 0` in the active Azahar launch
  log. On 7th Dragon's visible title screen the overlay reported 412 game FPS. A 20-second Thor
  mode-2 sample consumed 1,484 process CPU ticks and the two KGSL busy snapshots were 99.67% and
  99.76%, with the Adreno at 680 MHz. No screen or texture filter was active; Vulkan, 3x internal
  resolution, dual displays, Turnip Mesa 25.99.99, and Eco Turbo were active.
- Restoring **Limit Speed** to its normal 100% value changed the same visible title screen to 30
  game FPS. The matched 20-second sample used 520 process CPU ticks and KGSL busy snapshots of
  9.53% and 9.98%, with the GPU at Thor mode 2's 615 MHz floor. That is 65.0% less Azahar process
  CPU time and roughly 90% less GPU active share in this title-screen test. It is not a whole-game
  speedup or watt figure: the device was AC-powered, so battery-current telemetry could not isolate
  emulator power. A temporary mode-0 check lowered the GPU floor to 401 MHz, but that capture was
  not a visually identical scene and is not promoted to a power percentage. Mode 2 was restored.
- The uncapped run also exposed a correctness failure in entry 149's combined presentation path.
  The Vulkan worker held `swapchain_mutex` across an effectively unbounded image acquire while the
  presentation thread needed the same lock to return older images. Turnip eventually returned
  numeric result 2 (`VK_TIMEOUT`), which the code treated as unreachable and aborted in
  `Swapchain::AcquireNextImage()`. Rotating binary acquire semaphores also lacked proof that the
  prior queue wait had completed before a semaphore could be signaled again.
- Commit `2a265cd23` uses a finite one-millisecond acquire timeout, treats `VK_TIMEOUT` and
  `VK_NOT_READY` as retryable, releases the swapchain host lock between retries, and consumes valid
  suboptimal acquisitions instead of abandoning a signaled semaphore. Each presentation frame now
  owns its acquire semaphore and records the scheduler submission tick; frame reuse waits for that
  tick so the binary wait is complete before the semaphore can be signaled again. The submission
  tick is published inside the worker callback before `queue_mutex` exposes the frame to the
  presentation thread, avoiding a producer/presenter data race.
- The profiling-off ARM64 native and package builds passed. The corrected APK visibly rendered 7th
  Dragon and stayed alive for multiple minutes, and Art Academy stayed alive beyond 55 seconds;
  both exceeded the old roughly 18-second acquire failure. With the normal 100% cap, neither title
  logged a fatal signal, swapchain-acquire abort, or dequeue timeout during the recorded runs.
- Optimization/candidate entry 151 closes the remaining power trap without redefining uncapped
  emulation. Commit `774a2ce5a` makes Eco Turbo's 60 FPS host-presentation token budget apply when
  the frame limit is either above 100% or exactly zero/unthrottled. Guest CPU/PICA emulation remains
  uncapped. The same predicate keeps FIFO back-pressure when VSync is enabled, and compile-time
  assertions cover normal, explicit Turbo, uncapped, Eco-off, and low-refresh policy states. The
  Android UI now explicitly warns that disabling **Limit Speed** can produce hundreds of FPS and
  high power use.
- In a follow-up uncapped candidate run, the overlay reached 615 game FPS, the GPU remained about
  99.8% busy because guest PICA rendering itself was still uncapped, and the process used 1,336
  ticks over 20 seconds. The important correctness result was zero dequeue/acquire-timeout log
  lines instead of the baseline timeout storm. Eco Turbo can remove surplus host composition; it
  cannot make maximum-rate guest rendering low power. The final user configuration was restored to
  `use_frame_limit = true`, Thor performance mode 2 was restored, and Azahar was force-stopped.
- Entry 151 raises the ledger to 151 numbered entries and 150 active accepted entries because the
  unsafe absolute-offset ARM64 page-table entry remains withdrawn. The numeric CPU/GPU reductions
  above apply only to enabling the normal cap in this title scene and must not be added to other
  optimization percentages or translated into battery watts.

## 2026-08-19 Direct-to-Swapchain Android Vulkan Composition

- Optimization/candidate entry 152 removes the intermediate full-frame transfer when Android's
  final layout dimensions exactly match the current Vulkan swapchain. The renderer performs one
  finite acquire before recording final composition. If an image is immediately available, that
  swapchain image and its Android-only framebuffer become the final color attachment; the render
  pass discards prior contents on entry and finishes in `PresentSrcKHR`.
- Correctness and recovery are deliberately conservative. Acquisition still uses the frame-owned
  binary semaphore whose prior scheduler submission tick is complete before reuse. The combined
  submission waits at `ColorAttachmentOutput`, and an explicit external-to-subpass dependency makes
  the acquired image available to color writes. A retry, surface recreation, invalid swapchain,
  extent mismatch, or incomplete direct framebuffer set returns to the established intermediate
  image and copy/blit path. Swapchain recreation waits for the graphics queue, destroys the direct
  framebuffers and views before the old swapchain, and creates replacements only after a valid new
  image set exists.
- A successful direct frame eliminates the intermediate presentation image's final write-to-
  transfer dependency, the full-frame copy, both transfer-image transitions, and the intermediate
  image's post-copy restore barrier. It does not remove layout composition draws, swapchain acquire
  or present, the graphics submission, or guest rendering. The opt-in profiler now reports
  `PresentDirectRenders` separately from fallback copies and blits; ordinary builds still compile
  the profiling operations out.
- The indexed local Qualcomm Adreno Game Developer Guide was read directly and its SHA-256 matched
  `0872AA49B763ACB46AEB7427784E926D2BF3939F2E731B405DEF977A5BFAECAC`. Pages 60-61 recommend
  minimizing final render passes and avoiding wasteful GMEM-to-system-memory resolves; page 64
  says Vulkan image layouts should be specific and recommends FIFO swapchains for efficient GPU
  use and battery behavior. Page 70 documents blit/SurfaceFlinger scaling for mismatched final
  sizes, which is why this change retains the existing copy/blit fallback instead of stretching the
  direct route beyond exact extents. Those manual statements rank the candidate; the profiler and
  pixel comparison below are the acceptance evidence.
- The synchronization model follows Khronos' [`VkSubmitInfo`
  reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkSubmitInfo.html), including
  waiting at the first stage that consumes an acquired image, and its official [swapchain semaphore
  reuse guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html), which ties
  presentation-finished semaphore reuse to swapchain images.
- JDK 17 Kotlin compilation passed all 22 tasks. A clean profiling-off ARM64 package build compiled
  all 2,203 native actions and linked both `libcitra-android.so` and the native test executable; the
  later profiler-on and final profiler-off packages also passed. The profiler cache records
  `ENABLE_THOR_FRAME_PROFILING=ON` and its linked library contains the
  `ThorFrameProfile present ... direct={}` format. The final normal cache records the option `OFF`,
  and the linked normal library contains no `ThorFrameProfile present` string.
- Wi-Fi ADB identified the physical target as AYN Thor / QCS8550 on Android 13 with Turnip Adreno
  740. The installed old profiler build booted 7th Dragon III (`000400000018F800`) to its animated
  title at a visible 30 FPS. Its steady 5.014-second windows reported `swaps=300`, `presented=300`,
  `present copies=300`, and `511.920` presentation MPix. The new profiler build booted the same
  title and its steady windows reported `presented=300`, `direct=300`, `copies=0`, and the same
  `511.920` MPix. Thus every presented exact-size frame in this scene took the direct route and the
  measured full-frame copy count fell from one per presentation to zero.
- Profiler-only follow-up commit `ca82a5fc3` attributes the next apparent whole-frame costs at the
  rasterizer-cache call sites. In every steady 300-swap window, all 150 internal copies / 129.600
  MPix were accelerated guest PICA texture-copy commands, all 300 internal blits / 233.280 MPix
  were accelerated guest PICA display-transfer commands, and cache-validation copies and blits
  were both zero. This matches one scaled 400x240 copy plus the scaled 400x240 and 320x240 display
  transfers per 30 FPS guest frame. They are guest-visible framebuffer operations rather than a
  second removable host presentation pass. The attribution counters compile out of ordinary
  builds and are not a ledger entry; any attempt to alias or eliminate those transfers requires
  exact PICA memory/coherency proof and broader title coverage.
- Correctness was checked independently of counters. The old-copy screenshot, profiler-on direct
  screenshot, and profiling-off direct screenshot are byte-identical with SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`. There were no fatal log
  entries. The accepted native implementation is commit `57044971c`, pushed to `origin/master`.
- The profiling-off ARM64-only APK is 29,010,992 bytes with SHA-256
  `206B09DE56B0A5078A60715E18756CB7AB9F468E8E2445A09BA72F5EE9CE1FDF`. It reports package
  `org.azahar_emu.azahar.debug`, version `57044971c-vanilla-thor`, minimum SDK 29, and target SDK 37,
  installed successfully over Wi-Fi ADB, and booted the same title without profiler log lines.
- Entry 153 targets the next measured CPU cost without changing page-table representation.
  Simpleperf attributed 2.57% of a steady 30-second 7th Dragon title capture to
  `ARM_Dynarmic::SetPageTable`. The scheduler commonly passed the exact same shared page-table
  object to a live JIT, but the function still copied the complete register banks out and back into
  that same JIT. Commit `10cb11ad7` returns only for that exact live-JIT/same-object case; initial
  JIT construction and real page-table changes retain context save, lookup/creation, and restore.
- Three matched 15-second baseline runs averaged 4,162.069 ms task-clock, 8.093625 billion cycles,
  and 2.749295 billion retired instructions. Three candidate runs averaged 4,052.940 ms,
  7.878676 billion cycles, and 2.602196 billion instructions: reductions of 2.622%, 2.656%, and
  5.350%, respectively. Mean sampled frequency also fell from 1.893561 to 1.888335 GHz. A second
  30-second profile reduced `SetPageTable` itself from 2.57% to 0.45% of sampled cycles.
- The native test executable linked, 7th Dragon reproduced screenshot SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`, and Art Academy reproduced
  `5C64ED5BC0A4B10DF61376E71498D8285D0C48B2A9663B7E2EBD27D7187DF932`. Both titles remained alive
  without page-table, fastmem, fatal, or Vulkan device-lost logs. The measurements used a temporary
  debuggable package with native frame profiling disabled; the Lite package remains the acceptance
  and battery-power target.
- Entry 154 follows the next measured scheduler costs without removing a scheduling action. The
  pre-change 30-second profile attributed 1.07% of sampled cycles to `SetRunningCPU`, 0.90% to
  `SetCurrentProcess`, and 0.30% to `std::__shared_weak_count::lock`. Commit `6b3c1b6d8` passes the
  process/page-table `shared_ptr` values by const reference and avoids assigning retained ownership
  when it already points at the same object. Memory page-table selection, the guarded live-JIT
  selection, CPU/timer switching, and `owner_process.lock()` remain intact.
- Three immediately preceding 15-second runs averaged 4,052.940 ms task-clock, 7.878676 billion
  cycles, 2.602196 billion retired instructions, and 1.888335 GHz. Three candidate runs averaged
  4,072.964 ms, 7.912913 billion cycles, 2.520637 billion instructions, and 1.888516 GHz. Thus
  instructions fell 3.134%, while task-clock (+0.494%), cycles (+0.435%), and frequency (+0.010%)
  remained within run noise. This is accepted as lower recurring CPU work at equal measured speed,
  not as a frametime or power improvement.
- In the post-change 30-second profile, `SetRunningCPU` fell to 0.73% and `SetCurrentProcess` fell
  below the 0.20% reporting floor. The required weak-pointer lock remained 0.31%, effectively the
  same as its 0.30% baseline. The temporary frame-profiler-disabled debug APK linked all native
  tests, reproduced 7th Dragon SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C` and Art Academy SHA-256
  `5C64ED5BC0A4B10DF61376E71498D8285D0C48B2A9663B7E2EBD27D7187DF932`, and emitted no fatal log.
  AC remained connected at 80%, so these observations do not prove lower battery watts.
- The final profiling-off Lite APK built from pushed documentation/source head `8352ca9d6` is
  ARM64-only, 29,009,128 bytes, and SHA-256
  `8EBB33FAFA6AE2CAF3D1824FD6C8BD236B2CF8160F2EA74367F9ADAF935865D1`. The installed package is
  version `8352ca9d6-vanilla-thor`, minimum SDK 29, target SDK 37, and non-debuggable; the native
  library contains no `ThorFrameProfile` string. Wi-Fi ADB installation succeeded, the final Lite
  process remained alive and reproduced 7th Dragon SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`, and its log contained no
  fatal, profiler, page-table, fastmem, or Vulkan device-lost evidence. The user's High Performance
  and smart-fan modes remained restored at values 2 and 4. AC remained connected at 80%, 4.264 V,
  and 25.0 C, so the final run remains correctness/stability evidence rather than battery-watt
  evidence.
- Call-stack attribution rejected an atomic no-signal fast path before implementation. Although
  `RunLoop` appeared prominently in the flat profile, only about 0.05% of whole-app sampled cycles
  were attributable to its ordinary signal-mutex lock. That does not justify changing asynchronous
  reset/save/load/shutdown observation timing, so the mutex contract remains unchanged.
- Entry 155 instead removes a recurring cross-language call identified under renderer and cache
  work. `GetResolutionScaleFactor()` calls `GetWorkingGraphicsAPI()` from final-screen drawing and
  cache ticks. On Android, that function crossed JNI each time to ask whether the Java-side OpenGL
  renderer string contained `ANGLE`, even though the string is fixed for the process lifetime.
  Commit `d75d854d4` caches only that boolean once; mutable graphics settings and every prior
  ANGLE-to-Vulkan result remain unchanged.
- Three matched baseline runs averaged 4,035.343 ms task-clock, 7.839703 billion cycles,
  2.515142 billion retired instructions, and 1.886438 GHz. Three candidate runs averaged
  4,040.253 ms, 7.847734 billion cycles, 2.518091 billion instructions, and 1.886515 GHz:
  differences of +0.122%, +0.102%, +0.117%, and +0.004%, respectively, all within run noise. No
  whole-app speed result is claimed.
- The baseline `GetResolutionScaleFactor` call tree entered Java CheckJNI below the native ANGLE
  query. In a second 30-second capture after startup, the same function's recovered call tree was
  self-only, proving the recurring JNI subtree was removed. The native test target linked, 7th
  Dragon reproduced SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`, Art Academy reproduced
  `5C64ED5BC0A4B10DF61376E71498D8285D0C48B2A9663B7E2EBD27D7187DF932`, and no fatal log appeared.
  Debug-app CheckJNI magnifies the boundary cost, so this is accepted only as exact recurring-work
  elimination, not as a normal-Lite FPS or watt improvement.
- A follow-up `dirty_regions.empty()` early return in `RasterizerCache::FlushRegion` was rejected
  and reverted. Three candidate runs appeared to reduce mean task-clock 0.51%, cycles 0.49%, and
  instructions 0.37%, but the ranked function itself rose from 1.03% to 1.12% of sampled cycles.
  The dirty map therefore was not usually empty in this workload, and the apparent aggregate change
  was run noise rather than attribution-confirmed work removal. Required interval ownership and
  dirty-surface download behavior remain unchanged; this is not entry 156.
- A device-policy follow-up checked the forest-level power control before selecting another small
  code path. The Thor Quick Settings UI identified `performance_mode=2` as High Performance. In the
  final profiling-off 7th Dragon title scene it pinned Adreno at 615 MHz, used 517 process CPU ticks
  over the matched interval, and reported 8.268% mean / 8.404% P95 KGSL busy. The setting was
  temporarily changed through the vendor UI to Standard (`performance_mode=0`), then restored.
- Standard held Adreno at 401 MHz, used 509 process ticks, and reported 10.375% mean / 10.499% P95
  busy while reproducing the exact established screenshot hash. A cleared 20-second SurfaceFlinger
  window measured 33.431 ms mean / 33.524 ms median / 34.314 ms P95 with no interval over 50 ms;
  restored High Performance measured 33.444 / 33.516 / 34.355 ms with the same zero-drop result.
  Thus Standard is the default recommendation for capped titles and the pending under-6-W run,
  escalating only if a title misses speed. AC remained connected, so lower battery watts are not
  claimed from frequency and utilization alone.
- Power remains an explicit open gate. Both 60 Hz panels were on, primary brightness was 255,
  secondary brightness was 100, Thor performance mode was 2, fan mode was 4, and Adreno was held
  at 615 MHz. The normal build's single steady sample used 25.54% of one CPU core with KGSL busy
  mean 8.03% and P95 8.17%, but the battery reported AC-powered at 80%, 4.265 V, and 23.0 C with
  `current_now=0`. Those are utilization observations only. No watt, thermal, battery-life, or
  whole-game FPS improvement is claimed until the charger is physically unplugged and a matched
  discharge capture confirms the required ceiling.
- Entry 156 removes detailed frame-breakdown timing from Android's normal hidden-overlay path.
  SVC, IPC, GPU, and swap accounting each took high-frequency `steady_clock::now()` samples even
  when only normal FPS/system frametime reporting was needed. Commit `65e2f5a9f` keeps those scopes
  behind a native gate that follows `PERF_OVERLAY_ENABLE && PERF_OVERLAY_SHOW_FRAMETIME`; normal
  FPS, emulation-speed, and system-frametime statistics remain active. Per-scope active booleans
  preserve matched nesting across a live setting change, and the JNI state refreshes both during
  overlay updates and when `EmulationFragment` resumes from Settings.
- A same-binary three-run-by-three-run 15-second 7th Dragon comparison averaged 3,887.149 ms
  task-clock, 7.586378 billion cycles, and 2.448694 billion retired instructions with the detailed
  breakdown hidden, versus 3,895.499 ms, 7.607516 billion, and 2.475697 billion with it enabled.
  Hiding the unused breakdown reduced task-clock 0.214%, cycles 0.278%, and retired instructions
  1.091%; sampled frequency differed by only -0.030%. This is a reduction in recurring CPU work,
  not a demonstrated FPS or watt reduction.
- In the 30-second hidden-overlay profile, `__kernel_clock_gettime` self share was 0.29% and
  `std::chrono::steady_clock::now` self share was 0.02%, down from 1.11% and 0.86% in the prior
  baseline profile. Caller recovery found only required renderer, frame-limiter, and system timing;
  the SVC, IPC, GPU, and swap detailed scopes were absent. On the same candidate, enabling the
  detailed UI showed nonzero `CMD`, `SWP`, `IPC`, and `SVC` fields; returning from Settings then
  immediately restored the FPS-only overlay. The full RelWithDebInfo build linked the production
  native library and ARM64 native-test executable. The FPS-only run reproduced 7th Dragon SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C` with no fatal, profiler,
  page-table, fastmem, or Vulkan device-lost logs.
- Audio decode was also rejected as the next target: `DecodePCM16` was only 0.14% inclusive and
  `DequeueBuffer` 0.20% in the ranked profile. Replacing its buffer container would add churn and
  risk for less measured cost than the accepted timing gate, so the audio path remains unchanged.
- A follow-up scheduler experiment was rejected and fully reverted. `PopNextReadyThread` was 0.65%
  self in the accepted profile, and `ThreadQueueList` walked priorities that had once been used even
  after their deques emptied. A candidate maintained a 64-bit nonempty-priority mask, rebuilt it
  after savestate loads, and selected the first live priority with AArch64 `RBIT`/`CLZ`. Its focused
  ARM64 tests passed 18 assertions covering ordering, reuse, move/clear, and binary save/load, but
  linked `PopNextReadyThread` grew from 1,240 to 1,284 bytes.
- Three candidate runs averaged 3,928.017 ms task-clock, 7.674068 billion cycles, 2.493656 billion
  retired instructions, and 1.921900 GHz. Against the immediately preceding hidden-overlay means,
  those are regressions of 1.051%, 1.156%, and 1.836%, with frequency only 0.110% higher. A new
  30-second profile also raised `PopNextReadyThread` itself from 0.65% to 0.73%. Maintaining the mask
  on every queue mutation cost more than eliminating the scan in this title, so no scheduler code
  or test from the experiment remains and this is not entry 157. The clean rebuild restored the
  linked function to 1,240 bytes, and the final Lite device checks above were repeated after that
  rebuild.
- A second scheduler experiment was also rejected and fully reverted. The accepted profile placed
  `Core::Timing::UnscheduleEvent` at 0.33% self because `SwitchContext` cancelled a possible thread
  timeout whenever a ready thread was selected. A candidate instead skipped cancellation for the
  timeout callback, whose event had already left the heap, and cancelled early-wakeup timeouts once
  in `ResumeFromWait`. The ARM64 binary linked successfully and all 85 assertions in the four
  `CoreTiming` test cases passed directly on Thor. The candidate reproduced the exact 7th Dragon
  SHA-256 `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C` and produced no fatal,
  profiler, fastmem, page-fault, or Vulkan device-lost log evidence.
- The relocation did not reduce whole-app work. Three 15-second candidate samples averaged
  3,906.960 ms task-clock, 7.626750 billion cycles, 2.470328 billion retired instructions, and
  1.920094 GHz. Against the accepted hidden-overlay means, task-clock, cycles, and instructions
  regressed 0.510%, 0.532%, and 0.883%, while frequency differed by only +0.016%. A new 30-second
  profile reduced `UnscheduleEvent` from 0.33% to 0.08%, but `ResumeFromWait` rose from 0.14% to
  0.24% and `ThreadWakeupCallback` from 0.16% to 0.22%; the measured cost was moved rather than
  removed. No source or test change remains and this is not entry 157. Thor was still AC-powered at
  80%, 4.264 V, and 25.0 C, so no battery-watt conclusion is drawn from the rejected experiment.
- A third scheduler experiment removed `ready_queue.remove()` from `ThreadManager::SwitchContext`
  after observing that the queue's ordinary selection helpers pop their result. A clean ARM64
  RelWithDebInfo build succeeded, and the broad on-device `[core]` run passed 61 of 62 test cases
  and 439,504 of 439,505 assertions; its only test failure was the Android test harness lacking the
  unrelated `get_build_flavor` function. Those tests did not exercise the scheduler's self-switch
  path, and the candidate crashed 7th Dragon within one second of launch with SIGTRAP at
  `SwitchContext+964`: `Thread must be ready to become running`.
- The failed invariant was isolated before profiling. When no higher-priority thread is available,
  `PopNextReadyThread` returns the current running thread without popping it. `SwitchContext` first
  pushes that same previous thread onto the ready queue, and the removed operation is what takes it
  back out before marking it running. Leaving it queued causes a later reschedule to select a
  running, rather than ready, thread. The unconditional deletion was fully reverted and is not
  entry 157; any refined fast path must retain removal when `new_thread == previous_thread` and
  repeat the real game-launch gate that exposed the missing unit-test coverage.
- Entry 157 takes the safe scheduler fast path one level higher. After
  `PopNextReadyThread()` performs runnable selection and core-1 CPU-limit handling, `Reschedule()`
  now returns immediately when the result is the exact current thread. In that case there is no
  context handoff to perform; the old path saved and loaded the same CPU context, temporarily
  requeued and removed the same thread, cancelled a nonexistent running-thread timeout, and
  repeated process/TLS checks. Real thread changes and thread-to/from-idle transitions still run
  the complete `SwitchContext` path, including the required ready-queue removal.
- The accepted ARM64 RelWithDebInfo APK was 32,436,895 bytes with SHA-256
  `CB4D0D201B9534F6958FBE98D26882D8AA58E5B790E59B0BAAFC37384FDA05F8`. Its linked 450 MB native
  test binary ran directly on Thor and repeated the broad result: 61 of 62 `[core]` cases and
  439,504 of 439,505 assertions passed, with only the Android harness-only missing
  `get_build_flavor` failure. Unlike the rejected deletion, the refined build launched and held
  both real scenes: 7th Dragon reproduced SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`, and Art Academy reproduced
  `5C64ED5BC0A4B10DF61376E71498D8285D0C48B2A9663B7E2EBD27D7187DF932`, with no fatal, assertion,
  page-fault, fastmem, profiler, or Vulkan device-lost log match.
- Three separate 15-second 7th Dragon samples averaged 3,840.967 ms task-clock, 7.499688 billion
  cycles, 2.428297 billion retired instructions, and 1.921733 GHz. Against the accepted
  hidden-overlay baseline, those are reductions of 1.188%, 1.143%, and 0.833%, while sampled
  frequency was only 0.101% higher. A new 30-second, 7,582-sample call-graph profile lost zero
  samples and supported the intended mechanism: `SwitchContext` inclusive share fell from 1.75%
  to 1.32%, while `UnscheduleEvent` self share fell from 0.33% to 0.16%. This is measured recurring
  CPU-work removal, not a demonstrated FPS or battery-watt improvement. Thor was still AC-powered
  at 80%, 4.265 V, 25.0 C, `performance_mode=2`, and `fan_mode=4`.
- After source and evidence commits `7d31114d6` and `919279705` were pushed, the required JDK 17
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build passed in 1 minute 14
  seconds. The final ARM64-only, v2-signed APK is 29,008,524 bytes with SHA-256
  `1DCF223144A8310D92E73D5C88A8EF04AEDFA1D755AA827776ED50BEB7B4FC2E`; signer-certificate SHA-256
  remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`. It reports package
  `org.azahar_emu.azahar.debug`, version `919279705-vanilla-thor`, minimum SDK 29, and target SDK 37.
  The native cache records `ENABLE_THOR_FRAME_PROFILING=OFF`, the linked library has zero profiler
  marker strings, and the installed package flags contain no `DEBUGGABLE` bit. Wi-Fi ADB installed
  this exact artifact; its final 7th Dragon run again reproduced SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C` with no fatal, assertion,
  page-fault, fastmem, profiler, or Vulkan device-lost log match.
- Entry 157 raises the ledger to 157 numbered entries and 156 active accepted entries because the
  unsafe absolute-offset ARM64 page-table entry remains withdrawn. Entries 152 through 157 are
  measured recurring presentation-traffic and CPU-work reductions, not additive speed percentages.

- Entry 158 addresses the highest exact actionable function in the new 7th Dragon profile without
  weakening SoundTouch's audio-quality policy. Android AArch64 integer-NEON stereo full search now
  calculates four adjacent WSOLA correlations together: one compare load is shared, two contiguous
  input loads form all four candidates with `EXT`, and the paired multiply, arithmetic shift,
  32-bit accumulation, heuristic, and best-offset order remain unchanged. Four normalizer deltas
  are derived from contiguous removed/added samples and applied sequentially. Non-AArch64,
  non-NEON, OpenMP, alignment-avoidance, non-stereo, and scalar-tail cases retain the prior path;
  `quickseek` remains disabled because its documented speed trade also accepts minor quality loss.
- The first correct helper expanded to 3,328 bytes because Clang replicated four general
  channel-count normalizer loops. The accepted implementation batches the stereo deltas explicitly
  and links as a spill-free 568-byte `calcCrossCorrBatch4`, an 82.9% helper-size reduction. The
  pinned JDK 17 / NDK 27.3 ARM64 RelWithDebInfo build passed, and its binary ran directly on Thor:
  all five focused SoundTouch cases passed with 3,776 assertions. The new independent scalar full
  search covers 8 kHz/2 ms, 44.1 kHz/8 ms, and 48 kHz/30 ms overlap configurations rather than
  comparing the optimized function to itself.
- Exact-scene correctness held for both real titles. 7th Dragon reproduced SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`; Art Academy reproduced
  `5C64ED5BC0A4B10DF61376E71498D8285D0C48B2A9663B7E2EBD27D7187DF932`. Both processes remained
  alive with no fatal, assertion, fastmem, page-fault, profiler, or Vulkan device-lost match. The
  refined candidate debug APK was 32,439,999 bytes with SHA-256
  `89B111A9E5C5CDFB836FB1576BB453D89FE65E54CFED257000B2363530B9E741`; the same-session legacy
  control was 32,436,899 bytes with SHA-256
  `E7CAF6FFC732B3DAE7B394BB98D0EE72B008423AE95209054643F17B61006631`.
- In 30-second, zero-lost-sample profiles from that same device session, the control recorded
  `calcCrossCorrAccumulate` at 1.30% self, `seekBestOverlapPositionFull` at 1.59% inclusive, and
  complete SoundTouch processing at 1.66% inclusive. The candidate recorded its four-way helper at
  0.87% self, full search at 0.96% inclusive, and complete processing at 1.01% inclusive. That is a
  33.1% reduction in targeted correlation self share and a 39.2% reduction in the complete
  SoundTouch share. These are recurring-hotspot measurements, not additive whole-app percentages.
- A separate six-versus-six exact-scene counter bracket kept the whole application neutral. The
  candidate averaged 3,883.210 ms task-clock, 7.585033 billion cycles, 2.433118 billion retired
  instructions, and 1.920746 GHz; the legacy control averaged 3,885.211 ms, 7.581736 billion,
  2.434348 billion, and 1.920302 GHz. Candidate deltas were -0.052%, +0.044%, -0.051%, and +0.023%,
  respectively, all noise-scale. Accept entry 158 strictly as removal of a measured recurring
  audio hotspot, with no FPS claim and no whole-app speed claim. Thor was still AC-powered at 80%,
  about 4.264 V and 25.0 C, so the under-6-W gate remains open and no battery-power claim is made.
  Source and independent coverage were committed and pushed as `ac8037b39`.
- After the `AGENTS.md` and evidence checkpoint `22bcfa820` was pushed, the pinned JDK 17
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build passed in 1 minute 11
  seconds. The final ARM64-only APK is 29,010,536 bytes with SHA-256
  `F3E492E0C1188AC7F0D9F09CB91D205484EEB338B551D132CEB9D760E005DB83`. It reports package
  `org.azahar_emu.azahar.debug`, version `22bcfa820-vanilla-thor`, minimum SDK 29, and target SDK 37;
  APK Signature Scheme v2 verification passed with signer-certificate SHA-256
  `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`. Its CMake cache records
  `ENABLE_THOR_FRAME_PROFILING=OFF`, the stripped linked library contains zero `ThorFrameProfile`
  strings, and neither the manifest nor installed package flags contain `DEBUGGABLE`.
- Wi-Fi ADB installed that exact final APK. Its 7th Dragon title screen reproduced SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`; the adjacent capture that
  initially differed was decoded and localized entirely to the expected pulsing prompt, and the
  next frame matched byte-for-byte. Art Academy likewise reproduced
  `5C64ED5BC0A4B10DF61376E71498D8285D0C48B2A9663B7E2EBD27D7187DF932` on the next live-FPS update.
  Both processes remained alive with no fatal, assertion, signal, fastmem, page-fault, profiler, or
  Vulkan device-lost log match. The final power check was still AC-powered at 80%, 4.263 V, and
  24.0 C, so this production validation still cannot close the under-6-W battery gate.
- A post-entry-158 instruction-level audit localized `FlushRegion`'s 0.91% self share to Boost ICL
  erase machinery at the final `dirty_regions -= flushed_intervals`, rather than the previously
  rejected `dirty_regions.empty()` probe. A candidate skipped that subtraction only when the local
  `flushed_intervals` result was empty, which is algebraically a no-op and preserved all required
  downloads. The ARM64 build passed, the focused rasterizer-cache case passed all four assertions,
  and 7th Dragon reproduced the exact accepted screenshot. The 32,438,459-byte candidate debug APK
  had SHA-256 `DE5A2B3D6BE9BF0E9E683BD2C8C8B37422B4C55E4BF5E236D259F0F9D5403993`.
- The measured mechanism did not improve. Against the accepted 8,265-sample, zero-lost-sample
  profile with 14,022,307,417 recorded cycles, `FlushRegion` changed only from 0.91% self to 0.89%
  in an 8,261-sample, zero-lost-sample candidate profile with 14,034,484,337 cycles. That noise-scale
  target movement does not justify an extra recurring branch, so the implementation was completely
  reverted. This rejected follow-up is not entry 159 and makes no speed, FPS, or power claim.
- A subsequent `ProcessNormalCommandBatch()` candidate was also rejected and fully reverted. Its
  caller already limited the ordinary-command prefix to the remaining command pairs, so the helper
  removed its duplicate recurring bounds check and accumulated the same delay count once after the
  prefix instead of once per command. The total delay and special-register ordering were unchanged,
  the linked AArch64 helper shrank from 236 to 208 bytes, the pinned native build passed, and all
  21,008 assertions in the three focused ARM64 PICA cases passed. Its 32,440,487-byte debug APK had
  SHA-256 `34C34D4C6FEECD0EBDE1772CAD9D3D886F927AA01C743E879760E437E173304E`;
  7th Dragon reproduced exact SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C` and remained alive.
- The target profile was noise-scale rather than a win. The candidate's 8,353-sample, zero-lost
  30-second capture recorded 13,887,596,480 cycles and placed `ProcessCmdList` at 5.57% inclusive /
  0.71% self. The closest untouched profile recorded 8,261 samples, zero lost, 14,034,484,337 cycles,
  and 5.72% inclusive / 0.73% self. A stricter exact-scene six-versus-six counter bracket then
  rejected the smaller helper: candidate means were 4.976310 billion CPU cycles and 1.608208 billion
  retired instructions per ten-second window versus control means of 4.948643 and 1.604904 billion.
  Candidate deltas were +0.559% cycles and +0.206% instructions; median deltas were +0.467% and
  +0.504%. The 32,439,999-byte rebuilt control had SHA-256
  `5F7EEE22B280B8C972578A1147FFAE2889636722E3E68D114FC89437AE16460C` and reproduced the same exact
  scene. Smaller code did not reduce application work here, so no source or test remains and this is
  not entry 159. The Thor remained AC-powered, so neither profile can establish battery watts.
- Entry 159 removes a recurring Android display-vsync wakeup from the Vulkan renderer. The fragment
  previously reposted itself with `Choreographer.postFrameCallback()` on every display frame and
  crossed JNI into `TryPresenting()`, even though only `EmuWindow_Android_OpenGL` implements that
  presentation hook; the Vulkan window presents through its renderer/swapchain path. Android's
  official [Choreographer API](https://developer.android.com/reference/android/view/Choreographer)
  documents that a posted callback runs once and is automatically removed, so continuous renderers
  must explicitly repost it. Azahar now reposts only when the active window subclass reports that it
  requires the callback. Initialization keeps the first callback alive until the native window is
  constructed, and OpenGL alone opts into the recurring path.
- The pinned JDK 17 / NDK 27.3 ARM64 native and RelWithDebInfo APK build passed, as did
  `:app:testVanillaRelWithDebInfoUnitTest`. The 32,438,811-byte candidate debug APK had SHA-256
  `A329E59002A2FBE839FFCF8D809303987FE64EA2B90A145F071EBAF828088C30`. Vulkan 7th Dragon reproduced
  exact SHA-256 `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C` both before and after a real
  HOME/resume cycle, stayed alive, and produced no fatal, assertion, fastmem, page-fault, profiler,
  or Vulkan device-lost log match.
- The renderer split was tested rather than inferred. The device's 412-byte configuration was
  backed up at SHA-256 `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`, only
  `graphics_api` was temporarily changed from Vulkan `2` to OpenGL `1`, and the original file was
  restored byte-for-byte to the same hash. OpenGL rendered the title at 30 FPS. Its zero-lost-sample
  profile retained `EmulationFragment.doFrame` at 25.53% inclusive, JNI `doFrame` at 22.92%,
  `EmuWindow_Android_OpenGL::TryPresenting` at 16.87%, and `eglSwapBuffers` at 6.05%, proving the
  callback-driven renderer was not accidentally disabled.
- The accepted Vulkan profile captured 7,631 samples, lost zero, and recorded 12,650,989,901 cycles,
  down 9.858% from the closest untouched profile's 8,261 samples and 14,034,484,337 cycles. The
  control attributed 2.92% inclusive to Azahar's `EmulationFragment.doFrame`, 2.78% to its Java
  `postFrameCallback` route, and 0.02% self to JNI `doFrame`; all three Azahar symbols/routes were
  absent from the accepted profile. Other Android Choreographer activity remained and is not claimed
  as removed.
- A same-scene six-versus-six ten-second hardware-counter bracket confirmed the whole-process win.
  Candidate means were 4.483033 billion CPU cycles and 1.523423 billion retired instructions versus
  control means of 4.948643 and 1.604904 billion. Candidate deltas were -9.409% cycles and -5.077%
  instructions; median deltas agreed at -9.447% and -5.067%. Mean sampled frequency changed only
  from 1.921693 to 1.921866 GHz (+0.009%). The Thor remained AC-powered at 80%, 4.263 V, and 24.0 C,
  so this demonstrates recurring CPU/display-scheduling work removal but not battery watts or an
  under-6-W result. Source commit `b62eb36f2` was pushed directly to `origin/master`.
- After evidence commit `ccf81a6b2` was pushed, the clean post-commit JDK 17
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build passed in 1 minute 8
  seconds. The final ARM64-only, v2-signed APK is 29,009,796 bytes with SHA-256
  `AC45D7C8A517580F308F59CDD5A0172F221C955382698555FB71C3008EC87515`; signer-certificate SHA-256
  remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`. It reports package
  `org.azahar_emu.azahar.debug`, version `ccf81a6b2-vanilla-thor`, minimum SDK 29, target SDK 37,
  only `arm64-v8a`, no installed `DEBUGGABLE` flag, `ENABLE_THOR_FRAME_PROFILING=OFF`, and zero
  `ThorFrameProfile` strings in the stripped native library.
- Wi-Fi ADB installed that exact Lite artifact. 7th Dragon reproduced SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`, and Art Academy reproduced
  `5C64ED5BC0A4B10DF61376E71498D8285D0C48B2A9663B7E2EBD27D7187DF932`, both on their first capture.
  Both processes stayed alive with no fatal, assertion, fastmem, page-fault, profiler, or Vulkan
  device-lost log match. The final device check was still AC-powered at 80%, 4.263 V, and 24.0 C,
  so the production validation also leaves the under-6-W battery gate open.
- Entry 159 raises the ledger to 159 numbered entries and 158 active accepted entries because the
  unsafe absolute-offset ARM64 page-table entry remains withdrawn. Entries 152 through 159 are
  measured recurring presentation, scheduler, audio, and Android wakeup reductions, not additive
  percentages.
- Entry 160 removes redundant Android UI redraws from the enabled performance overlay. Its
  one-second updater still fetches and formats every enabled statistic, so changed FPS, speed,
  frametime, memory, and battery-temperature values remain live. It now calls `TextView.setText()`
  only when the complete formatted text differs, and applies the configured background when the
  overlay is enabled or refreshed instead of recreating it on every timer tick.
- The pinned JDK 17 / NDK 27.3 `:app:testVanillaRelWithDebInfoUnitTest` and debuggable APK build both
  passed. The 32,438,927-byte candidate APK had SHA-256
  `21A72EF49E70A527EACD720314268327B90D0F512A08C610E9879520937F1910`. Both the candidate and the
  exact accepted control reproduced 7th Dragon screenshot SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`; a live Android UI hierarchy
  independently reported the candidate overlay text as `FPS: 30`.
- The candidate's 30-second profile recorded 7,497 samples, lost zero, and counted
  12,354,324,643 cycles versus the accepted control profile's 7,631 samples, zero lost, and
  12,650,989,901 cycles. The overlay updater's inclusive share fell from 0.75% to 0.27%,
  `ViewRootImpl.doTraversal` from 1.40% to 0.19%, `TextView.setText` from 0.54% to 0.10%, and UI
  render-thread drawing from 0.75% to 0.09%. This supports the intended suppression mechanism;
  other Android Choreographer work remains and is not claimed as removed.
- A same-scene six-versus-six ten-second hardware-counter bracket confirmed lower whole-process
  work. Candidate means were 2,276.578 ms task-clock, 4.451113 billion CPU cycles, and 1.519717
  billion retired instructions versus control means of 2,360.924 ms, 4.608947 billion, and
  1.548599 billion. Candidate deltas were -3.573%, -3.425%, and -1.865%; median deltas agreed at
  -3.532%, -3.334%, and -1.712%. Mean sampled frequency changed from 1.919399 to 1.922776 GHz
  (+0.176%). The Thor remained AC-powered at 80%, 4.262 V, and 23.0 C, so entry 160 proves recurring
  Android UI-work removal but not battery watts or the under-6-W requirement. Source commit
  `2b02f5cf5` was pushed directly to `origin/master`.
- After evidence commit `37053eb9d` was pushed, the clean JDK 17
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build passed in 1 minute 9
  seconds. The final ARM64-only, v2-signed APK is 29,009,852 bytes with SHA-256
  `7EA1786E86C7B040D76E52FBFD5673E508C397D49A7E72C5982895931253AAEC`; signer-certificate SHA-256
  remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`. It reports package
  `org.azahar_emu.azahar.debug`, version `37053eb9d-vanilla-thor`, minimum SDK 29, target SDK 37,
  only `arm64-v8a`, no installed `DEBUGGABLE` flag, `ENABLE_THOR_FRAME_PROFILING=OFF`, and zero
  `ThorFrameProfile` strings in the stripped native library.
- Wi-Fi ADB installed that exact Lite artifact. 7th Dragon reproduced SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C` on the next animated-title
  capture, and Art Academy reproduced `5C64ED5BC0A4B10DF61376E71498D8285D0C48B2A9663B7E2EBD27D7187DF932`
  on its first capture. Both processes stayed alive with no fatal, assertion, fastmem, page-fault,
  profiler, or Vulkan device-lost log match. The active 412-byte device configuration remained
  byte-exact at SHA-256 `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`,
  with Thor performance/fan modes still 2/4. The final power check remained AC-powered at 80%,
  4.262 V, and 24.0 C, so production validation also leaves the under-6-W battery gate open.
- Entry 160 raises the ledger to 160 numbered entries and 159 active accepted entries because the
  unsafe absolute-offset ARM64 page-table entry remains withdrawn. Entries 152 through 160 are
  measured recurring presentation, scheduler, audio, Android wakeup, and overlay-work reductions,
  not additive percentages.
- A post-entry-160 forest-level audit rejected treating the remaining Vulkan present cost as
  removable duplicate work. A separate profiler-enabled debug APK (32,439,187 bytes, SHA-256
  `419EA866DEF7976BD0F84DC48B732590B67603338C8A65366F142C511C41D998`) reproduced the exact 7th
  Dragon screenshot SHA-256 `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`
  and stayed alive without a fatal/assertion/device-lost match. Two consecutive steady 5.014-second
  windows each reported `swaps=300`, `presented=300`, `direct=300`, and
  `duplicate_prepare_skipped=150`; presentation copies and blits remained zero. Android's live
  display inventory showed physical display 0 at 1080x1920 and display 4 at 1080x1240, both on and
  active at 60 Hz, while SurfaceFlinger listed two Azahar `SurfaceView`/BLAST pairs. Source tracing
  confirms that a new guest frame is sent to the main and secondary windows before
  `game_frames_updated` is cleared. The combined 300 presents therefore account for 150 new
  30-FPS guest frames on each of two physical panels, rather than a redundant 60-FPS present route
  on either panel. No source optimization was accepted from this audit. The device was still
  AC-powered at 80%, 4.266 V, and 23.0 C in performance/fan modes 2/4, so the under-6-W battery
  gate remains open.
- `tools/measure-thor-power.ps1` now makes that open gate reproducible over Wi-Fi ADB. It reads the
  Thor's native battery `power_now`, `power_avg`, current, voltage, charge counter, temperature,
  capacity, and USB, wireless, and UCSI charger-online nodes. Each raw CSV sample also records the
  Azahar process CPU ticks and KGSL GPU busy/clock state. The JSON summary includes mean, median,
  nearest-rank P95, maximum, temperature range, least-squares thermal slope, workload activity, and
  a coarse charge-counter-derived energy cross-check. The default acceptance policy requires
  production ARM64 version `37053eb9d-vanilla-thor`, the accepted config hash, Standard mode 0, fan
  mode 4, both mean and P95 at or below 6 W, at least 10 Azahar process CPU ticks/second, and at least
  1% mean GPU busy. An expected brightness and before/after exact screenshot hash can lock the scene.
- The deterministic script self-test passed its statistics, pass/fail, thermal-slope, real-battery,
  simulated-battery, `/proc` stat, KGSL busy, complete sample, and anti-idle cases. A live
  production-package negative test on Wi-Fi Thor
  `192.168.1.33:5555` correctly stopped at preflight because AC was connected; it created no result
  directory and no watt claim. On that hardware, AC state also read `usb/online=1`,
  `ucsi.../online=1`, `current_now=0`, and `power_now=46,960,186`, demonstrating why the charger
  checks cannot be replaced with a superficially plausible battery status. A separate 10-second
  AC-only workload calibration (not a watt test) kept PID 2542 alive, advanced process CPU time by
  227 ticks (22.7 ticks/second), and read about 8.2% GPU busy at 615 MHz. The deliberately loose
  10-tick/second and 1%-busy floors therefore reject an idle/frozen 7th Dragon run without turning
  activity counters into speed targets. This is measurement tooling, not optimization entry 161,
  and the under-6-W result remains pending a physical unplug.
- A forest-level Android audio-buffer experiment was rejected and completely reverted. The accepted
  entry-160 profile attributed about 3% inclusive to the AudioTrack callback stack, while Cubeb's
  Android backend documents a power-saving selection above 4,000 requested frames. Live
  AudioFlinger state showed that production used a 32,728-Hz, 1,962-frame normal track at 131.24 ms
  reported latency with zero current underruns. An Android-only 4,096-frame candidate built and
  installed successfully, but its live track reported 271.84 ms latency and accumulated 989
  underruns within roughly one minute. That severe latency/stability regression rejects the
  theoretical reduction in callback wakeups without requiring a watt claim.
- The exact preserved production APK was restored with ADB's explicit downgrade flag after Android
  correctly rejected its older version code on the first attempt. The installed package again
  reported `37053eb9d-vanilla-thor`, ARM64, and no `DEBUGGABLE` flag; its first captured 7th Dragon
  frame reproduced SHA-256 `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`.
  The restored AudioTrack returned to 1,962 frames, 117.56 ms live latency, and zero underruns, with
  no fatal, assertion, fastmem, page-fault, device-lost, or profiler log match. AC remained connected,
  so this is a rejection/correctness result, not battery-power evidence and not entry 161.
- The battery gate now prevents a low-power/low-performance false positive with production-safe
  SurfaceFlinger evidence. At both the end of warmup and after the measurement it requires exactly
  two live Azahar `SurfaceView[…](BLAST)` layers, at least one layer with 60 presentation
  intervals, at least 29 FPS mean, at most 40 ms P95, and zero intervals above 50 ms. On the
  restored exact 7th Dragon scene the primary layer exposed 127 frames / 126 intervals at 29.892 FPS
  mean, 33.454 ms mean interval, 34.431 ms P95, and zero intervals above 50 ms. The second physical
  panel's BLAST layer was live but its Android 13 SurfaceFlinger latency history contained only zero
  rows, so the gate requires its presence while using the primary layer's timestamps. The complete
  parser and pass/fail paths are deterministic-self-tested. This strengthens measurement validity;
  it is not optimization entry 161 and does not close the still-AC-powered 6 W gate.
- Audio quality is now part of the same acceptance gate rather than an informal follow-up. At the
  end of warmup and after measurement, the tool parses the one active AudioFlinger track for the
  exact Azahar PID and requires its track ID to remain unchanged, sample rate to remain 32,728 Hz,
  frame count at or below 2,048, reported latency at or below 150 ms, and total underruns to remain
  zero. The deterministic control fixture is the restored 1,962-frame / 117.56 ms / zero-underrun
  production row; the rejected 4,096-frame / 271.84 ms / 989-underrun row fails. This is another
  correctness guard for the future physical-battery run, not optimization entry 161.
- A matched internal-resolution sweep then tested a larger, forest-level control on the accepted
  production APK. The fixed 7th Dragon title scene, Vulkan renderer, Turnip driver, dual-display
  layout, High Performance mode 2, fan mode 4, and AC power remained fixed; only the 412-byte
  `resolution_factor` configuration field changed. Twenty-sample KGSL/process windows and strict
  SurfaceFlinger/AudioFlinger checks produced:

  | Internal resolution | Mean KGSL busy | Change from 3x | Process CPU ticks/s | Mean FPS / P95 interval | Audio |
  | --- | ---: | ---: | ---: | --- | --- |
  | 3x accepted control | 8.37423% | control | 21.2938 | control workload | accepted production track |
  | 2x | 5.48828% | -34.46% | 21.1700 | 29.9069 / 34.2943 ms | 32,728 Hz, 1,962 frames, 121.87 ms, 0 underruns |
  | 1x | 3.93265% | -53.04% | 21.3567 | 29.9125 / 34.3012 ms | 32,728 Hz, 1,962 frames, 120.85 ms, 0 underruns |

  All three windows held the Adreno clock at 615 MHz. Both alternatives had zero presentation
  intervals over 50 ms. Their exact config SHA-256 values were
  `2634CCBD55F25961886DFC53B98BFE15A2C229E479595D60256AAD88CC8C712E` for 2x and
  `A0B9B5544DAE6A7E53EA8EA554E4F10BD12427878B029795153450A409C83E18` for 1x; screenshots were
  `A940E8486BD0179D0BE022BF395C8FC863DD0032F56471D6BFAFC6873C5C1233` and
  `31C4C2605D6D34F8173277890B62809405FCCB527502837165A07C864FD243E8` respectively.
- Static-region comparison against the accepted exact 3x screenshot sampled 288,100 pixels. The 2x
  capture had 71.37% exact samples and 40.37 dB PSNR; 1x had 72.50% and 48.05 dB. The surprising
  ordering is plausible for a mostly-2D source under different raster/resampling alignments and is
  why these numbers must not be generalized into 3D image-quality claims. Visually both title
  screens remained close, but a stable representative 3D scene is still required before choosing
  a lower resolution as a general profile.
- This sweep establishes internal resolution as a materially larger GPU-work lever than the
  remaining profiled micro-hotspots. It does not establish battery watts because the Thor remained
  on AC, and it does not change the user's 3x default. The exact original config SHA-256
  `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21` was restored, production
  relaunched, and the accepted 3x screenshot hash reproduced. A physical-battery 3x/2x/1x matrix in
  Standard mode, with matching config and screenshot expectations per row, is the next decision
  gate. This experiment is not optimization entry 161.
- The three custom Vulkan packages already retained on the Thor were then bracketed without a
  download or source change. The accepted production APK, exact 3x config and title scene, dual
  displays, High Performance mode 2, fan mode 4, AC power, and 615 MHz GPU clock stayed fixed.
  Each driver reproduced the exact accepted screenshot SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C` and remained alive:

  | Custom driver | Runtime identity | Mean KGSL busy | Process CPU ticks/s | Result |
  | --- | --- | ---: | ---: | --- |
  | generic `Turnip_v26.0.0_R8.zip` | Turnip Mesa 25.99.99, Vulkan 1.4.335 | 8.02202% | 21.4438 | accepted bracket |
  | forced-Sysmem R8 | Turnip Mesa 25.99.99, Vulkan 1.4.335 | 9.77545% | 20.8972 | reject: 21.86% more GPU time |
  | `T26-toasted` | PurpleVK public 26.0.99, Vulkan 1.4.344 | 8.00780% | 21.6490 | 0.18% tie; no demonstrated win |

  Each row used 20 KGSL samples over about 24.6-24.9 seconds. Sysmem's large penalty rejects it for
  this workload even though output matched. PurpleVK's tiny difference is below a credible decision
  threshold and its CPU activity was slightly higher, so there is no basis to replace the current
  generic R8. A harder representative 3D/shader-compilation workload and a physical-battery bracket
  remain necessary before changing the recommendation.
- Generic R8 was restored after both alternatives. The running production package again reported
  `37053eb9d-vanilla-thor`, ARM64, no `DEBUGGABLE` flag, Turnip Mesa 25.99.99, the exact accepted
  config hash `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`, and the exact
  screenshot hash. This AC-only ranking is not a watt result and is not optimization entry 161. It
  also exposes a measurement requirement: a config and frame hash can match across materially
  different driver paths, so the strict power gate must record and validate active Vulkan-driver
  identity.
- Source commit `bc25ea052` adds that missing evidence without adding steady-state work. Android
  reads the already-installed driver metadata once during startup, logs its name, version, and
  library name as one JSON object, then passes the same cached library name to AdrenoTools. The
  vanilla RelWithDebInfo Kotlin compile passed, followed by the full ARM64-only production Lite
  build in 1 minute 23 seconds. The APK is 29,010,132 bytes with SHA-256
  `C9376EE04D3E0C12D305536E0D45432BE76070B2E1EC0C5FCB2C563982F22038`, reports version
  `bc25ea052-vanilla-thor`, has no manifest debuggable flag, uses APK Signature Scheme v2, and keeps
  signer-certificate SHA-256
  `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
- Wi-Fi ADB installed that artifact without changing the exact config hash. The fixed 7th Dragon
  scene reproduced the accepted screenshot, logged generic R8 metadata as
  `{"name":"Mesa Turnip driver v26.0.0 - R8","version":"Vulkan 1.4.335","libraryName":"vulkan.ad07xx.so"}`,
  and kept PID 19705 alive. Live audio was 32,728 Hz, 1,962 frames, 123.40 ms, and zero underruns.
  SurfaceFlinger exposed both required BLAST layers; the measurable layer had 127 frames / 126
  intervals, 29.895 FPS mean, 34.327 ms P95, and zero intervals over 50 ms. A final 20-sample window
  measured 8.245% mean KGSL busy at 615 MHz and 21.546 process CPU ticks/second, inside the earlier
  generic-R8 control range and with no recurring logging path.
- `tools/measure-thor-power.ps1` now parses the latest JSON record for the current PID, defaults to
  the exact generic-R8 name/version/library above, stores the identity in `summary.json`, and fails
  before sampling on a mismatch or missing structured record. Its expanded deterministic self-test
  passes both latest-record selection and missing-record rejection. On the live package, the correct
  expectation advanced to the genuine AC-power rejection, while an explicit Sysmem expectation
  failed on driver identity; neither negative test created a result directory. This measurement
  hardening and one-time log are not optimization entry 161, and AC power still prevents a watt
  result.
- A representative 60-FPS 3D follow-up used the owned Super Mario 3D Land title/attract loop
  (`0004000000054000`) without entering gameplay or touching a save. Mario Kart 7 was considered
  first but stopped at its existing no-Mii screen; no Mii or system data was created. Android's
  unrelated microphone prompt was denied rather than granting a sensitive permission for the test.
  Production `bc25ea052`, generic R8, Vulkan, dual displays, High Performance mode 2, fan mode 4,
  AC power, and the 615 MHz GPU clock stayed fixed. Each resolution config was exactly 412 bytes and
  differed from the restored file by the one `resolution_factor` byte only.

  | Resolution | Mean KGSL busy | CPU ticks/s | SurfaceFlinger mean FPS / P95 | Audio |
  | --- | ---: | ---: | --- | --- |
  | 3x opening | 20.79792% | 45.0204 | 58.8238 / 21.3931 ms | 32,728 Hz, 1,962 frames, 123.47 ms, 0 underruns |
  | 2x | 13.11865% | 46.3974 | 59.2559 / 20.6730 ms | 32,728 Hz, 1,962 frames, 123.12 ms, 0 underruns |
  | 1x | 8.65131% | 46.0998 | 58.8475 / 27.6145 ms | 32,728 Hz, 1,962 frames, 122.14 ms, 0 underruns |
  | 3x closing | 20.85599% | 43.3503 | 57.4434 / 28.4334 ms | 32,728 Hz, 1,962 frames, 119.25 ms, 0 underruns |

  Every row used 20 KGSL samples over about 24.6-24.9 seconds and had two live Azahar BLAST layers
  with zero presentation intervals over 50 ms. The two 3x GPU controls differ by only 0.28%, which
  brackets the animated-loop phase variation. Against their 20.82696% mean, 2x removed 37.01% of
  GPU busy and 1x removed 58.46%; 1x removed another 34.05% relative to 2x.
- This 3D result confirms 2x as the current efficiency/quality candidate for demanding 60-FPS
  titles: it materially reduced GPU work and had the best measured pacing row, while 1x added no
  demonstrated pacing benefit and looked visibly softer on sharp 3D edges. The screenshots captured
  different animation phases, so that quality observation is visual rather than a pixel-matched
  metric. The loop's non-repeatable hashes also prevent using it unchanged as the strict battery
  scene; a stable menu/gameplay phase is required first. The exact 3x config SHA-256
  `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21` was restored and the title
  relaunched afterward. AC power still prevents a watt claim, the user's default did not change,
  and this settings experiment is not optimization entry 161.
- The same 2x Super Mario 3D Land loop then bracketed the Thor's three vendor performance policies
  with fan mode 4 forced back to the matched value. This matters because changing High Performance
  to Standard unexpectedly reset fan mode from 4 to 1 on the live firmware; the experiment caught
  and corrected that coupling before launch.

  | Thor policy | Fixed GPU clock | Mean KGSL busy | CPU ticks/s | Mean FPS / P95 | Audio |
  | --- | ---: | ---: | ---: | --- | --- |
  | High Performance (2) | 615 MHz | 13.11865% | 46.3974 | 59.2559 / 20.6730 ms | 32,728 Hz, 1,962 frames, 123.12 ms, 0 underruns |
  | Performance (1) | 550 MHz | 13.81158% | 44.8352 | 58.3970 / 27.6261 ms | 32,728 Hz, 1,962 frames, 122.78 ms, 0 underruns |
  | Standard (0) | 401 MHz | 17.70573% | 45.3293 | 57.9355 / 27.4599 ms | 32,728 Hz, 1,962 frames, 123.33 ms, 0 underruns |

  Every policy kept two live BLAST layers, zero intervals over 50 ms, and a clean AudioTrack. KGSL
  busy percentage rises as the same work occupies more time at a lower clock, so it is not a direct
  energy ranking across policies. SurfaceFlinger shows that both lower policies gave up speed on
  this demanding 60-FPS loop; Performance recovered only about 0.46 FPS over Standard, while High
  Performance had the best pacing snapshot. A physical-battery run is still required to decide
  whether either trade is worthwhile under the 6-W gate.
- The policy/config cleanup force-stopped the experiment, restored the exact 3x config, explicitly
  restored performance/fan modes 2/4, relaunched the generic-R8 production package, and reproduced
  the accepted 7th Dragon screenshot SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`. This policy matrix is not
  optimization entry 161 and makes no AC-derived watt claim.
- Entry 161 removes redundant Boost ICL dirty-region rewrites found by the post-forest CPU profile.
  The pre-change Super Mario 3D Land (`0004000000054000`) capture attributed 2.08% inclusive and
  0.78% self of sampled CPU cycles to `RasterizerCache<Vulkan::Traits>::InvalidateRegion`. Its call
  tree included 14.02% of the function's event count below an interval-map/free branch and 10.15%
  below an interval-map/allocation branch. `FramebufferHelper` repeatedly reasserted that an
  interval belonged to the owner already covering the complete interval, but `interval_map::set()`
  still erased, allocated, reinserted, and coalesced nodes.
- A nonzero-owner invalidation now asks Boost ICL whether the complete interval already contains the
  exact owner segment and skips only that semantic no-op. Wrong-owner, partially covered, extending,
  and mixed-owner intervals still call `set()`. Owner-zero invalidation still erases the interval,
  and surface discovery, invalidation, unregister, flush, and page-count behavior are unchanged.
  Permanent tests cover exact and contained same-owner intervals plus wrong-owner, extension, and
  mixed-owner rejection.
- The profiler-enabled ARM64 build completed all 2,203 native actions and linked the test runner.
  Its focused device test passed 9 assertions in two rasterizer-cache cases. In steady 5.014-second
  title windows with 600 combined dual-panel presents, `dirty_updates=1650` while
  `dirty_updates_elided` ranged from 47,400 to 114,604: 96.64%-98.58% of candidate same-owner
  rewrites were no-ops. The rendered top screen remained at a visible 60 FPS. The profiler APK was
  32,440,183 bytes with SHA-256
  `643B55C66CFFEBA470E4339E3116A6E9A85F61271EAE9148F7D0E64D563D0853`.
- A separate profiling-off build completed all 2,203 native actions and packaged successfully in
  9 minutes 23 seconds. Its APK was 32,439,807 bytes with SHA-256
  `9EC1DF3108C1E6FF29E2277E6979B4290D8BAB2C17D68B74E6A96853C007548C`. Wi-Fi ADB preserved the
  exact configuration SHA-256
  `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`, then relaunched the exact
  title on Mesa Turnip 25.99.99 / Adreno 740 without any `ThorFrameProfile` line.
- Two profiling-off candidate captures ran for 29.995 seconds apiece with zero lost samples. The
  candidate trace whose command/draw mix most closely matched the control placed `ProcessCmdList`
  at 20.75% versus 20.17% control, while `InvalidateRegion` fell from 2.08% inclusive / 0.78% self
  to 1.35% / 0.74%. Process-wide Scudo allocation fell from 1.29% to 0.93% inclusive, deallocation
  from 1.55% to 1.00%, and quarantine/deallocation from 1.07% to 0.74%. The independent candidate
  trace put `InvalidateRegion` at 1.13% / 0.61%. Candidate call trees retain the bounded ownership
  lookup and genuine map mutations but no longer contain the control's recurring allocation/free
  branches for already-owned segments.
- The complete patched native suite on the physical Thor ran 925,380 assertions in 173 cases:
  167 cases and 925,377 assertions passed. The only three failures are the established standalone-
  JNI `get_build_flavor` omissions, and the only three skips are the established missing-DSP-
  firmware cases. This is the expected one-case/one-pass increase over the pre-change 172/166
  result.
- Source/test commit `d733724db` was pushed directly to `origin/master`. The post-commit JDK 17
  `:app:assembleVanillaRelWithDebInfoLite -PthorFrameProfiling=false --no-configuration-cache`
  build passed in 1 minute 45 seconds. The ARM64-only, v2-signed production APK is 29,010,744 bytes
  with SHA-256 `1B77BC14D942609269D5E24D0386DC29B54293522EAFBB6FE934EE219EF9FC20`;
  its signer certificate SHA-256 is
  `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`. It reports package
  `org.azahar_emu.azahar.debug`, version `d733724db-vanilla-thor`, minimum SDK 29, target SDK 37,
  ARM64 ABI, and no `DEBUGGABLE` flag. The build cache records profiling OFF and the unstripped
  library contains zero `ThorFrameProfile` strings.
- Wi-Fi ADB installed that exact APK: the on-device base APK reproduced SHA-256
  `1B77BC14D942609269D5E24D0386DC29B54293522EAFBB6FE934EE219EF9FC20`. The unchanged 3x
  configuration reproduced SHA-256
  `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`; performance/fan modes
  remained 2/4. Super Mario 3D Land reported program ID `0004000000054000`, Turnip Mesa 25.99.99,
  and Adreno 740, while the rendered attract loop visibly held 60 FPS without corruption. Two
  Azahar BLAST surfaces were live. The active AudioFlinger track was 32,728 Hz, 1,962 frames,
  123.35 ms reported latency, and zero underruns. No profiler, fatal, device-lost, Vulkan-error,
  ANR, or native-crash log matched.
- Entry 161 raises the ledger to 161 numbered entries and 160 active accepted entries because the
  unsafe absolute-offset ARM64 page-table entry remains withdrawn. The measured 35.1% reduction in
  this function's inclusive sampled share and the allocator-share reductions are path-local CPU-
  work evidence; they are not additive with earlier entries and do not prove FPS or battery watts.
  The Thor remained AC-powered, so the physical discharging-battery mean/P95 <=6 W gate remains
  open.

## Borrowed Vulkan Vertex Spans (2026-08-20)

- Entry 162 follows the post-forest Super Mario 3D Land CPU profile rather than another isolated
  instruction idea. In the fresh 30-second control, `RasterizerVulkan::SetupVertexArray()` consumed
  3.80% inclusive CPU cycles. `__aarch64_ldadd8_acq_rel` consumed 1.49% self process-wide; 39.17%
  of its sampled cycles originated in vertex setup, where it represented 30.65% of that call-tree
  branch. The recurring source was `GetPhysicalRef()` copying a `shared_ptr` for each PICA attribute
  loader even though setup copies the bytes immediately and the enclosing draw already guarantees
  the physical backing lifetime.
- `MemorySystem::GetPhysicalSpan()` now returns a non-owning view from the existing physical-region
  descriptor without copying retained ownership. Vulkan vertex setup uses it only for the immediate
  bounds check and stream-buffer copy. Retained or asynchronous users keep `MemoryRef`; the span
  must not survive a draw, reset, remap, or backing replacement. Permanent coverage verifies the
  exact FCRAM base pointer and size, an interior pointer and remaining size, and a valid one-past-end
  empty view. An attempted invalid-region fixture check was rejected because error logging requires
  a test CPU that this lightweight `MemorySystem` fixture does not initialize; it was not treated as
  product behavior or hidden behind a crash waiver.
- The local Cortex-A510 software optimization guide page 51 says atomic instructions with acquire
  or release semantics are multicycle issue entries and that multicycle entries suppress co-issue
  until their final cycle. That explains why removing the ownership atomics is a sound A510
  candidate, but it is not acceptance evidence by itself. The physical Thor profile, source-level
  lifetime proof, bounds tests, and final linked code control acceptance.
- Profiling-off control and candidate APKs were built from the same source state with only the
  `SetupVertexArray()` access route toggled. The 32,440,559-byte control APK SHA-256 was
  `80592A94B1F185CFD81E89460627BCEDB85F6647A607C6F3D766B66A867A0E0F`; its unstripped library
  SHA-256 was `4395335E4FB7F2BDFBBB5D7C0161AA5DFE3E453106A72CE28D79DC6A8AE26916`. The
  32,439,343-byte candidate APK SHA-256 was
  `C61B78746F5B0AC9C74304BF6B813CC0EDB6503A6E9272420943434FADEF643F`; its unstripped library
  SHA-256 was `02B41CCD486060019F3CE9F51943BBD91BF47681ECFB215FB5DCB5CE90187E83`.
  Every accepted run used program ID `0004000000054000`, Mesa Turnip 25.99.99, Adreno 740, the
  unchanged configuration, a 60-second warmup, and a 30-second 4-kHz user-cycle call-graph capture.

  | Alternating run | Samples / lost | Process cycles | `AccelerateDrawBatch` cycles | `SetupVertexArray` cycles | Setup / parent | Visible FPS |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | Control 1 | 44,318 / 0 | 16,322,663,168 | 1,988,936,747 | 435,772,418 | 21.9098% | 60 |
  | Candidate 1 | 47,582 / 0 | 17,893,940,456 | 2,168,089,834 | 358,331,016 | 16.5275% | 60 |
  | Control 2 | 47,799 / 0 | 17,629,247,054 | 2,251,936,134 | 478,473,990 | 21.2472% | 59 |
  | Candidate 2 | 48,387 / 0 | 17,655,572,436 | 2,132,998,108 | 346,189,388 | 16.2302% | 59 |

  Aggregating each two-run side, candidate parent work was 1.42% higher, yet inclusive vertex-setup
  cycles fell 22.94%, setup self cycles fell 30.66%, and setup's share of the parent fell from
  21.56% to 16.38% (24.02% relative). Process totals varied with the animated attract scene, so no
  whole-process speed claim is made. All four accepted screenshots were visually clean at 59-60
  FPS. A wrong-card automation run was discarded before comparison because its logged program ID
  was `0004000000086300`; the corrected launch used the left search rail and revalidated the exact
  title ID.
- The restored candidate rebuilt all four expected ARM64 actions successfully. Final ThinLTO makes
  `SetupVertexArray()` 1,408 bytes (`0x580`), directly calls `GetPhysMemRegionInfo()`, and contains
  no `LDADD`, `LDXR`/`STXR`, CAS, or `GetPhysicalRef()` route. The focused physical-A510 memory suite
  passed all 22 assertions in three cases. The complete candidate-linked suite then ran 925,386
  assertions in 174 cases: 168 cases and 925,383 assertions passed. The only three failures remain
  the established standalone-JNI `get_build_flavor` omissions, and the only three skips remain the
  established missing-DSP-firmware cases. This is one new passing case and six new passing
  assertions over entry 161.
- Source/test/guidance commit `6987ffd79` was pushed directly to `origin/master`. Its post-commit
  JDK 17 `:app:assembleVanillaRelWithDebInfoLite -PthorFrameProfiling=false
  --no-configuration-cache` build passed in 1 minute 16 seconds. The ARM64-only, v2-signed
  production APK is 29,010,636 bytes with SHA-256
  `AFC0BB31BB3BE703E3208FB0C0A56CB5328B8CD8E7CAADC8C91D8FCF7B6DE01E`; its signer certificate
  SHA-256 remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `6987ffd79-vanilla-thor`, minimum SDK 29,
  target SDK 37, ARM64 ABI, and no `DEBUGGABLE` attribute. The active native cache records profiling
  OFF and the unstripped library contains zero `ThorFrameProfile` strings.
- Wi-Fi ADB installed that exact APK, and the on-device base APK reproduced SHA-256
  `AFC0BB31BB3BE703E3208FB0C0A56CB5328B8CD8E7CAADC8C91D8FCF7B6DE01E`. The configuration
  remained byte-identical at SHA-256
  `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`, with performance/fan
  modes 2/4. The required structured record identified `Mesa Turnip driver v26.0.0 - R8`, Vulkan
  1.4.335, and `vulkan.ad07xx.so`; the renderer identified Turnip Mesa 25.99.99 / Adreno 740 and the
  exact title identified program ID `0004000000054000`. Two Azahar BLAST surfaces were live, and a
  visually clean gameplay frame showed 59 FPS. The active AudioFlinger track was 32,728 Hz, 1,962
  frames, 112.20 ms reported latency, and zero underruns. No profiler, fatal, device-lost,
  Vulkan-error, ANR, or native-crash log matched. The strict power-tool default now names this
  production version and its complete self-test passes.
- Entry 162 raises the ledger to 162 numbered entries and 161 active accepted entries because the
  unsafe absolute-offset ARM64 page-table entry remains withdrawn. The measured reduction is real
  for the ranked Vulkan vertex-setup path but cannot be added to earlier entries or converted into
  whole-game FPS or energy. The Thor remained AC-powered at 80%, so physical discharging-battery
  mean and nearest-rank P95 power at or below 6 W remain an open gate.

## Rejected extended-dynamic-state-3 blending experiment (2026-08-20)

- The fresh post-entry-162 profile ranked Turnip's `tu_CmdBindPipeline` at 1.24-1.25% self and
  showed that about 43% of Vulkan-worker `memcpy` samples originated below it. Khronos documents
  `VK_EXT_extended_dynamic_state3` as separate feature bits rather than an all-or-nothing feature;
  the attempted candidate therefore required and enabled only dynamic logic-op enable, color-blend
  enable, color-blend equation, and color-write mask. It removed only the corresponding blend
  fields from Azahar's optimized pipeline hash and retained the logic-op value as static state.
  Unsupported, ARM-proprietary, and Qualcomm-proprietary drivers would have kept the old route.
  See the official [extension proposal](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_extended_dynamic_state3.html)
  and [dynamic-state map](https://docs.vulkan.org/guide/latest/dynamic_state_map.html).
- The ARM64 debuggable candidate built successfully. On the physical Thor, the selected custom
  driver explicitly enabled `VK_EXT_extended_dynamic_state3`; the exact test title logged program
  ID `0004000000054000` and both candidate/control captures remained visually clean at 60 FPS.
  The control APK was 32,439,351 bytes with SHA-256
  `C5444E11FA1BCA9B809B1E8B4644C614AA5C190BBAB73111267D9287D5937B69`; the candidate was
  32,441,535 bytes with SHA-256
  `69719FAE330A108B9A1175A7ABEFD23CB72D72D16A896BAD1D4541D593F1540A`.
- After at least 60 seconds of warmup per install, the 30-second 4-kHz user-cycle captures recorded
  47,914 control samples and 42,342 candidate samples with zero lost. Control/candidate total
  cycles were 17,355,249,544/16,326,911,286. Turnip pipeline-bind self cycles were
  217,456,180/204,120,969, or 1.25297%/1.25021% of their process totals: only a 0.22% relative share
  reduction, well inside animated-scene noise. Azahar pipeline-bind self cycles were
  27,312,814/28,049,824, increasing from 0.15737% to 0.17180% of process work (9.17% relative).
  Unrelated descriptor-update/bind shares also moved upward, reinforcing that the pair established
  no whole-frame win rather than a blend-state reduction.
- The entire source candidate was reverted; entry counts remain unchanged. The result closes this
  route for the current representative workload without claiming the extension is universally
  harmful. Reopen it only for a title with measured blend-only pipeline-key churn and require fewer
  actual driver pipeline binds plus matched whole-frame evidence. Capability support by itself is
  not a speed or power result, and this AC-powered experiment says nothing about the <=6 W gate.

## Generation-Guarded Framebuffer Surface Selection Cache (2026-08-20)

- Entry 163 follows the next ranked whole-frame CPU subtree. In a fresh 30-second 4-kHz control,
  `RasterizerCache<Vulkan::Traits>::GetFramebufferSurfaces()` consumed 2.16100% inclusive sampled
  user cycles. `GetSurfaceSubRect()` alone consumed 1.15304%, including the page-table walk and
  surface-match search. Surface creation, registration, and removal were much rarer than the
  recurring selection work, making a generation-guarded last-selection cache a better target than
  changing surface ownership or validation semantics.
- Consecutive draws now reuse the selected color/depth surface IDs and scaled framebuffer rectangle
  only when the active attachment parameters, active/inactive flags, resolution scale, and surface
  topology generation all match. Registration, unregistration, surface replacement, and every
  scale-up site advance the generation. A miss records its result only if validation did not change
  that generation. Even on a hit, the path still reacquires surfaces and mip levels, marks render-
  target use, validates both active viewport intervals, performs the normal backend framebuffer
  lookup, and returns the same `FramebufferHelper`; its destructor therefore retains draw-region
  invalidation. This deliberately removes only repeated selection work.
- `SurfaceParams::operator==` omits resolution scale, so the cache checks it separately. The cache
  record lives beside framebuffer parameters so permanent tests can exercise its contract without
  constructing a renderer. The physical-Thor focused suite passed 18 assertions in four
  `[video_core][rasterizer_cache]` cases, covering valid/invalid records, topology generations,
  active-attachment changes, exact parameter changes, scale-only changes, and ignored inactive
  attachment parameters. The JDK 21 ARM64 debuggable APK build linked those tests and
  `libcitra-android.so` successfully in 1 minute 36 seconds.
- The physical AYN Thor used Wi-Fi ADB at `192.168.1.33:5555`, Mesa Turnip 25.99.99 / Adreno 740,
  exact program ID `0004000000054000`, the unchanged configuration, at least 60 seconds of warmup
  per install, and 30-second 4-kHz user-cycle call-graph captures. Every trace lost zero samples.
  The two control runs bracketed the three candidate runs, and the last candidate used the final
  tested source layout. The final debuggable candidate APK is 32,440,095 bytes with SHA-256
  `CD4B80BDE4D5FF70EDA0F8119CC8116B726BC1D6EAB03B22862B3164BA671F9B`.

  | Run | Samples | Process cycles | `GetFramebufferSurfaces` cycles / share | `GetSurfaceSubRect` cycles / share | `ValidateSurface` cycles / share |
  | --- | ---: | ---: | ---: | ---: | ---: |
  | Control 1 | 46,969 | 17,221,683,872 | 372,160,394 / 2.16100% | 198,572,776 / 1.15304% | 121,219,388 / 0.70388% |
  | Candidate 1 | 45,907 | 17,249,708,754 | 272,562,655 / 1.58010% | 84,681,630 / 0.49092% | 116,948,623 / 0.67797% |
  | Candidate 2 | 43,580 | 16,557,556,495 | 247,037,339 / 1.49199% | 76,757,436 / 0.46358% | 109,451,305 / 0.66104% |
  | Control 2 | 45,290 | 16,618,872,734 | 360,279,063 / 2.16789% | 207,072,601 / 1.24601% | 110,175,425 / 0.66295% |
  | Candidate 3 | 46,031 | 17,101,284,471 | 241,555,451 / 1.41250% | 78,910,687 / 0.46143% | 117,872,712 / 0.68926% |

- Aggregating raw cycles on each side, framebuffer selection fell from 2.16438% to 1.49514% of
  process work, a 30.92% relative reduction and 0.66924 process-percentage-point saving in this
  workload. Subrect lookup fell from 1.19870% to 0.47212%, a 60.61% relative reduction. The cache
  comparison/branch work increased the function's self share from 0.16884% to 0.28224%, but the
  avoided search was much larger. `ValidateSurface()` stayed effectively neutral at 0.68378% versus
  0.67626% (1.10% relative), confirming that the optimization did not merely skip required
  validation. All inspected control and candidate frames were visibly clean; the animated title
  sequence displayed 58-62 FPS.
- Source/test/guidance commit `f692d3962` was pushed directly to `origin/master`. Its post-commit
  JDK 21 `:app:assembleVanillaRelWithDebInfoLite -PthorFrameProfiling=false
  --no-configuration-cache` build passed in 1 minute 53 seconds. The ARM64-only, v2-signed
  production APK is 29,011,488 bytes with SHA-256
  `D07E1FFC918934F53694550F52B47CDA2748D506C0DA3E4112C3064C4FDE04F2`; its signer certificate
  SHA-256 remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `f692d3962-vanilla-thor`, minimum SDK 29,
  target SDK 37, ARM64 ABI, and no `DEBUGGABLE` attribute. The active native cache records profiling
  OFF, and the unstripped library contains no profiler log or warning string.
- Wi-Fi ADB installed that exact production APK, and the on-device base APK reproduced SHA-256
  `D07E1FFC918934F53694550F52B47CDA2748D506C0DA3E4112C3064C4FDE04F2`. The 412-byte configuration
  remained byte-identical at SHA-256
  `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`; performance/fan modes
  remained 2/4 and brightness remained 255. The exact title again logged program ID
  `0004000000054000`, Turnip Mesa 25.99.99, and Adreno 740; its final production frame was visibly
  clean at 60 FPS, with no fatal, device-lost, or ANR match. AC remained connected at 80%, 4.266 V,
  and 25.0 C, so none of this runtime evidence is presented as battery power. The strict power-tool
  default now names this production version.
- Exact bounded cleanup removed 7,807,498,854 logical host bytes. The obsolete 3.24-GB native
  configuration, Gradle/JNI/R8/symbol/mapping staging, 450-MB native test runner, both experiment
  evidence directories, debug APK, and device-side traces/test runner/screenshots were removed by
  exact validated path. The retained build output is only the 2,801,868,169-byte active
  `5h1x5ud1` ARM64 CMake/Ninja cache plus the 29,011,488-byte production APK and 476-byte metadata.
  Azahar was force-stopped and no PID remained. Existing user source edits and historical repo-root
  artifacts were not staged, modified, or deleted.
- Entry 163 raises the ledger to 163 numbered entries and 162 active accepted entries because the
  unsafe absolute-offset ARM64 page-table entry remains withdrawn. This bracket proves less CPU
  work in one recurring rasterizer-cache path; it does not prove a whole-game FPS gain, cannot be
  added to earlier overlapping wins, and is not a battery-power measurement. The Thor remained
  AC-powered, so physical discharging-battery mean and nearest-rank P95 power at or below 6 W remain
  an open gate.

## Generation-Guarded Aligned Texture Surface Selection Cache (2026-08-20)

- Entry 164 follows the next ranked recurring rasterizer-cache subtree in the exact Super Mario 3D
  Land workload. `RasterizerCache<Vulkan::Traits>::SyncTextureUnits()` consumed 1.57% inclusive in
  the fresh post-entry-163 profile. Its ordinary aligned texture path repeatedly rebuilt identical
  `SurfaceParams`, walked `GetSurface()`, and selected the same registered surface. Odd-sized
  textures instead require an existing temporary-surface/blit path and were deliberately excluded.
- The accepted path keeps four recent aligned selections in circular replacement order. A record
  matches only the complete `SurfaceParams`, an explicit resolution scale, and the shared surface
  topology generation; scale is compared separately because `SurfaceParams::operator==` omits it.
  Registration, unregistration, slot replacement, cache clear, and scale-up already advance that
  generation. A hit skips only `GetSurface()` selection and still calls `ValidateSurface()` over
  the exact requested interval. Misses use the unchanged lookup and publish only a nonzero result.
  The odd-size temporary texture creation, mipmap constraint, resource sentencing, and blit route
  are unchanged.
- The physical AYN Thor used Wi-Fi ADB at `192.168.1.33:5555`, exact program ID
  `0004000000054000`, Mesa Turnip 25.99.99 / Adreno 740, the byte-identical accepted configuration,
  at least 60 seconds of warmup per install, and 30-second 4-kHz user-cycle call-graph captures.
  All four alternating control/candidate traces lost zero samples. The preserved control APK is
  32,440,095 bytes with SHA-256
  `817C0FCC9D6C6536A86C623C2BF3BEAE853D7874B05CD8FA2EC3B105520D873C`; the profiled candidate APK
  is 32,440,671 bytes with SHA-256
  `E3AA9758AAD86FC12EF7B053B99A747E1EFBD23FC1147AC107205C0490839465`.

  | Alternating run | Samples | Process cycles | `SyncTextureUnits` cycles | `GetTextureSurface` cycles | Aligned `GetSurface` cycles | `ValidateSurface` cycles |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | Control 1 | 43,543 | 16,604,098,863 | 260,227,114 | 100,738,264 | 140,120,581 | 122,335,330 |
  | Candidate 1 | 46,051 | 16,946,857,220 | 199,289,658 | 79,874,754 | 110,258,696 | 113,686,734 |
  | Control 2 | 47,477 | 17,752,744,424 | 250,036,613 | 112,986,536 | 135,274,421 | 128,905,504 |
  | Candidate 2 | 48,871 | 17,798,350,739 | 218,932,488 | 83,735,273 | 96,402,668 | 126,050,059 |

- Aggregating raw cycles on each side, texture synchronization fell from 1.485188% to 1.203683% of
  process work, an 18.95% relative reduction and 0.281505 process-percentage-point saving.
  `GetTextureSurface()` fell from 0.622073% to 0.470885% (24.30% relative), and the aligned
  `GetSurface()` subtree fell from 0.801572% to 0.594791% (25.80% relative). The small cache
  comparisons increased `GetTextureSurface()` self share from 0.051840% to 0.084340%, but the
  avoided lookup was materially larger. `ValidateSurface()` changed from 0.731269% to 0.689985%
  (5.65% relative), close enough to animated-scene variation that no validation win is claimed.
  Inspected frames remained visually clean at the capped 58-60 FPS range.
- The final source layout moves the independently testable record contract into
  `surface_selection_cache.h` without changing the profiled runtime algorithm. The physical-Thor
  focused rasterizer-cache suite passed 24 assertions in five cases, including exact parameters,
  generation changes, explicit scale changes despite base equality, and invalid records. The final
  ARM64 source build after test extraction succeeded in 1 minute 32 seconds. Source/test commit
  `1e2c106bc` was pushed directly to `origin/master`.
- The exact post-commit JDK 21 `:app:assembleVanillaRelWithDebInfoLite
  -PthorFrameProfiling=false --no-configuration-cache` production build passed in 2 minutes 42
  seconds. The ARM64-only APK is 29,012,988 bytes with SHA-256
  `FB110D6656C2B65D7D0317025533AA960F471C778869EF684652D22EC20A83D5`. It reports package
  `org.azahar_emu.azahar.debug`, version `1e2c106bc-vanilla-thor`, minimum SDK 29, target SDK 37,
  and no manifest `DEBUGGABLE` attribute. APK Signature Scheme v2 verification passed with the
  established signer-certificate SHA-256
  `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`. The active native cache
  records `ENABLE_THOR_FRAME_PROFILING=OFF`, and the unstripped production library contains no
  Thor frame-profiler marker or warning string. The strict power-tool default now names this exact
  production version, and its deterministic self-test passes.
- Wi-Fi ADB installed that exact APK, and the on-device base APK reproduced SHA-256
  `FB110D6656C2B65D7D0317025533AA960F471C778869EF684652D22EC20A83D5`. The accepted configuration
  remained byte-identical at SHA-256
  `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`; Thor performance/fan modes
  remained 2/4 and primary brightness remained 255. Super Mario 3D Land reported exact program ID
  `0004000000054000`, Turnip Mesa 25.99.99, and Adreno 740. Both physical panels rendered cleanly:
  the primary attract-loop frame showed 59 FPS with SHA-256
  `F18067587881283AA10FEDE333C444FCAE927A41AB06D1BA30E53ED991D92998`, and the secondary control
  frame had SHA-256 `BC7BEFAF4185BBC0D32B8409220A3207307AAAB435210CC7E6E50B30EEFB84CC`.
  Two Azahar BLAST surfaces were live. The active AudioFlinger track was 32,728 Hz, 1,962 frames,
  122.23 ms reported latency, and zero underruns. No profiler, fatal, device-lost, or ANR log matched.
  AC remained connected at 80%, 4.265 V, and 22.0 C, so this is production correctness evidence and
  not a battery-watt measurement.
- Exact bounded cleanup removed 3,032,860,665 logical host bytes: obsolete debug packaging,
  Gradle/JNI/R8/mapping/symbol staging, the four A/B traces and APK twins, and temporary production
  screenshots/UI dumps. The device-side test runner, Simpleperf binary, four traces, and temporary
  screenshots/UI dumps removed another 486,925,748 bytes. The retained build output is only the
  3,252,280,333-byte active `5h1x5ud1` ARM64 CMake/Ninja cache plus the 29,012,988-byte production
  APK and 476-byte metadata. Azahar was force-stopped and no PID remained. Historical repo-root
  artifacts and the user's four pre-existing source edits were not staged, modified, or deleted.
- Entry 164 raises the ledger to 164 numbered entries and 163 active accepted entries because the
  unsafe absolute-offset ARM64 page-table entry remains withdrawn. The repeated bracket proves less
  CPU work in the aligned texture-selection path; it is not additive with earlier animated-scene
  results and does not prove higher capped FPS or lower battery watts. Testing remained AC-powered,
  so the physical discharging-battery mean and nearest-rank P95 power at or below 6 W remain open.

## MrPurple T30 Driver Compatibility and Rejection Bracket (2026-08-20)

- Entry 165 tests the latest official MrPurple package instead of treating release enthusiasm as
  performance evidence. GitHub release `vturnip_mrpurple_T30-toasted.adpkg`, published 2026-08-17,
  supplied `turnip_mrpurple_T30-toasted.adpkg.zip`: 3,713,730 bytes with SHA-256
  `F65B2D3353FD4AA7190BB5426B94468E99FFEA7A58A830BC0C4651DB89353227`, exactly matching the
  publisher's digest. Its metadata names `Turnip Adreno Driver T30 (@Mr_Purple_666)`, version
  `26.3.0-T30-1.4.359`, minimum API 30, and `vulkan.purple.so`. The extracted metadata and library
  SHA-256 values are `BF6C432FFD05A254A9531920F6EC826ABB2BCF91C52A0C5467167A0EE1940F73` and
  `1D80DFA019659B008E4669311DB5B1E4A02AF59FF1D5458A98E2F5FE18ED013B`.
- Wi-Fi ADB placed the verified archive in the user-facing Azahar driver library at
  `/sdcard/Azaharuser/gpu_drivers/turnip_mrpurple_T30-toasted.adpkg.zip`. A controlled private-driver
  swap then proved this was the active renderer rather than a system-driver fallback: Azahar logged
  PurpleVK public driver 26.2.99, PurpleVK-public Adreno 740 commit `62ac221a33`, Vulkan 1.4.359, and
  exact Super Mario 3D Land program ID `0004000000054000`. Both physical displays rendered cleanly,
  including the attract loop and title prompt, and the primary held the 60-FPS cap.
- The fixed comparison used the byte-identical accepted configuration, modes 2/4, brightness 255,
  exact title and direct launch intent. Every replica started cold, warmed for 45 seconds, and then
  recorded an exact 20-second process `simpleperf stat` interval. Three independent replicas per
  driver produced:

  | Driver | CPU cycles, three replicas | Mean cycles | Instructions, three replicas | Mean instructions |
  | --- | --- | ---: | --- | ---: |
  | MrPurple T30 | 13,120,406,125 / 14,174,994,770 / 14,479,132,655 | 13,924,844,516.667 | 9,046,952,833 / 9,080,200,309 / 9,043,685,074 | 9,056,946,072.000 |
  | generic Turnip R8 | 13,804,906,416 / 14,258,355,535 / 12,883,507,646 | 13,648,923,199.000 | 8,966,501,796 / 8,864,318,830 / 8,954,128,552 | 8,928,316,392.667 |

- T30 therefore consumed 2.021561% more CPU cycles and 1.440694% more instructions than R8. The
  cycle counter was noisy at about 5.1% coefficient of variation for each driver, but instruction
  variation was only 0.2231% for T30 and 0.6246% for R8, and the two instruction ranges did not
  overlap. Equivalently, R8 saved 1.981504% cycles and 1.420232% instructions relative to T30.
  T30 is compatible and may still help a different title, but it supplied no speed or CPU-energy
  gain in this representative 3D bracket. Generic R8 was restored byte-identically and remains the
  accepted default; T30 remains available for explicit per-title experiments.
- This is a rejected candidate, so Entry 165 does not raise the active accepted-entry count. The
  Thor remained AC-powered at the 60-FPS cap; battery status during the rolling check was 80%,
  4.265 V, and 24.0 C. These counters are not watts, and the physical discharging-battery mean and
  nearest-rank P95 power at or below 6 W remain an open gate.
- Final cleanup removed 36,294,298 logical bytes of bounded T30 host screenshots/archive staging,
  the redundant private T30 archive and extracted library, and the two post-entry-164 Simpleperf
  traces. The verified external T30 archive remains in Azahar's driver library as the requested
  manual option. The production APK was restored from the retained profiling-off artifact: its
  on-device base APK reproduced SHA-256
  `FB110D6656C2B65D7D0317025533AA960F471C778869EF684652D22EC20A83D5` and version
  `1e2c106bc-vanilla-thor`; the accepted configuration reproduced SHA-256
  `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`. A cold title launch logged
  active generic R8 metadata, Turnip Mesa 25.99.99 / Adreno 740, Vulkan 1.4.335, and exact program
  ID `0004000000054000`, with no fatal, device-lost, or ANR match. Modes 2/4 and brightness 255 were
  unchanged. Azahar was force-stopped and no PID remained.

## 2026-08-20 Upstream Android Surface and Motion Safety Merge

- Command-line Git fetched official `upstream/master` from `f6a3e3aa5` through `c0d923ba2`, seven
  commits. The Android-relevant commits add native-window/surface lifetime guards
  (`3c6a44017`) and serialize NDK motion queue/factory lifetime (`07ec274d4`); the remainder updates
  macOS networking/UI, Qt 6.10.3, compatibility data, and translations. Merge `411e559ba` includes
  the complete upstream tip and was pushed to `origin/master`.
- Two conflicts were resolved semantically. `native.cpp` retains the Thor game/cache helpers and
  adds upstream's recursive surface mutex, primary-surface condition variable, `System::Init()`
  callback, and guarded primary/secondary replacement/destruction. `PresentWindow` retains the
  Thor one-submission/direct-swapchain route and adds duplicate-native-window suppression plus
  cleanup of an unconsumed `next_surface`. Upstream's obsolete `command_pool` destruction was not
  copied because this fork deliberately removed that command pool, its second queue submission,
  `render_ready`, and `present_done`.
- The full profiling-off JDK 17 ARM64 native build passed after a clean broad CMake rebuild. The
  debuggable APK is 32,447,459 bytes with SHA-256
  `0DFD9D505A4E4226F5CEDAC618265B35F6B8FD42362725368D61F8D531E06F47` and version
  `411e559ba-vanilla-debug`. The release-optimized Lite APK is 29,017,848 bytes with SHA-256
  `C0E93E2CF489E7896250B8327AA41E8C7DDC91FC13B5CD3B453A9CC4445EA534` and version
  `411e559ba-vanilla-thor`; it has no `DEBUGGABLE` package flag. APK Signature Scheme v2 verification
  passed with signer-certificate SHA-256
  `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`, and the active native cache
  records `ENABLE_THOR_FRAME_PROFILING=OFF`.
- Wi-Fi ADB installed the debug APK and cold-launched exact Super Mario 3D Land program ID
  `0004000000054000` on generic Turnip R8 / Mesa 25.99.99 / Vulkan 1.4.335. Five consecutive
  home/resume cycles retained PID 10014, destroyed/recreated the secondary surface each time, and
  finished with both physical BLAST surfaces live. No native-window-in-use, fatal, device-lost,
  Vulkan, or ANR error matched. The clean 60-FPS primary frame had SHA-256
  `F6B2A4385EF9C86A3A2FC0D71EAF9909003267B24111679AF905ACACF74D7A9D`; the secondary frame had
  SHA-256 `BC7BEFAF4185BBC0D32B8409220A3207307AAAB435210CC7E6E50B30EEFB84CC`.
- The first production cold launch encountered one Turnip `VK_ERROR_DEVICE_LOST` after about nine
  seconds and aborted from `MasterSemaphoreFence::WaitThread`; this failure is retained in the
  record rather than hidden. It did not reproduce in two subsequent independent 30-second cold
  launches. The second surviving production process retained PID 13043 across three additional
  home/resume surface cycles, kept both BLAST surfaces live, and rendered a clean 60-FPS primary
  frame with SHA-256 `A1A1EF458CBB7DDA7447CDF3DAD8997D0EF8158E85861EFD7B7F96A19F672EA1`.
  This supports the surface-lock integration but does not prove that rare Turnip device loss is
  eliminated; future long stability runs must continue to count every device loss.
- The freshly linked 449,046,848-byte ARM64 test runner executed on the Thor: 171 of 177 cases
  passed, three failed, and three skipped; 925,398 of 925,401 assertions passed. The only failures
  were the established standalone-JNI missing `get_build_flavor` cases, and the only skips were the
  established missing-DSP-firmware cases. No new failure appeared.
- The installed production base APK reproduced SHA-256
  `C0E93E2CF489E7896250B8327AA41E8C7DDC91FC13B5CD3B453A9CC4445EA534`; the configuration remained
  byte-identical at SHA-256 `EC42812B2580738DB6994126A1BB92BBEC4BBBDC11D3035330901E58ACD44E21`.
  Generic R8 remains active, while the verified T30 archive remains available manually. Modes 2/4
  and brightness 255 were unchanged. Cleanup removed 496,403,144 logical host bytes and 452,950,957
  device bytes of bounded APK/library twins, test runner, and screenshots. Azahar was force-stopped
  and no PID remained. This safety merge is not a performance entry and supplies no FPS gain or
  battery-watt evidence; the discharging-battery mean/P95 <=6 W gate remains open.

## 2026-09-07 Upstream Vulkan Surface-Lifetime Merge and Conception II Crash

- Command-line Git fetched official `upstream/master` from `c0d923ba2` through `abdc43af0`, 44
  commits, merged as `3b27718bf`. Eight of them rewrite Vulkan surface lifetime and recycling:
  `079bda1f3` latent surface garbage collection, `2ef875ac1` separate resource-tick and
  resource-free-tick, `d216f961a` surface recycling implementation, `604ade099` recycling criteria,
  `c2783e110` framebuffer removal for upscaled recycled textures, `aaa3d3032` copy-texture
  invalidation after `ScaleUp`, `ab89916d1` destroy-before-recreate, and `d08a7c654` stale
  shadow-buffer images. The remainder is Android UI, audio, Qt, libretro, and translations.
- The reported Conception II (`0004000000112C00`) failure on `bfaf28f39-vanilla-thor` was a driver
  segfault, not an emulator assertion: `SIGSEGV` / `SEGV_ACCERR` in `fdl6_view_init<(chip)7>` inside
  `vulkan.ad07xx.so` on the `VulkanWorker` thread, 172 s into the process, with Azahar's own log
  ending mid-session and no fatal entry. That signature is a freed `VkImage` reaching Turnip's view
  initialization, which is precisely the inter-frame recycling dependency `d216f961a` and the
  long-lived descriptor reference `d08a7c654` describe. The merge did not reproduce it across a
  boot, character creation, opening cinematics, and gameplay run at 60 FPS.
- Eight merge conflicts were resolved against this fork's ledger rather than by taking a side.
  `source.cpp` keeps the protected PCM16 suffix decoder and the underrun tail clear, gains
  upstream's `current_buffer_length`/`mono_or_stereo`/`format`/`is_looping` bookkeeping so the new
  looping refresh has its inputs, and drops the old `frame_position * rate_multiplier` accounting
  that upstream replaced with exact AudioInterp consumption; upstream's unconditional
  `current_frame.fill({})` was not restored. `rasterizer_cache.h` keeps
  `IsResourceRetirementComplete()` but takes upstream's separate free-tick. Guest-texture and
  presentation samplers keep anisotropy disabled, so upstream's `use_anisotropy` probe was removed
  rather than left unused. `strings.xml` keeps the descriptive texture-filter labels.
  `EmulationActivity.kt` takes `NativeLibrary.initMultiplayer()` and keeps the fork's
  `SecondaryDisplay(context, settings)` signature, including on upstream's new early-exit path.
  `break_on_unmapped_memory_access` was upstream's own setting, removed by `03c0a94e6`; retaining
  its log line was a resolution error that the first native build caught.
- The merge then exposed a second, different crash at 441 s: `SIGTRAP` / `TRAP_BRKPT` on the
  `NativeEmulation` thread with abort message `vk_texture_runtime.cpp:1308: Assertion Failed!`,
  through `Surface::ImageView` <- `SyncUtilityTextures` <- `Draw`. Upstream's rewritten
  `SyncUtilityTextures()` binds the unit named by `lighting.config0.shadow_selector` as an R32Uint
  storage image, but storage usage and the mutable RGBA8 allocation are only applied when
  `TextureInfo::is_shadow_source` was true at construction, which requires the unit's type to be
  `Shadow2D` or `ShadowCube`. Conception II enables shadow reading on a unit that is not typed that
  way, so the surface kept its native format and `StorageView()` tripped
  `traits.native == eR8G8B8A8Unorm`. The merged function is byte-identical to upstream's tip, so
  this is an upstream defect that the fork's previous early return simply never reached.
- `Surface::SupportsStorageView()` now mirrors the allocator's exact
  `native == eR8G8B8A8Unorm && storage_support` condition, and the shadow-reading branch binds the
  null surface, with a one-shot warning, when it fails. This is the same fallback upstream already
  uses for the no-shadow case, so the crash degrades to a null shadow map instead of aborting.
- Evidence status. The merge's fix for the original Turnip segfault is device-verified. The
  shadow-source guard is verified statically and by a clean 10 min 39 s session on
  `ab70d18d2-vanilla-thor`, but the fallback warning did not fire, so the shadow-reading path was
  not re-exercised by scripted input and that guard has no on-device confirmation yet. ADB
  `input keyevent` reaches `BUTTON_A` but cannot drive this title's menu cursor, which needs a hat
  axis. The APK carrying the guard is 29,117,484 bytes with SHA-256
  `1E5EEF15931C4B18A6B3C8B934C65E109F9BD72610489E802EED3F59C0566AA3`. The session ran on USB serial
  `c3ca0370`, generic Turnip R8 / Vulkan 1.4.335, resolution factor 3, Anime4K, Eco Turbo on, with
  the user's performance mode 1 and fan mode 4 left unchanged. No power or FPS claim is made.

## 2026-09-07 Per-Title Settings, Snapdragon GSR, and the Conception II Crisp Profile

- The complaint was image softness, not missing detail. On the Thor the 1920x1080 primary panel
  shows the 400x240 top screen at 1800x1080 and the 1240x1080 secondary panel shows the 320x240
  bottom screen at 1240x930 (both from `dumpsys display`). At the accepted 3x the screens are
  1200x720 and 960x720, so presentation stretches them 1.5x and 1.29x with bilinear filtering
  after the Anime4K texture filter has already smoothed every texture. At 5x they are 2000x1200
  and 1600x1200 and presentation downscales 0.9x and 0.78x instead. That is a geometric argument
  for supersampled downscale over any upscaling filter when the goal is crispness without
  invented detail; it is a per-title choice and the global 3x acceptance configuration is
  unchanged. 4x would leave the bottom screen at 0.97x and the top at 1.125x.
- Android per-title settings are now data-driven. `Config::ApplyGameSettings()` overlays
  `GameSettings/<16-hex title id>.ini` after the global reload at boot and on
  `reloadSettings()`, session-only, never writing `config.ini`. The overlay runs `ReadValues()`
  with `sparse_overlay` set: `ReadSetting` returns early for absent keys, the raw `Get*` reads
  are guarded the same way, and Controls, Camera, log filter, LLE selection, Debugging, and
  Miscellaneous are skipped. The hardcoded E.X. Troopers branch in `native.cpp` is gone; its
  manifest now carries the same values. Bundled profiles under `assets/game_profiles/` are
  copied into `GameSettings/` only when absent. The Kotlin side finishes the dormant per-game
  scaffolding: the long-press **Game Settings** button opens the ordinary settings UI scoped to
  the title, hides Camera/Controls/Network/Debug and the reset action, and writes only keys that
  differ from the global value or were already overridden, replacing the file so removed
  overrides disappear. `DocumentsTree.resolvePath()` gates both the Kotlin seeding step and
  native user-directory opens through a fixed whitelist; `/GameSettings/` had to be added or the
  directory was created empty and the launch-time read was refused.
- Snapdragon Game Super Resolution 1 is vendored verbatim from Qualcomm's BSD-3-Clause
  `sgsr1_shader_mobile.frag` as `ScreenFilter::SGSR`, present pipeline 4, with only the
  `ViewportInfo` and sampler plumbing adapted. Its upscale-only guard compares covered area
  because `o_resolution` is stored height-first and the guest screen may be rotated. At 5x the
  guard keeps SGSR inert; it engages only if a title's resolution is lowered. SGSR 2 needs motion
  vectors and depth that PICA content cannot supply, and Adreno Frame Motion Engine is not an
  app-callable API. Because the Thor runs Mesa Turnip rather than the proprietary driver, the
  `VK_QCOM_*` vendor extensions are largely unavailable; SGSR works precisely because it is a
  plain shader.
- Evidence, USB serial `c3ca0370`, generic Turnip R8. APK 29,131,387 bytes, SHA-256
  `E356CA31624308B3F076AEB17DE3384B414109607EC85EB1C4C2A1CBA8CAF099`. First launch seeded
  `GameSettings/0004000000053700.ini` and `0004000000112C00.ini` with no install errors. Launching
  Conception II logged `Applied per-title settings /GameSettings/0004000000112C00.ini`, then
  `Renderer_UseResolutionFactor: 5`, `Renderer_TextureFilter: None`,
  `Renderer_ScreenFilter: Snapdragon GSR 1`, `Renderer_FilterMode: true`, at 60 FPS on the title
  screen with the crash buffer empty; `config.ini` still read `resolution_factor = 3`,
  `texture_filter = 1`, `screen_filter = 0`. The per-title root showed exactly General, System,
  Graphics, Layout, Audio. Changing Screen Filter to None and backing out rewrote the file to
  the four overlay keys with `screen_filter = 0` and left `config.ini` untouched; switching back
  restored `screen_filter = 2`. The opening FMV is pre-rendered video and is not crispness
  evidence; the rendered title screen is. No power or sustained-FPS claim is made for 5x: it is
  roughly 2.8x the pixel work of 3x and needs a matched bracket before any title adopts it as a
  default beyond this profile.
- Texture packs were considered and rejected for this goal. No Conception II pack exists in the
  known 3DS indexes, no machine-readable 3DS pack index exists at all, every available pack is
  AI-upscaled, and dumping textures writes large PNG sets to the SD card. A manifest-driven pack
  browser modelled on the driver picker remains possible later.

## Combined Vulkan Vertex/Fixed Stream Reservation Rejection (2026-08-20)

- Entry 166 tested the next ranked post-entry-164 CPU candidate rather than accepting a source-level
  operation count. `RasterizerVulkan::SetupVertexArray()` was still about 3% inclusive in the fresh
  Super Mario 3D Land profile. The candidate reserved vertex bytes plus the existing 256-byte
  fixed/default-attribute upper bound in one `StreamBuffer::Map()`, filled the same contiguous byte
  layout, and committed the used combined size once. The prior path mapped and committed the vertex
  region, then immediately mapped and committed its contiguous fixed/default tail. This removed one
  `Map()` and one `Commit()` from every accelerated draw without changing bindings, offsets, data,
  or the reservation/watch lifetime.
- Both profiling-off APKs completed the full ARM64 build. The 32,447,459-byte control APK has
  SHA-256 `0DFD9D505A4E4226F5CEDAC618265B35F6B8FD42362725368D61F8D531E06F47`;
  its 459,200,920-byte unstripped library has SHA-256
  `D4D683C4766E4A283F0A501A11FC2DFDBB01026B0D49CED1CD96141ED30C6A99`. The
  32,448,507-byte candidate APK has SHA-256
  `7A45CB8D9F59AD9BB4A824422CFD58F18E7776BD47AC88820E290F470B110673`;
  its 459,200,936-byte library has SHA-256
  `A7B46E0F46885A65C7D12207EA12287771CD20850AFD02D3642C2FA78294D16E`.
  Final linked code confirmed two total `Map()`/`Commit()` calls across the control pair of
  functions and one of each across the candidate pair; their combined linked size fell 32 bytes,
  from `0x93c` to `0x91c`.
- The physical AYN Thor bracket used Wi-Fi ADB, exact program ID `0004000000054000`, accepted generic
  Turnip R8 metadata and Mesa 25.99.99 runtime, the byte-identical accepted configuration, modes
  2/4, brightness 255, a 45-second cold-launch warmup, and a 30-second 4-kHz user-cycle call-graph
  capture. Each side ran three times in alternating control/candidate order. All six traces lost
  zero samples and all six logs had the exact title and driver with no fatal, device-lost, or ANR
  match.

  | Alternating run | Samples | Process cycles | `AccelerateDrawBatch` cycles | `SetupVertexArray` cycles | Aggregate `Map` cycles | Aggregate `Commit` cycles |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | Control 1 | 47,372 | 17,531,110,726 | 2,020,582,157 | 329,793,991 | 125,781,314 | 30,602,578 |
  | Candidate 1 | 44,771 | 16,858,370,665 | 1,992,156,860 | 335,620,105 | 112,376,610 | 27,115,199 |
  | Control 2 | 47,935 | 17,670,779,067 | 2,021,843,633 | 360,410,684 | 147,050,665 | 32,885,268 |
  | Candidate 2 | 48,093 | 17,618,078,367 | 1,998,606,210 | 356,220,104 | 105,221,524 | 30,479,773 |
  | Control 3 | 46,039 | 17,045,068,200 | 1,996,380,409 | 333,374,758 | 129,586,273 | 29,593,443 |
  | Candidate 3 | 46,457 | 17,197,181,417 | 1,961,062,322 | 356,424,327 | 109,810,608 | 21,276,090 |

- Aggregated raw `Map()` and `Commit()` cycles did fall 18.639689% and 15.266470%, respectively,
  but the complete target path did not improve. `SetupVertexArray()` rose from 1,023,579,433 to
  1,048,264,536 cycles, a 2.411645% regression; normalized process share rose from 1.959118% to
  2.028626%, and its share of `AccelerateDrawBatch()` rose from 16.950030% to 17.612488%. The three
  paired path deltas were +1.766592%, -1.162724%, and +6.914011%. Control 3 and Candidate 3 captured
  the exact same clean 60-FPS primary frame, SHA-256
  `FC32F121D33BA1FDE4C5A003D47AE42438BE6319983F5618A94DCAAAFBF63BE4`, making the strongest
  regression pair visually and phase matched. Fewer helper calls were not a forest-level win when
  the full recurring path became slower.
- The candidate was fully reverted with no source delta left, so Entry 166 does not raise the 163
  active accepted-entry count. The exact signed production APK was restored and reproduced
  on-device SHA-256 `C0E93E2CF489E7896250B8327AA41E8C7DDC91FC13B5CD3B453A9CC4445EA534`,
  version `411e559ba-vanilla-thor`. Its cold 30-second title check logged exact R8, Mesa 25.99.99,
  Adreno 740, Vulkan 1.4.335, and program ID `0004000000054000`, with no fatal, device-lost, or ANR
  match. Both physical panels were visually clean; primary and secondary screenshot SHA-256 values
  were `75F9C130F7420BF2537C011D23DB101688F2943CEFED63BA39510C449F2D1803` and
  `6D566815461890C3DD7344FF2C1FF521A2410177A8CDEAD600A097D4540C98C6`. The
  accepted config hash, modes, fan, and brightness remained unchanged. Azahar was force-stopped and
  no PID remained.
- The Thor stayed AC-powered at 80%, 4.264 V, and 25.0 C. This rejects a CPU-path candidate; it is
  not battery-discharge evidence. The physical mean and nearest-rank P95 power at or below 6 W gate
  remains open.

## Consecutive Vulkan Texture-Descriptor Reuse Rejection (2026-08-20)

- Entry 167 tested a one-entry cache for consecutive ordinary texture descriptor sets in the fresh
  Super Mario 3D Land profile. The candidate still resolved and validated all three texture
  surfaces and samplers, but reused the prior descriptor set when the exact image views, samplers,
  and rasterizer-cache surface generation matched. It retained the set through the current GPU tick
  and avoided redundant descriptor updates and binds. Cube descriptors kept the established route.
  A successful full 2,203-action ARM64 build produced a 32,447,243-byte profiling-off candidate APK
  with SHA-256 `B2891AB29EA2154F713B571D56F0E3A912A81D399C1BD47B7FC4F062D80227E1`;
  its 460,158,192-byte unstripped library has SHA-256
  `1C08A6D5726AD6766A96C3D85E19B51C45F71C602A8580AC4D11885D167B329B`.
- The physical Thor bracket alternated the preserved Entry 166 control and this candidate three
  times. Every run used Wi-Fi ADB, exact program ID `0004000000054000`, generic Turnip R8 metadata,
  Mesa 25.99.99, Adreno 740, Vulkan 1.4.335, the accepted config, modes 2/4, brightness 255, a
  45-second cold warmup, and a 30-second 4-kHz user-cycle call graph. All six profiles lost zero
  samples. All six logs matched the title and driver and had no fatal, device-lost, or ANR match.
  Control 2/Candidate 3 and Candidate 2/Control 3 captured the same two frames in swapped order,
  with exact screenshot SHA-256 values
  `C4BE5AF1CCB3D238CCA83F07D952F04B6114E2725DB41B1566022D90CA134A4F` and
  `CADC7C9DFDAF921E2104F523EB75EB49287EA6FA87F711AB2D8CCCE9071DBDBE`.

  | Alternating run | Process cycles | `AccelerateDrawBatch` cycles | `SyncTextureUnits` cycles | Descriptor-update self cycles | Descriptor-bind self cycles |
  | --- | ---: | ---: | ---: | ---: | ---: |
  | Control 1 | 17,413,061,757 | 2,010,554,943 | 218,981,274 | 114,699,123 | 81,049,003 |
  | Candidate 1 | 17,179,104,958 | 1,974,771,957 | 225,017,311 | 65,062,287 | 64,508,107 |
  | Control 2 | 16,854,788,318 | 1,998,420,316 | 200,466,187 | 107,442,710 | 68,989,212 |
  | Candidate 2 | 16,354,027,857 | 1,795,382,101 | 195,436,075 | 61,677,531 | 72,021,495 |
  | Control 3 | 16,292,947,527 | 1,826,772,537 | 201,856,182 | 105,275,739 | 86,644,608 |
  | Candidate 3 | 17,381,589,303 | 2,044,176,664 | 228,904,639 | 74,512,773 | 62,908,936 |

- The cache did reduce its intended leaves: aggregate descriptor-update self cycles fell
  38.533357%, descriptor-bind self cycles fell 15.735948%, and descriptor-pool commit self cycles
  fell 36.207488%. Including the cache's own `SyncTextureUnits()` self work, those four direct
  costs fell 22.620343% raw and 23.158234% in process-normalized share. This is real targeted-path
  evidence, not a whole-emulator win.
- The forest-level gate did not clear. Aggregate process work rose 0.699998%, complete
  `AccelerateDrawBatch()` work fell only 0.366998% raw, and its normalized share fell 1.059579%.
  `SyncTextureUnits()` itself rose 4.515406% inclusive and 3.788886% in normalized share because the
  exact surface/sampler comparisons remained recurring work. Animated-scene variation is larger
  than the complete-path delta, so the result cannot support an FPS or energy claim.
- The 128-line, seven-file candidate was fully reverted with no source delta left. Entry 167 does
  not raise the 163 active accepted-entry count. Reconsider descriptor reuse only with a simpler
  ownership design or a title/scene where complete draw-path or frame-level work falls materially,
  not merely because driver leaf calls decline. The Thor was wall-powered, so the discharging-
  battery mean and nearest-rank P95 at or below 6 W gate remains open.

## Consecutive Vulkan Pipeline Bookkeeping Rejection (2026-08-20)

- Entry 168 tested two ways to reduce the recurring Vulkan pipeline lookup/bookkeeping subtree in
  the post-`a5de2546c` Super Mario 3D Land profile. The first remembered the last exact static state
  and pipeline and bypassed `ShaderDiskCache::GetPipeline()` entirely on a match. Its focused
  physical-device suite passed 53 assertions in six `[video_core][vulkan]` cases, but the 32,502,453-
  byte APK (`340E75870BDB070E9F7D249B9D94F898FCFDA66184689DD46BAD586680E74FBD`) crashed the exact title
  after about 18 seconds. The Vulkan worker raised `SIGSEGV` at address `0x40` in Turnip
  `tu_cmd_render<chip7>+312` (driver build ID `c4461fe...`). The shortcut was immediately reverted;
  a rebuilt control survived the same launch. Pipeline map lookup/lifetime is therefore mandatory
  even when consecutive Azahar state appears equal.
- The narrower candidate always computed `StaticPipelineInfo::OptimizedHash()`, always performed
  `graphics_pipelines.try_emplace()`, and always returned through the established map. It skipped
  only the full static hash, known-pipeline set lookup, and disk append check when the immediately
  preceding 152-byte `StaticPipelineInfo` matched; dynamic blend color, stencil reference, and
  viewport changes intentionally did not invalidate that disk-only comparison. The same Thor suite
  passed all 53 assertions in six cases. The preserved profiling-off control APK was 32,501,545
  bytes with SHA-256 `F65033F361385CBF43ED43955FEA6354C7E303BD71A9714F396DF32B41259141`;
  the 32,502,725-byte candidate was
  `6522023D0271DC842EEA533A1E2DEE144F56E982E02C2D7233BA0B62FEA91004`.
- Wi-Fi ADB retained the byte-identical accepted configuration, exact program ID
  `0004000000054000`, generic Turnip R8 metadata, Mesa 25.99.99, Adreno 740, and profiling-off
  builds for the performance screens. The 30-second candidate call graph recorded 45,157 samples,
  17,874,885,557 user cycles, and zero lost samples. Two controls recorded 18,147,651,668 and
  20,103,045,597 cycles; their `GetPipeline()` inclusive shares varied from 0.93% to 0.59%, while
  the candidate was 0.85%. The candidate looked 1.50% better than one control but 11.08% better
  than the other, proving animated-scene/system variance dominated this sub-percent subtree rather
  than proving a whole-emulator gain.
- A temporary profiler-only counter then supplied the decisive work-frequency check and was removed
  before commit. Cumulative repeated-static-state counts were 50,312/100,000, 102,554/200,000, and
  153,847/300,000 queries: only 51.2823% of calls avoided the hash/set route. Paying the full static
  comparison on every draw to skip tiny bookkeeping on half the calls is not a forest-level win
  when the complete function is below 1% of process work. The narrow candidate and its tests were
  fully reverted; Entry 168 does not raise the 163 active accepted-entry count. Reconsider only if
  a ranked title shows materially higher consecutive repetition and a matched complete-path bracket
  clears scene noise. Testing was AC/USB-powered, so no FPS or wattage gain is claimed and the
  discharging-battery mean and nearest-rank P95 at or below 6 W gate remains open.

## Dynamic Vertex Input Pipeline-Churn Rejection (2026-08-20)

- Entry 169 investigated whether `VK_EXT_vertex_input_dynamic_state` could materially reduce the
  recurring Vulkan pipeline-bind path seen in the post-`a5de2546c` Super Mario 3D Land profile.
  The clean profiling-off baseline attributed 1.62% inclusive/0.16% self to Azahar
  `BindPipeline()` and 1.55% self to Turnip `tu_CmdBindPipeline`; this made actual bind frequency,
  rather than sub-percent lookup bookkeeping, the relevant first gate.
- A temporary `THOR_FRAME_PROFILING` probe counted consecutive state changes without modifying the
  render path. On the physical AYN Thor, Wi-Fi ADB confirmed exact program ID
  `0004000000054000`, generic Turnip R8 metadata, Mesa 25.99.99, and Adreno 740. At 300,000 draws,
  the pipeline object changed 136,467 times (45.4890%). Within those overlapping changes, the
  vertex shader changed 89,689 times, geometry shader 50,117, fragment shader 110,894, blending
  state 38,452, attachments 3,576, and vertex layout 76,656.
- The decisive counter compared the complete optimized static key with vertex layout excluded.
  Pipeline changes caused only by vertex layout were exactly 0/100,000, 0/200,000, and 0/300,000.
  Every observed layout change coincided with a shader, blend, or attachment change that would
  still require a different pipeline. Dynamic vertex input therefore has a measured 0% maximum
  consecutive-bind elimination opportunity in this title/scene, despite vertex layout appearing
  in 56.17% of pipeline-change events.
- No extension path was implemented. The profiler-only counters were removed after collection, and
  Entry 169 does not raise the 163 active accepted-entry count. Reconsider only if a different
  ranked title produces a nonzero layout-only rate. Testing was AC/USB-powered, so no FPS or
  wattage gain is claimed and the discharging-battery mean and nearest-rank P95 at or below 6 W
  gate remains open.

## Beginner Guest-Memory Search UX (2026-08-20)

- Android's paused offline-only guest-memory search now appears as an always-visible `Find value`
  toolbar action. Its main path explains the visible-number loop in plain language and starts a
  32-bit exact search directly; 8/16-bit sizes and hexadecimal input remain available under
  `Advanced`. Refinement choices now describe what happened in the game (`Enter the new number`,
  `It went up`, `It stayed the same`, and similar), while possible matches are presented by ordinal
  and current value instead of leading with guest addresses.
- The flow now uses the existing activity lifecycle instead of asking users to manage pause state:
  opening Cheats from EmulationActivity pauses guest execution, and the new `Back to game` action
  closes Cheats so EmulationActivity resumes it. A verified temporary write offers the same direct
  return. The native safety contract is unchanged: offline single-player only, acknowledged pause,
  surviving candidates, one pending write, verified readback, and guarded restoration. A later
  safety audit removed one-session raw-address promotion to persistent Gateway cheats; results stay
  temporary until a stable pointer, AOB, or relaunch-validated path exists.
- Pure Kotlin coverage checks decimal/underscored/hexadecimal parsing, missing/negative/out-of-range
  rejection, the complete unsigned 32-bit boundary, all three Gateway code widths, address masking,
  and unsupported-width rejection. The focused Vanilla debug unit suite passed five tests. The
  exact JDK 21 `:app:assembleVanillaRelWithDebInfoLite -PthorFrameProfiling=false
  --no-configuration-cache` build passed in 2 minutes 21 seconds and produced a 29,073,548-byte APK
  with SHA-256 `A8EA9BDDE7AF243B838F5B5A940E3004B1AFED66C7996AA91515760065533A7F`.
- Wi-Fi ADB installed that exact production artifact on the AYN Thor, and the installed `base.apk`
  reproduced the same SHA-256. APS3e was force-stopped before installation and both of its process
  checks stayed empty. In a legally owned offline copy of 7TH DRAGON III CODE: VFD, display-0 visual
  validation confirmed the labeled toolbar action, correctly paused Cheats activity, fitted
  beginner dialog, and optional Advanced size dialog. The Wi-Fi ADB transport disconnected before
  the initial scan tap could be delivered; therefore scan/refine/temporary-write/restore remains an
  explicit pending device validation, and no live write or gameplay-effect claim is made here.
- Source/test commit `aa043576f` was pushed directly to `origin/master`. This is a usability and
  safety improvement, not optimization entry 170: it adds no FPS, frametime, or wattage claim and
  does not change the active accepted-optimization count of 163.

### Guest-memory cheat safety follow-up (2026-08-20)

- Initial exact scans now have a native operation token and a visible Cancel action. Cancellation
  is checked between mapped guest pages, clears partial candidates, waits for the native scan to
  stop, and reports that no game memory was changed before returning control to the UI.
- Temporary-write recovery now retains an undo record if both test-write verification and rollback
  verification fail. Restore still writes only when the current value exactly matches the verified
  temporary value. If the game changed or unmapped the value first, Azahar performs no write and
  permanently discards the stale record so it cannot block later searches or revive on a
  coincidental future value.
- The search UI no longer promotes a one-session raw address into a persistent Gateway cheat.
  Persistent promotion requires a stable pointer, AOB signature, module-relative path, or relaunch
  validation; ordinary manually supplied Gateway cheats remain available through the existing
  editor.
- Gateway parsing now rejects unsupported opcode families instead of accepting lines that execute
  as silent no-ops. Invalid or unsupported lines are initialized safely, and a cheat containing
  one cannot be enabled. All seven bundled Android cheat files passed a local shape and supported-
  opcode audit.
- The focused Vanilla Lite Kotlin suite passed four value-parser/range tests. The ARM64 native test
  and app targets compiled successfully with six expanded memory-search cases and three Gateway
  parser cases embedded in the linked test executable. Per the user's direction, this follow-up
  did not launch, install to, scan on, or otherwise use the Thor; the newly expanded native cases
  therefore remain compile-verified rather than device-executed.

## 2026-08-16 Upstream and RPCS3 ARM64 Review

- Merged 37 commits from `upstream/master` (`d81195bdc` through `b34de55b5`) in merge commit `abb63f2c3`.
- Fetched the later `upstream/master` tip `3392c56ce`. Its two new commits only revise the
  MSVC workaround in `src/core/hle/service/service.{h,cpp}`; they were applied narrowly as
  `44b30dc92` and `5f3b01a9f` so the divergent Thor fork could not replace fork-only files.
  Both service files match the fetched upstream tip exactly.
- Mirrored the directly relevant sibling-project research under [`research/`](research/README.md), including the chapter-by-chapter notes for Whatcookie's "PS3 emulation is fast on ARM now" video and the follow-up "what didn't make the cut" article.
- Kept the new PICA command-list lookup, batching, and four-command vectorizable path from `c688076ac`. This is directly relevant to ARM host CPU time and energy because it reduces command-dispatch overhead without changing guest semantics.
- Kept the shader output register-banking work from `74d38ddcc`, including its A64 shader-JIT changes.
- Corrected the resource-tick comparison introduced by `b34de55b5`. A sentenced surface is retained while the runtime's completed tick is equal to or older than its retirement tick and is deleted only after the completed tick advances beyond it. The upstream comparison did the opposite and could retain sentenced surfaces indefinitely.

### RPCS3 ideas deliberately not copied

- **RPCS3 timer-scaled `busy_wait`:** [RPCS3 #18055](https://github.com/RPCS3/rpcs3/pull/18055) fixed waits that treated a low-frequency ARM generic timer like a multi-GHz x86 cycle counter. Azahar has no equivalent host-timer-calibrated busy-wait utility in its active CPU, audio, or Vulkan paths, so there is no constant to copy. Adding a second correction would repeat the kind of double-calibration regression already seen in related ARM ports.
- **ISB-based spin waiting:** Azahar has no equivalent hot emulator/render spin loop. Its Vulkan scheduler and master semaphore block on condition variables, Vulkan fences, or timeline semaphores, while Dynarmic's ARM64 lock already uses `SEVL`/`WFE`. [RPCS3 #18151](https://github.com/RPCS3/rpcs3/pull/18151) was a small improvement over ineffective ARM `yield`, and [RPCS3 #18830](https://github.com/RPCS3/rpcs3/pull/18830) later confirmed that hardware waits are the better primitive but did not measure an application-level power win at its first call sites.
- **Single-instruction `FMAX`/`FMIN`:** PICA's asymmetric NaN behavior differs from the PPC operation RPCS3 optimized. Azahar's A64 shader JIT intentionally uses `FCMGT` plus `BIF` to preserve PICA results.
- **SPU checksum, SHUFB, SHA3, and dot-product paths:** these target PS3 SPU/PPC workloads and have no direct 3DS guest equivalent. [RPCS3 #18056](https://github.com/RPCS3/rpcs3/pull/18056) is still useful as a method: express the guest permutation directly with native ARM vector operations and verify final codegen. This review found that Azahar's A64 PICA source-swizzle fallback still used a vector copy plus serial lane inserts; the AArch64 shader-swizzle change below closes that specific gap. Its A64/x64 JIT compiler method sets remain equal at 44 methods each.
- **LLVM ARM feature attributes:** [RPCS3 #18133](https://github.com/RPCS3/rpcs3/pull/18133) prevents LLVM from assuming that Snapdragon 8 Gen 2 exposes the Cortex-X3's disabled SVE feature. Azahar's 3DS CPU backend is Dynarmic rather than an LLVM guest recompiler and targets baseline AArch64/NEON, so it cannot reuse that patch. Azahar's existing AArch64 feature detector currently feeds host-information logging, not generated-code feature attributes; optional dot-product/i8mm paths should be added only with Android HWCAP gates and a proven hot integer kernel.
- **A dedicated Vulkan garbage-collection thread:** Azahar already has a Vulkan scheduler, presentation thread, master-semaphore completion waiter, and shader/pipeline workers. Another wake-producing thread is not justified until a Thor trace shows render-thread GC stalls; if needed, GC should first be attached to the existing completion path.
- **A global Cortex-X3 `-mcpu` setting:** Snapdragon 8 Gen 2 is a mixed X3/A715/A710/A510 system, and shipping Thor hardware does not expose every optional architecture feature such as SVE/SVE2. Runtime capability gates are safer than architecture-wide compiler assumptions.

### Next Thor measurements

- Compare the pre-merge baseline and this build in the same fixed title/scene, performance mode, fan mode, brightness, renderer, resolution, and GPU driver.
- Record average FPS, 1% low or frametime distribution, battery power, battery temperature, and thermal slope over a long enough run to reach steady state.
- Use Snapdragon Profiler to check render-pass binning cost and whether dependencies or barriers break concurrent binning. Qualcomm's guidance treats memory writes/resolves and unnecessary CPU-core wakeups as power costs.
- Compare the system Vulkan driver with the current stable Turnip option using both cold and warm caches. Do not claim a driver win until visual output and stability match.

## 2026-08-16 3DS and AYN Manual Review

- Added the public Arm ARM11 MPCore DDI 0360E and ARM946E-S DDI 0201D manuals to the untracked workspace reference library. Their cache, control, WFE/WFI, and cycle-timing sections are guest correctness and instrumentation inputs; they are not host optimization recipes.
- The ARM11 manual documents parallel ALU, multiply, and load/store pipelines with forwarding and instruction-dependent interlocks. Optimize Dynarmic output from measured Snapdragon hot paths rather than trying to preserve guest pipeline structure in generated ARM64 code.
- No complete public PICA200 technical reference manual was found. The archived DMP SIGGRAPH 2007 slide is a high-level, pre-3DS description of PICA200/MAESTRO features and nominal power/throughput, not an authoritative register or shader reference.
- The AYN manual confirms a 120 Hz primary display and 60 Hz secondary display. Benchmark at fixed refresh/brightness and account for the second panel when comparing power; a frame limiter that avoids needless work above the guest rate is more useful than targeting the panel maximum.
- The AYN manual and current AYN product page disagree about UFS generation. Do not use either UFS 3.1 or UFS 4.0 bandwidth as a performance explanation until the connected Thor is queried or measured.
- Full provenance, hashes, and source links live in [`hardware/README.md`](hardware/README.md). The PDF binaries remain outside Git by explicit project policy.

## 2026-08-16 AArch64 Indexed-Draw Reduction

- `RasterizerAccelerated::AnalyzeVertexArray()` scans every indexed draw's `u8` or `u16` index buffer through `Common::FindMinMax()`. The release AArch64 binary proved that the original NEON port accumulated vector minima/maxima but then stored the vectors to the stack and expanded `std::min_element` / `std::max_element` into per-lane extracts, comparisons, conditional selects, stack traffic, and stack-protector work.
- The AArch64 path now reduces the vectors with the architecture's `vminvq_u8` / `vmaxvq_u8` and `vminvq_u16` / `vmaxvq_u16` intrinsics. The SIMD crossover is one full vector on AArch64 (16 byte indices or 8 halfword indices); x86 SSE4.2 and 32-bit NEON retain their existing two-vector crossover and fallback behavior.
- Correctness is unchanged because unsigned horizontal min/max is associative and produces the same extrema as reducing the vector lanes in scalar order. Scalar tails still process every non-vector-aligned index. The empty and scalar-only cases now also initialize their extrema explicitly instead of relying on uninitialized fallback values.
- A focused test checks every prefix length across scalar, exact-vector, multi-vector, and tail cases for both index widths against `std::minmax_element`. It passed on the AYN Thor's Snapdragon 8 Gen 2 with 200 assertions in one test case.
- Binary verification on the built `libcitra-android.so` found one `uminv` and one `umaxv` in each function, with zero `umov` lane extracts and zero stack-check references. The `u8` function shrank from 904 to 284 bytes and the `u16` function from 600 to 284 bytes: 936 bytes, or 62.2%, removed from the pair.
- `:app:assembleVanillaRelWithDebInfoLite` completed successfully. The generated APK is 28,944,839 bytes with SHA-256 `CBD28CDBD3F254FA8F896AFBEF02D95EEF87F9AF55068EA121030363FCADF152`.
- ADB now enumerates the Thor over USB as `c3ca0370` and over Wi-Fi as `192.168.1.33:5555`; USB is the deterministic deployment target. The focused correctness test passes on device, but power/FPS claims remain pending. Compare a fixed indexed-draw-heavy scene with identical title, cache state, renderer, resolution, driver, display layout, performance/fan mode, and brightness, then record FPS, frametime distribution, battery power, temperature, and thermal slope.

## 2026-08-16 AArch64 HLE Audio Downmix

- The 3DS HLE audio final mixer downmixes three 160-sample quadraphonic buses into stereo or mono every DSP frame. The original 2016 generic loop remained scalar in the release AArch64 object even though the larger per-source mixer loop auto-vectorizes successfully.
- The AArch64 path now processes four frames at a time with NEON structure loads/stores, vector integer-to-float conversion, the same multiply/FMA order as the old AArch64 code, truncating float-to-integer conversion, saturating `s32`-to-`s16` narrowing, and saturating accumulation into the current stereo frame. Mono, stereo, and the existing surround-as-stereo fallback all retain their prior behavior; non-AArch64 builds retain the scalar implementation.
- A focused end-to-end mixer test feeds all three buses with lane-varying values that cross both saturation limits and compares mono, stereo, and surround output with the scalar reference. It passed on the AYN Thor with three assertions covering mono, stereo, and surround.
- Exact release codegen changed from one-sample scalar loops to four-sample NEON loops. The stereo body fell from 39 instructions per sample to 20 instructions per four samples (5 per sample), while the mono body fell from 35 instructions per sample to 19 instructions per four samples (4.75 per sample). The containing function shrank from 436 to 280 bytes, a 156-byte or 35.8% reduction.
- The complete `:app:assembleVanillaRelWithDebInfoLite` build passed in 5m33s. The APK is 28,945,479 bytes with SHA-256 `34549C9F41FB5B6D773E40DC8DBD26E9B22D5DA210453915F1358873E2A067B2`.
- The same machine-code audit rejected several tempting false positives: Crypto++ already compiles Rijndael with `-march=armv8-a+crypto`; SoundTouch ships in 16-bit integer mode and its correlation loop already auto-vectorizes to NEON; and the per-source 24-channel HLE mixer already becomes an eight-frame NEON loop. Crypto acceleration can improve encrypted content and service latency, but it is not currently evidence of a sustained FPS or wattage win.

## 2026-08-16 AArch64 PICA Tile Codec

- `MortonCopyTile()` sits in every rasterizer-cache texture upload and download. Exact release AArch64 codegen proved that a full 8x8 RGBA8 tile was still expanded into 64 scalar loads, 64 scalar stores, and, for Vulkan's required RGBA conversion, 64 scalar byte reversals.
- The AArch64 path now maps the PICA 2x2/4x4/8x8 Morton structure directly onto NEON structured loads and stores. RGBA8 upload uses eight `LD2` operations to deinterleave pairs of rows, sixteen vector `REV32` operations when Vulkan needs component reversal, and eight paired 256-bit row stores. That replaces 192 scalar load/reverse/store operations with 32 vector memory/shuffle operations per full converted tile, an 83.3% reduction in those core operations. The reverse download path uses the corresponding `ST2` interleave. Native RGB5A1, RGB565, RGBA4, and D16 tiles use the same approach at 16 bits per pixel.
- The always-expanded PICA texture formats were a second scalar gap. IA8, RG8, I8, A8, and IA4 now combine Morton deinterleave with NEON `ST4` RGBA expansion during uploads instead of processing and storing one pixel at a time. For I8, the old exact AArch64 tile function ran a 53-instruction row body eight times; the ThinLTO full-tile vector body is 54 instructions total, an 87.3% reduction in that hot body. IA4 performs both nibble replications in vector lanes.
- That first optimization was gated to AArch64 and only to formats whose old behavior could be represented exactly: RGBA8 with or without its existing whole-pixel byte reversal, non-converted raw 16-bit formats, and the five upload-only expansion formats above. It deliberately left RGB8/D24 packing, D24S8 rotation, ETC decoding, I4/A4 unpacking, and expanded-format downloads on their scalar paths; the separate RGB8/D24 follow-up below closes the safe 24-bit part of that gap.
- A focused test covers both Morton-to-linear upload and linear-to-Morton download, a deliberately padded ten-pixel stride, native and converted RGBA8, all three native 16-bit color formats, D16, and exact IA8/RG8/I8/A8/IA4 expansion. It compares against byte-wise scalar Morton references and verifies exact raw-format round trips. It passed on the AYN Thor with 17 assertions in one test case.
- The latest `:app:assembleVanillaRelWithDebInfoLite` passed in 2m16s. ThinLTO preserved the intended `LD2`, `UZP`, vector `REV32`, `ST2`, paired-store, and `ST4` sequences in `libcitra-android.so`. The APK is 28,945,287 bytes with SHA-256 `4B8A57E3ECD8754389F6CA568E53BF463F1C0888D94B40BDB001ACA52B8B17B6`.
- This is a static work and instruction-count win, not yet a claimed FPS or wattage result. Texture-cache effectiveness makes the on-device impact title- and scene-dependent; the matched Thor A/B must include texture-streaming scenes and render-target readbacks as well as steady-state gameplay.
- The verified APK installed successfully as `org.azahar_emu.azahar.debug`, launched into `MainActivity`, and remained running. Temporary stripped test runners were deleted from both the PC temp directory and `/data/local/tmp` after the checks.

## 2026-08-16 AArch64 RGB8/D24 Tile Codec

- RGB8 and raw D24 were the remaining byte-packed full-tile formats in the scalar 64-pixel
  `MortonCopyTile()` loop. Vulkan cannot use RGB8 as an attachment and selects a converted RGBA8
  host surface, so RGB8 render-target traffic specifically paid both the Morton walk and scalar
  BGR-to-RGBA expansion. D24-to-float conversion keeps its existing scalar path because changing
  its normalization or rounding would not be byte-equivalent; raw D24 copies are safe to vectorize.
- The AArch64 path now loads each pair of Morton chunks with `LD3`, combines their component
  lanes, and uses one-table `TBL` permutations to recover the two linear rows. Native RGB8/D24
  writes use `ST3`. Converted RGB8 uses explicit `ZIP1`/`ZIP2` interleaving and ordinary paired
  32-byte stores, preserving the exact BGR-to-RGBA channel order and opaque alpha without a
  per-pixel loop. Downloads apply the inverse compile-time-checked permutation.
- This store choice came from the actual Thor core manuals indexed under
  [`hardware/`](hardware/README.md), not an x86 analogy. The X3 and A710 tables list D-form `ST4`
  throughput at one instruction per three cycles, while the A510 lists one per 25 cycles; the A715
  is much stronger at one per cycle. The portable `ZIP` plus paired-store sequence avoids the
  extreme efficiency-core cliff and remains vectorized on every core, but the A715 tradeoff is why
  this is not yet presented as a measured whole-device win.
- Focused tests add native RGB8 and D24 round trips plus converted RGB8 with a padded ten-pixel
  stride. The converted reference independently computes every Morton offset, checks BGR-to-RGBA
  order and alpha `0xFF`, and then verifies an exact inverse round trip. The ARM64 test executable
  linked successfully; per the no-device restriction it was not run.
- Final ThinLTO codegen contains the intended `LD3`, `TBL`, `ZIP`, `STP`, `LD4`, and `ST3`
  instructions. The common converted RGB8 upload symbol fell from 588 to 404 bytes (31.3%), native
  RGB8 upload from 552 to 372 bytes (32.6%), and raw D24 upload from 556 to 376 bytes (32.4%). The
  download wrappers grew by 220 bytes because partial-tile staging duplicates the inlined inverse
  body; full aligned tiles still take the vector path. Avoiding a call in the hot full-tile loop was
  kept as the better theoretical tradeoff pending profiles.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` and
  `:app:assembleVanillaRelWithDebInfoLite` both passed. The APK is 28,963,931 bytes with SHA-256
  `0349A65F8604ECB6A495F1A645CEE6915CAAF17E0343535FEE5E5FBECE38746A`. Only the active
  `arm64-v8a` RelWithDebInfo CMake hash remains. No ADB command, install, launch, or Thor execution
  was performed.
- This is a source, machine-code, and manual-supported instruction reduction, not an FPS or wattage
  claim. A future allowed A/B should target RGB8 render-target churn or readback-heavy scenes with
  fixed title, scene, caches, renderer, resolution, layout, driver, display state, performance/fan
  mode, and run time while recording CPU time, frametimes, battery power, temperature, and thermal
  slope.

## 2026-08-16 AArch64 D24S8 Tile Codec

- D24S8 was the last 32-bit full-tile format still using the scalar 64-pixel Morton loop. Each
  upload pixel performed a scalar load, an eight-bit rotate, and a scalar store; the inverse
  download repeated the same pattern in the other direction.
- The AArch64 path now reuses the exact RGBA8 `LD2`/paired-store Morton geometry and applies one
  `TBL` permutation per 16-byte vector. Upload maps each four source bytes from
  `[b0, b1, b2, b3]` to `[b3, b0, b1, b2]`, matching the old `rotl(u32, 8)`. Download applies
  `[b1, b2, b3, b0]`, matching `rotr(u32, 8)`. A compile-time composition check proves that the
  two table permutations are inverses.
- Final ThinLTO upload code contains eight `LD2`, sixteen one-table `TBL`, and eight paired vector
  stores per full 8x8 tile. That is 32 core load/shuffle/store operations in place of 64 scalar
  loads, 64 rotates, and 64 scalar stores: an 83.3% reduction in that core tile body. The full
  aligned download path is vectorized with the inverse table as well.
- The optimized upload symbol grew from 408 to 440 bytes (7.8%). The download wrapper grew from
  592 to 1,116 bytes because ThinLTO duplicated the inlined vector body into partial-tile staging
  paths. The larger wrapper is an explicit tradeoff for avoiding a call in the full-tile hot loop;
  runtime profiling can revisit it if instruction-cache pressure outweighs the reduced tile work.
- A focused test independently computes the scalar Morton layout with a padded ten-pixel stride,
  verifies the exact D24S8 byte rotation on upload, and verifies an inverse round trip on download.
  The ARM64 test executable compiled and linked, but was not run because the current restriction
  forbids using the Thor.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` and
  `:app:assembleVanillaRelWithDebInfoLite` both passed. The APK is 28,963,743 bytes with SHA-256
  `D3D20220D444185398929E8BC247F54E22B7E77B58D73C3E85B05DCB2DE14C23`. Only the active
  `arm64-v8a` RelWithDebInfo CMake hash remains. No ADB command, install, launch, or Thor execution
  was performed.
- This is a major instruction-count reduction in one texture conversion hot path, not evidence of
  a major whole-emulator FPS or wattage gain. A future allowed matched A/B should target titles and
  scenes with frequent D24S8 depth-stencil upload, readback, or render-target churn and record the
  complete correctness, frametime, power, and thermal acceptance set.

## 2026-08-16 ETC1 Block Decoder

- ETC1 and ETC1A4 were the most expensive remaining texture-upload scalar paths. The old 8x8
  tile loop called the 356-byte `SampleETC1Subtile()` function once for every pixel: 64 calls per
  tile. Each call re-extracted differential/separate base colors, table indices, and flip state
  from the same 64-bit 4x4 block, so those block-invariant calculations were repeated 16 times.
- Uploads now decode the four 4x4 blocks directly. Each block computes both base colors and its
  modifier-table indices once, then reuses them for all 16 pixels. The unchanged per-pixel
  sampler remains available for individual software texture lookups and as an independent
  correctness oracle.
  Final ARM64 ThinLTO code contains exactly four block-decoder calls per full tile and no calls back
  to the old per-pixel sampler.
- The ETC1 upload wrapper shrank from 688 to 408 bytes (40.7%); the ETC1A4 wrapper shrank from 448
  to 396 bytes (11.6%). More importantly, both changed from 64 decoder calls to four. At this
  checkpoint the scalar block helpers were 312 bytes for ETC1 and 348 bytes for ETC1A4, while the
  original 356-byte sampler was unchanged. This is an algorithmic invariant-hoisting win on every
  host architecture, including the Thor's AArch64 cores, rather than a guest-semantics shortcut;
  the later AArch64 SIMD follow-up is recorded below.
- Temporary host differential harnesses checked 100,000 arbitrary raw blocks in both ETC1 and
  ETC1A4 modes (1.6 million pixels per format) and then 10,000 complete padded-stride tiles per
  format. Every byte matched the old sampler, including all flip/differential combinations,
  modifier/sign bits, clamping, ETC1A4 alpha nibble order, subblock placement, and bottom-up output
  rows. The temporary executables and sources were deleted afterward. A permanent focused Catch2
  test covers the same layout and all four mode combinations; the ARM64 test executable compiled
  and linked but was not run because the current restriction forbids using the Thor.
- A direct Vulkan ETC2 compressed-image route was also evaluated. It could eventually avoid CPU
  decompression and reduce texture memory traffic substantially on Adreno, but the current surface
  cache requires common transfer, attachment, and blit behavior and maps PICA compressed formats
  to RGBA8. Guest block orientation, partial updates, copies, and mip generation need a separate
  sampled-only surface design plus device correctness testing, so that larger route was not enabled
  blindly.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` and
  `:app:assembleVanillaRelWithDebInfoLite` both passed. The APK is 28,965,083 bytes with SHA-256
  `A5FDA902BF284313BD710CB3527C2F4F4B6240BEA143694F3E817468A871A75F`. Only the active
  `arm64-v8a` RelWithDebInfo CMake hash remains. No ADB command, install, launch, or Thor execution
  was performed.
- This proves a large reduction in repeated ETC decode work, not a whole-game FPS or wattage
  result. A future allowed matched A/B should use ETC-heavy texture-streaming scenes and record
  texture upload time, frametimes, battery power, temperature, and visual correctness.

## 2026-08-16 AArch64 I4/A4 Tile Expansion

- I4 and A4 uploads previously expanded every 8x8 Morton tile through the scalar per-pixel path.
  A full tile issued 64 byte loads and 192 scalar stores: 256 memory instructions before surrounding
  loop and address work.
- The AArch64 path now processes two rows together. `UZP` recovers the two Morton rows, `SLI`
  replicates each four-bit intensity or alpha value to eight bits, and the existing ZIP/STP RGBA
  store sequence writes both rows. This preserves the required low-nibble-first pixel order and
  deliberately avoids interleaved `ST4` stores on the Thor's X3/A710/A510 core mix.
- A full tile now uses eight word loads and eight paired vector stores: 16 memory instructions,
  93.75% fewer than the old 256. ARM64 ThinLTO code also shrank the I4 wrapper from 616 to 376 bytes
  (39.0%) and the A4 wrapper from 584 to 364 bytes (37.7%). Generated code contains the intended
  `UZP`, `USHR`, `SLI`, `ZIP`, and `STP` instructions and no `ST4`.
- An independent model checked 10,000 arbitrary 32-byte tiles against scalar Morton/nibble
  expansion with exact results. A permanent focused Catch2 test covers both formats and padded
  output stride; the ARM64 test executable compiled and linked but was not run because the current
  restriction forbids using the Thor.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` and
  `:app:assembleVanillaRelWithDebInfoLite` both passed. The APK is 28,963,431 bytes with SHA-256
  `3B942483933BC86B845DCB15706A1ED011C47433BC6F6BDBF99A634F2E83F586`. Only the active
  `arm64-v8a` RelWithDebInfo CMake hash remains. No ADB command, install, launch, or Thor execution
  was performed.
- This proves a major reduction in I4/A4 conversion work, not a whole-game FPS or wattage result.
  A future allowed matched A/B should target I4/A4-heavy texture uploads and compare upload time,
  frametimes, battery power, temperature, and visual correctness.

## 2026-08-16 AArch64 Linear RGBA8/RGB8 Conversion

- The tiled texture paths had gained explicit NEON coverage, but converted linear surfaces still
  used the old per-pixel codec. Final ARM64 ThinLTO output showed RGBA8 uploads doing one scalar
  load, byte reverse, and store per pixel. RGBA8 downloads were auto-vectorized into twelve shifts,
  four table lookups, and an interleaved `ST4` per 16 pixels. Both RGB8 directions remained scalar.
- Converted linear RGBA8 now handles 16 pixels with two paired 128-bit loads, four `REV32`
  operations, and two paired stores. RGB8 uses a compile-time-verified four-register `TBL` mapping:
  uploads turn 48 packed BGR bytes into 64 RGBA bytes with opaque alpha, while downloads remove
  alpha and restore packed BGR order. Buffers shorter than 16 pixels and final partial blocks retain
  the scalar oracle path.
- In the steady 16-pixel loop bodies visible in final ThinLTO code, RGBA8 upload fell from about 96
  instructions to 14 (85.4%), and RGBA8 download from 21 to 14 (33.3%) while eliminating `ST4`.
  RGB8 upload fell from about 208 instructions to 13 (93.75%), and RGB8 download from about 208 to
  12 (94.2%). The RGBA8 download wrapper also shrank from 372 to 148 bytes. Across all four wrappers
  the explicit vector loops add only 124 bytes because the other three now carry both a vector loop
  and scalar tail.
- An independent shuffle model checked 100,000 arbitrary 16-pixel RGB8 and RGBA8 blocks: 1.6
  million pixels per direction matched exact scalar decode, alpha insertion, byte reversal, and
  round-trip packing. Permanent Catch2 tests use 37 pixels to cover two vector blocks, a five-pixel
  scalar tail, and deliberately incomplete final source/destination bytes. The ARM64 tests compiled
  and linked but were not executed because the current restriction forbids using the Thor.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` and
  `:app:assembleVanillaRelWithDebInfoLite` both passed. The APK is 28,963,935 bytes with SHA-256
  `EC094D00FAB9D6556F4AE68BA9367A49055A341CC56049EC470107380CD2651C`. Only the active
  `arm64-v8a` RelWithDebInfo CMake hash remains. No ADB command, install, launch, or Thor execution
  was performed.
- This proves a large CPU instruction reduction whenever a game uploads or reads back converted
  linear RGBA8/RGB8 surfaces. It is not yet a whole-game FPS or wattage result; a future allowed
  matched A/B should target linear-surface-heavy scenes and record conversion time, frametimes,
  battery power, temperature, and visual correctness.

## 2026-08-16 AArch64 Shader JIT Entry/Exit Traffic

- The PICA AArch64 shader JIT previously saved all twelve ABI callee-saved GPRs and all eight
  callee-saved vector registers on every shader invocation. The emitted shader only assigned two
  vector registers from that set (`Q14` and `Q15`) plus the link register. Including stack
  allocation and the unconditional dummy return slot, the fixed entry/exit path emitted 26
  instructions, performed 20 register memory operations, moved 448 register bytes, and reserved
  256 stack bytes even for a leaf shader.
- The constant and final vector scratch register now use free caller-saved `Q5` and `Q6`. The
  persistent-register mask automatically saves the full `Q5` around the rare external geometry
  callback, which also avoids relying on AAPCS64's guarantee for only the low 64 bits of
  callee-saved vector registers. A complete symbolic-register audit found no remaining generated
  use of `X19`-`X29` or `Q8`-`Q15`.
- Shader bytecode is scanned before emission. Ordinary leaf shaders now have no entry/exit stack
  frame at all: the fixed 26 instructions, 20 memory operations, 448 register bytes, and 256-byte
  frame all fall to zero. A shader containing `EX2` or `LG2` preserves only `X30` once, producing
  four fixed entry/exit instructions and 16 bytes of register traffic. With one math-helper call,
  removing its old local `X30` spill reduces the relevant overhead from 28 instructions to four
  (85.7%).
- Shaders containing PICA `CALL`, `CALLC`, or `CALLU` preserve `X30` and retain the 16-byte dummy
  return frame. Their fixed entry/exit/dummy sequence falls from 26 to eight instructions while
  keeping the old per-math-helper link-register spill, because a helper may execute inside a guest
  subroutine. External `EMIT`/error callbacks retain their existing caller-save wrapper.
- The existing Catch2 shader cases cover direct `LG2`, direct `EX2`, and a guest `CALL` whose
  subroutine executes `EX2`, so both link-register strategies compiled and linked into the ARM64
  test executable. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` and
  `:app:assembleVanillaRelWithDebInfoLite` passed. The APK is 28,964,819 bytes with SHA-256
  `FD5A41A44EE6C7796FCAB0CB448FD222794D20BEB84B913EFCFA0998E4A91DFE`; it contains only
  `arm64-v8a` native libraries. Only the active `6t472v1d` RelWithDebInfo CMake hash remains.
- No ADB command, install, app launch, or Thor execution was performed. The emitted-code reduction
  is exact, but whole-game FPS, battery power, temperature, and sustained wattage remain unmeasured
  until a future allowed matched scene A/B.

## 2026-08-16 AArch64 Shader JIT State Traffic

- Even after removing the oversized ABI frame, every compiled shader invocation still loaded all
  three PICA address/loop registers and both condition flags, then wrote all five values back at
  every `END`. That was four load instructions plus four stores and 28 bytes of state traffic even
  when the shader never referenced or modified any of that state. The uniform-base move and
  all-ones vector initialization were also unconditional.
- The existing whole-program control-flow scan now records exact conservative access sets before
  emitting code. Relative float-uniform operands mark only their selected `a0.x`, `a0.y`, or `aL`
  register; enabled `MOVA` lanes mark their corresponding writes; `LOOP` marks `aL`; conditional
  flow marks only the condition lanes selected by `JustX`, `JustY`, `Or`, or `And`; and `CMP` marks
  both condition writes. Float-uniform operands and uniform flow mark the uniform base, while only
  relative-address fallback, DPH/SGE/SLT/RCP/RSQ, and LG2 mark the `ONE` constant.
- Any register that might be written is preloaded as well as written back. This intentionally keeps
  the original value if runtime control flow skips the write or execution enters at a later shader
  offset. Unreferenced state stays in `ShaderUnit` memory and is never transferred. The additional
  analysis runs once when compiling a shader, while the removed instructions ran for every vertex
  or geometry shader invocation.
- A simple uniform-free `MOV` shader now removes ten emitted instructions per invocation: all eight
  state memory operations, the dead uniform-base move, and the unused `ONE` initialization. Its 28
  bytes of PICA state traffic fall to zero. A read-only `a0.x` shader emits one state load instead of
  eight transfers (87.5% fewer); an x-only `MOVA` emits one load and one store (75% fewer); an aL
  loop emits one load and one store; and a condition-only `JustX` path emits one byte load and no
  condition store. Shaders using every state category conservatively retain the old traffic.
- The same access map narrows caller-save wrappers around geometry `EMIT` and error callbacks. A
  shader with no address, condition, loop, uniform, or `ONE` dependency now preserves only `STATE`
  and `X30`: wrapper memory operations fall from ten to two (80%), register traffic from 144 to 32
  bytes (77.8%), emitted save/restore instructions from twelve to four (66.7%), and stack use from
  80 to 16 bytes. State-heavy geometry shaders keep every register they can actually need.
- Focused Catch2 coverage now checks unused-state preservation, partial `MOVA` preservation,
  initial-state relative uniform reads, and `CMP` writeback. Existing conditional-flow and nested
  loop cases cover condition reads and aL persistence. The complete ARM64 test executable compiled
  and linked, but was not run under the current no-Thor restriction.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` and
  `:app:assembleVanillaRelWithDebInfoLite` passed. The arm64-only APK is 28,964,975 bytes with
  SHA-256 `5341AA99FBFABF37F301FA7651F529F06B32BB295325BE029F0A73A8A5E3A0FE`.
  Only the active `6t472v1d` RelWithDebInfo CMake hash remains. No ADB command, install, launch, or
  Thor execution was performed, so whole-game FPS and wattage remain unmeasured.

## 2026-08-16 Vulkan Anime4K Repair

- The old Vulkan path did not implement Anime4K. It bound one surface as all three shader inputs and ran only the final refine shader while rendering back into that same image. That omitted both gradient passes and created an invalid sample/render feedback dependency.
- Vulkan now copies the requested unscaled source rectangle to an independent image, renders the X gradient to RG16F at 2x, renders the Y/luma gradient to R16F at 2x, and uses those results for the final refine pass into the scaled surface. The passes use independent framebuffers, cached format-compatible pipelines, clamp-to-edge samplers, and explicit `GENERAL`-layout dependencies between transfer writes, color writes, fragment reads, and the final surface use.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` and `:app:assembleVanillaRelWithDebInfoLite` both passed. The resulting APK installed on the AYN Thor and loaded 7TH DRAGON III CODE: VFD at 3x resolution with Vulkan, Turnip Adreno 740, and Anime4K explicitly reported in the native configuration log.
- A fixed title-screen capture rendered at 30 FPS with no fatal, pipeline-creation, Vulkan, or shader error. Its expected Anime4K edge refinement matched a control capture from the existing OpenGL Anime4K path; screenshots and device logs were test artifacts and are not committed.
- This restores correctness, not efficiency. Anime4K performs three full-screen texture passes plus a source copy for each filtered upload and keeps intermediate images by source size/format for safe reuse across queued Vulkan work. It is labeled very heavy in the Android UI. None or Bicubic remains the better Thor choice when lower GPU load, memory use, and battery power matter more than aggressive anime-line refinement.

## 2026-08-16 Anime4K v4 Mobile Screen Filter

- Android Graphics settings now has a separate **Screen Filter** selector beside Linear Filtering.
  It is distinct from **Texture Filter**: the old filter changes individual emulated textures before
  games use them, while the new mode filters each finished 3DS screen as it is scaled into the
  final layout. The default is None, so existing output and GPU cost do not change unless selected.
- **Anime4K v4 Mobile** is derived from the MIT-licensed non-CNN DoG upscaler shipped in the
  official [Anime4K v4.0.1 release](https://github.com/bloc97/Anime4K/tree/v4.0.1/glsl/Upscale).
  The desktop-oriented implementation uses luma, horizontal Gaussian, vertical Gaussian, and
  apply passes. This Thor port fuses the DoG idea into one presentation pass with a normalized 3x3
  Gaussian, the same 0.8 luma correction strength, and a local luma min/max clamp. It samples nine
  source texels per output fragment but creates no intermediate image and performs no extra
  full-frame read/write pass, reducing mobile tile-memory bandwidth at the cost of not being a
  pixel-identical port of the full separable filter or its multi-pass CNN alternatives.
- The shader activates only when both output axes exceed the input by 1.2x, matching Anime4K's
  upscale-only intent. It preserves source alpha, clamps corrected RGB, forces the required linear
  sampler only while the screen filter is active, and leaves normal nearest/linear presentation
  alone at native size or while downscaling. Anaglyph and interlaced 3D retain their existing
  two-eye shaders and the user's ordinary presentation sampler.
- Vulkan has a dedicated fourth presentation pipeline and supports both dynamic descriptor-array
  indexing and the existing switch fallback. OpenGL has a matching built-in presentation shader.
  The setting is persisted through Android and desktop configuration and is logged by name.
- NDK `glslc` accepted both Vulkan indexing variants and the OpenGL fragment shader. The complete
  ARM64 `:app:assembleVanillaRelWithDebInfoLite` build passed; the APK is 28,961,939 bytes with
  SHA-256 `4C7979B6A8AB5A6A725AF9EE07536A7D5172BB809286112E093F51F8EA58E543`.
- Per the active no-launch restriction, the APK was not installed or run. Visual quality, Adreno
  frametime, and power remain unmeasured. The required A/B is None versus Anime4K v4 Mobile in the
  same anime-heavy title and fixed scene, with identical renderer, internal resolution, layout,
  driver, brightness, fan/performance mode, cache state, and duration; record frame distribution,
  KGSL busy time, battery power, temperature, and thermal slope before changing recommendations.

## 2026-08-16 Dynarmic A32 ARM64 FastDispatch

- The ARM64 A32 backend previously sent every `FastDispatchHint` terminal back through
  the C++ dispatcher even though the x64 backend had a native two-tier dispatch cache.
  ARM64 now checks a 65,536-entry direct-mapped table in generated AArch64 code and calls
  `GetOrEmit()` only on a descriptor miss. Return-stack-buffer misses use the same fast
  path when the optimization is enabled.
- The descriptor is loaded directly from the adjacent A32 PC and upper-descriptor state,
  and the emitted hash exactly matches the C++ invalidation hash. Range invalidation
  clears a matching entry; full cache clears discard the table and block-range map at the
  same explicit virtual cache-clear boundary. Single-step mode retains the slow dispatcher.
- Two focused cache tests warm a FastDispatch entry, mutate guest code, and verify that
  range invalidation and full-cache clearing cannot execute stale host code. The release
  ARM64 test binary passed all 11 assertions on the AYN Thor.
- A hidden benchmark alternates between two dynamically selected ARM blocks so constant
  propagation and ordinary block linking cannot bypass dispatch. Every other Dynarmic
  optimization is identical between the baseline and candidate. Each long sample performs
  one million indirect dispatches and the process is pinned to Thor CPU7.
- The reverse-order long run measured the C++ dispatcher at 4.014-4.016 ms per million
  dispatches and stable ARM64 FastDispatch at 2.128 ms: 1.89x dispatch throughput and
  47.0% less time in this dispatcher-saturated workload. Across shorter pinned/unpinned
  runs and both orderings, measured throughput ranged from 1.69x to 1.95x.
- This is not a claim of 1.89x game FPS. Game impact scales with the share of CPU time spent
  on unpredictable block dispatch; GPU-bound scenes may show no FPS change. The benchmark
  was too short for a useful battery-power comparison, so lower watts remains unproven until
  matched sustained game-scene A/B runs.
- Dynarmic and its recursive build dependencies are now vendored in this repository from
  upstream commit `e77b1ba0b7da7cbe93021b01a663acfe7c4dd516`; provenance and update rules
  are in [`research/dynarmic-vendor.md`](research/dynarmic-vendor.md).
- The final `:app:assembleVanillaRelWithDebInfoLite` build passed. The APK is 28,953,903
  bytes with SHA-256 `F85766DB96E8F820BF5C6FE945714CFA111A36082CE4B2A9028B5C41D6AD2B89`.
  It installed on the Thor and booted 7TH DRAGON III CODE: VFD through the vendored JIT
  with Vulkan, Turnip Adreno 740, and Anime4K; the launch log had no fatal signal,
  exception, or native abort.

## 2026-08-16 Android Eco Turbo

- Android fast-forward previously allowed every emulated VBlank to prepare and submit a host
  presentation. On the Thor's 120 Hz primary panel, high turbo limits can therefore run layout,
  composition, and presentation work well above the normal 3DS refresh rate even though the user
  is primarily asking the game to advance faster.
- The new General setting **Eco Turbo** defaults on. Whenever the active frame limit is above 100%,
  it caps only host presentation/composition to 60 FPS. Guest CPU execution, PICA work, audio,
  timing, and the requested turbo limit continue unchanged. Both Vulkan and OpenGL skip the
  surplus host frames; screenshots and video dumping still prepare their required framebuffers.
- The limiter uses elapsed wall time and a one-frame token budget rather than dividing by the
  requested turbo percentage. This matters when a heavy scene requests 400% but achieves less than
  100%: frames at or below 60 FPS are still all presented. Returning to normal speed or disabling
  Eco Turbo resets the budget immediately.
- The final release APK was tested on 7TH DRAGON III CODE: VFD (`000400000018F800`) at 3x,
  Vulkan, Turnip Mesa 25.99.99, Anime4K, duplicate-frame skipping enabled, and a temporary 400%
  frame limit. The title screen sustained 120 game FPS and 399-403% speed in the overlay with Eco
  both off and on.
- In the reversed-order final-code 20-second A/B, Eco off measured 32.45% KGSL GPU busy and 444
  process CPU ticks. Eco on measured 26.81% GPU busy and 409 ticks: **17.37% less GPU active time**
  and **7.9% less process CPU time** while emulation speed was retained. Both runs held the same
  615 MHz GPU frequency and 24.0 C battery temperature.
- This is not a 17.37% battery-watt claim. The device reported USB power and active charging during
  the run, so battery-current telemetry could not isolate emulator power. A long, unplugged,
  fixed-brightness/fan/performance-mode A/B is still required for watts and thermal slope.
- Benefit depends on workload. At 200% this title submits 60 game frames per second, so the existing
  duplicate-frame setting already removes surplus presentation and Eco Turbo has little extra work
  to skip. The win is larger when a title/turbo combination produces more than 60 unique frames per
  second. Disabling Eco Turbo remains available for maximum fast-forward smoothness on 120 Hz.
- `:app:assembleVanillaRelWithDebInfoLite` passed, producing a 28,957,711-byte APK with SHA-256
  `67CE6DB9E4D153899B84C54249C76E8FB009D2840FE3D4BEB849C9CD8338FF53`.
  It installed on the Thor, restored the user's original config after testing, and booted the same
  game at the original 100% limit with Eco Turbo defaulting on and no fatal exception or signal.

## 2026-08-16 Rejected Dynarmic A32 ARM64 Absolute-Offset Page Table

- This experiment changed every AArch64 page-table entry from a real host page-base pointer to
  `host_page_pointer - guest_page_base`, allowing Dynarmic to add the full guest address and omit
  one page-offset mask instruction. Its arithmetic-only unit test and ARM64 package build passed,
  but the then-active no-launch restriction meant the JIT path was not executed on the Thor.
- Runtime testing on 2026-08-19 proved that evidence insufficient. Art Academy
  (`0004000000095800`) and 7th Dragon III Code: VFD (`000400000018F800`) both aborted about
  1.5-3 seconds after launch in
  `Dynarmic::Backend::Arm64::AddressSpace::FastmemCallback`. The callback rejected the faulting JIT
  instruction because it was an ordinary inline page-table access, not a registered fastmem patch
  site. A clean build before the procedural presentation-quad change reproduced the same fault,
  excluding that Vulkan change.
- Disabling only `absolute_offset_page_table` and restoring real page-base entries made both games
  remain alive past 15 seconds and render their title screens. The clean fix removes the encoding,
  decoding, architecture branch, and absolute-offset configuration instead of leaving a dormant
  option. The raw page table, normal C++ accessor, unmapping, and savestate reconstruction again
  share the same real pointer representation. The pointer-consistency unit test now asserts that
  invariant directly on every host architecture.
- The corrected profiling-off ARM64 package build passed in 4 minutes 27 seconds. A second Thor
  smoke run of the clean implementation kept 7th Dragon and Art Academy alive past 12 seconds with
  no native fatal signal. Source commit `5a4f33ee1` was pushed directly to `origin/master` over SSH.
- This entry is withdrawn from the active optimization count. Saving one generated instruction is
  irrelevant when the representation can crash normal games, and an algebra-only test is not an
  adequate acceptance gate for a JIT memory-path representation change.

## 2026-08-16 Dynarmic A32 ARM64 NZCV Register Cache

- Dynarmic's A32 ARM64 backend kept the guest ARM11 N/Z/C/V flags in
  `A32JitState::cpsr_nzcv`. A flag-producing block wrote that word to memory, while the
  next conditional block, carry consumer, or conditional select loaded it again. This
  made the architectural-state structure part of ordinary linked-block execution even
  though AArch64 has enough callee-saved registers to keep the four bits live.
- A32 now reserves callee-saved `W23` for the packed guest NZCV value. The run and step
  preludes load it once, linked blocks consume and replace it directly, and the common
  exit stores it once. The A64 frontend keeps its original memory representation and its
  complete 21-register allocator order; only A32 trades one allocator register for the
  persistent flag cache.
- Generated callback relocations and generic host-function calls store `W23` before the
  host call and reload it afterward. This preserves the old observable behavior for
  SVC, exception, coprocessor, slow-memory, timer, and hook callbacks: a callback sees
  current guest flags and may update them before guest execution resumes. `X23` is both
  compile-time-checked as AAPCS64 callee-saved and excluded from the A32 allocator.
- Exact emitted-sequence accounting for a `SUBS`/conditional-branch loop changes the
  NZCV path from `STR + LDR + MSR + B` to `MOV + MSR + B`: four instructions to three
  (25% fewer) and eight bytes of per-iteration flag-state traffic to zero. An NZ-only
  update that preserves C/V followed by a condition falls from seven instructions and
  three state-memory operations to four instructions and no state-memory operations
  (42.9% fewer instructions). A carry read falls from two instructions to one, and a
  conditional select falls from three instructions to two.
- The entry/exit cost is one four-byte load and one four-byte store per `Run()` or
  `Step()`, rather than per guest block. Host callbacks deliberately add a store/load
  pair for coherence. The new hidden `SUBS`/`BNE` benchmark makes the register-pressure
  tradeoff and the removed cross-block traffic repeatable against the parent revision;
  it must be measured before deciding whether the reserved register is a net win in
  real game code.
- The focused regression compiles a sequence that sets Z/C, enters SVC, verifies the
  callback sees those flags, replaces them with N, and then verifies subsequent MI/EQ
  guest conditions use the callback's replacement. The complete ARM64 Dynarmic test
  executable and `libcitra-android.so` compiled and linked. The release-style
  `:app:assembleVanillaRelWithDebInfoLite` build also passed; its 28,965,311-byte APK
  contains only `arm64-v8a` native libraries and has SHA-256
  `09F52B9EC343F62F9E8B3E0EB04402C3537741E73335E7117285D58848F13728`. Per the active
  no-device restriction, neither executable was run on the Thor.
- This is a broad generated-code and memory-traffic reduction, not yet an emulator FPS
  or battery-watt result. A future allowed A/B should compare the parent and candidate
  revisions with the focused benchmark plus identical game scenes, and must capture
  frametimes, process CPU time, battery power, temperature, thermal slope, and visual
  correctness.

## 2026-08-16 Dynarmic A32 ARM64 Conditions and Cycle Checks

- A cycle-counted linked A32 block previously ended with `SUB Xticks`, `CMP Xticks, #0`,
  and `B.LE`. The subtraction now sets host flags with `SUBS`, and an A32 `LinkBlock`
  reuses those flags when the cycle subtraction produced them. The ordinary linked-block
  budget check therefore falls from three instructions to two (33.3% fewer). Zero-cycle,
  cycle-counting-disabled, single-step, and non-link paths keep their existing safe behavior.
- A32 guest conditions no longer copy packed guest flags into the host NZCV system register.
  EQ/NE, CS/CC, MI/PL, and VS/VC become one exact `TBZ`/`TBNZ` instead of `MSR` plus a
  conditional branch: two instructions to one (50% fewer) and no system-register write.
  HI/LS and GE/LT remain two instructions but avoid `MSR`; GT/LE need three flag-preserving
  instructions, yet do not add instructions to the combined condition-plus-cycle terminal
  because the separate cycle `CMP` is gone.
- Combining the two changes takes a common simple conditional linked-block path from five
  ARM64 control/cycle instructions (`MSR + B.cond + SUB + CMP + B.LE`) to three
  (`TBZ/TBNZ + SUBS + B.LE`), a 40% static reduction. The condition sequences use only
  flag-preserving bit-test branches and non-flag-setting `EOR`, so the signed `Xticks <= 0`
  result remains valid until the link decision.
- A focused regression covers all 16 N/Z/C/V combinations against all 14 meaningful ARM
  conditions and places an unconditional guard immediately after the exact cycle boundary.
  The source passes an Android ARM64 Clang syntax check. The standalone Dynarmic test runner
  was not executed because the current restriction forbids using the Thor; an auxiliary CMake
  attempt to enable that runner in the Android app tree was stopped after its cross-test
  configuration hung, and `DYNARMIC_TESTS` was restored to `OFF`.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` compiled and linked the modified ARM64 backend and
  `libcitra-android.so` successfully in 1m13s. After the two current Azahar master updates were
  integrated, `:app:assembleVanillaRelWithDebInfoLite` also passed in 3m28s. The resulting
  28,965,611-byte APK contains only `arm64-v8a` libraries and has SHA-256
  `BF5E20ABCEE5653CEE82289AE36B97FC1886A08B3338AF3625E4B9C0A92AA124`. No ADB command,
  install, launch, or device test was performed.
- These are pervasive generated-instruction reductions, but they are not additive with the
  earlier FastDispatch, page-table, or NZCV-cache percentages. Whole-game FPS, battery watts,
  and thermal-slope effects remain unmeasured until a controlled parent-versus-candidate Thor
  A/B is allowed.

## 2026-08-16 AArch64 PICA Shader Swizzles

- The PICA AArch64 vertex-shader JIT's arbitrary source-swizzle fallback previously copied the
  full vector to a scratch register and then inserted each changed 32-bit lane separately. That
  executed two instructions for a one-lane change and three to five instructions when two to four
  lanes changed.
- Identity and four broadcast selectors retain their existing zero- and one-instruction fast
  paths. A selector with exactly one changed lane now emits one direct lane move, a 50% reduction
  from two instructions. Selectors changing two or more lanes load a 16-byte byte-index literal
  and execute one baseline AdvSIMD `TBL`, reducing the old three-to-five-instruction sequence to
  two instructions (33.3%-60% fewer executed shuffle instructions).
- The byte-index literal is aligned and deduplicated by raw selector within each compiled shader.
  Sparse label bookkeeping is cleared after literal emission instead of remaining in every cached
  shader object. Compile-time assertions verify the identity and reverse mappings. The focused
  regression builds a MOV shader for every one of the 256 selector encodings and checks all four
  result lanes, for 1,024 lane assertions.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` completed from a full native rebuild in 10m37s. After
  replacing the dense persistent label array with temporary sparse bookkeeping, an incremental
  1m09s build recompiled the modified AArch64 JIT and exhaustive test source and relinked both the
  ARM64 test runner and `libcitra-android.so`. The test runner was not executed because the active
  restriction forbids using the Thor and the binary cannot run on the x64 host.
- The first release-style APK attempt hit a transient Windows lock on R8's generated `classes.dex`.
  After stopping Gradle daemons, final no-daemon packaging completed in 1m02s. The resulting
  28,966,207-byte APK contains only `arm64-v8a` native libraries and has SHA-256
  `3315A6500AB273EB7EFA744F5809BD40BB20A677FDF56B1C6D1AB88386F18381`.
- This is an exact generated-instruction reduction, not a whole-game FPS or battery-watt claim.
  The two-instruction path trades serial lane operations for a literal load, and each unique
  multi-lane selector adds 16 bytes to the shader code pool. A future allowed matched A/B should
  profile vertex-heavy scenes with identical title, driver, resolution, layout, cache state,
  performance mode, brightness, and duration while recording shader time, frametimes, process CPU
  time, battery power, temperature, thermal slope, and visual correctness.

## 2026-08-16 AArch64 PICA Partial Destination Stores

- The AArch64 PICA vertex-shader JIT previously implemented every partial x/y/z/w destination
  mask as a read-modify-write: load the old 16-byte vector, materialize a mask, select enabled
  source lanes with `BSL`, and store all 16 bytes. Disabled components were preserved correctly,
  but the sequence created a needless load-to-store dependency and touched 32 bytes of explicit
  generated memory traffic for a write that may change only one 4-byte component.
- The replacement groups enabled x/y and z/w pairs and emits baseline AdvSIMD `ST1` element
  stores. Aligned x/y or z/w pairs use one 64-bit lane store; remaining components use 32-bit lane
  stores. Any nonzero partial mask therefore needs at most two stores. The zero hardware mask now
  emits no destination write, and the existing full-mask `STR Q` fast path remains unchanged.
- Correctness follows the actual representations: each `ShaderUnit` destination is an aligned
  `Common::Vec4<f24>`, and `f24` is stored internally as four contiguous IEEE float words. PICA's
  destination bits map x/y/z/w to SIMD lanes 0/1/2/3. Arm Architecture Reference Manual DDI0487
  M.c section C7.2.366 defines `ST1 {Vt.S}[index]` and `ST1 {Vt.D}[index]` as storing exactly the
  selected register element. Output-bank selection and its address calculation are retained.
- This lowering was chosen after checking the Cortex-X3, A715, A710, and A510 optimization guides
  used for the Thor's Snapdragon 8 Gen 2 core mix. All four document the single-lane `ST1` forms;
  the X3 guide additionally notes that stores are buffered while committing in the background.
  The change is baseline Armv8-A/AdvSIMD and does not assume optional SVE.
- Exact generated instruction counts for a temporary partial write fall from four or five to two
  through four. Output-register partial writes, including bank address selection, fall from eight
  or nine to five through seven. Explicit generated memory traffic falls from a 16-byte load plus
  a 16-byte store to 4-12 store-only bytes, a 62.5%-87.5% reduction. These counts do not include
  surrounding shader arithmetic and are not a whole-game performance claim.
- The focused shader regression now preloads a nonzero sentinel destination and checks every one
  of the 14 nonzero partial masks, proving enabled lanes change and disabled lanes survive. A
  manually encoded zero mask verifies that it leaves all four lanes untouched. The existing full
  mask coverage remains. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` compiled and linked the changed
  JIT, this test source, the ARM64 test executable, and `libcitra-android.so` successfully in 4m34s;
  a final incremental verification after adding representation assertions passed in 1m03s. The
  executable was not run because the active restriction forbids using the Thor and it cannot run
  on the x64 host.
- The final incremental `:app:assembleVanillaRelWithDebInfoLite` passed in 27s. The resulting
  28,966,319-byte APK contains only `arm64-v8a` native libraries and has SHA-256
  `F708BEBE442ACBF9BCE1EB990CD1BC6796D9D4ECF80277C4B35A428732453D49`. No ADB command,
  install, launch, or device test was performed.
- This should reduce CPU work and data-cache/store traffic in vertex shaders with partial writes,
  but no FPS or watt result is claimed without a controlled parent-versus-candidate Thor A/B. A
  future allowed test should use the same title and vertex-heavy scene, cache state, driver,
  resolution, layout, performance mode, brightness, fan mode, and duration, recording frametimes,
  process CPU time, battery power, temperature, thermal slope, stability, and visual correctness.

## 2026-08-16 AArch64 PICA Output-Bank Pointer Cache

- Every AArch64 PICA output-register write previously rebuilt the same selected-bank address. A
  full write emitted `ADD` for the fixed `ShaderUnit::output` offset, `LDRB` for `output_bank`, a
  separate `LSL` by the 256-byte bank size, another `ADD`, and finally `STR Q`: five executed
  instructions per write. Partial writes repeated the first four address instructions before their
  lane stores. Temporary-register writes did not have this cost.
- The JIT now reserves caller-saved `X8` for the current output-bank pointer. Shader entry emits
  three instructions once: `LDRB` zero-extends the Boolean bank selector, `ADD (shifted register)`
  folds the bank-size shift into the address addition, and one immediate `ADD` reaches the output
  array. Full output writes then use one `STR Q` with a register-relative immediate. Partial writes
  use one immediate `ADD` before the existing `ST1` lane-store sequence.
- For `N` full output writes, the address/store sequence falls exactly from `5N` instructions to
  `3 + N`, saving `4N - 3`: one instruction for one write, five for two, and thirteen for four.
  The partial-write address portion falls from `4N` to `3 + N`, saving `3N - 3`; the earlier lane
  store reduction remains separate. Geometry `EMIT` deliberately pays the three-instruction setup
  again because it switches banks. These counts exclude shader arithmetic and are not an FPS or
  wattage measurement.
- Correctness follows the real state layout rather than an assumed x86 alias: the two output banks
  are contiguous arrays, `ShaderUnit::OutputBankSize` is statically required to be a power of two,
  and `output_bank` is a Boolean. Arm Architecture Reference Manual DDI0487 M.c section C6.2.6
  defines `ADD (shifted register)`, which directly represents `STATE + bank * 256`. The cached
  caller-saved register is included in the JIT's live-register save set around external calls.
  After the `EMIT` helper toggles `output_bank`, generated code refreshes the pointer before any
  later write. Temporary destinations retain their original `STATE`-relative addressing.
- The destination-mask regression now runs all 14 partial masks against both output banks, so a
  stale or misbased pointer fails while enabled and disabled lanes are checked. A new geometry
  regression writes bank 0, executes a manually encoded `EMIT`, verifies the emitted vertex came
  from bank 0, then verifies the following write lands in bank 1. The same source covers the
  interpreter and JIT implementations.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` rebuilt both changed sources and linked the AArch64
  test executable and `libcitra-android.so` successfully in 1m05s. After merging the two newest
  Azahar upstream commits, the same target passed again in 1m09s. The test executable cannot run
  on the x64 host and the active restriction forbids using the Thor, so this is compile/link plus
  regression-source evidence, not a runtime test. The final
  `:app:assembleVanillaRelWithDebInfoLite` completed in 1m58s. Its 28,966,043-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `3E0783EC7BE887AC38AECBEED34D5851462EDDB2D4CC0327BB7C705F3038616C`. No ADB command, install,
  launch, or device test was performed.
- After verification, the final exact 1,854,228,806-byte reproducible
  `src/android/app/build/intermediates` tree was removed. The Gradle daemon was stopped first; the
  final APK and active `arm64-v8a` RelWithDebInfo CMake cache were retained.
- A future allowed Thor A/B should use the same title and vertex-heavy scene, cache state, driver,
  resolution, layout, performance mode, fan mode, brightness, and duration. Record frametimes,
  process CPU time, battery power, temperature, thermal slope, stability, and visual correctness;
  do not infer a whole-game speed or power result from the exact instruction counts alone.

## 2026-08-16 AArch64 SoundTouch Stereo-Overlap NEON

- Android configures SoundTouch for 16-bit integer samples and defines `SOUNDTOUCH_USE_NEON`, but
  SoundTouch 2.3.3 has no NEON-specific implementation class. Disassembly showed that Clang already
  autovectorizes the expensive WSOLA cross-correlation loops into AArch64 `SMULL`/`SMLAL`, so those
  loops were deliberately left alone. The stereo overlap loop remained scalar and issued two
  `SDIV` instructions per frame, one for each channel.
- SoundTouch's integer path rounds `overlapLength` to a power of two from 16 through 1024 samples.
  The new AArch64-only path processes four stereo frames per loop with two 128-bit loads,
  `SMULL`/`SMULL2`, `SMLAL`/`SMLAL2`, signed variable shifts, narrowing, and one 128-bit store. Final
  object-code inspection contains zero `SDIV` in this function, replacing eight scalar divides for
  the same four frames. This is a scoped generated-code fact, not a whole-emulator speed claim.
- Correctness preserves the scalar expression rather than copying SoundTouch's older MMX rounding.
  For a negative numerator, adding `(overlapLength - 1)` before the arithmetic right shift exactly
  reproduces C++ signed division's truncation toward zero; nonnegative values receive no bias.
  Boundary-heavy verification covered 292,608 numerator/weight combinations at every supported
  power-of-two length. The committed differential test fills both channels with positive, negative,
  and signed-16-bit edge values and compares the production overlap against scalar division at
  16-, 256-, and 1024-frame overlap lengths. Arm's Neon Intrinsics on Android guide documents the
  widening signed multiply-accumulate operation used here.
- `externals/soundtouch` is now vendored from former submodule commit
  `9ef8458d8561d9471dd20e9619e3be4cfe564796`; a custom dependency gitlink would otherwise require a
  separate repository. The upstream LGPL license remains in-tree. The unused Android wrapper JAR
  and three prebuilt example DLL/shared-library files were omitted, avoiding 780,377 bytes of
  irrelevant binary payload while retaining source, build files, and documentation.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` compiled the NEON source and differential test, linked
  `libSoundTouch.a`, the AArch64 test executable, and `libcitra-android.so`, and completed in 1m14s.
  `:app:assembleVanillaRelWithDebInfoLite` then passed in 54s. The resulting 28,965,579-byte APK
  contains only `arm64-v8a` native libraries and has SHA-256
  `226FC37EB4037E42D24AA8A3F6436E8654BC5C85944D9F588F74A61CB62BB5A1`. The test executable cannot
  run on this x64 host and the active restriction forbids Thor use, so runtime regression and power
  measurements remain pending. No ADB command, install, launch, or device access was performed.
- After verification, Gradle was stopped and the exact 1,854,299,737-byte reproducible
  `src/android/app/build/intermediates` tree was removed. The verified APK and active AArch64
  RelWithDebInfo CMake cache were retained.
- A future allowed A/B should use an audio-active scene that holds below full speed so time stretch
  remains engaged, with the same title, save, cache state, renderer, driver, resolution, display
  layout, speed limit, performance/fan mode, brightness, and duration. Record audio glitches,
  output underruns, frametimes, process CPU time, battery power, temperature, and thermal slope.

## 2026-08-16 AArch64 PICA Four-Command Fast Path

- `PicaCore::ProcessCmdList()` identifies itself as Azahar's most CPU-expensive function outside
  draw calls. Final Android ThinLTO disassembly showed that its existing four-command source loop
  contained no SIMD: Clang expanded the partial-batch control flow into repeated scalar header
  loads, bounds checks, LUT branches, stack staging, register updates, and dirty-bit read/modify/
  writes. The baseline function was 1,476 bytes.
- This change was selected from the actual Snapdragon core guides. Cortex-X3 issue 4.0 table 3-19,
  Cortex-A715 issue 5.0 table 3-19, and Cortex-A710 issue 4.0 table 3-35 give Q-form B/H/S `LD2`
  an eight-cycle L1-hit latency and 3/2-instruction-per-cycle throughput. Cortex-A510 issue 6.0
  table 3-35 gives the same form four-cycle latency and one-instruction-per-two-cycle throughput.
  One `LD2` per four interleaved pairs is therefore a bounded use; heavier structure-load patterns
  were not generalized across the parser. The manual PDFs and temporary rendered pages remain
  outside git and were removed after review.
- The AArch64 common path now deinterleaves four `[value, header]` pairs, validates register bounds
  and extra-data bits together, reduces the invalid mask with `UMAXV`, gathers four special-handler
  flags, and branches once. Four consecutive ordinary IDs use one 128-bit register load, byte-mask
  blend, 128-bit store, and one dirty-word update. Nonconsecutive IDs retain ordered scalar writes
  so duplicate IDs still observe preceding writes; when all four dirty bits share a word they are
  merged into one read/modify/write. Short, invalid, extended, or special batches use a separate
  236-byte scalar loop and then the original slow handler. The call is direct, not through the PLT.
- Correctness is an exact refactoring of the prior conditions. Header bits 0-15 remain the register
  ID, bits 16-19 remain the byte mask, and only bits 20-27 reject the ordinary path; reserved high
  bits and the group bit retain their prior treatment when extra length is zero. The fast path makes
  no state change before all four headers and special-handler flags pass. It adds exactly four delay
  commands, advances exactly eight words, preserves byte-select semantics, and reproduces the same
  dirty set. A consecutive four-register group is unique by construction; its rare 64-bit dirty-
  word crossing explicitly updates the next word.
- The committed ARM64 test runs every `16^4` combination of four parameter masks against an
  independent scalar expansion and blend, checking 262,144 lanes. It also checks all 65,536 IDs
  with extra lengths 0, 1, and 255 and both clear/set reserved/group high bits, for 393,216 header
  cases. Matching host-side semantic sweeps passed. The AArch64 test executable compiled and linked
  but was not run because the host is x64 and Thor use remains forbidden.
- Final ThinLTO contains the intended `LD2`, vector comparisons, `UMAXV`, Q register load/store,
  and direct compact fallback. `ProcessCmdList()` is 1,336 bytes, 140 bytes or 9.5% smaller than the
  1,476-byte baseline; the separate scalar fallback is 236 bytes. These are codegen facts, not a
  whole-game speed or power claim. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` compiled the source and
  tests and linked `libcitra-android.so`; `:app:assembleVanillaRelWithDebInfoLite` passed in 24s.
  The 28,966,471-byte APK contains only `arm64-v8a` native libraries and has SHA-256
  `EC03BDBB838F23748E553D0A12C56D5817C496659209BF7B228E2998229EA388`. No ADB, install, launch,
  or device access occurred.
- A future allowed A/B should use a command-heavy, CPU-limited scene with identical title, save,
  caches, renderer, driver, resolution, layout, performance/fan mode, brightness, and duration.
  Record command counts, ordinary-four hit rate, frametimes, process CPU time, battery power,
  temperature, thermal slope, stability, and rendering correctness.

## 2026-08-17 AArch64 PICA EX2 Literal Packing

- The AArch64 PICA vertex-shader JIT's `EX2` approximation previously executed eight independent
  `ADR` plus scalar `LDR` pairs to materialize its input clamps, `0.5`, and five polynomial
  coefficients every time the helper ran. The constants are compile-time literals and already
  have a fixed lifetime, so repeated address generation was unnecessary.
- This change follows the actual Snapdragon CPU-core guides. Cortex-X3 issue 4.0 table 3-6 gives
  `ADR` latency 1 and throughput 4 instructions/cycle; table 3-13 gives Q-form `LDP` latency 6 and
  throughput 3/2. Cortex-A715 issue 5.0 tables 3-6 and 3-13 give `ADR` latency 1/throughput 2 and
  Q-form `LDP` latency 6/throughput 3/2. Cortex-A710 issue 4.0 tables 3-11 and 3-23 give `ADR`
  latency 1/throughput 4 and Q-form `LDP` latency 6/throughput 3/2. Cortex-A510 issue 6.0 tables
  3-10 and 3-23 give `ADR` latency 1/throughput 2 and Q-form `LDP` latency 3/throughput 1. The
  cited instruction tables are on PDF pages 18/23, 20/26, 27/39, and 22/32 respectively. The
  manuals stay in the external research library indexed by `docs/hardware/README.md`; temporary
  rendered review pages are not retained.
- The eight unchanged 32-bit words now occupy one 16-byte-aligned 32-byte block. One `ADR` and one
  Q-form `LDP` place them in two vector registers; one lane `DUP` supplies `0.5`. Polynomial
  coefficients remain in their loaded lanes and are added with `FMLA` whose multiplicand is exact
  `1.0`. Multiplication by `1.0` is exact for these finite coefficients, so each fused operation
  has the same one addition-rounding step as the former `FADD`; the Horner multiplication and
  addition order, clamps, exponent reconstruction, and NaN branch are unchanged.
- Constant setup therefore falls from 16 executed instructions (eight `ADR` plus eight scalar
  `LDR`) to 3 (`ADR`, Q `LDP`, and lane `DUP`), 13 fewer instructions per helper execution. A
  shader that otherwise needs no constant `1.0` adds one `FMOV` at entry, making the net reduction
  12 instructions for one `EX2`; shaders that already need `ONE`, and subsequent `EX2` helpers,
  retain the full 13-instruction reduction. Alignment can affect emitted padding, so this is an
  executed-instruction count rather than an exact code-byte claim.
- Focused Catch2 coverage now adds fractional negative and positive inputs around the polynomial
  range (`-1`, `-0.5`, `0.5`, and `1.5`) to the existing NaN, clamp, zero, integer-power, and
  high-magnitude cases. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1m15s, compiling and
  linking the complete AArch64 test executable and `libcitra-android.so`; the executable was not
  run because the host is x64 and the current instruction forbids using the Thor.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 2m03s. The 28,967,111-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `F43CFD5C41900F957380EE4AA4D65135C848D351792C7D26F071132511BD516F`. No ADB, install,
  launch, or device access occurred. The instruction reduction is not a whole-game speed or
  battery-watt claim.
- A future allowed A/B should use an `EX2`-heavy vertex-shader scene with identical title, save,
  caches, renderer, driver, resolution, display layout, performance/fan mode, brightness, and
  duration. Record helper hit counts, frametimes, process CPU time, battery power, temperature,
  thermal slope, stability, and visual correctness.

## 2026-08-17 AArch64 PICA LG2 Literal Packing

- The normal positive-input PICA `LG2` helper previously materialized its first polynomial
  coefficient with `ADR`, scalar `LDR`, and a general-to-vector lane `MOV`, then issued another
  `ADR` and Q `LDR` for the remaining four coefficients. That is five executed setup instructions
  before the otherwise register-only Horner polynomial.
- The same official Snapdragon core tables used for `EX2` were visually rechecked for this change.
  Cortex-X3 issue 4.0 tables 3-6/3-13, Cortex-A715 issue 5.0 tables 3-6/3-13, Cortex-A710 issue
  4.0 tables 3-11/3-23, and Cortex-A510 issue 6.0 tables 3-10/3-23 cover `ADR` and Q-form `LDP` on
  PDF pages 18/23, 20/26, 27/39, and 22/32. Q `LDP` has latency 6 and throughput 3/2 on X3,
  A715, and A710; A510 gives latency 3 and throughput 1. The external PDFs remain indexed in
  `docs/hardware/README.md`, and temporary rendered pages are removed after review.
- The five unchanged coefficient words (`3d74552f`, `beee7397`, `3fbd96dd`, `c02153f6`, and
  `4038d96c`) now occupy an aligned two-Q-register block. One `ADR` plus one Q `LDP` loads `c0-c3`
  into `SRC2` and `c4` into the low lane of `VSCRATCH2`. Every Horner multiply/add remains in the
  same order. The only operand-order spelling change is finite positive `c0 * mantissa` to
  `mantissa * c0`; an exhaustive sweep of all 8,388,608 normalized float32 mantissas in `[1,2)`
  found zero result-bit mismatches. NaN, zero, negative, and infinity control flow and literal
  vectors were not changed.
- Positive-input coefficient setup falls from five executed instructions to two, removing three
  instructions per `LG2` helper execution with no new shader-entry setup. The aligned block adds
  12 bytes of unused literal padding; alignment can also move following code, so this is an exact
  runtime instruction-count result rather than a generated-code-byte claim.
- Focused Catch2 coverage now adds `0.5`, `1.0`, `1.5`, and `2.0` around both range-reduction
  boundaries to the existing NaN, negative, zero, integer-power, and high-magnitude checks.
  `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1m13s and linked the complete 443,607,952-byte
  ELF64/AArch64 test executable plus `libcitra-android.so`. The executable was not run because the
  host is x64 and the current instruction forbids using the Thor.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1m57s. The 28,966,559-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `7C163D6E16A1BB1EFCF4D8C7ED28FAA2EA4EC38571217967745D1CE77081E055`. No ADB, install,
  launch, or device access occurred. No whole-game speed or battery-power claim is made.
- A future allowed A/B should use an `LG2`-heavy vertex-shader scene with identical title, save,
  caches, renderer, driver, resolution, display layout, performance/fan mode, brightness, and
  duration. Record helper hit counts, frametimes, process CPU time, battery power, temperature,
  thermal slope, stability, and visual correctness.

## 2026-08-17 Vulkan Recycled-Chunk Wakeup Fix

- `Scheduler::CommandChunk` maintained both a linked-list head and a `recorded_counts` field.
  `Record()` incremented the counter before checking storage capacity, while `ExecuteAll()`
  destroyed every command and reset `submit`, `command_offset`, `first`, and `last` but never reset
  the counter. Once any recycled chunk had carried work, its `Empty()` result therefore remained
  false forever even when its actual command list was empty.
- This intersects the normal frame path. `RendererVulkan::RenderToWindow()` records screen work and
  calls `scheduler.Flush()`, which dispatches the submit chunk and acquires a new or recycled chunk.
  `RasterizerVulkan::TickFrame()` then calls `WaitWorker()`, which first calls `DispatchWork()`.
  With a stale recycled counter, that call could invoke the descriptor dispatch callback, queue an
  empty chunk, notify and wake the worker, acquire the queue/execution/reserve locks, execute zero
  commands, and recycle the chunk again. The bug could therefore add up to one empty worker job to
  a frame after chunk reuse; exact frequency still depends on scheduling and reserve timing.
- `Empty()` now checks `first == nullptr`, and the redundant counter and per-command increment are
  removed. `first` becomes non-null only after a command passes the capacity check and is placed;
  it stays non-null while any linked command is pending and is cleared only after `ExecuteAll()`
  executes and destroys the entire list. A full chunk remains dispatchable, a submit chunk always
  contains its recorded submission callback before `MarkSubmit()`, and a truly empty recycled
  chunk is skipped. No GPU command, submission, fence, semaphore, descriptor update needed by
  recorded work, or callback ordering is removed.
- The shared condition variable still uses `notify_all`: a dispatch must wake the worker even if a
  simultaneous queue-drain waiter also sleeps on that variable. Queue-before-execution lock order,
  command-buffer allocation, submit serialization, and reserve ownership are unchanged. This keeps
  the fix scoped to stale empty-state accounting rather than attempting a risky scheduler rewrite.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1m18s, rebuilding the scheduler and all Vulkan
  consumers and linking the complete 443,612,688-byte ELF64/AArch64 test executable plus
  `libcitra-android.so`. `:app:assembleVanillaRelWithDebInfoLite` passed in 2m00s. The 28,966,427-byte
  APK contains only `arm64-v8a` native libraries and has SHA-256
  `056AABB1F4348A5C55E01343E0692EA85932D1F07D61EAE83E350041D7B04D53`.
- No ADB, install, launch, or device access occurred. This proves removal of the stale empty-job
  route in source and validates the ARM64 build, but it does not quantify a watt or FPS result. A
  future allowed A/B should instrument dispatched chunks and executed command counts in the same
  Vulkan title/scene, caches, driver, resolution, display layout, performance/fan mode, brightness,
  and duration, then record empty-dispatch count, worker wakeups, process CPU time, frametimes,
  battery power, temperature, thermal slope, stability, and rendering correctness.

## 2026-08-17 AArch64 PICA Eight-Word Range Scan

- PICA command processing sends contiguous shader program-code and swizzle writes through
  `UpdateProgramCodeRange()` and `UpdateSwizzleDataRange()`. Their AArch64 path compared four words
  at a time, then used `UMAXV` first to decide whether any lane changed and again to locate the
  highest changed lane. Re-uploaded shader data therefore paid one horizontal reduction and the
  complete loop bookkeeping for every four unchanged words.
- This change is based on the official guides for every CPU type in Thor's Snapdragon 8 Gen 2,
  not an x86 analogy. For 4H/4S max/min reductions including `UMAXV`, Cortex-X3 issue 4.0 PDF page
  26 reports latency 2 and throughput 2 instructions/cycle; Cortex-A715 issue 5.0 page 29 reports
  latency 3 and throughput 1; Cortex-A710 issue 4.0 page 43 reports latency 2 and throughput 2;
  and Cortex-A510 issue 6.0 page 36 reports latency 4 and throughput 1. The A510 dependency is the
  strongest reason not to repeat the reduction unnecessarily. The external manuals remain indexed
  by hash in `docs/hardware/README.md`; no PDF or rendered review page is committed.
- A new baseline-Armv8-A block loads two old and two new Q vectors, compares both, ORs their change
  masks, and performs one `UMAXV` for the common all-equal path. It stores both vectors only after
  detecting a difference. High-half index constants are 4-7, so a zero high reduction
  unambiguously selects the low half; the earlier combined reduction proves that low lane zero is a
  valid changed result rather than an all-equal sentinel. The existing four-word NEON loop and
  scalar remainder handle lengths below eight and every tail. SSE and non-NEON behavior are
  unchanged.
- Final ThinLTO emits `LDP` for the adjacent old vectors, two Q `LDR` instructions for new data,
  two vector compares, `MVN`/`ORN`, and one `UMAXV` on the unchanged path; changed data uses `STP`.
  For eight unchanged words, the complete vector-loop body and its result bookkeeping fall from 42
  executed instructions across two old four-word iterations to 25 in one eight-word iteration: 17
  fewer, or 40.5% for that local loop case. Program and swizzle functions each grow by 156 bytes to
  retain optimized four-word and scalar tails (292 to 448 bytes and 296 to 452 bytes respectively),
  a 312-byte total code-size tradeoff. These are local machine-code facts, not whole-game FPS or
  battery-watt estimates.
- New Catch2 differential coverage compares range writes with scalar public-API writes for program
  and swizzle storage, offsets 0 and 3, every count from 0 through 24, unchanged/all/alternating
  masks, and every individual changed lane. It also replays an unchanged range after hash
  calculation and then changes the first word, comparing arrays, largest-used sizes, and hashes.
  A separate exhaustive semantic check passed all 256 eight-lane masks. The Android ARM64 test
  executable compiled and linked as a 443,683,104-byte ELF64/AArch64 PIE but was not run because the
  host is x64 and current instructions forbid using the Thor.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1m00s, and
  `:app:assembleVanillaRelWithDebInfoLite` passed in 1m56s. The 28,966,475-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `11535A31D050274F48E4E16E72D0E27F94E659280236F25D9C18663D2839F2DD`. No ADB, install, launch,
  or device access occurred.
- A future allowed A/B should use a shader-command-heavy scene with identical title, save, caches,
  renderer, driver, resolution, display layout, performance/fan mode, brightness, and duration.
  Instrument range-call count and size plus changed/unchanged block rates, then record frametimes,
  process CPU time, battery power, temperature, thermal slope, stability, and visual correctness.

## 2026-08-17 Vulkan Timeline-Poll Cadence

- Every `Scheduler::SubmitExecution()` previously called `MasterSemaphoreTimeline::Refresh()`,
  which enters the Vulkan driver through `vkGetSemaphoreCounterValueKHR`. The call occurred after
  recording the submission command but before dispatching that command to the worker, so it could
  only observe completion of older submissions. At one submission per rendered frame, this was a
  routine host/driver crossing every frame even when no resource or CPU wait needed fresh progress.
- Khronos documents timeline counters as monotonically increasing and explicitly warns that
  [`vkGetSemaphoreCounterValue`](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-semaphores-signaling)
  may be immediately out of date while queue work is pending. Azahar already treats `gpu_tick` as
  a conservative completion cache: `Refresh()` only advances it, `ResourcePool::CommitResource()`
  refreshes immediately if its stale value cannot free an entry, and `Wait()` refreshes before a
  blocking semaphore wait and again afterward. A stale-low value cannot authorize premature reuse.
- Routine submit polling now calls `RefreshOnSubmit()` and queries only for signal ticks divisible
  by four, matching the four-entry command-buffer pool. This leaves three intermediate submissions
  without a routine query; it does not assume that any query observes all older pending work.
  Resource-pool wrap still refreshes on demand; explicit waits are unchanged; rasterizer garbage
  collection may retain sentenced surfaces until a later periodic or on-demand refresh instead of
  destroying them before confirmed completion. The fence fallback's `Refresh()` is already a no-op.
- Final ThinLTO emits `TST signal_tick, #3` plus a conditional branch around the virtual refresh.
  Exactly three of every four scheduled submit polls are skipped, reducing that routine source of
  timeline-counter driver calls by 75% (for example, 60 scheduled calls/second become 15 at 60
  submissions/second). Actual total queries can be higher when waits or resource pressure demand
  fresh state, so this is not a whole-renderer CPU, FPS, or wattage percentage.
- New Catch2 coverage drives twelve sequential signal ticks through a fake master semaphore and
  proves refresh counts of 0, 0, 0, 1 through each four-tick group. A fake four-entry resource pool
  then keeps cached progress stale, fills every entry, wraps, refreshes immediately, and reuses only
  an entry whose recorded tick is confirmed complete. The 443,799,000-byte test executable compiled
  and linked as ELF64/AArch64 but was not run because the host is x64 and current instructions
  forbid using the Thor.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1m13s after the test dependency fix, and
  `:app:assembleVanillaRelWithDebInfoLite` passed in 1m49s. The 28,966,695-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `4BC405EF1EB848E1E3841DB951B3F287A17264BBBB4DD43D2FEB7193AEB5E984`. No ADB, install, launch,
  or device access occurred.
- A future allowed A/B should instrument scheduled and on-demand refresh counts in an identical
  Vulkan title/scene, save, caches, driver, resolution, layout, performance/fan mode, brightness,
  and duration. Record driver-call counts, garbage-collection backlog, command-pool growth,
  frametimes, process CPU time, battery power, temperature, thermal slope, stability, and rendering
  correctness. Revert or shorten the cadence if pool growth or retained-surface memory increases
  materially.

## 2026-08-17 ARM64 Vulkan Timeline Atomic Ordering

- A complete use-site audit found that `current_tick` allocates unique numerical submission IDs and
  `gpu_tick` caches only the highest completion ID observed from a Vulkan timeline semaphore or a
  completed fence. Neither atomic publishes command memory, queue entries, resource contents, or
  ownership. Queue work is published under the scheduler mutex/condition variable; the fence path
  waits for Vulkan completion and transfers its fence under `free_mutex`; Vulkan itself orders the
  submitted GPU work.
- Arm Architecture Reference Manual DDI 0487 M.c section C6.2.180 (PDF page 2214) states that
  `LDADDL` stores with release semantics while plain `LDADD` has neither acquire nor release
  semantics. Section C6.2.192 (PDF page 2240) defines `LDAR` as an acquire load. The Cortex-X3 page
  19, Cortex-A715 page 21, and Cortex-A710 page 29 instruction tables list ordinary AArch64 `LDR`
  at latency 4/throughput 3; the Cortex-A510 page 23 table lists latency 2/throughput 2. Those core
  tables do not separately quantify `LDAR`, so no unsupported per-instruction cycle saving is
  claimed.
- `CurrentTick()`, `KnownGpuTick()`, and `NextTick()` now use relaxed ordering. Completed progress
  is merged by `AdvanceGpuTick()`, a relaxed compare/exchange atomic max shared by the timeline and
  fence paths. `MasterSemaphoreTimeline::Refresh()` queries
  `vkGetSemaphoreCounterValueKHR` exactly once and then retries only the local cache update; the old
  weak-CAS loop could repeat the driver query after either a race or a spurious failure.
- Correctness depends on atomicity and monotonicity, not a cross-object happens-before edge. A
  relaxed fetch-add still has one atomic modification order and produces unique ticks. Every
  `gpu_tick` candidate originates only after actual Vulkan completion, and atomic max cannot move
  it backward. A stale-low read can delay reuse, deletion, or a wait short-circuit but cannot claim
  unfinished work is complete. The existing immediate refreshes for resource pressure and explicit
  waits are unchanged.
- Final release-style AArch64 code changed `Scheduler::SubmitExecution()` from
  `__aarch64_ldadd8_rel` to `__aarch64_ldadd8_relax`. Both timeline and fence completion updates call
  `__aarch64_cas8_relax`, and resource-pool progress reads are ordinary `LDR` rather than `LDAR`.
  The timeline refresh contains one driver call before its CAS loop. This is direct confirmation of
  the manual-directed lowering, not an estimate from source.
- Catch2 coverage now proves the cached completion sequence advances from 0 to 7, rejects a
  regression to 3, and then advances to 9, alongside the existing cadence and exhausted-pool
  tests. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1m21s, compiling and linking the
  ELF64/AArch64 test executable. It was not executed because this x64 host cannot run it and the
  current instruction forbids Thor/device use.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 2m19s. The 28,966,367-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `F87A7B10154F601D7FA880834167DF48AC713EF3538D3C3846552BAF05DC7AC5`. No ADB, install, launch, or
  device access occurred.
- This removes ordering work from frequent small counter operations and avoids redundant driver
  queries under contention, but it is not a major whole-emulator speed or wattage claim. A future
  allowed parent-versus-candidate Thor A/B should use an identical Vulkan title/scene, caches,
  driver, resolution, layout, performance/fan mode, brightness, and duration. Record submission and
  refresh counts, CPU time, frametimes, battery power, temperature, thermal slope, memory growth,
  stability, and rendering correctness.

## 2026-08-17 ARM64 HLE Audio Planar Mix Layout

- Command-line Git/SSH fetched Azahar upstream `master` at `3392c56ce` (`core: Fix another msvc
  compiler bug`). Fork `master` at `297e9a0da` was already 0 commits behind and 70 ahead, so no
  upstream merge or conflict resolution was required.
- The first proposed aux-copy NEON patch was rejected after inspecting the complete ThinLTO
  `libcitra-android.so`. Clang already recognizes the scalar-looking 4x160 transpose: the old
  `AuxReturn()` fast path loaded two four-sample groups and emitted two `ST4` instructions per loop,
  while `AuxSend()` emitted two `LD4` instructions plus ordinary vector stores. The old
  `Source::MixInto()` also used two `LD4` and two `ST4` instructions for each eight source samples.
  Hand-written `vld4q_s32`/`vst4q_s32` would therefore have duplicated existing optimization and
  risked worse loop control and alias behavior.
- Arm Architecture Reference Manual DDI 0487 M.c sections C7.2.213 and C7.2.371 confirm that
  multiple-structure `LD4` de-interleaves memory into four registers and `ST4` interleaves four
  registers into memory. The visually checked Cortex-A510 issue 6.0 table 3-37 on PDF page 49 lists
  Q-form B/H/S `ST4` execution throughput as `1/50`, not an extraction or footnote error. The
  corresponding X3 issue 4.0 page 36 and A710 issue 4.0 page 60 tables list `1/6`; A715 issue 5.0
  page 39 lists `1/2`, while its page 67 complex-instruction guidance still calls out quad
  multiple-structure `LD4`/`ST4` forms as decode-limited. X3 page 32, A715 page 35, A710 page 53,
  and A510 page 44 provide the comparison data for ordinary `TRN`/`ZIP` permutations. The source
  PDFs remain outside the repository and indexed through `docs/hardware/README.md`.
- Rather than replacing one transpose instruction with a core-dependent shuffle sequence, the HLE
  DSP now keeps its temporary four-channel mixes planar end to end. `Source::MixInto()` accumulates
  directly into four contiguous channels; mono/stereo downmix loads those channels directly; and
  shared-memory aux send/return copies an already matching planar layout. Little-endian hosts use
  one 2,560-byte `memcpy` per enabled bus and direction. The compile-time big-endian fallback keeps
  element assignment so `s32_le` conversion semantics remain intact.
- Mixer state still serializes through the historical `std::array<QuadFrame32, 3>` sample-major
  archive type. Save converts planar live state into that exact legacy shape before archival; load
  converts it back afterward. This preserves old save-state field structure and order rather than
  silently changing archives with the in-memory optimization.
- Final release-style ARM64 code contains no `LD4` or `ST4` in `Source::MixInto()` or
  `DownmixAndMixIntoCurrentFrame()`. Source mixing uses ordinary `LDP`/`LDR` and `STP`/`STR`; its
  vector loop grows by seven executed instructions per eight samples versus the structured path,
  an explicit big-core tradeoff for avoiding the A510 bottleneck. Downmix replaces one `LD4` with
  four independent Q loads. `AuxReturn()` shrinks from 0x164 to 0x5c bytes and `AuxSend()` from
  0x1b4 to 0x88 bytes, with one `memcpy` call per full enabled bus instead of transpose loops.
- Catch2 source coverage now fills every aux bus/sample/channel with distinct signed values, checks
  the exact planar aux send result, and compares aux return plus final stereo mixing against the
  scalar reference. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 56 seconds after the final
  save-state compatibility change, compiling and linking the full ELF64/AArch64 test executable and
  `libcitra-android.so`. The test executable was not run because this host is x64 and current
  instructions forbid Thor/device use.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1m21s. The 28,966,339-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `4141003D54AE8B454625EEF021A70A4517A5068D70FB9892F194CE78A25501E5`.
- No device, ADB, install, launch, or game was used. Static code generation makes this a strong
  efficiency candidate, especially if Android schedules HLE audio on an A510, but no whole-game FPS,
  frametime, or wattage gain is claimed. A future allowed A/B should compare audio-heavy gameplay
  with identical title, save, caches, renderer, resolution, driver, layout, performance/fan mode,
  brightness, and duration. Record DSP-thread CPU placement/time, audio underruns, frametimes,
  battery power, temperature, thermal slope, stability, and output correctness.

## 2026-08-17 AArch64 HLE Stereo Source Filters

- A source-level audit found that the 160-sample HLE simple and biquad filters still processed the
  independent left and right channels through duplicated scalar arithmetic. The time dimension
  cannot be parallelized because each output feeds the next sample, but stereo lanes have separate
  histories and can be evaluated together without changing filter order.
- The complete relevant AArch64 AdvSIMD table pages were visually checked in the Cortex-X3 issue
  4.0, Cortex-A715 issue 5.0, Cortex-A710 issue 4.0, and Cortex-A510 issue 6.0 optimization guides.
  Their tables cover `SMULL`/`SMLAL`, arithmetic shifts, and `SQXTN` on every Thor CPU class. Arm
  Architecture Reference Manual DDI 0487 M.c sections C7.2.319, C7.2.325, and C7.2.352 confirm that
  the multiply instructions widen signed elements and that `SQXTN` performs the exact signed
  saturating narrow required by the old clamp. This uses baseline AdvSIMD, not an SVE assumption.
  The source PDFs remain outside the repository and are indexed through `docs/hardware/README.md`.
- AArch64 now packs each stereo sample into two 16-bit lanes. Simple filtering uses one widening
  multiply and one widening multiply-accumulate; biquad filtering uses one widening multiply plus
  four widening multiply-accumulates. The arithmetic right shift and signed saturating narrow match
  the old per-channel fixed-point shift and clamp. Adjacent time samples are never combined.
- Coefficient vectors load once per 160-sample frame. Previous input/output vectors stay in NEON
  registers for the full loop and are written back only at frame end. The reset simple coefficient
  is `32768`, which does not fit signed 16-bit, so reset passthrough is handled as an exact frame copy
  with final history advancement rather than being truncated. Biquad reset passthrough likewise
  records the last two inputs/outputs without redundant arithmetic. The non-AArch64 scalar path is
  unchanged.
- Final release-style ThinLTO code contains the intended by-element `SMULL`/`SMLAL`, `SSHR`, and
  `SQXTN`. For the simple filter, four scalar multiply operations, two scalar shifts, and two
  duplicated clamp sequences become four vector arithmetic/saturation instructions for both
  channels. For biquad, ten scalar multiply operations, two shifts, and two clamp sequences become
  seven vector arithmetic/saturation instructions for both channels. Coefficient and recurring
  state loads/stores also leave the per-sample loop.
- Focused Catch2 coverage compares simple-only, biquad-only, combined-order, multi-frame history,
  signed extremes, channel independence, saturation, and reset-passthrough history against a
  sequential scalar reference. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1m4s,
  compiling and linking the full ELF64/AArch64 test executable and `libcitra-android.so`. The test
  executable was not run because this host is x64 and current instructions forbid Thor/device use.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1m29s. The 28,965,995-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `EC107B8FE6D0AB226B68ABE620DC277A4165950E95D00F77C69B9DF3CBA19A11`.
- No device, ADB, install, launch, or game was used. This is a real sustained HLE DSP instruction and
  memory-traffic reduction when source filters are active, but its whole-game FPS and wattage effect
  remains unmeasured. A future allowed matched A/B should use an audio/filter-heavy title with the
  same save, caches, renderer, resolution, driver, layout, performance/fan mode, brightness, and
  duration, then record DSP-thread CPU time/placement, audio underruns, frametimes, battery power,
  temperature, thermal slope, stability, and output correctness.

## 2026-08-17 AArch64 HLE Linear Interpolation

- The HLE linear resampler runs once per output sample for every active source configured for
  linear interpolation. Its independent left and right lanes still used duplicated signed
  subtraction/clamp sequences, scalar 64-bit multiplies, and shifts in the final AArch64 binary.
  Polyphase interpolation remains a separate TODO and currently falls back to this path.
- The full relevant manual pages were visually checked rather than inferred from x86 code. Arm
  Architecture Reference Manual DDI 0487 H.a section C7.2.289 (PDF pages 2663-2664) defines vector
  `SQDMULH` as a corresponding-lane signed saturating doubling multiply that returns the truncated
  high half. The AArch64 ASIMD tables list `SQDMULH` with latency/throughput 4/2 on Cortex-X3 issue
  4.0 page 27, 4/1 on Cortex-A715 issue 5.0 page 29, 4/1 on Cortex-A710 issue 4.0 page 43, and
  latency 4 with the documented `2,1` throughput notation on Cortex-A510 issue 6.0 page 36. These
  are the actual X3/A715/A710/A510 classes in Snapdragon 8 Gen 2, and the implementation uses
  baseline AdvSIMD rather than SVE. The PDFs remain outside Git and are indexed in
  `docs/hardware/README.md`.
- The DSP delta is first saturated to signed 16-bit exactly as before. The phase is always in
  `[0, 2^24 - 1]`; shifting it left seven produces a positive Q31 multiplier no greater than
  `0x7fffff80`. `SQDMULH(delta, phase << 7)` therefore equals the signed arithmetic form of
  `(delta * phase) >> 24`, and its saturation case is unreachable for the bounded delta. For a
  negative product, the old unsigned C++ promotion differs by `2^40` after the shift, which vanishes
  under the final signed-16 narrowing, so every output bit remains identical. The non-AArch64
  scalar path is unchanged.
- Final release-style ThinLTO contains one `SSUBL`, one `SQXTN`, one `SSHLL`, and one two-lane
  `SQDMULH` for both channels. The old binary emitted two scalar `SMULL`s, two scalar logical
  shifts, four signed sample loads, and two duplicated four-instruction clamp chains. The complete
  function shrank from 680 to 636 bytes (44 bytes, or 6.5%), while the deque traversal and sample
  timing remain unchanged.
- Focused Catch2 coverage compares output, output index, consumed input, history, and fractional
  state with an independent scalar DSP reference across six rates, five boundary phases, partial
  output frames, signed extremes, and both saturated-delta directions.
- Command-line Git/SSH refreshed `upstream/master` at `3392c56ce` (`core: Fix another msvc compiler
  bug`); this fork remains zero commits behind and no upstream merge was needed.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1m5s, compiling and linking the focused test
  source, the full ELF64/AArch64 Catch2 executable, and `libcitra-android.so`. The test executable
  was not run because this host is x64 and current instructions forbid Thor/device use.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1m27s. The 28,965,971-byte APK contains only
  `arm64-v8a` native libraries and has SHA-256
  `74A107FA9EEBE2E39A3C04413BD774AD8C4D8AAA7168EB510EC7D07C38339DB0`.
- No device, ADB, install, launch, or game was used. This is a verified per-sample DSP instruction
  reduction, not a whole-game FPS or wattage measurement. A future allowed matched A/B should use a
  title that selects linear resampling, with identical save, caches, renderer, resolution, driver,
  layout, performance/fan mode, brightness, and duration, then record DSP-thread time/placement,
  audio underruns, frametimes, battery power, temperature, thermal slope, and output correctness.

## 2026-08-17 AArch64 ETC1 Block SIMD

- The block-level ETC1 change removed repeated setup, but final release AArch64 code still decoded
  all 16 pixels with a scalar nested loop. Each pixel performed variable selector/sign shifts,
  modifier and base-color table choices, three adds, three duplicated clamps, and four byte stores.
  The approximately 37-instruction pixel body ran 16 times per block, and an 8x8 PICA tile contains
  four blocks. This was the clearest remaining x86-originated scalar texture-upload gap.
- The complete relevant manual pages were visually checked. Arm Architecture Reference Manual DDI
  0487 H.a sections C7.2.309, C7.2.339, C7.2.390, and C7.2.403 (PDF pages 2717, 2801, 2911, and
  2938) define the exact vector operations used here: `SQXTUN` saturates signed lanes into narrower
  unsigned lanes, `TBL` performs byte lookup, signed per-lane `USHL` counts select left or
  truncating right shifts, and `ZIP1` interleaves the lower halves of two vectors. These are
  baseline AdvSIMD instructions and do not assume SVE.
- The Snapdragon 8 Gen 2 core manuals support the choice across the heterogeneous CPU. Cortex-X3
  issue 4.0 pages 27/31/32 list latency/throughput of 2/2 for `USHL`, 4/2 for `SQXTUN`, 2/2 for a
  one-table `TBL`, and 2/4 for `ZIP`. Cortex-A715 issue 5.0 pages 30/34/35 and Cortex-A710 issue 4.0
  pages 44/52/53 list 2/1, 4/1, 2/2, and 2/2 respectively. Cortex-A510 issue 6.0 pages 37/43/44
  list latencies 3, 4, 4, and 3 with the guide's `2,1` execution-throughput notation. The A510's
  table lookup is slower, but one 16-byte lookup still replaces 16 scalar alpha-nibble extractions.
  The external PDFs remain uncommitted and are indexed in `docs/hardware/README.md`.
- AArch64 now decodes each block as two compile-time eight-pixel bands. Lane shifts gather ETC's
  column-major selector and negation bit `4 * x + y` into row-major order. A vector flip mask
  selects horizontal `x / 2` subblocks or whole-band `y / 2` subblocks without per-lane scalar
  setup. Modifier selection, sign, base-color selection, and signed addition stay in 16-bit lanes;
  six `SQXTUN` instructions reproduce the old `[0,255]` clamps for RGB. ETC1A4 splits all 16 alpha
  nibbles, reorders them once with `TBL`, and duplicates each nibble with `SLI`. A ZIP network then
  writes the complete RGBA block with four 16-byte stores, replacing 64 scalar byte stores. The
  non-AArch64 scalar decoder is unchanged.
- Final ThinLTO contains no pixel loop and no lane-by-lane mask construction. The ETC1 function has
  114 straight-line instructions after its unchanged decoder-constructor call; ETC1A4 has 123.
  Static helper sizes grow from 312/348 bytes to 516/560 bytes because the two bands are unrolled,
  but the old roughly 600 dynamically executed pixel-body instructions are gone. Linked code shows
  the intended `USHL`, `SQXTUN`, `ZIP`, and four Q stores; ETC1A4 additionally shows exactly one
  `TBL` and one `SLI`.
- Permanent Catch2 coverage now generates 128 deterministic raw blocks with all flip/differential
  combinations, all modifier-table indices, structured selector/sign extremes, random base colors,
  and alpha extremes. Each block is checked as ETC1 and ETC1A4 with both `+24` and `-24` output
  stride against the independent scalar sampler, comparing the whole guarded buffer so row padding
  and canaries must remain untouched. That is 512 direct decoder cases / 8,192 pixels in addition
  to the existing complete padded-tile tests.
- Command-line Git/SSH refreshed `upstream/master` at `3392c56ce` (`core: Fix another msvc compiler
  bug`); this fork remains zero commits behind, so no upstream merge was needed.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` compiled and linked the source plus the 444,079,176-byte
  ELF64/AArch64 Catch2 executable. `:app:assembleVanillaRelWithDebInfoLite` then passed in 1m59s.
  The 28,964,919-byte APK contains only `arm64-v8a` native libraries and has SHA-256
  `172CFB1B162F118869DCA8360B576AF1AC89BB7B7C0A3CA024EBF17DBE90D5B5`.
- No device, ADB, install, launch, or game was used, and the AArch64 test executable was not run on
  this x64 host. This is a large instruction and store-count reduction when ETC blocks are decoded,
  not yet a whole-game FPS or wattage measurement. A future allowed matched A/B should use an
  ETC-heavy texture-streaming scene with identical save, caches, renderer, resolution, driver,
  layout, performance/fan mode, brightness, and duration, then record texture-upload CPU time,
  frametimes, battery power, temperature, thermal slope, visual correctness, and stability.

## 2026-08-17 HLE Audio Resampler Window

- The shared None/Linear stepping loop previously inserted `xn2` and `xn1` at the front of its
  `std::deque` on every call. Final release AArch64 code then repeated deque block-map arithmetic,
  block-pointer loads, and separate adjacent-sample loads for every output sample, including when
  upsampling reused the same integer input position. This bookkeeping survived even after the
  stereo interpolation arithmetic itself had been reduced to one exact two-lane `SQDMULH`.
- The complete AArch64 load-table pages were rendered and visually checked in the Cortex-X3 issue
  4.0 guide (table 3-7, PDF pages 18-19), Cortex-A715 issue 5.0 guide (table 3-7, pages 20-21),
  Cortex-A710 issue 4.0 guide (table 3-13, pages 28-29), and Cortex-A510 issue 6.0 guide (table
  3-12, pages 23-24). The big-core tables list four-cycle L1-hit latency for the relevant ordinary
  register loads; A510 lists two cycles. Removing dependent container loads is therefore useful on
  every Snapdragon 8 Gen 2 CPU class, without assuming SVE or changing the interpolation ISA. The
  external PDFs remain uncommitted and indexed in `docs/hardware/README.md`.
- The replacement treats history as a virtual sequence: `V(0) = xn2`, `V(1) = xn1`, and
  `V(j) = input[j - 2]` for `j >= 2`. A cached adjacent-sample window follows the monotonic integer
  input position. Reusing a position touches no deque sample; advancing by one performs one
  sequential iterator load. End-of-input state records the same final two virtual samples,
  subtracts the same consumed Q24 position, and erases exactly the corresponding real input
  samples. No history elements are inserted or moved.
- Two apparently broader variants were rejected only after linked-code inspection. A target-based
  rebase lambda became a 488-byte helper called for every output. A later large-decimation seek
  guard still became a 356-byte helper called for every output and forced a 192-byte stack frame.
  Both would have made the common path worse despite looking reasonable in C++. The accepted
  monotonic cursor inlines completely; the only 72-byte out-of-line lambda is the cold
  `ASSERT(rate > 0)` failure path.
- Final ThinLTO `Linear()` has no call in the valid per-output loop. Its normal advance is one
  post-increment `LDR`, while an unchanged position branches directly into the arithmetic. The
  exact `SSUBL`, `SQXTN`, `SSHLL`, `SQDMULH`, `SADDW`, and `UZP1` sequence remains. `Linear()`
  shrinks from 636 to 408 bytes and `None()` from 560 to 368 bytes. For rates above one, the cursor
  loads each skipped sample; the permanent tested matrix reaches 2.75x, where that is at most two
  or three sequential loads per output and avoids the old repeated map lookups. Do not infer a win
  for extreme unprofiled rate multipliers from this static result.
- Permanent Catch2 coverage now compares both None and Linear against the old independent
  deque-prefix algorithm across six rates, five boundary phases, signed extremes, partial output,
  empty and one-to-three-sample input, already-full output, and four consecutive calls. The full
  ELF64/AArch64 test executable compiled and linked successfully. Separately, the actual edited
  `interpolate.cpp` was compiled for the x64 host with only the assertion backend stubbed and
  matched the old algorithm across 20,000 randomized streaming cases, one to four calls per case,
  rates from 0.0625x through 8x, arbitrary PCM16 history/data, input sizes 0-700, appended data, and
  output starts 0-160.
- Command-line Git over SSH refreshed `upstream/master` at `3392c56ce` (`core: Fix another msvc
  compiler bug`). The fork remains zero commits behind, so no upstream merge was required.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 52 seconds after the final rejected
  experiment was removed. `:app:assembleVanillaRelWithDebInfoLite` then passed incrementally in 23
  seconds. The 28,963,055-byte APK contains only `arm64-v8a` native libraries and has SHA-256
  `4D3402454B4D1C499EC736791EA26429B2A670ADAD45E90C49FDF638E8970D2A`.
- No Thor, ADB, install, launch, game, FPS run, or battery measurement was used. This is a verified
  sustained DSP bookkeeping reduction, not a whole-game speed or wattage claim. A future allowed
  matched A/B should use a title with multiple resampled sources and identical save, caches,
  renderer, resolution, driver, layout, performance/fan mode, brightness, and duration. Record
  audio-thread CPU time/placement, underruns, frametimes, battery power, temperature, thermal slope,
  stability, and output correctness.

## 2026-08-17 AArch64 Converted 16-bit Texture Codec

- Converted `RGB5A1`, `RGB565`, and `RGBA4` texture copies still performed scalar per-pixel
  expansion or packing on AArch64. Before this change, final ThinLTO Morton-copy symbols were
  1,436, 1,340, and 1,536 bytes for the encode direction, and 372, 364, and 380 bytes for decode.
  These are common PICA texture formats, so the remaining scalar work was a better target than
  adding a broad architecture flag or approximate color math.
- The relevant instruction tables were read from the actual Cortex-X3 issue 4.0 guide (PDF pages
  27 and 31-36), Cortex-A715 issue 5.0 guide (pages 29-30 and 34-39), Cortex-A710 issue 4.0 guide
  (pages 44 and 52-60), and Cortex-A510 issue 6.0 guide (pages 37 and 43-49). All four cores make
  shifts, narrowing, and `ZIP` useful building blocks. The A510 table is the critical constraint:
  Q-form byte/halfword `ST4` is documented at only `1/50` throughput while ordinary `ST1` is
  `1/cycle`. The implementation therefore interleaves RGBA with `ZIP1`/`ZIP2` and emits ordinary
  paired Q stores rather than using an attractive-looking `ST4` output. D-form `LD4` remains useful
  where encode must deinterleave existing RGBA input. The external manuals remain uncommitted and
  are indexed in `docs/hardware/README.md`.
- `texture_codec.h` now converts sixteen pixels per linear iteration. Full Morton decode handles
  two eight-pixel rows per iteration with `LD2`, vector shifts/masks, exact narrowing, `ZIP`, and
  paired Q stores. Reverse Morton encode uses D-form `LD4` to deinterleave RGBA and `ST2` to write
  the two Morton rows. Exact 5/6/4-bit replication is retained on decode, encode still truncates
  to the high source bits, RGB5A1 alpha remains one bit, bottom-up row placement and padded strides
  are unchanged, and every non-AArch64 path remains scalar.
- Final linked-code inspection confirms that the intrinsics survived ThinLTO. The RGB565 Morton
  decode body contains two `LD2`, two paired Q stores, no `ST4`, and no halfword scalar load. Its
  encode body contains `LD4`/`ST2` and no `ST4`. The linear decode's only halfword scalar load is in
  the tail. Final encode-direction Morton symbols shrink to 1,004, 944, and 932 bytes for RGB5A1,
  RGB565, and RGBA4: 30.1%, 29.6%, and 39.3% below baseline. Decode-direction symbols become 464,
  432, and 440 bytes; they are 15.8-24.7% larger but replace the full 64-pixel scalar conversion
  loop with the vector body. Linear encode symbols are 276, 252, and 256 bytes, while linear decode
  symbols are 392, 352, and 368 bytes.
- Permanent Catch2 coverage exhaustively round-trips all 65,536 packed values for each format
  through Morton tiles. Separate 37-pixel linear decode/encode cases exercise vector bodies,
  scalar tails, and canaries. The ELF64/AArch64 test executable compiled and linked successfully.
  A temporary independent model also verified every possible packed decode and round trip plus
  one million random RGBA encodes per format; it was deleted after use and was not committed.
- While this slice was in progress, command-line Git over SSH refreshed upstream to `32a3c0bfd`
  (`core: dsp: Add volume ramping to the HLE backend (#2409)`). The merge conflict was limited to
  the fork's planar HLE mixer: the resolution keeps `PlanarQuadFrame32` and channel-major indexing
  while adopting upstream ramp state, serialization, dirty activation, per-frame ramp completion,
  and the required non-const `MixInto()`. The complete ARM64 native build passed after the merge.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed, followed by a successful
  `:app:assembleVanillaRelWithDebInfoLite` in 2 minutes 35 seconds. The resulting 28,963,995-byte
  APK contains only `arm64-v8a` libraries and has SHA-256
  `21F4D58969445E3FA3732F9AD1940BB09A170A68B5BF5D53A4DF098C108ABDFA`.
- After verification, only exact generated paths under `src/android/app` were cleaned: Gradle
  intermediates and the 444,317,952-byte linked test executable. The final APK and active ARM64
  release native cache were retained, while free C: space increased by 2,019,221,504 bytes
  (about 1.88 GiB). No source, manual, save, or unrelated file was touched.
- No Thor, ADB, install, launch, game, FPS run, or battery measurement was used. This is an exact
  16-pixel-at-a-time texture conversion and a major dynamic instruction/store-count reduction when
  these formats are copied, not yet a whole-game FPS or wattage claim. A future allowed matched A/B
  should hold title, scene, save, caches, renderer, resolution, driver, layout, performance/fan
  mode, brightness, and duration constant, then record texture-upload CPU time, frametimes, battery
  power, temperature, thermal slope, visual correctness, and stability.

## 2026-08-17 AArch64 Morton Structured-Store Removal

- A final audit of the AArch64 texture codec found two x86-shaped leftovers: native RGB8/D24
  copies emitted D-form byte `ST3`, while IA8, RG8, I8, A8, and IA4 expansion emitted D-form byte
  `ST4`. These instructions are concise in source but are a poor match for Thor's efficiency
  cluster, so this slice targets them without changing formats, guest-visible math, or the scalar
  fallback.
- The decision comes from the checked Arm manuals, not a generic NEON assumption. The Cortex-X3
  tables on PDF pages 32 and 35-36 list D-form byte/halfword `ST3` at `1/2`, `ST4` at `1/3`,
  ordinary one-register `ST1` at `2/cycle`, and `ZIP` at `4/cycle`. The A715 tables on pages 35 and
  38-39 list D-form `ST3`/`ST4` at `1/cycle`, ordinary `ST1` at `2/cycle`, and `ZIP` at `2/cycle`.
  The A710 tables on pages 53 and 59-60 list `ST3` at `1/2`, `ST4` at `1/3`, ordinary `ST1` at
  `2/cycle`, and `ZIP` at `2/cycle`. Most importantly, the A510 tables on pages 44 and 48-49 list
  D-form byte/halfword `ST3` at only `1/17`, `ST4` at only `1/25`, and ordinary one-register `ST1`
  at `1/cycle`. The external PDFs remain uncommitted and are indexed in
  `docs/hardware/README.md`.
- IA8/RG8/I8/A8/IA4 expansion now preserves the existing per-row component generation, combines
  each two-row band, and reuses `StoreRGBA8RowsA64()`. Final code performs the exact interleave
  with `ZIP1`/`ZIP2` and two paired Q stores per band. Native RGB8/D24 retains D-form `LD3` for
  deinterleaving, then uses two exact two-register `TBL` permutations and ordinary Q/D stores for
  each packed 24-byte row. A compile-time proof checks all 24 shuffle indices against
  `component * 8 + pixel`.
- Existing Catch2 cases cover native RGB8 and D24 in both swizzle directions and expanded IA8,
  RG8, I8, A8, and IA4 decoding, including full tiles, bottom-up row placement, and padded linear
  strides. The final ELF64/AArch64 test executable compiled and linked. It was not run because the
  host is x64 and this work deliberately did not use the Thor.
- Final ThinLTO inspection confirms that all edited symbols contain no `ST3` or `ST4` stores.
  Expanded-format bodies contain `ZIP1`/`ZIP2` and paired Q stores. Packed RGB8/D24 bodies contain
  the expected `LD3`, table permutations, and ordinary Q/D stores, with shuffle constants hoisted
  outside the full-tile loop. IA8, RG8, I8, A8, and IA4 symbols shrink from 480, 480, 440, 440, and
  504 bytes to 328, 324, 320, 308, and 344 bytes: reductions of 31.7%, 32.5%, 27.3%, 30.0%, and
  31.7%. RGB8 decode/encode grows from 372/844 to 444/1,028 bytes, and D24 grows from 376/848 to
  448/1,032 bytes, trading about 19-22% more code for removal of the A510's pathological stores.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 2 minutes 14 seconds after the final
  compile-time proof was added. The resulting 28,964,139-byte APK contains only `arm64-v8a`
  libraries and has SHA-256
  `3707EC72ED8EF52A30B9C39E307B180EA2EAB0B158F1608BB6783476409A9BC7`.
- After verification, only exact generated paths under `src/android/app` and the repo-local
  temporary manual/codegen extracts were removed. The final APK and active ARM64 native cache were
  retained; free C: space increased by 2,060,705,792 bytes (about 1.92 GiB). No source, external
  manual, save, or unrelated file was touched.
- No Thor, ADB, install, launch, game, FPS run, or battery measurement was used. This is a verified
  removal of severe structured-store bottlenecks when these texture-copy paths execute, not a
  whole-game speed or wattage result. A future allowed matched A/B must hold scene, save, caches,
  renderer, resolution, driver, layout, performance/fan mode, brightness, and duration constant,
  then record texture-upload CPU time, frametimes, battery power, temperature, thermal slope,
  visual correctness, and stability.

## 2026-08-17 AArch64 Vulkan D24S8 Staging Unpack

- Vulkan uploads reserve five staging bytes per D24S8 pixel, then split contiguous little-endian
  S8D24 input into a four-byte depth plane followed by a one-byte stencil plane before the buffer
  copy. Final baseline ARM64 code did this strictly one pixel at a time. The native D24 loop
  executed ten load/store/shift/bookkeeping instructions per pixel; the D32 fallback executed
  thirteen and issued one scalar `FDIV` per pixel. This pixel-count-wide Vulkan path ranked above
  Y2R and crypto setup work because it is in the active renderer, grows directly with uploaded
  surface area, and had authoritative scalar ThinLTO evidence.
- The relevant manual pages were visually checked before choosing the data layout. Cortex-A510
  issue 6.0 pages 23-24 cover ordinary loads, page 43 lists `XTN`, page 44 lists `UZP`, page 45
  lists one-register `LD1` at `2/cycle`, page 47 lists Q-form byte `LD4` at only `1/3`, and pages
  39-40 list integer-to-float conversion plus Q-form F32 `FDIV` at `1/10`. Cortex-X3 issue 4.0
  pages 18-19 cover ordinary loads, page 31 lists `XTN` at `4/cycle`, and page 32 lists `UZP` at
  `4/cycle`. This favors ordinary vectors plus a narrowing/permute tree over structured `LD4` or
  table constants on both the prime and efficiency ends of Thor. The external PDFs remain
  uncommitted and indexed in `docs/hardware/README.md`.
- `VideoCore::UnpackDepthStencil()` now handles sixteen pixels per AArch64 band. It loads all four
  packed Q vectors before overwriting the in-place depth plane, shifts exact 24-bit depth values,
  narrows the four low stencil-byte streams into one Q vector, and writes contiguous planes. The
  native D24 mode stores shifted integers. The fallback converts with exact vector `UCVTF` and
  `FDIV` by 16,777,215; it deliberately does not substitute reciprocal multiplication. All
  non-AArch64 and sub-sixteen-pixel work retains the scalar expression.
- Final ThinLTO improves on the source intrinsics: Clang folds the six-step narrowing expression
  into three `UZP1` operations. The D24 loop is two `LDP Q`, four `USHR`, two `UZP1 .8H`, two
  `STP Q`, one `UZP1 .16B`, one `STR Q`, and five loop/address instructions: seventeen executed
  instructions per sixteen pixels versus the baseline 160, an 89.4% core-loop instruction-count
  reduction. The D32 loop is twenty-five instructions per sixteen pixels versus the baseline 208,
  an 88.0% reduction, and replaces sixteen scalar divisions with four four-lane `FDIV`. The new
  shared helper is 496 bytes; moving conversion out of the Vulkan command lambda shrinks that
  lambda from 4,616 to 4,424 bytes.
- Permanent Catch2 coverage checks both depth modes at 0, 1, 3, 15, 16, 17, 31, 32, 33, 63, 64,
  and 65 pixels. It includes zero, one, midpoint, top-edge, and patterned 24-bit depths, varied
  stencil bytes, exact float bit patterns, returned depth-plane size, full output, and 32 bytes of
  trailing canary. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1 minute 16 seconds,
  compiling the tests and linking the full ELF64/AArch64 test executable plus final ThinLTO
  library. The executable was not run because the host is x64 and this slice deliberately did not
  use the Thor.
- `:app:assembleVanillaRelWithDebInfoLite` then passed in 2 minutes 25 seconds. The resulting
  28,964,523-byte APK contains only `arm64-v8a` libraries and has SHA-256
  `65BEBCAF86469FC740A8C8E4D18DA02DA3CF6D31B4FF01E2DFF83C85D4007440`.
- After verification, exact generated Gradle intermediates, the 444,353,384-byte ARM64 test
  executable, and repo-local manual renders were removed. The APK and active ARM64 native cache
  were retained; final cleanup increased free C: space by 2,018,963,456 bytes (about 1.88 GiB).
  No source, external manual, save, or unrelated file was touched.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. Static codegen
  proves much less CPU work when D24S8 staging is unpacked; it does not prove a whole-game speed or
  wattage change. A future allowed matched A/B should record D24S8 upload CPU time alongside
  frametimes, battery power, temperature, thermal slope, visual correctness, and stability.

## 2026-08-17 AArch64 HLE Source Gain Mixing

- Every enabled HLE source accumulates 160 stereo samples into four planar channels for each of
  three intermediate mix buses. Final baseline ThinLTO kept this loop scalar despite the planar
  layout: its steady body executed 31 instructions per sample, and the ramped body executed 38
  while testing the frame-wide ramp flag again for every sample. This ranked above fallback PICA
  shader swizzles because normal hardware-shader draws bypass that CPU JIT, whereas this mixer is
  sustained work for every active HLE source and enabled bus.
- The complete relevant manual pages were visually inspected before selecting the loop shape.
  Cortex-A510 issue 6.0 pages 39-40 cover Q-form integer/float conversion, multiply, and FMA, while
  pages 43-44 cover widening and `UZP`; Cortex-A710 issue 2.0 pages 47-48 and 52-54, Cortex-A715
  issue 3.0 pages 31-32 and 34-35, and Cortex-X3 issue 4.0 pages 28-29 and 31-32 provide the
  corresponding AdvSIMD execution data. A710 page 82, A715 page 59, and X3 page 56 also recommend
  loop unrolling and non-writeback `LDP`/`STP`. Those tables favor an eight-sample ordinary-load
  plus `UZP` design that works across Thor's prime, performance, and efficiency cores; the external
  PDFs remain uncommitted and indexed in `docs/hardware/README.md`.
- `Source::MixInto()` now selects the ramped or steady AArch64 specialization once per frame. Each
  iteration consumes eight interleaved stereo samples with one compiler-combined `LDP Q`, uses
  `UZP1`/`UZP2` to separate left and right, shares four `SSHLL` plus four `SCVTF` operations across
  the four destinations, and performs vector `FMUL`/`FCVTZS`/integer accumulation directly on the
  planar buses. The ramped specialization creates exact integer sample indices, converts and
  scales them by `1 / 159`, and retains the old fused `start + (end - start) * progress` operation.
  It therefore avoids both a per-sample flag branch and accumulated floating-point index drift.
  Non-AArch64 builds retain the original scalar implementation.
- Final linked ThinLTO contains both specializations inline in `Source::MixInto()` with no helper
  call or vector spill. The steady loop is 52 instructions per eight samples, or 6.5 per sample,
  versus 31 before: a 79.0% executed-instruction reduction. The ramped loop is 74 per eight, or
  9.25 per sample, versus 38 before: a 75.7% reduction. The containing function grows from 268 to
  736 bytes, a deliberate 468-byte tradeoff for eliminating repeated work across this sustained
  DSP loop.
- Focused Catch2 coverage compares steady and ramped output against an independent channel-major
  scalar reference. It includes signed-16 minimum/maximum, zero and +/-1 input, positive,
  negative, fractional, and zero gains, nonzero destination accumulators, 32-byte prefix/suffix
  canaries, ramp-state transitions, and a disabled source. The test compiles and links into the
  native ARM64 test executable; it is not executed on this x64 host because this slice deliberately
  does not use the Thor. `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1 minute 9 seconds.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 15 seconds. The resulting
  28,965,523-byte APK contains only `arm64-v8a` libraries and has SHA-256
  `719AF98D109686002BB37FA19A2AFA43F62647960035837445D5A0B52F8E4C27`.
- After verification, exact generated Gradle intermediates, mapping/debug-symbol output, the
  444,476,912-byte ARM64 test executable, and repo-local manual renders were removed. The APK and
  active ARM64 native cache were retained; final cleanup increased free C: space by 1,053,016,064
  bytes (about 0.98 GiB). No source, external manual, save, or unrelated file was touched.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. The instruction
  reduction should lower DSP-thread work when this path executes, but it is not yet a measured
  whole-game speed or wattage gain. A future allowed matched A/B must hold title, scene, save,
  caches, renderer, resolution, driver, performance/fan mode, brightness, layout, and duration
  constant, then record DSP-thread time/placement, audio underruns, frametimes, battery power,
  temperature, thermal slope, output correctness, and stability.

## 2026-08-17 HLE Silent-Bus Elision

- `DspHle::Impl::GenerateCurrentFrame()` previously called `Source::MixInto()` separately for each
  of 24 sources and three buses: 72 cross-function calls per audio frame, even when a source was
  disabled or an auxiliary bus had exact-zero gain. The MerryAudio fixture supplies a representative
  routing shape: it sets only main-bus left/right gains to one while marking all three gain groups
  dirty, leaving both auxiliary buses at zero. This supports optimizing a common case without
  assuming that every title uses it.
- The complete relevant instruction-table pages were visually inspected in the external manuals.
  The 4S `UMINV` and vector `FCMEQ` entries are on Cortex-A510 issue 6.0 pages 36 and 39,
  Cortex-A710 issue 4.0 pages 43 and 46, Cortex-A715 issue 5.0 pages 29 and 30, and Cortex-X3
  issue 4.0 pages 26 and 28. The tables make a single 128-bit compare/reduction a sound
  heterogeneous-core trade for avoiding a full 160-sample pass; the PDFs remain outside Git and
  their hashes stay recorded in `docs/hardware/README.md`.
- `Source::MixInto()` now accepts all three planar destinations at once, handles a disabled source
  once, and loops over the buses internally. A bus is skipped only when all ending gains compare
  equal to zero and, for an active ramp, all starting gains also compare equal to zero. Thus `+0`
  and `-0` are silent, while NaN, every nonzero steady gain, nonzero-to-zero ramps, and
  zero-to-nonzero ramps keep the existing arithmetic and state-transition path. Non-AArch64 builds
  retain `std::any_of`; AArch64 uses one Q load, `FCMEQ #0.0`, `UMINV 4S`, and `FMOV` per checked
  gain vector.
- Final ThinLTO proves one `MixInto()` call inside the 24-source caller loop, reducing calls from 72
  to 24 per frame (66.7%). A steady silent bus executes about 13 predicate/control instructions and
  bypasses the 1,040 instructions in the full 20-iteration NEON body, about 98.8% less core work on
  that bus. A zero-to-zero ramp takes about 20 instructions and bypasses the 1,480-instruction
  ramped body, about 98.6% less. The vector predicate also reduced the combined function from the
  scalar-predicate interim's 936 bytes to 832 bytes. The caller shrank from 552 to 492 bytes. Active
  ramp mixing saves/restores `d8`/`d9` once at function entry/exit, but has no spill/reload traffic
  inside the sample loop; that measured pair replaces two removed outer calls.
- Focused Catch2 coverage now treats all three destinations as one guarded object and checks exact
  steady/ramped output, signed-zero steady silence, zero-to-zero ramp silence, nonzero-to-zero ramp
  arithmetic, zero-to-nonzero ramp arithmetic, all-three-bus disabled-source state advancement,
  unchanged inactive buses, and 32-byte canaries.
  The final `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1 minute 9 seconds, compiling and
  linking the ARM64 test ELF and final library. The test ELF was not run because this host is x64
  and device use is currently forbidden.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 2 minutes 13 seconds. The resulting
  28,965,375-byte APK contains only `arm64-v8a` libraries and has SHA-256
  `3683A746697B6E731264EFA00941BE81ED21931392349FEF09B0BCDBB0FB5070`.
- After verification, exact Gradle intermediates, mapping/debug-symbol output, the 444,493,136-byte
  ARM64 test executable, and repo-local manual renders were removed. The APK and active ARM64
  CMake cache were retained; free C: space increased by 2,027,655,168 bytes (about 1.89 GiB). No
  source, external manual, save, or unrelated file was touched.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. This is a strong
  DSP-thread efficiency and power candidate when buses are silent, not a whole-game FPS or wattage
  claim. A future allowed matched A/B should hold title, scene, save, caches, renderer, resolution,
  driver, layout, performance/fan mode, brightness, and duration constant, then record DSP-thread
  time/placement, audio underruns, frametimes, battery power, temperature, thermal slope, output
  correctness, and stability.

## 2026-08-17 HLE Front-Stereo Specialization

- The all-bus elision still leaves four destination channels of work on every active bus, even when
  only front-left/front-right gains are configured. The in-tree MerryAudio fixture sets only main
  gains `[0][0]` and `[0][1]`, while its biquad fixture sets only auxiliary gain `[1][0]`; both
  dirty all three gain groups. This is direct repository evidence for a common reduced-routing
  shape, while the unchanged full path remains available for games that use rear channels.
- The complete relevant load/store table pages were visually inspected in the external manuals:
  Cortex-A510 issue 6.0 pages 45 and 48, Cortex-A710 issue 4.0 pages 55 and 58, Cortex-A715 issue
  5.0 pages 36 and 38, and Cortex-X3 issue 4.0 pages 33 and 35. Across the efficiency, performance,
  and prime cores, vector loads/stores consume load/store and vector-side resources; the latter
  three also document forwarding cost into FP/AdvSIMD/vector consumers, and store operations split
  into address and data work. That makes eliminating proven-unused destination traffic preferable
  to performing zero multiplies. The PDFs remain outside Git and their hashes stay recorded in
  `docs/hardware/README.md`.
- AArch64 now selects a front-only template when both ending rear gains are exact signed zero and,
  for an active ramp, both starting rear gains are exact signed zero. One D load plus a 64-bit
  `AND`/`TST #0x7fffffff7fffffff` removes only the two sign bits. Thus `+0` and `-0` can skip rear
  work, while every subnormal, finite nonzero, infinity, or NaN takes the unchanged four-channel
  arithmetic path. Each source/destination iterator remains within its own `std::array` object;
  no flattened cross-subarray pointer arithmetic is used. Non-AArch64 behavior is unchanged.
- Final ThinLTO proves that front-only loops contain no rear offsets (`0x500`, `0x510`, `0x780`,
  or `0x790`). Their two paired destination loads and two paired stores move 2,560 bytes per active
  bus/frame instead of 5,120, a 50% reduction. The steady body falls from 52 to 32 instructions per
  eight samples (38.5%), and the ramped body falls from 74 to 46 (37.8%). The four-channel steady
  and ramped bodies remain exactly 52 and 74, avoiding the one-instruction fallback regression in
  an earlier index-loop draft. `Source::MixInto()` grows from 832 to 1,244 bytes, a 412-byte
  instruction-cache trade for the two specialized loops. The 24-source caller remains 492 bytes
  with one `MixInto()` call per source.
- Focused Catch2 sections compare both steady and ramped front-only routing with the independent
  scalar reference and guarded three-bus destination object, proving exact front accumulation,
  untouched rear channels, ramp-state advancement, and intact canaries. The final native build
  compiled and linked those tests and the ThinLTO library in 56 seconds. The 444,501,016-byte ARM64
  test ELF was not executed on the x64 host because device use remains forbidden.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 2 minutes 19 seconds. The resulting
  28,965,995-byte APK contains only `arm64-v8a` libraries and has SHA-256
  `A1EEF78968F5CACAA42C877F68B0C9E77BEE9F8EDCC8EDC21267F3A1A3B6F62A`.
- After verification, exact Gradle intermediates, downloaded JNI copies, mapping/debug-symbol
  output, the ARM64 test executable, and repo-local manual renders were removed. The APK and active
  ARM64 CMake cache were retained; free C: space increased by 2,465,615,872 bytes (about 2.30 GiB).
  No source, external manual, save, or unrelated file was touched.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. These are exact
  linked hot-loop and traffic reductions for a repository-evidenced routing shape, not a whole-game
  speed or wattage claim. A future allowed matched A/B must hold title, scene, save, caches,
  renderer, resolution, driver, layout, performance/fan mode, brightness, and duration constant,
  then record DSP-thread time/placement, audio underruns, frametimes, battery power, temperature,
  thermal slope, output correctness, and stability.

## 2026-08-17 HLE Zero-Volume Final-Mix Elision

- `Mixers::MixCurrentFrame()` cleared the output and then unconditionally downmixed all three
  160-sample intermediate buses. Repository evidence shows that this is sustained zero work in a
  normal routing shape: MerryAudio explicitly configures `master_volume = 1.0` and both
  `aux_return_volume` entries to `0.0`. Aux send/return still has to run because it exchanges DSP
  data and updates saved intermediate state, but a zero-volume bus cannot contribute to the final
  signed-16 frame.
- The frame loop now compares each volume with zero before output-format dispatch. Both signs of
  zero skip only the downmix; every finite nonzero and infinity continues to mix, and unordered
  AArch64 `FCMP` makes NaN fall through to the existing arithmetic/conversion path. Integer input
  samples multiplied by signed zero convert to integer zero, so omitting their saturated add is
  exact. `current_frame.fill({})`, aux copies, intermediate buffers, configuration parsing, and
  status behavior are unchanged on all architectures.
- Final ThinLTO proves that production `Mixers::Tick()` contains one `FCMP S, #0.0` plus `B.EQ`
  ahead of the existing format dispatch; the separately emitted `MixCurrentFrame()` has the same
  lowering. The change adds eight bytes to each symbol (`Tick`: 588 to 596 bytes; outlined mixer:
  384 to 392) while leaving active stereo and mono bodies exactly 24 and 23 instructions per four
  samples. A skipped stereo bus avoids all 40 iterations, or 960 loop instructions; mono avoids
  920. Each iteration otherwise reads four input Q vectors and 16 interleaved output bytes and
  writes 16 output bytes, so either skip avoids 3,840 bytes of buffer traffic per bus/frame.
- For MerryAudio's one-active/two-zero stereo shape, the three downmix bodies fall from 2,880 to
  960 executed instructions, a 66.7% loop-work reduction, and traffic falls from 11,520 to 3,840
  bytes, saving 7,680 bytes per audio frame. Predicate and outer-loop control remain, so these
  figures deliberately describe the downmix bodies rather than the entire DSP frame.
- Focused Catch2 sections compare Mono and Stereo output against the independent scalar reference
  with `{0.5, -0.0, +0.0}` volumes, in addition to the existing all-active Mono/Stereo/Surround,
  saturation-edge, and auxiliary-buffer coverage. The ARM64 native build passed and linked the
  tests; the 444,502,472-byte test ELF was not executed on the x64 host because device use remains
  forbidden.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 2 minutes 18 seconds. The resulting
  28,966,503-byte APK contains only `arm64-v8a` libraries and has SHA-256
  `7BC55E8E453CC2AA66D7E5EA452A840FC6A03067F18E77FC80AB71CAADE6666B`.
- After verification, exact Gradle intermediates, downloaded JNI copies, mapping/debug-symbol
  output, and the ARM64 test executable were removed. The APK and active ARM64 CMake cache were
  retained; free C: space increased by 2,027,388,928 bytes (about 1.89 GiB). No source, manual,
  save, or unrelated file was touched.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. This is an exact
  linked zero-work elimination with direct fixture evidence, not a whole-game speed or wattage
  claim. A future allowed matched A/B must hold title, scene, save, caches, renderer, resolution,
  driver, layout, performance/fan mode, brightness, and duration constant, then record DSP-thread
  time/placement, audio underruns, frametimes, battery power, temperature, thermal slope, output
  correctness, and stability.

## 2026-08-17 AArch64 Eight-Sample Final Downmix

- The active final stereo and mono downmix loops still handled only four output samples per
  iteration after their inputs became planar. Their interleaved signed-16 accumulator made D-form
  `LD2`/`ST2` natural, but repeated the structured memory instructions and loop control 40 times per
  active bus/frame.
- The decision to widen the structured pair comes from the actual Snapdragon 8 Gen 2 core manuals.
  Cortex-A510 issue 6.0 pages 46/49 list D-form halfword `LD2`/`ST2` at throughput 1 and Q form at
  `1/2`, so Q form moves twice the samples with proportional execution cost. Cortex-A710 issue 4.0
  pages 55/59 and Cortex-X3 issue 4.0 pages 33/35 list D/Q `LD2` at 2 versus `3/2` and D/Q `ST2` at
  1 versus `1/2`; Q form improves load bytes per cycle and preserves store bytes per cycle.
  Cortex-A715 issue 5.0 pages 36/38 lists `LD2` at 2 versus `3/2` and `ST2` at 2 for both widths, so
  Q form improves useful bytes per cycle for both. Ordinary loads plus `UZP`/`ZIP` were rejected:
  they add permutation instructions to a two-way interleave whose structured operations are already
  efficient across every Thor core class.
- AArch64 stereo and mono now handle eight samples per iteration. Each half retains the exact prior
  conversion and multiply/FMA sequence; `SQXTN`/`SQXTN2` combines the halves, `.8h` `SQADD` retains
  saturating accumulation, and Q-form `LD2`/`ST2` preserves interleaved output. The non-AArch64 path
  is unchanged, and 160 samples has no tail.
- Final ThinLTO contains Q-form `LD2 {v?.8h, v?.8h}` and `ST2 {v?.8h, v?.8h}`, `SQXTN2`, and `.8h`
  saturated adds with no D-form structured operation, extra `UZP`/`ZIP`, or vector spill in either
  hot loop. Stereo falls from two 24-instruction four-sample iterations to one 39-instruction
  eight-sample iteration: 960 to 780 instructions per active bus/frame, an 18.75% reduction. Mono
  falls from two 23-instruction iterations to one 37-instruction iteration: 920 to 740, a 19.6%
  reduction. Input/output traffic remains 3,840 bytes per active bus/frame; exact zero-volume buses
  still bypass the active body entirely.
- Existing differential Catch2 coverage exercises Mono, Stereo, and Surround output, signed-zero
  buses, all-active gains, saturation edges, aux exchange, and all 160 sample positions against an
  independent scalar reference. The ARM64 native build compiled and linked that test executable and
  the final ThinLTO library successfully; the ARM64 executable was not run on this x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 2 minutes 21 seconds. The resulting
  28,966,095-byte APK contains only `arm64-v8a` libraries and has SHA-256
  `B7C89A4157658BFBE4F9054F0C6182280346A7D934487E7E68D33BCB4E41B1C0`.
- After verification, exact Gradle intermediates, downloaded JNI copies, mapping/debug-symbol
  output, the 444,504,472-byte ARM64 test executable, and repo-local manual renders were removed.
  The final APK and active ARM64 CMake cache were retained; free C: space increased by
  2,024,067,072 bytes (about 1.88 GiB). No source, external manual, save, or unrelated file was
  touched.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. This is a bounded
  linked DSP-loop instruction reduction and a plausible CPU-energy improvement, not a measured
  whole-game speed or wattage result. A future allowed matched Thor A/B must hold title, scene,
  save, caches, renderer, resolution, driver, layout, performance/fan mode, brightness, and duration
  constant, then record DSP-thread time/placement, audio underruns, frametimes, battery power,
  temperature, thermal slope, output correctness, and stability.

## 2026-08-17 AArch64 Converted-D24 Morton Tiles

- The Vulkan renderer's converted `D24` path still expanded each 192-byte PICA Morton tile to
  256 bytes of D32 float, and packed it back again, one pixel at a time on AArch64. Baseline final
  ThinLTO showed 102 instructions in the decode x-column loop repeated eight times, or 816 core
  inner instructions per tile. Encode showed 93 instructions repeated eight times, or 744 per
  tile. Both contained 64 scalar conversions and encode also issued 192 scalar byte stores.
- The complete relevant table pages were visually inspected in all four external Snapdragon 8 Gen
  2 core manuals: Cortex-A510 issue 6.0 pages 39-46, Cortex-A710 issue 4.0 pages 46-56,
  Cortex-A715 issue 5.0 pages 30-37, and Cortex-X3 issue 4.0 pages 28-34. A direct four-register
  table gather was rejected because A510 documents four-table `TBL` at latency 16 and throughput
  `1/9`. D-form `LD3` directly de-interleaves eight packed D24 pixels, while one-table `TBL`,
  ZIP/UZP, and narrowing avoid that efficiency-core cliff. The PDFs remain outside Git and their
  hashes stay recorded in `docs/hardware/README.md`.
- The new AArch64 path handles a complete two-row, sixteen-depth band per iteration. Decode uses two
  D-form byte `LD3`, three one-table Morton permutations, ZIPs to assemble little-endian `u32`
  lanes, and four-lane `UCVTF` plus true `FDIV`. Encode uses paired Q float loads, exact `FMUL` plus
  `FCVTZU`, narrowing/UZP byte extraction, three inverse Morton permutations, and the existing
  exact two-`TBL2` packed-store helper. The scalar non-AArch64 path is unchanged; no reciprocal
  approximation or changed truncation was introduced.
- Final ThinLTO proves decode's two-row loop is 37 instructions repeated four times, or 148 core
  inner instructions per tile: 81.9% fewer than the 816-instruction scalar baseline. Encode is 57
  instructions repeated four times, or 228 per tile: 69.4% fewer than the 744-instruction baseline.
  The linked loops contain the intended `LD3`, `TBL1`, ZIP/UZP/narrow, vector conversion/divide,
  and ordinary packed stores, with no four-table `TBL`, per-pixel fallback, or hot-loop spills.
  Tile memory traffic is unchanged; this removes CPU instruction and scalar memory-operation work.
- Focused Catch2 coverage constructs all 64 Morton positions from zero, one, midpoint, maximum,
  near-maximum, recognizable edge values, and deterministic patterns. It compares every decoded
  float byte against the scalar division, preserves a ten-pixel padded stride and canaries, and
  compares encode against the scalar float-multiply/truncate expression instead of assuming every
  decoded float round-trips to its original integer.
- The final native ARM64 build compiled and linked the focused test plus the production ThinLTO
  library successfully. The 444,519,336-byte ARM64 test executable was not run on this x64 host
  because device use remains forbidden. `:app:assembleVanillaRelWithDebInfoLite` then passed in
  1 minute 22 seconds; the resulting 28,966,415-byte APK contains only `arm64-v8a` libraries and
  has SHA-256 `E0A8C836AA1E9D1240F71223E30DC8C0BD54115451ED9CD9631FCD38B5F07DC8`.
- After verification, exact Gradle intermediates, generated sources, Kotlin/temp output,
  mapping/debug-symbol output, the ARM64 test executable, and every repo-local manual render were
  removed. The final APK and active ARM64 CMake cache were retained; free C: space increased by
  1,935,851,520 bytes (about 1.80 GiB). No source, external manual, save, or unrelated file was
  touched.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. This is a strong
  depth-upload/readback CPU-efficiency candidate when converted D24 surfaces are active, not a
  measured whole-game speed or wattage result. A future allowed matched Thor A/B must hold title,
  scene, save, caches, renderer, resolution, driver, layout, performance/fan mode, brightness, and
  duration constant, then record renderer-thread time, upload/readback frequency, frametimes,
  battery power, temperature, thermal slope, visual depth correctness, and stability.

## 2026-08-17 AArch64 SoundTouch FIR

- SoundTouch documents `LONG_SAMPLETYPE` as its 32-bit integer accumulation type, but defined it as
  C++ `long`. That is 32-bit under Windows LLP64 and 64-bit under Android AArch64 LP64. Final ARM64
  code therefore ran the 64-tap stereo anti-alias FIR as scalar `LDRSH`/`SMADDL` with two 64-bit
  accumulators: 25 inner instructions repeated 32 times, or about 800 core inner instructions per
  output stereo frame.
- `LONG_SAMPLETYPE` is now explicitly `int32_t`. The AArch64 stereo loop also reads the canonical
  coefficient table once for both channels instead of loading the table that duplicates every
  coefficient for older generic SIMD compilers. Non-AArch64 builds retain that generic duplicated
  table path. The 64 taps, signed products, arithmetic result shift by 14, signed-16 saturation,
  scalar remainder, and output count are unchanged.
- The complete relevant pages were visually checked in the external Cortex-X3 issue 4.0 guide
  (pages 26-28 and 33), Cortex-A715 issue 5.0 guide (pages 28-29 and 36), Cortex-A710 issue 4.0
  guide (pages 42-43 and 55), and Cortex-A510 issue 6.0 guide (pages 35-36 and 46). They cover the
  emitted `ADDV`, `SMLAL`/`SMLAL2`, multiply-accumulate dependency behavior, and Q-form `LD2`.
  This directly drove the choice to keep eight independent accumulation vectors and remove two
  unnecessary structured coefficient loads per sixteen taps.
- Final linked ARM64 code handles sixteen taps with one paired coefficient load, two sample `LD2`,
  eight `SMLAL`/`SMLAL2`, and loop control. Its 17-instruction inner body repeats four times, or
  68 instructions per output frame: 91.5% fewer than the roughly 800-instruction scalar baseline.
  The first 32-bit auto-vectorized form was 24 instructions repeated four times; using the canonical
  coefficients removes another 28 instructions per output frame (29.2%) and cuts coefficient reads
  from 256 to 128 bytes. Total sample-plus-coefficient input traffic falls from 512 to 384 bytes per
  output frame. The full function grows from 336 to 444 bytes to hold vector and remainder paths.
- A coefficient sweep across 100,001 cutoffs from 0 through 0.5 found a maximum absolute 64-tap
  coefficient sum of 36,421. Even full-scale signed-16 input bounds accumulation at 1,193,443,328,
  below `INT32_MAX`. Permanent Catch2 coverage independently designs the same 64-tap Hamming/sinc
  filters at cutoffs 0.2, 0.391755, and 0.5; it compares every stereo output to a 64-bit scalar
  reference, checks every tested sum fits `int32_t`, exercises signed-16 extremes, and guards both
  ends of the destination buffer.
- A standalone Windows build of the real SoundTouch FIR passed the same reference vectors and was
  deleted afterward. The complete Android ARM64 test executable and final ThinLTO library compiled
  and linked successfully; the linked `evaluateFilterStereo` retains the audited 444-byte NEON
  body. The ARM64 tests were not executed because device use remains forbidden.
- `:app:assembleVanillaRelWithDebInfoLite` passed. The resulting 28,966,279-byte APK contains only
  `arm64-v8a` libraries and has SHA-256
  `DADBA13F988DC6E5E614C814BEF197E96074A4B6E1B67D763F53AAE90BE05F30`.
- After verification, exact Gradle intermediates, downloaded JNI copies, Kotlin/temp output,
  mapping/debug-symbol output, the 444,568,360-byte ARM64 test executable, host-verifier files, and
  repo-local manual renders were removed. The final APK and active ARM64 CMake cache were retained;
  free C: space increased by 2,018,377,728 bytes (about 1.88 GiB). No source, external manual, save,
  or unrelated file was touched.
- This targets SoundTouch's anti-alias filter while time stretching/rate transposition is active.
  It is a large local instruction and memory-traffic reduction, not evidence of a whole-game FPS
  or battery-watt gain. A future allowed matched Thor A/B must hold title, scene, save, caches,
  renderer, resolution, driver, layout, performance/fan mode, brightness, and duration constant,
  then record DSP/audio-thread time, audio underruns, frametimes, battery power, temperature,
  thermal slope, output correctness, and stability.

## 2026-08-17 AArch64 SoundTouch WSOLA Correlation

- SoundTouch's integer WSOLA code explicitly scales its correlation to avoid overflowing a 32-bit
  register, and its adaptive normalizer thresholds top out at 1.6 billion. Nevertheless, it used
  C++ `long`/`unsigned long` for `corr`, `lnorm`, and `maxnorm`: 32 bits on Windows LLP64 but 64
  bits on Android AArch64 LP64. The linked Android loops consequently widened every shifted 32-bit
  lane into 64-bit accumulators even though the pair products and shifts were already 32-bit.
- Those values are now exact `int32_t`/`uint32_t`. The code keeps SoundTouch's paired product/shift
  arithmetic and adaptive thresholds unchanged. Android AArch64 Clang is limited to one vector
  interleave group: its unrestricted 32-bit lowering processed sixteen frames per iteration but
  spilled/restored callee-saved `d8`; the selected eight-frame loop exposes four independent 4S
  accumulators and has no stack or vector-register spill.
- The complete relevant pages were visually checked in the external Cortex-X3 issue 4.0 guide
  (pages 26-28), Cortex-A715 issue 5.0 guide (pages 28-29), Cortex-A710 issue 4.0 guide (pages
  42-43), and Cortex-A510 issue 6.0 guide (pages 35-36). Their basic/widening arithmetic,
  `SMULL`/`SMLAL`, reduction, dependency, latency, and throughput tables drove the decision to
  retain multiple independent 32-bit chains and defer `ADDV` until after the loop.
- Final linked `calcCrossCorr` shrinks from 464 to 416 bytes (10.3%). Its core loop falls from 24
  to 20 instructions per eight stereo frames (16.7%); at a 512-frame overlap, that is 1,536 to
  1,280 inner instructions, saving 256 for the initial correlation window.
- Final linked `calcCrossCorrAccumulate`, used at every subsequent full-search offset, shrinks from
  1,004 to 788 bytes (21.5%). Its correlation body falls from 30 instructions per sixteen frames
  to 12 per eight, or 24 per sixteen (20%); at a 512-frame overlap, that is 960 to 768 inner
  instructions, saving 192 per tested search offset. The final body is exactly two `LD2`, four
  `SMULL`/`SMLAL`, two vector shifts, two vector adds, loop control, and a deferred `ADDV`.
- Permanent Catch2 coverage uses independently generated signed samples and checks the real
  16-, 256-, and 1024-frame overlap configurations across an initial correlation plus nine rolling
  offsets. It asserts every tested correlation, normalizer, and delta fits the intended width and
  models SoundTouch's important rounding detail: initial norm shifts paired squares, while rolling
  updates shift the outgoing/incoming samples individually. An optimized Windows build of the
  real SoundTouch sources passed the same scalar differential algorithm and was removed afterward.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` compiled and linked the permanent test and production
  ThinLTO library. `:app:assembleVanillaRelWithDebInfoLite` then passed in 2 minutes 19 seconds. The
  resulting 28,965,755-byte APK contains only `arm64-v8a` libraries and has SHA-256
  `E8DD5F641E9DDFB4E1A949ADDAFFA5D9DD82CC7F3B923F60ADC6B61D29A33DF9`.
- After verification, exact Gradle intermediates and downloaded JNI copies, mapping/debug-symbol
  output, the 444,622,688-byte ARM64 test executable, host-verifier objects, and repo-local manual
  renders were removed. The APK and active ARM64 CMake cache were retained; net free C: space
  increased by 1,050,345,472 bytes (about 0.98 GiB). No source, external manual, save, or unrelated
  file was touched.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. These are exact
  path-local instruction/code-size reductions, not a whole-game speed or wattage claim. A future
  allowed Thor A/B should hold title, scene, save, caches, renderer, resolution, driver, layout,
  performance/fan mode, brightness, audio backend, speed limit, and duration constant, then record
  DSP/audio-thread time, audio underruns, frametimes, battery power, temperature, thermal slope,
  output correctness, and stability.

## 2026-08-17 SoundTouch Pure-Tempo Rate-Transposer Bypass

- Azahar's `TimeStretcher` changes only tempo; it explicitly holds pitch and playback rate at
  exact `1.0`. SoundTouch's own algorithm documentation says tempo control is implemented purely
  by time stretching, while rate transposition exists for playback-rate and pitch changes.
  Nevertheless, the generic crossover-safe `putSamples()` path sent unity-rate input through
  RateTransposer before TDStretch: a 64-tap anti-alias FIR at cutoff 0.5, linear interpolation at
  rate 1.0, and several intermediate FIFO transfers. See the upstream
  [SoundTouch algorithm description](https://soundtouch.surina.net/README.html#about-algorithms).
- A new default-off `SETTING_BYPASS_RATE_TRANSPOSER_AT_UNITY` restores the documented pure-tempo
  topology only for clients that opt in before processing. Azahar enables it in the
  `TimeStretcher` constructor. Explicit topology changes are rejected once input/output accounting
  or TDStretch buffers are live; leaving exact unity effective rate automatically disables it.
  Default generic SoundTouch rate/pitch crossover behavior is unchanged, while `clear()` and
  `flush()` retain the setting for Azahar's continuing pure-tempo stream. Initial latency now
  excludes the unused transposer's 32-sample FIR delay.
- Final ARM64 ThinLTO shows the enabled `SoundTouch::putSamples()` branch loading the setting flag
  and tail-calling `TDStretch::putSamples` directly. It executes no call to RateTransposer,
  `FIRFilter::evaluateFilterStereo`, or `InterpolateLinearInteger::transposeStereo`. The disabled
  branch retains all existing generic behavior.
- In steady state this removes the already optimized FIR's 68 core instructions and 384 bytes of
  logical input reads per output stereo frame, plus the unity interpolator's 32-instruction loop,
  eight sample bytes read, and four bytes written. Removing the following FIFO transfer saves
  another four-byte read/write. The initial RateTransposer input copy is replaced by TDStretch's
  direct input copy, so the net path-local reduction is about 100 DSP instructions, 396 logical
  read bytes, and 12 intermediate write bytes per stereo frame. These are instruction/load counts,
  not estimates of physical DRAM traffic.
- Permanent Catch2 coverage feeds 24,000 deterministic signed-16 stereo frames at tempos 0.72,
  0.93, and 1.08 through chunk sizes from one to 1,024 frames. With x86 extensions disabled for a
  portable host reference, the bypass output matches a standalone TDStretch stage byte-for-byte
  after every chunk. It also checks TDStretch input backlog, reduced latency, flush, clear,
  setting persistence, rejection of explicit mid-stream topology changes, and automatic disable at
  rate 1.01.
- A separate optimized Windows verifier passed the same differential checks. Its five-round,
  order-alternated 192,000-frame microbenchmark measured median generic SoundTouch processing at
  48.70 ms before and 38.97 ms with the bypass, or 1.250x isolated throughput. This x64 result
  validates that material work disappeared but is not a Thor, game, FPS, or wattage measurement.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` compiled and linked the production path, permanent
  test, and ThinLTO library after lifecycle hardening in 1 minute 2 seconds. The ARM64 test
  executable was not run on this x64 host because device use remains forbidden.
- `:app:assembleVanillaRelWithDebInfoLite` then produced an ARM64-only APK successfully. Final
  package: `app-vanilla-relWithDebInfoLite.apk`, 28,966,067 bytes, SHA-256
  `8721FB2078B65E0BF03E342E44026E35F80E89DF78D7D90350EDF658AE9436EF`.
- Post-verification cleanup retained that APK and the active ARM64 RelWithDebInfo CMake cache while
  deleting the temporary native verifier/objects, 445 MB ARM64 test ELF, Gradle intermediates,
  downloaded JNI staging, mapping, native-symbol, and other reproducible package trees. The build
  tree fell from 2,041,610,960 to 28,966,543 bytes, the retained CMake tree fell from 3,236,646,241
  to 2,786,013,944 bytes, and reported C: free space rose by 2,034,778,112 bytes.
- No device, ADB, install, launch, game, FPS run, or battery measurement was used. A future allowed
  Thor A/B should hold title, scene, save, caches, renderer, resolution, driver, layout,
  performance/fan mode, brightness, audio backend, speed limit, and duration constant, then record
  DSP/audio-thread time, audio underruns, frametimes, battery power, temperature, thermal slope,
  output correctness, and stability.

## 2026-08-17 AArch64 PICA Register-Only Source Swizzles

- The CPU PICA shader JIT previously handled identity, four broadcasts, and twelve single-lane
  substitutions directly. Every other selector loaded a 16-byte byte-index literal into a scratch
  vector and executed `TBL`. That extra data load and table dependency execute for each affected
  source operand on every software shader invocation.
- The actual Cortex-X3 instruction tables list element `DUP`, `EXT`, element `INS`, `REV64`,
  `TRN`, `ZIP`, and `UZP` at latency 2 and throughput 4 instructions/cycle, while one-table `TBL`
  has latency 2 and throughput 2. Cortex-A715 and A710 list both simple permutations and one-table
  `TBL` at latency 2 and throughput 2. Cortex-A510 lists the simple operations at latency 3 and
  one-table `TBL` at latency 4. The old path additionally depended on the index-literal load, so a
  register-only sequence removes data-cache work on all four Thor core classes.
- A compact compile-time planner models 26 exact operations: three rotations with `EXT`, one
  `REV64`, both `ZIP`/`UZP`/`TRN` halves, four lane broadcasts, and twelve lane moves. Composing at
  most two operations covers exactly 149 selectors: one identity, 26 one-operation plans, and 122
  two-operation plans. The other 107 selectors keep the exhaustive literal `LDR` plus `TBL` path.
- Relative to the prior emitter, 10 additional selector values shrink from two generated
  instructions to one. Another 122 retain two generated instructions but replace the literal load
  and `TBL` with two register permutations. If one shader used every newly covered selector, its
  unique literal pool would be 2,112 bytes smaller; real savings depend on each shader's selector
  distribution because literals are shared by selector within a compiled shader.
- Compile-time assertions compose and compare every accepted plan against its exact eight-bit PICA
  selector, lock the `1/26/122/107` distribution, and reject plans longer than two operations. The
  permanent `All Source Swizzles` generated-shader test covers all 256 selectors and the Android
  ARM64 build compiles it along with the production emitter.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` rebuilt and linked the production emitter and ARM64
  test executable successfully in 1 minute. Final ThinLTO retains a 768-byte plan table, a
  104-byte operation table, and the register-permutation emitter. The test executable was not run
  on this x64 host because device use remains forbidden.
- `:app:assembleVanillaRelWithDebInfoLite` produced an ARM64-only package successfully. Final APK:
  `app-vanilla-relWithDebInfoLite.apk`, 28,966,315 bytes, SHA-256
  `895095A30723E9F3FB1A7106B05DCE58EAB44EBE65A6B03C4FB9DEAE66DEB46A`.
- Post-verification cleanup retained that APK and the active ARM64 RelWithDebInfo CMake cache while
  removing the temporary baseline library, 445 MB ARM64 test ELF, Gradle intermediates, downloaded
  JNI staging, mapping, native-symbol, and other reproducible package trees. The build tree fell
  from 2,041,677,559 to 28,966,791 bytes, the retained CMake tree fell from 3,236,885,695 to
  2,786,232,198 bytes, and reported C: free space rose by 2,058,338,304 bytes.
- Scope is deliberately narrow: normal draws that successfully use hardware vertex shaders bypass
  this CPU JIT. The reduction applies to immediate-mode draws, geometry-shader work, and batches
  that fall back to software vertex processing. No whole-game FPS or battery-watt gain is claimed
  without a controlled Thor A/B. No device, ADB, install, launch, game run, or battery measurement
  was used for this slice.

## 2026-08-17 AArch64 Linear RGB8 Table-Width Removal

- Converted linear RGB8 upload/download still used four-register table lookup for every vector
  conversion. Each sixteen-pixel decode loaded 48 packed BGR bytes, appended an opaque vector, and
  issued four `TBL4` operations to write 64 RGBA bytes. Encode loaded 64 RGBA bytes and issued
  three `TBL4` operations to pack 48 BGR bytes. This runs in the rasterizer-cache linear conversion
  tables; non-converted copies remain `memcpy` and Morton surfaces keep their separate tile paths.
- The complete relevant manual tables were checked in Cortex-X3 issue 4.0 pages 31-35,
  Cortex-A715 issue 5.0 pages 34-38, Cortex-A710 issue 4.0 pages 52-56, and Cortex-A510 issue 6.0
  pages 43-49. X3/A715/A710 list `TBL4` at latency 4 and throughput `2/3`, versus latency 2 and
  throughput 2 for `TBL2`. A510 lists `TBL4` at latency 16 and throughput `1/9`, versus latency 8
  and throughput `2/5` for `TBL2`; its Q-form byte `LD3` is latency 5, throughput `1/3`, and ZIPs
  are latency 3. The PDFs remain external and uncommitted.
- Decode now performs one exact Q-form `LD3` over the complete 48-byte source block. It reverses
  BGR component order, inserts `0xFF` alpha, and emits sixteen RGBA pixels with the existing
  ZIP/store helper. This removes all four `TBL4` operations, four 16-byte shuffle-mask
  loads, and 64 bytes of mask data. It does not over-read the source or approximate any color math.
- Encode proves that each 16-byte packed output block touches only two adjacent Q input vectors.
  Three overlapping two-vector tables therefore retain the same four ordinary Q loads, three
  ordinary Q stores, and twelve-instruction loop while changing all three lookups from `TBL4` to
  `TBL2`. A compile-time proof checks every one of the 48 output indices against exact
  `pixel * 4 + 2 - component` RGBA-to-BGR selection and guarantees every local index is below 32.
- From the manuals' steady issue rates, the encode lookup-only budget falls from 4.5 to 1.5 cycles
  per sixteen pixels on X3/A715/A710 and from 27 to 7.5 cycles on A510. Decode removes a four-`TBL4`
  lookup budget of 6 cycles on the performance cores or 36 cycles on A510, replacing it with
  structured load, simple ZIP, and store work on their corresponding pipelines. These are
  instruction-class issue bounds, not measured loop latency, FPS, or watts.
- Isolated Android-clang `-O3` codegen confirms decode contains `LD3`, ZIPs, and no `TBL`, while
  encode contains three `TBL2` instructions and no `TBL3`/`TBL4` or extra loop instructions. The
  two wrappers' `.text` plus shuffle data fall from 336 to 268 bytes; decode code shrinks from 120
  to 116 bytes, encode stays 104 bytes, and shuffle data falls from 112 to 48 bytes.
- The permanent 37-pixel Catch2 case checks both directions, exact BGR/RGBA component order, opaque
  alpha, two vector iterations, a five-pixel scalar tail, and source/destination canaries. The
  compile-time proof and full ELF64/AArch64 test executable linked successfully with production
  ThinLTO in 1 minute 35 seconds; the final rebuild after strengthening the local-index proof passed
  in 1 minute 10 seconds. Final linked `LinearCopy<..., RGB8, true>` bodies retain the exact intended
  `LD3`/ZIP and three-`TBL2` loops. The ARM64 executable was not run on this x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` then passed in 1 minute 27 seconds and produced an
  ARM64-only 28,966,375-byte APK with SHA-256
  `4490B56AB6749AB2D5B81B87246B56E8F3723571F181ED0CE573E023DEE294E0`. After verification,
  2,463,355,063 logical bytes of disposable intermediates, test binaries, and shuffle-codegen
  scratch were removed. C: free space increased by 2,018,582,528 bytes; the APK and active ARM64
  CMake cache remain in the repository workspace.
- No device, ADB, install, launch, game run, FPS test, or battery measurement was used. A future
  allowed matched Thor A/B should record RGB8 linear conversion frequency and renderer-thread time
  alongside frametimes, battery power, temperature, thermal slope, and visual correctness.

## 2026-08-17 AArch64 16-bit Encode Band Fusion

- Converted RGB5A1, RGB565, and RGBA4 encode already handled sixteen pixels per vector body, but
  prepared each eight-pixel half independently. Linear conversion issued two D-form byte `LD4`
  operations and duplicated masks, shifts, widening, and field assembly. Morton conversion needed
  the two loads because its rows can be separated by padded stride, but still duplicated most of
  the arithmetic.
- The complete relevant tables were checked in Cortex-X3 issue 4.0 pages 31-37, Cortex-A715 issue
  5.0 pages 34-40, Cortex-A710 issue 4.0 pages 52-59, and Cortex-A510 issue 6.0 pages 43-50.
  X3 lists D-form byte `LD4` at throughput 1 and Q-form at `1/2`; A715 lists both at `1/2`;
  A710 lists D-form at 1 and Q-form at `1/2`; A510 lists both at `1/3`. One Q-form load transfers
  the same 64 bytes as two D-form loads, so the linear structured-load issue budget stays two
  cycles on X3/A710, falls from four to two on A715, and falls from six to three on A510. The PDFs
  remain external and uncommitted.
- Encode now keeps all sixteen channel bytes in Q registers. Exact high-bit truncation is expressed
  before widening: RGB565 uses `(R & 0xF8) << 8`, `(G & 0xFC) << 3`, and `B >> 3`;
  RGB5A1 changes green to `(G & 0xF8) << 3` and assembles
  `((B >> 2) & 0x3E) + (A >> 7)`; RGBA4 uses `(R & 0xF0) << 8`,
  `(G & 0xF0) << 4`, and `(B & 0xF0) | (A >> 4)`. `SHLL`/`SHLL2` then produces both packed
  halves from each shared byte vector.
- Linear conversion reads the complete sixteen-pixel RGBA block with one Q-form `LD4` and writes
  both packed halves with one `STP`. Morton conversion retains the required D-form `LD4` for
  each non-contiguous row, combines matching channels, shares their byte preparation, and preserves
  the exact two `ST2` tile stores. Neither path over-reads padding or changes the scalar fallback.
- Isolated Android-Clang 18 `-O3` output for a sixteen-pixel encode falls from 25 to 18
  instructions for RGB565 (100 to 72 bytes), 30 to 20 for RGB5A1 (120 to 80 bytes), and 24 to 17
  for RGBA4 (96 to 68 bytes): 28.0%, 33.3%, and 29.2% fewer instructions. This is code shape, not a
  claim that the whole loop or game is faster by those percentages.
- Production ThinLTO confirms one Q-form `LD4`, Q masks/field assembly, `SHLL`/`SHLL2`, and one
  paired Q store in every linear vector loop. Final RGB5A1/RGB565/RGBA4 linear encode symbols shrink
  from 276/252/256 to 232/220/224 bytes, reductions of 15.9%, 12.7%, and 12.5%. Their full Morton
  encode symbols shrink from 1,004/944/932 to 956/912/912 bytes, reductions of 4.8%, 3.4%, and
  2.1%, while retaining the two row loads and exact tile stores.
- An independent 132,608-case component/paired-component algebra check found zero mismatches.
  Permanent Catch2 source exhaustively round-trips all 65,536 packed values for each format through
  Morton encode/decode and separately covers 37-pixel linear vector bodies, scalar tails, and
  canaries. The complete ELF64/AArch64 test executable and production shared library compiled and
  linked successfully with ThinLTO in 1 minute 34 seconds. The ARM64 executable was not run on this
  x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 2 minutes 43 seconds and produced an ARM64-only
  28,966,475-byte APK with SHA-256
  `852CD2B4A43DAAEAD8A2381ADDF262499AD06B38AD6B2DED63782703C361231E`. After verification,
  2,463,352,987 logical bytes of scratch, test binaries, and disposable package intermediates were
  removed. C: free space increased by 2,018,557,952 bytes; the APK and active ARM64 CMake cache
  remain in the repository workspace.
- Command-line Git over SSH refreshed `upstream/master` to `32a3c0bfd`; this fork already
  contained it and remained 93 commits ahead with no upstream-only commit. No device, ADB, install,
  launch, game run, FPS test, or battery measurement was used for this slice.

## 2026-08-17 AArch64 GC-ADPCM Nibble Decode

- HLE source buffers use the GameCube-style ADPCM decoder for fourteen sequential samples in each
  eight-byte frame. The original implementation mapped each high and low four-bit value through a
  sixteen-entry `int` table. Final AArch64 ThinLTO showed two reads of every packed source byte and
  two indexed 32-bit nibble-table loads in the repeated two-sample body: 28 data loads per complete
  frame just to obtain fourteen compressed nibbles.
- The complete relevant instruction pages were checked in Cortex-X3 issue 4.0 page 18,
  Cortex-A715 issue 5.0 page 20, Cortex-A710 issue 4.0 pages 27-28, and Cortex-A510 issue 6.0 pages
  22-23. X3/A715/A710 list basic `SBFM` at one-cycle latency and throughput 6/4/4 respectively;
  A510 lists `SBFX` at two-cycle latency and throughput 3. Direct bitfield sign extension also
  removes the indexed address work and L1 data accesses. The PDFs remain external and uncommitted.
- Decode now reads one packed byte, retains it across the high-nibble result and recurrent state
  update, and sign-extends both four-bit fields directly. Scale, coefficient pair, fixed-point add
  order, high-before-low history dependency, signed clamp, duplicate stereo stores, partial-frame
  behavior, and the historical padded second output/state update for odd sample counts are
  unchanged. This is scalar AArch64 acceleration because the second-order recurrence prevents
  time-lane SIMD without changing the algorithm.
- Production ThinLTO changes the repeated two-sample body from 50 to 46 instructions. A full
  fourteen-sample frame therefore removes 28 inner-loop instructions, fourteen indexed table
  loads, and seven redundant packed-byte loads. Two one-time table-address setup instructions also
  disappear per decoder call. `DecodeADPCM()` shrinks from 500 to 476 bytes (4.8%), and its separate
  64-byte `SIGNED_NIBBLES` constant is removed. Final code retains one post-indexed `LDRSB`, direct
  `SBFX`/bitfield scaling, the dependent `MADD` chains, exact clamp selects, and duplicate stores.
- An independent 512-case byte/nibble sweep found zero differences from the former table. New
  permanent Catch2 coverage compares the complete decoder against an independent table-based
  reference across sixteen data phases, twelve lengths from zero through nine frames, four initial
  histories, every scale/coefficient pair, clipping values, partial frames, and odd sample counts:
  768 complete decode/state comparisons. The full ELF64/AArch64 test executable and production
  shared library compile and link successfully; the executable is not run on this x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 22 seconds and produced an ARM64-only
  28,966,955-byte APK with SHA-256
  `949C8F7851C87CF324B482249D7DBAD00EA6A5E6F2DC3BC73CC4DD36910C5F9D`. After verification,
  2,496,851,053 logical bytes of the temporary disassembly, native test executable, and disposable
  package intermediates were removed. C: free space increased by 2,059,210,752 bytes; the APK and
  active ARM64 CMake cache remain in the repository workspace.
- This reduces HLE DSP-thread work for games that stream GC-ADPCM. It is not a whole-game FPS or
  battery-watt measurement, and no device, ADB, install, launch, or game run was used. A future
  allowed matched Thor A/B should instrument ADPCM-decoded samples and DSP-thread time while
  holding the normal title, scene, renderer, driver, display, thermal, and power controls fixed.

## 2026-08-17 Exact-Unity HLE Linear Resampler Bypass

- HLE Linear resampling still entered its full stereo AdvSIMD interpolation body when the requested
  rate was exactly `1.0f` and the Q24 phase had no fractional bits. In that state the step is one
  complete input sample, every subsequent fraction remains zero, and the existing saturated linear
  formula returns `x0` exactly. Running delta formation, Q24-to-Q31 conversion, `SQDMULH`, and result
  repacking cannot change the output.
- Linear now checks both necessary conditions once per call and tail-routes this case through the
  existing None implementation. None uses the same `StepOverSamples()` traversal and therefore
  preserves output fill, monotonic deque-window advancement, consumed input, `xn2`/`xn1`, and final
  `fposition`. A fractional starting phase, including exact-unity calls restored from such a state,
  and every non-unity rate retain the unchanged Linear path.
- The first inlined experiment duplicated a complete copy loop into both template instances and was
  rejected after production ThinLTO grew None and Linear by 216 bytes each without improving None's
  repeated loop. The retained implementation shares the already optimized loop: None stays 368
  bytes and Linear grows from 408 to 448 bytes for its two predicates and tail route.
- Final AArch64 disassembly shows the rate compare and low-24-bit phase test before a tail branch to
  None. The general path still contains the exact `SSUBL`/`SQXTN`/`SSHLL`/`SQDMULH` sequence. For
  every sample on the routed path, the copied output body omits two `FMOV`, `UBFIZ`, `DUP`, `SSUBL`,
  `SQXTN`, `SSHLL`, `SQDMULH`, `SADDW`, and `UZP1`: ten interpolation/packing instructions per
  output, amortized against one small dispatch per resampler call. This is stage elimination proven
  from the exact Q24 arithmetic and linked code rather than an instruction-latency estimate.
- Existing independent scalar-reference Catch2 coverage exercises None and Linear across rates
  `0.25`, `0.5`, `0.9999`, `1.0`, `1.25`, and `2.75`; five starting fractions including zero;
  partial/full output positions; tiny inputs; and exact state/input/output comparison. The complete
  ELF64/AArch64 test executable and production shared library compiled and linked successfully in
  54 seconds. The ARM64 executable was not run on this x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 19 seconds and produced an ARM64-only
  28,966,763-byte APK with SHA-256
  `FEE9145F766D7279D10E3B5E012A8B0DF1E14A8A11AEBE50AB3FA69C58659048`. After verification,
  2,457,557,368 logical bytes of the native test executable and disposable package intermediates
  were removed. C: free space increased by 2,019,803,136 bytes; the APK and active ARM64 CMake
  cache remain in the repository workspace.
- This reduces HLE DSP-thread work only for exact-unity, aligned Linear sources. It is not a
  whole-game FPS or battery-watt measurement, and no device, ADB, install, launch, game run, or
  battery measurement was used. A future allowed matched Thor A/B should instrument how many
  sources take the route and record DSP-thread time/placement, underruns, frametimes, battery power,
  temperature, and thermal slope with title, scene, renderer, driver, display, and fan mode fixed.

## 2026-08-17 Sequential HLE PCM Decode Output

- PCM8 and PCM16 decode allocate a `StereoBuffer16` deque and fill it in strict sample order before
  the source resampler consumes from the front. The old loops nevertheless used indexed
  `deque::operator[]` for every output. Final AArch64 ThinLTO recalculated the logical start plus
  index, shifted/masked it into a block number and offset, loaded the deque block-map entry, and only
  then formed the destination address for each sample.
- Decode now obtains the output iterator once and advances it with a counted loop. PCM8 still maps
  each unsigned byte into the high byte of signed 16-bit output. PCM16 still performs native
  little-endian unaligned-safe loads. Mono duplicates exactly into both lanes, stereo keeps left/
  right order, the returned deque size is unchanged, and no input byte or output format changes.
- The four Thor core manuals confirm why removing the map dependency matters even for an L1 hit.
  Cortex-X3 pages 18-19, Cortex-A715 pages 20-21, and Cortex-A710 pages 28-29 list ordinary integer
  load latency 4 and throughput 3; Cortex-A510 pages 23-24 list latency 2 and throughput 2. These are
  the manuals' L1-hit figures. The retained code does not depend on those estimates: linked output
  directly proves that the destination pointer now advances between samples and the deque map no
  longer reconstructs each store address. Three loops retain the current block base across samples;
  PCM16 stereo's remaining per-sample base load only checks the boundary and is not on the store-
  address dependency chain.
- Production ThinLTO changes the repeated PCM8 mono loop from 12 to 10 instructions (16.7%) and
  stereo from 14 to 13 (7.1%). PCM16 mono falls from 11 to 9 (18.2%) and stereo from 11 to 8
  (27.3%). Per-iteration data loads fall from 2/3/2/4 to 1/2/1/2 in the same order. Over 160 decoded
  samples this removes 160-480 loop instructions and 160-320 data loads, depending on format and
  channel count, before the rare block transition. PCM8 grows from 240 to 288 bytes and PCM16 from
  228 to 268 bytes; the 88-byte total code-size trade avoids repeatedly executing the indexed path.
- New permanent Catch2 coverage independently generates PCM8 and little-endian PCM16 inputs for
  mono and stereo, verifies both output lanes, and covers counts 0, 1, 7, 159, 1023, 1024, 1025,
  and 2049. This crosses the Android libc++ 1024-element/4 KiB deque boundary and a second block.
  The complete ELF64/AArch64 test executable and production shared library compile and link
  successfully in 58 seconds; the executable is not run on this x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 28 seconds and produced an ARM64-only
  28,966,767-byte APK with SHA-256
  `1F08973FEFA3460C1F1D220F26622A097BC70DBCC921B1C05F569D92DBDA8CF3`. After verification,
  2,457,624,332 logical bytes of the native test executable and disposable package intermediates
  were removed. C: free space increased by 2,019,819,520 bytes; the APK and active ARM64 CMake
  cache remain in the repository workspace.
- This reduces HLE audio decoding work when a title submits PCM8 or PCM16 buffers. It is not a
  whole-game FPS or battery-watt measurement, and no device, ADB, install, launch, game run, or
  battery measurement was used. A future allowed matched Thor A/B should count decoded PCM samples
  and record DSP-thread time/placement, underruns, frametimes, battery power, temperature, and
  thermal slope with the usual title, scene, renderer, driver, display, and fan controls fixed.

## 2026-08-17 Tail-Only HLE Source Frame Silence

- `Source::GenerateFrame()` previously called `memset(current_frame, 0, 640)` before checking the
  source buffer. During normal playback, None, Linear, and the Polyphase placeholder all fill the
  produced prefix through the same resampler traversal. A complete 160-sample frame therefore
  overwrote all 640 bytes immediately, making the initial clear pure write-before-write traffic.
- Generation now clears the whole frame only on empty entry, before either dequeue or disable can
  return a silent frame. A running source starts with its previous contents, lets the resampler
  overwrite `[0, frame_position)`, and clears `[frame_position, 160)` only after an underrun. The
  tail clear remains before sample accounting and `SourceFilters::ProcessFrame()`, so recurrent
  filters still observe the exact same zero padding and histories. Reset, sleep/wakeup, buffer
  state, resampler state, and sample-count behavior are unchanged.
- At the native 32,728 Hz rate, a DSP frame runs about 204.55 times per second. The maximum 24
  active sources formerly wrote 15,360 redundant bytes per tick, or 3,141,888 bytes per second,
  before producing the real samples. This is modest DRAM bandwidth but continuous avoidable store,
  L1/cache-line, and dirty-data work on the DSP thread.
- Baseline production AArch64 ThinLTO emitted a 392-byte `GenerateFrame()` with an unconditional
  entry `memset` of `0x280` bytes. The retained 440-byte function branches around that call when
  `current_buffer` is nonempty; a full 160-sample result reaches accounting/filtering with no clear.
  The empty path still passes `0x280`, and the underrun path computes exactly `640 - 4 *
  frame_position` bytes. The 48-byte code-size increase retains one extra integer register and the
  two correctness paths in exchange for removing the large call and stores from steady playback.
- Permanent Catch2 coverage dirties the previous frame and checks three cases independently: a
  complete frame overwrites every sample, a 35-sample underrun preserves its produced history/input
  prefix and zeros the complete tail, and an empty source returns all-zero output while disabling
  itself. Sample accounting and enabled state are checked as well. The production shared library
  and complete ELF64/AArch64 test executable compile and link successfully; the executable is not
  run on this x64 host, and no Thor/device/ADB action was used.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 27 seconds and produced an ARM64-only
  28,967,359-byte APK with SHA-256
  `E415F1AC895877AED56896AA4AF4A0F9E7E263F7C67EB7466688A974113FE21C`.
- After verification, 2,334,759,497 logical bytes of the native test executable and disposable
  Gradle intermediates were removed. C: free space increased by 1,896,480,768 bytes; the final APK
  and active ARM64 CMake cache remain in the repository workspace.
- This change removes continuous source-generation writes but is not a whole-game FPS or wattage
  result. A future allowed Thor A/B should count active/full/partial sources and record DSP-thread
  time and placement, underruns, frametimes, battery power, temperature, and thermal slope with the
  title, scene, renderer, driver, resolution, display layout, fan, and performance mode fixed.

## 2026-08-17 First-Audible-Bus Final Mix

- `Mixers::MixCurrentFrame()` previously cleared its complete 640-byte stereo output before
  examining three intermediate buses. The first audible bus then loaded those zeros through
  twenty Q-form `LD2` instructions and executed forty lane-wise saturating adds. Each bus
  contribution is already independently clamped to signed 16-bit, so its saturating addition to
  known zero is exactly the contribution itself.
- The mixer now skips leading signed-zero buses and lets the first audible main or auxiliary bus
  define the output directly. Every nonzero or NaN gain still takes the arithmetic path. Later
  audible buses retain the original per-bus clamp followed by saturating accumulation, preserving
  order-dependent clipping. If all three buses are silent, the complete output is cleared so an
  audible previous frame cannot leak. Aux send/return, persistent intermediate buffers, Mono,
  Stereo, and Surround-as-Stereo behavior are unchanged.
- The AArch64 Stereo and Mono downmixers use compile-time direct and accumulated variants, with the
  choice made once outside the 160-sample loop. The non-AArch64 scalar path implements the same
  distinction. `MixCurrentFrame()` is deliberately `CITRA_NO_INLINE`; without that barrier ThinLTO
  duplicated the full mixer into `Tick()`, while the retained form keeps `Tick()` at its baseline
  236 bytes.
- Production ThinLTO immediately before the change emitted common inlined Stereo/Mono bodies of
  40/38 instructions per eight samples. The direct first-main-bus bodies are now 36/35, with no
  output `LD2` or `SQADD`, removing 80/60 repeated instructions per 160-sample frame. This also
  removes the 640-byte initial clear and 640 bytes of output reloads: 1,280 bytes per frame, or
  261,824 bytes/second at 32,728 Hz. Later accumulated Stereo/Mono paths remain 38/36 instructions
  and retain their output load plus two saturating adds.
- The code-size trade is 372 bytes across the outlined implementation: `MixCurrentFrame()` grows
  by 80 bytes and the downmix dispatcher by 292 bytes. That prevents work in the common path at a
  small instruction-cache cost while avoiding the much larger ThinLTO duplication into `Tick()`.
- Focused Catch2 coverage retains exact scalar saturation checks for Mono, Stereo, Surround, and
  multiple active buses, and adds first-audible auxiliary/final-bus cases plus an audible frame
  followed by an all-silent frame for both Mono and Stereo. The production shared library and full
  ELF64/AArch64 test executable compile and link successfully; the executable was not run on this
  x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 22 seconds and produced an ARM64-only
  28,966,155-byte APK with SHA-256
  `EB4E9DA3446929B240BDA3CFC97D8818160C5869B526C7DC67F42F06B7B0AC8F`.
- After verification, 2,334,803,746 logical bytes of the native test executable and disposable
  Gradle intermediates were removed. Reported C: free space increased by 1,818,783,744 bytes; the
  final APK and active ARM64 RelWithDebInfo CMake cache remain in the repository workspace.
- No device, ADB, install, launch, game run, FPS test, or battery measurement was used. This is an
  exact final-mixer instruction and memory-traffic reduction, not a whole-game speed or wattage
  result. A future allowed Thor A/B should hold title, scene, save, caches, renderer, resolution,
  driver, display layout, performance/fan mode, brightness, audio backend, speed limit, and
  duration constant, then record DSP/audio-thread time, audio underruns, frametimes, battery power,
  temperature, thermal slope, output correctness, and stability.

## 2026-08-17 Live-Input Final-Mixer Routing

- `Mixers::Tick()` previously staged every current 2,560-byte planar bus in
  `state.intermediate_mix_buffer` before final downmix. The main bus was always copied. Each
  disabled auxiliary bus was copied to state, while an enabled bus copied the ARM11 return to
  state and separately sent its new input to shared memory. Final mixing immediately read the
  staged main/disabled data back; no later operation consumed it.
- Main and disabled auxiliary buses now mix directly from the const input whose lifetime spans the
  complete tick. Enabled buses still mix the ARM11-returned state populated by `AuxReturn()`, and
  `AuxSend()` still writes their new input to shared memory. The three historical state-buffer
  slots remain serialized for archive compatibility. Their main/disabled values need not be
  refreshed: the current output is serialized separately, and the next tick bypasses those slots
  or overwrites an enabled return before use. Sleep/wakeup behavior is unchanged for the same
  reason.
- Baseline production AArch64 ThinLTO made three plus the number of enabled auxiliaries 2,560-byte
  `memcpy` calls per tick. The retained code makes two per enabled auxiliary and zero when both are
  disabled. This removes one to three state-staging copies every DSP frame: 5,120 to 15,360 bytes
  of load-plus-store traffic. At 32,728 Hz / 160 samples, that is 1,047,296 bytes/second with both
  auxiliaries enabled, 2,094,592 with one enabled, and 3,141,888 with both disabled.
- `Mixers::Tick()` shrinks from 236 to 188 bytes (20.3%) and `AuxSend()` from 136 to 108 bytes
  (20.6%). The outlined `MixCurrentFrame()` grows from 596 to 644 bytes to select live versus
  returned input, so the complete retained mixer-function set shrinks by 28 bytes. The all-disabled
  `Tick()` disassembly has no `memcpy`; enabled branches retain only the required return/send calls.
  The established Stereo/Mono direct-first-bus NEON loops and later saturating accumulation bodies
  are unchanged.
- Existing tests already cover all-disabled direct input and both-enabled ARM11 return/send
  routing. New mixed coverage enables aux 0 only, verifies main and disabled aux 1 use live input,
  verifies aux 0 mixes its ARM11 return and sends its new input, and proves disabled aux 1 shared
  output remains byte-for-byte untouched. The complete ELF64/AArch64 test executable and
  production ThinLTO shared library compile and link successfully in 1 minute 18 seconds; the ARM64
  executable was not run on this x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 28 seconds and produced an ARM64-only
  28,966,083-byte APK with SHA-256
  `F96E7B5E1F23C030770F666C3B041D0EF55910B902C2046E8A25027E5AE8C7FB`.
- Across packaging and the final post-format verification cleanup, 3,745,992,762 logical bytes of
  native test executables and disposable Gradle intermediates were removed. Reported C: free space
  increased by 2,859,163,648 bytes across the two cleanup passes; the final APK and active ARM64
  RelWithDebInfo CMake cache remain in the workspace.
- No device, ADB, install, launch, game run, FPS test, or battery measurement was used. The result
  is a proven continuous DSP memory-traffic reduction, not a whole-game speed or wattage claim. A
  future allowed Thor A/B should hold title, scene, save, caches, renderer, resolution, driver,
  display layout, performance/fan mode, brightness, audio backend, speed limit, and duration
  constant, then record aux-enable patterns, DSP/audio-thread time, underruns, frametimes, battery
  power, temperature, thermal slope, output correctness, and stability.

## 2026-08-17 Native Aux-Return Direct View

- The preceding live-input route still copied each enabled ARM11 auxiliary return from its shared
  `s32_le[4][160]` buffer into a persistent planar frame before immediately downmixing it. Android
  AArch64 is little-endian, so `s32_le` is native `s32`; the source lifetime covers `Tick()` and no
  ownership, alignment, or conversion boundary requires that staging copy.
- Native-endian final mixing now reads enabled returns through four independent channel pointers.
  This avoids undefined pointer traversal between nested-array subobjects. The generic non-native-
  endian path retains `CopySharedToPlanar()` and consumes the converted state buffer. Historical
  three-slot state serialization, enabled sends, aux selection, arithmetic order, saturation,
  sleep/wakeup behavior, and output serialization remain unchanged.
- The original mixer made three state-staging copies plus one required send for each enabled aux.
  The final native route makes only the zero, one, or two required sends. It therefore removes
  three 2,560-byte staging copies for every configuration: 15,360 bytes of load-plus-store traffic
  per DSP frame, or 3,141,888 bytes/second at 32,728 Hz / 160 samples. Relative to the preceding
  slice, this saves another 1,047,296 bytes/second with one enabled aux and 2,094,592 with both;
  the already-copy-free all-disabled path is unchanged.
- Production AArch64 ThinLTO shrinks `Mixers::Tick()` from 188 to 136 bytes and `AuxReturn()` from
  92 bytes to a 4-byte `RET`. `AuxSend()` remains 108 bytes. The pointer-view selection grows
  `MixCurrentFrame()` from 644 to 716 bytes and the retained downmixer from 712 to 776 bytes; the
  full retained set nevertheless falls from 1,840 to 1,836 bytes. Disassembly proves the four
  source pointers load once before the loop. The established direct-first-bus and accumulated NEON
  loop bodies retain their instruction counts and have no new spills.
- Existing all-disabled, both-enabled, and mixed enabled/disabled tests cover live main/disabled
  inputs, direct shared returns, required sends, and untouched disabled shared output. The complete
  ELF64/AArch64 test executable and production ThinLTO library compile and link successfully in
  1 minute 9 seconds; the ARM64 executable was not run on this x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 43 seconds and produced an ARM64-only
  28,966,523-byte APK with SHA-256
  `50255CCA1E44F0E646B8F3C3178C5D2952CD348E9CB989F32839CE6F2BC1538A`.
- After verification, 2,334,854,595 logical bytes of the native test executable and reproducible
  Gradle intermediates were removed. Reported C: free space increased by 1,896,767,488 bytes; the
  final APK and active ARM64 RelWithDebInfo CMake cache remain in the workspace.
- No device, ADB, install, launch, game run, FPS test, or battery measurement was used. This is a
  bounded always-on DSP memory-system reduction, not evidence for a specific whole-game FPS or
  battery-watt gain.

## 2026-08-17 First-Source Intermediate-Bus Definition

- `DspHle::Impl::GenerateCurrentFrame()` previously value-initialized all three 2,560-byte source
  buses before visiting the 24 HLE sources. The first source with any audible routing then loaded
  the known-zero destinations, added its converted samples, and stored the result. That source can
  instead define each bus it routes directly without changing later accumulation order.
- The complete three-bus set now starts pending. Until one source is audible,
  `Source::MixIntoFirst()` tests the same exact ending gains and ramp starts, direct-writes every
  bus that source routes, and returns a three-bit mask. The caller clears adjacent silent-bus runs
  immediately, marks the complete set initialized, and sends every later source through the
  original `MixInto()` accumulator. This deliberately gives up a later per-bus direct opportunity
  to avoid carrying recurring initialization checks across the remaining sources. An all-silent
  frame still performs one contiguous 7,680-byte `memset`. Signed zero remains silent; NaN and
  every nonzero start/end gain take arithmetic, and every ramp state advances exactly once.
- AArch64 direct full-bus steady/ramped loops are 38/60 instructions per eight samples versus
  52/74 for accumulation. They contain no destination loads or vector adds, saving 280 repeated
  instructions and 5,120 bytes of load/store traffic per 160-sample first contribution. That is
  1,047,296 bytes/second at 32,728 Hz. Direct front-stereo loops are 26/40 versus 32/46, saving 120
  instructions and 2,560 bytes per frame, or 523,648 bytes/second; one 1,280-byte contiguous clear
  explicitly defines the omitted rear planes. If that first source fully routes all three buses,
  the bound is 840 instructions and 15,360 bytes per frame, or 3,141,888 bytes/second.
- Final production ThinLTO removes the unconditional entry clear. `GenerateCurrentFrame()` grows
  from 584 to 768 bytes; `MixInto()`, `MixIntoFirst()`, and the direct helper are 1,244, 440, and
  940 bytes. Against the former 1,828-byte source/driver pair, the complete retained set is 3,392
  bytes, a 1,564-byte code-size cost. `DspHle::Impl::Tick()` remains 124 bytes. The accumulator is
  exactly its baseline size, stays a leaf with only the established `d8`/`d9` save pair, and keeps
  its 52/74 full and 32/46 front loop counts without spills. The initialized state stays in `w19`,
  leaving one `TBNZ` choice per later source rather than per-bus checks. The all-silent route retains
  one bulk clear with no final flag scan.
- Permanent Catch2 coverage checks accumulated output plus first steady/ramped full and front
  contributions, exact rear zeroing, simultaneous direct main/final-bus output and mask, a silent
  first source with ramp transition, disabled-source state, existing destinations, and guard
  canaries. The full ELF64/AArch64 test executable and production ThinLTO library compile and link
  successfully in 1 minute 10 seconds; the final coverage-only test relink passed in 35 seconds.
  The ARM64 executable was not run on this x64 host.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 20 seconds and produced an ARM64-only
  28,969,479-byte APK with SHA-256
  `2E4346084247EEE1C375745BBB240A7B0864DF1C2F1BCB615AB3D715DDD96112`.
- After verification, 2,334,959,336 logical bytes of the native test executable and disposable
  Gradle intermediates were removed. Reported C: free space increased by 1,896,804,352 bytes; the
  final APK and active ARM64 RelWithDebInfo CMake cache remain in the workspace.
- No device, ADB, install, launch, game run, FPS test, or battery measurement was used. These are
  continuous DSP path-local instruction and memory-system reductions, not evidence for a specific
  whole-game FPS or wattage gain. A future allowed matched Thor A/B should hold title, scene, save,
  caches, renderer, resolution, driver, display layout, performance/fan mode, brightness, audio
  backend, speed limit, and duration constant, then record first-source routing, active source
  counts, DSP/audio-thread time and placement, underruns, frametimes, battery power, temperature,
  thermal slope, output correctness, and stability.

## 2026-08-17 Raster Fill Bulk Materialization

- `RasterizerCache::DownloadFillSurface()` previously rounded every requested start down to its
  fill-pattern boundary, backed up the bytes before the requested interval, called runtime-sized
  `memcpy` once per two-, three-, or four-byte pattern, and restored that prefix. A 1 MiB four-byte
  fill consequently executed about 262,144 loop iterations and tiny copies even though the output
  is just one repeating pattern. `SurfaceBase::CanFill()` separately heap-allocated a vector to
  inspect a compatibility pattern whose maximum size is 16 bytes.
- `SurfaceBase::FillMemory()` now writes only `[start_offset, end_offset)`. It copies the partial
  first pattern from the exact phase, seeds one aligned complete pattern, and repeatedly doubles
  the initialized prefix through non-overlapping `memcpy` ranges. Work changes from
  `O(bytes / fill_size)` copy calls to `O(log bytes)`; an aligned 1 MiB four-byte fill needs about
  19 calls. A fill whose two to four source bytes are all equal takes one `memset`. Compatibility
  checks retain their original byte comparisons but use a fixed 16-byte stack array.
- Permanent Catch2 coverage compares the production helper against a byte-at-a-time reference for
  all fill sizes 2/3/4, eight patterns including the solid fast path, starts 0 through 15, and every
  length from 0 through 256. This is 98,688 phase/range cases with full before/after guard checks.
  A separate unaligned 1 MiB three-byte case verifies the exponential path and both canaries.
- Final AArch64 ThinLTO keeps `FillMemory()` as a 292-byte helper. Its solid branch tail-calls
  `memset`; its patterned branch performs one phase calculation and a compact loop around bulk
  `memcpy`. Both OpenGL and Vulkan `DownloadFillSurface()` are 272 bytes and contain one
  `FillMemory()` call with no per-pattern copy loop. `CanFill()` contains no allocation/deallocation
  call. The complete ARM64 test executable and production `libcitra-android.so` compiled and linked
  successfully in 1 minute 33 seconds; the ARM64 executable was not run on the x64 host.
- A temporary optimized x64 verifier first confirmed the old and new output matched, then ran seven
  order-alternated timing rounds. Median 64 KiB time fell from 53.16 to 0.95 microseconds for a
  distinct three-byte pattern (55.9x), and from 46.71 to 0.69 microseconds for a solid four-byte
  pattern (67.3x). At 1 MiB, the same cases fell from 830.77 to 29.05 microseconds (28.6x) and from
  706.63 to 19.57 microseconds (36.1x). The temporary executable/source were removed afterward.
- `:app:assembleVanillaRelWithDebInfoLite` passed in 1 minute 26 seconds. The final ARM64-only APK
  is 28,969,403 bytes with SHA-256
  `6A977BD4DAC49E57080C6816B37F0CB457CBD7969F76592DC59A70DCB74B7073`. Post-build cleanup kept
  that APK and the active ARM64 RelWithDebInfo CMake cache while removing 2,458,871,607 logical
  bytes of the test ELF, JNI staging, native symbols, mappings, and reproducible Gradle output.
  One 5,179,280-byte R8 dex intermediate remains because an existing Java process has it open; it
  is bounded and not required by the APK. Reported C: free space increased by 123,207,680 bytes.
- This is a large isolated CPU-overhead reduction when GPU fill surfaces are materialized back into
  emulated memory. Cache hit rate, fill sizes, and CPU readback behavior determine whole-game
  exposure. No device, ADB, install, launch, game, FPS, power, or temperature measurement was used,
  so no whole-game speed or wattage percentage is claimed.

## 2026-08-17 AArch64 Indexed-Draw Scan Unroll

- `RasterizerAccelerated::AnalyzeVertexArray()` calls `Common::FindMinMax()` for every indexed draw
  to derive the vertex range. The earlier AArch64 fix replaced scalar lane extraction with native
  horizontal reduction, but its main loop still carried one minimum and one maximum dependency
  across every 16-byte vector.
- The Cortex-X3, Cortex-A715, and Cortex-A710 optimization guides list AdvSIMD integer `UMIN`/`UMAX`
  at two-cycle latency; the Cortex-A510 lists three cycles. All four guides recommend memory-loop
  unrolling, and their Q-load tables give two Q loads and one Q-form `LDP` the same useful-byte
  issue rate. The large-scan path now uses four independent minimum and four independent maximum
  accumulators so each chain is revisited only after 64 bytes of other work.
- The throughput path starts at 128 bytes. This requires at least two 64-byte batches before paying
  for six extra accumulator initializations and the six-instruction final tree reduction. Shorter
  scans retain the prior compact one-vector loop; the vector tail and scalar remainder are
  unchanged. Unsigned minimum and maximum are associative, so grouping the same elements into four
  accumulators and reducing them afterward is exactly equivalent for `u8` and `u16`.
- Android Clang 18 compilation of the previous source emitted seven repeated instructions per
  16-byte vector: one Q load, two address updates, one compare, `UMIN`, `UMAX`, and one branch. That
  is 28 instructions per 64 bytes with one recurrence chain. Final production AArch64 ThinLTO emits
  two Q-form `LDP`, four independent `UMIN`, four independent `UMAX`, and five address/control
  instructions per 64-byte batch: 15 instructions, no spills, and 46.4% fewer repeated
  instructions. Both `u8` and `u16` functions grow from 284 to 436 bytes to retain separate small-
  and large-scan paths, a 304-byte combined code-size trade.
- Permanent reference coverage now checks every prefix through 145 byte indices and 73 halfword
  indices. Explicit extrema straddle 63/64 and 127/128 byte positions, covering scalar tails, the
  one-vector loop, the 127/128/129-byte crossover, multiple batches, and empty-input sentinels.
  The complete ELF64/AArch64 test executable and production shared library compiled and linked
  successfully after the final crossover refinement in 1 minute 11 seconds. The ARM64 executable
  was not run on the x64 host, and no device, ADB, install, launch, game, FPS, power, or temperature
  measurement was used.
- `:app:assembleVanillaRelWithDebInfoLite` passed with JDK 17 in 1 minute 23 seconds and produced an
  ARM64-only 28,969,775-byte APK with SHA-256
  `167832B7CBC8F2478F807BFFB62FFA6093503BF0B48BCD6D9ABB14649F40D0E9`. Cleanup retained that APK
  and the active ARM64 RelWithDebInfo CMake cache while removing 2,452,981,293 logical bytes of the
  test ELF, JNI/native staging, mappings, symbols, Kotlin/Gradle intermediates, and temporary
  assembly/manual-inspection files. Reported C: free space increased by 2,011,570,176 bytes. One
  bounded 5,179,280-byte R8 `classes.dex` remains because an existing Java process has it open.

## 2026-08-17 Crypto++ ARM64 Feature-Probe Repair

- Crypto++'s Android compiler supports ARMv8 CRC32 and PMULL, but both CMake probes failed because
  their small `try_compile` projects included `<cryptopp/arm_simd.h>` without the vendored
  `include/` directory. The configure log therefore reported a missing header and treated it as an
  unsupported instruction set. Direct NDK Clang checks proved both probe sources compile when that
  directory is present.
- The shared probe helper now passes its public-header directory through the try-project's
  `INCLUDE_DIRECTORIES`. A clean re-probe reports both `CRYPTOPP_HAVE_ARM_CRC32` and
  `CRYPTOPP_HAVE_ARM_PMULL` successful. Global Crypto++ definitions remain only
  `CRYPTOPP_ARM_NEON_HEADER=1` and `CRYPTOPP_ARM_ACLE_HEADER=1`: the baseline Android binary does
  not receive a global optional-ISA assumption.
- Production `crc_simd.cpp` alone compiles with `-march=armv8-a+crc`, while `gcm_simd.cpp` and
  `gf2n_simd.cpp` alone compile with `-march=armv8-a+crypto`. LLVM disassembly proves `CRC32B/W`
  and `CRC32CB/CW` in the CRC object and `PMULL`/`PMULL2` in the GCM/GF objects. Generic callers
  retain Crypto++'s Android `HasCRC32()` and `HasPMULL()` runtime gates before dispatching to them.
- Azahar currently calls Crypto++ AES, SHA, CCM/CBC/CTR, CMAC/HMAC, RSA, and ECC paths but has no
  direct Crypto++ CRC, GCM, or GF(2) call site. AES and SHA were already compiling to their hardware
  instructions before this repair. The change restores correct latent acceleration and future
  dispatch coverage; it is not evidence of a current game-FPS or battery-watt gain.
- Crypto++ is now vendored from former submodule commit
  `8d92d788421483a43e09acf1cd4a2861cb2b8cab`, keeping the one-line probe repair in this repository
  rather than requiring a separate detached dependency fork. The upstream BSD license and its
  source/test material remain intact.
- The full `:app:buildCMakeRelWithDebInfo[arm64-v8a]` rebuild compiled 465 actions and linked the
  ELF64/AArch64 tests plus `libcitra-android.so` successfully in 4 minutes 23 seconds. Packaging
  with Java 17 then passed in 2 minutes 9 seconds. The ARM64-only APK is 28,969,783 bytes with
  SHA-256 `12B443B1B493AED3974529D587786F615AA93D3E3C933ED31B1F42D11B69A9CE`.
- Cleanup retained the APK and active ARM64 RelWithDebInfo CMake cache while removing
  2,453,230,198 logical bytes of the test ELF, JNI/native staging, mappings, symbols, and
  reproducible Gradle output. Reported C: free space increased by 2,012,958,720 bytes. One bounded
  5,179,280-byte R8 `classes.dex` remains because an existing Java process has it open. No device,
  ADB, install, launch, game, FPS, power, or temperature measurement was used.

## 2026-08-17 Rejected PICA RSQ and Blind Fastmem Shortcuts

- The tempting PICA `RSQ` lowering `FRSQRTE; FMUL; FRSQRTS; FMUL` would be four instructions, not
  three. The Cortex-X3/A715/A710 guides imply a roughly 13-cycle dependent chain versus about
  14-19 cycles for the retained exact `FSQRT; FDIV`; A510 makes the approximation look better at
  roughly 16 versus 27 cycles. That narrow performance-core margin does not justify changing
  numerical behavior. The hardware-tested PICA description documents its reduced float format and
  special cases, but not normal-result bit accuracy or rounding. The exact lowering stays in place
  until guest-output equivalence can be proved, consistent with the fork's no-approximate-PICA rule.
  See [3dbrew's hardware-tested PICA shader instruction behavior](https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set).
- A true RPCS3-style 4 GiB fastmem view was also rejected as a local toggle. Azahar's current
  AArch64 pointer page table emits page-index extraction, a page-entry load, null fallback, a
  page-offset mask, and the guest access. A safe direct-address view would require coherent 4 GiB virtual
  aliases for each 3DS process and a redesign of the ordinary-array backing/remapping model.
  Pointing Dynarmic fastmem at the existing storage without that aliasing would be incorrect; this
  remains a separately scoped VM architecture project rather than an unsafe shortcut.

## 2026-08-17 AArch64 Y2R Fixed-Point Conversion

- The emulated 3DS Y2R hardware converts video/camera strips from four planar 4:2:2/4:2:0 formats
  and interleaved YUYV into tiled `0xRRGGBB00` output. The Android AArch64 release object still ran
  the complete fixed-point matrix, shifts, clamp, and tile address calculation once per pixel. Its
  planar repeated loop was 36 instructions per pixel; the interleaved loop was 46 per pixel and
  only partially packed two lanes with vector operations.
- The AArch64 path now loads eight luma and four subsampled chroma values per band, duplicates the
  chroma lanes with ZIPs, and evaluates eight pixels with exact signed widening
  `SMULL`/`SMLAL`/`SMLSL`. It preserves `(value >> 3) + offset + 0x18`, the following `>> 5`, and
  clamp to `[0,255]`, then uses saturating narrows and register ZIPs to write the original tile
  words. Interleaved YUYV uses one D-form `LD2`; output avoids the Cortex-A510's slow D-form byte
  `ST4`. Non-AArch64 builds keep the original scalar path.
- The choice is grounded in the checked Thor core manuals: widening multiply/accumulate is listed
  on Cortex-X3 pages 27-28, A715 page 29, A710 page 43, and A510 page 36; ZIP/narrow operations are
  on X3 pages 31-32, A715 pages 34-35, A710 pages 52-53, and A510 pages 43-44. Every coefficient
  product and worst-case three-term channel sum fits signed 32 bits, including `s16` coefficient
  extremes, so no wider or approximate arithmetic is required.
- Independent Catch2 coverage checks all five input formats, widths 8/16/24, heights 1/2/7/8, six
  normal/mixed/extreme coefficient sets, deterministic lane-varying inputs, and untouched tile-row
  canaries against the original scalar formula. The ARM64 test translation unit and test ELF link
  successfully; the executable was not run because device execution is excluded for this review.
- Isolated optimized AArch64 codegen reduces the planar repeated work from 288 instructions for
  eight pixels to 65, or 77.4%, and the interleaved work from 368 to 64, or 82.6%. That corresponds
  to 4.43x and 5.75x less instruction-count work respectively, not measured cycle-speed ratios.
  The full ThinLTO library retains 30 `SMULL`/`SMULL2`, 20 `SMLAL`/`SMLAL2`, 20
  `SMLSL`/`SMLSL2`, 30 `SQXTUN`/`SQXTUN2`, and 15 `UQXTN` instances across the five format loops.
  The test-only 1,996-byte conversion entry point is hidden: it remains in the test ELF but is
  garbage-collected from `libcitra-android.so`, reducing the latter's debug-bearing file by 25,504
  bytes without changing the production converter.
- `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed twice, including the final hidden-test-entry
  relink; both the test executable and production shared library linked with ThinLTO. A subsequent
  clean portable-SDK graph compiled all 2,196 native actions and assembled the APK, although the
  first Gradle invocation then rejected its configuration cache because the build script calls
  command-line Git. The required incremental rerun with `--no-configuration-cache` passed cleanly
  in 33 seconds. The ARM64-only APK is 28,969,635 bytes with SHA-256
  `801E86EA3F3848C8BE362CE150D77806299082B583E05A4F83820E6657275922`.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake cache while removing
  2,505,957,707 logical bytes of the test ELF, APK staging/mappings/symbols, Gradle intermediates,
  and temporary baseline/codegen objects. Reported C: free space increased by 2,076,962,816 bytes.
  Three Gradle HTML reports remain file-handle locked after stopping the daemons and total only
  669,766 bytes. No device, ADB, install, launch, game, FPS, wattage, battery,
  or temperature measurement was used. The gain applies when titles exercise Y2R video/camera
  conversion and must not be added to unrelated dispatch, texture, audio, or Eco Turbo percentages.

## 2026-08-17 AArch64 Y2R Output Packing

- The completed eight-pixel YUV matrix path still handed every intermediate `0xRRGGBB00` word to
  scalar output-format helpers. Android AArch64 Clang partly auto-vectorized RGBA8 and RGB8, but the
  repeated bodies were 62 and 58 instructions per 32 pixels and ended in two Q-form `ST4` or `ST3`
  structured stores. RGB565 remained a 10-instruction scalar loop per pixel, while RGB5A1 remained
  13 instructions per pixel. This was the next bottleneck in the same Y2R strip pipeline.
- The AArch64 path now packs sixteen pixels explicitly. RGBA8 ORs alpha into the known-zero low byte
  of four ordinary Q-loaded intermediate vectors. RGB8 uses three compile-time-proved adjacent-
  input `TBL2` maps and ordinary Q stores; its small outlined helper keeps the three constants out
  of the repeated loop. RGB565 and RGB5A1 deinterleave sixteen `[0,B,G,R]` words with one Q-form
  `LD4`, apply exact byte masks and shifts, then use `SHLL`/`SHLL2` and paired Q stores. The scalar
  non-AArch64 path and an at-most-fifteen-pixel scalar remainder are unchanged.
- In final production ThinLTO, repeated RGB8 work is 12 instructions per sixteen pixels, RGBA8 is
  20, RGB565 is 21, and RGB5A1 is 23. Normalized to the pre-change release-object bodies, those are
  reductions of 58.6%, 35.5%, 86.9%, and 88.9%, respectively. The complete production
  `PerformConversion()` plus outlined RGB8 helper contains no `ST3` or `ST4`. These are instruction-
  count reductions in output packing, not equivalent cycle-speed ratios or whole-game gains; each
  CDMA unit also pays fixed setup/control work outside the repeated loops.
- Independent Catch2 coverage compares all four formats to a scalar byte-level reference at
  0/1/7/15/16/17/31/32/37 pixels, across 5 alpha edges and 16 channel truncation edges. It compares
  the complete output including 32-byte canaries. The ARM64 production object and test object
  compile, and both the full test ELF and `libcitra-android.so` link with ThinLTO. The hidden
  740-byte test dispatcher remains in the test ELF and is garbage-collected from the production
  library; the 104-byte RGB8 production helper remains as intended. The test executable was not
  run because device execution remains excluded.
- The final incremental `:app:buildCMakeRelWithDebInfo[arm64-v8a]` passed in 1 minute 19 seconds,
  compiling both Y2R objects and linking the ARM64 test ELF and production shared library.
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` then passed in 1 minute 48
  seconds. The ARM64-only APK is 28,970,251 bytes with SHA-256
  `497778385D6C494D94158351EA288FB3A1B1A30D1FE8C8D127FD97BBF2228CD6`.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake cache while removing
  2,467,481,007 logical bytes of temporary codegen objects, the test ELF/tools, native/JNI staging,
  mappings, symbols, Kotlin/R8 output, and other reproducible Gradle intermediates. Reported C:
  free space increased by 2,026,418,176 bytes.
- The implementation follows the checked X3/A715/A710/A510 load/store and table guidance already
  indexed in `docs/hardware/README.md`: avoid throughput-limited multiway structured stores on the
  A510 and confirm the compiler emitted the intended instruction forms. No device, ADB, install,
  launch, game, FPS, wattage, battery, temperature, or visual measurement was used. A future matched
  Thor A/B should target video/camera-heavy scenes and hold the standard title, cache, renderer,
  resolution, driver, performance/fan, brightness, and display-layout controls fixed.

## 2026-08-17 Direct Unrotated Linear Y2R Output

- After YUV conversion, the common `Rotation::None` plus `BlockAlignment::Linear` route still sent
  every tile through `RotateTile0()`. Its selected `linear_lut` is the exact identity, so that step
  copied each pixel into a 256-byte `tmp_tile`; `WriteTileToOutput()` immediately read the temporary
  and copied it to the final strip. A full tile therefore performed 1,024 logical bytes of
  arrangement loads plus stores even though only 512 bytes are required. The direct route saves
  512 bytes per full tile, or eight bytes per converted pixel; a 400x240 conversion avoids 768,000
  logical bytes of redundant traffic.
- The direct writer traverses output rows first and copies each 32-byte tile row into its final
  horizontal position. This is exactly the composition of the old identity remap and output copy:
  source pixel `tiles[tile][y * 8 + x]` still reaches
  `output[y * input_line_width + tile * 8 + x]`. Every rotated route and Block8x8 output retains
  the old remap, temporary, tile-order reversal where required, and write behavior.
- The pre-change Android AArch64 release object emitted a five-instruction scalar identity-scatter
  body for every pixel and a fourteen-instruction copy body for every eight-pixel row: 432 repeated
  instructions per full tile before surrounding setup and switches. Final production ThinLTO
  keeps the new writer as a 68-byte function. Its inner band is exactly a post-indexed Q-form
  `LDP`, decrement, post-indexed Q-form `STP`, and branch: 32 repeated instructions per full tile,
  plus 69 setup/control instructions amortized across a full eight-row strip. That is about 76.6%
  less arrangement work for one tile and 92.3% less for a 400-pixel/50-tile strip. These are static
  executed-instruction counts, not measured cycle speedups.
- The emitted shape follows the checked ordinary pair load/store tables on Cortex-X3 page 23,
  Cortex-A715 page 26, Cortex-A710 page 39, and Cortex-A510 page 32. It uses baseline AArch64 only,
  streams final destination rows contiguously, has no spills, and avoids trying to SIMD-accelerate
  an unnecessary intermediate pass. `PerformConversion()` grows only four bytes from `0x3140` to
  `0x3144`; the small outlined helper avoids duplicating this loop inside the already-large format
  dispatcher.
- Permanent independent coverage compares the direct writer with the old address mapping for
  zero, one, two, and three tiles; heights 1/2/7/8; exact and five-word-padded line strides; and
  sixteen-word guards on both sides. The production and test translation units compiled, and the
  complete ELF64/AArch64 Catch2 executable plus `libcitra-android.so` linked with ThinLTO in 1
  minute 22 seconds. The test name remains in the test ELF while its hidden entry point is absent
  from the production library. The ARM64 executable was not run because device execution remains
  excluded.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed with JDK 17 in 1 minute
  44 seconds. The resulting package contains only `arm64-v8a`, is 28,970,039 bytes, and has SHA-256
  `FC05BB9062FF80651E4EE83A4BDCF17BCB7FE27DBC909179D21BBBE0B501EED6`. Cleanup retained that APK
  and the active ARM64 RelWithDebInfo CMake cache while removing 2,469,286,588 logical bytes of
  APK/JNI/native staging, the test ELF, codegen objects, and local Gradle cache. Reported C: free space
  increased by 2,029,838,336 bytes to 109,481,762,816. One pre-existing Java process still holds a
  bounded 669,766-byte Gradle HTML report, so it remains rather than terminating an unidentified
  process. No device, ADB, install, launch, game, FPS, battery, wattage, temperature, or visual run
  was used; the gain applies only when a title exercises unrotated linear Y2R video/camera output.

## 2026-08-17 Direct Zero-Gap 8-bit Y2R Input

- Every 8-bit Y2R input plane previously copied its incoming CDMA bytes into `data_buffer` before
  the converter reread them. With a zero inter-unit gap, the source is already one contiguous byte
  stream: the old loop copied `input[i]` to `output[i]` without transforming it. The converter now
  borrows that read-only guest pointer for the duration of the strip and reproduces the only
  externally visible state changes, advancing `ConversionBuffer::address` by the byte count and
  subtracting the same count from `image_size`.
- The shortcut is exact for `YUV422_Indiv8`, `YUV420_Indiv8`, and interleaved `YUYV422`. Each plane
  independently chooses direct or compact input. Any nonzero gap retains the old per-transfer-unit
  compaction, and both 16-bit planar formats retain their every-other-byte extraction. Conversion
  consumes the complete input strip before output arrangement or CDMA writes begin, so the borrowed
  pointer does not outlive its read-only use. The old implementation already obtained one guest
  pointer and walked it across the complete transfer, so address-contiguity assumptions do not
  change.
- Removing the staging pass saves one source read and one staging write for every input byte. At
  400x240, 4:2:0 contains 144,000 input bytes and therefore avoids 288,000 logical bytes of copy
  traffic; 4:2:2 and YUYV contain 192,000 bytes and avoid 384,000. Per eight-row strip, the bounds
  are `24 * width` and `32 * width` logical bytes respectively. This is in addition to, but must not
  be numerically added as a speed percentage to, the separate conversion, packing, and linear-output
  reductions.
- The pre-change no-LTO AArch64 release object emitted the 8-bit receive logic inside
  `PerformConversion()`, whose code size was `0x3144`. The candidate outlines one shared 136-byte
  helper and shrinks the dispatcher to `0x29e0`, 1,892 bytes or 15.0% smaller. Final production
  ThinLTO retains those exact sizes. Its zero-gap route is seven instructions: load and test `gap`,
  paired-load address/size, add, subtract, paired-store, and return. It performs no source or
  staging data load/store. A proposed hot divisibility assertion was rejected during codegen review
  because it introduced `UDIV`/`MSUB` on every plane; permanent edge coverage supplies the safety
  check without burdening production.
- Independent Catch2 coverage exercises transfer units 1/7/16/31, zero/one/two/five units, zero
  gap with an untouched staging buffer, and gaps 1/5/13 against an independent byte reference. It
  checks exact returned pointers, address/size progression, and sixteen-byte guards. Both Y2R
  objects compiled, and the complete ELF64/AArch64 test executable plus `libcitra-android.so` linked
  with ThinLTO in 1 minute 26 seconds. Both test names remain in the test ELF while the hidden test
  wrapper is garbage-collected from the production library.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed with JDK 17 in 2 minutes
  50 seconds. The package contains only `arm64-v8a`, is 28,970,147 bytes, and has SHA-256
  `52A5E611D41E46F24FED8F249250F41DCCF274ABA04D5709732795158C7757BA`.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake cache while removing
  2,498,930,560 logical bytes of APK/JNI/native staging, the test ELF, codegen audit objects, and
  local Gradle cache. Reported C: free space increased by 2,030,465,024 bytes to 109,477,445,632.
  The pre-existing bounded 669,766-byte Gradle HTML report remains rather than terminating an
  unidentified process. No device, ADB, install, launch, game, FPS, battery, wattage, temperature,
  or visual run was used. This is a bounded video/camera memory-traffic and code-size win, not a
  measured whole-game speed or power claim.

## 2026-08-17 Fused Zero-Gap Linear Y2R Output

- The preceding unrotated-linear shortcut removed the identity tile remap, but the common zero-gap
  route still wrote four-byte `0xRRGGBB00` pixels into a contiguous strip and immediately reread
  them for final RGBA8, RGB8, RGB5A1, or RGB565 packing. The new route gathers each completed tile
  row and packs directly into the guest destination. It removes one 32-bit staging store and one
  32-bit staging load per pixel: eight logical bytes per pixel, or 768,000 bytes for 400x240.
- This is exact only for `Rotation::None`, `BlockAlignment::Linear`, and `dst.gap == 0`. Every
  rotated, Block8x8, or gapped-CDMA configuration retains the established arrangement and
  `SendData()` path. All inputs for a strip have already been consumed before the direct output
  begins, and the old implementation also committed guest output after each strip, so possible
  cross-strip source/destination overlap keeps the same ordering. Address advancement and remaining
  image size use the same `amount * bytes_per_pixel` values as the zero-gap old route.
- RGBA8 loads each eight-pixel tile row as two Q vectors, ORs the requested alpha into the
  intermediate words' known-zero byte, and uses two ordinary Q stores. RGB8 pairs two horizontally
  separated tile rows in registers and applies three compile-time-proved adjacent-input `TBL2`
  maps to produce 48 packed bytes; an odd final tile uses exact Q plus D table/store operations.
  RGB5A1 and RGB565 use one D-form byte `LD4` for the only contiguous eight-pixel tile row, preserve
  the existing high-bit truncation and alpha-bit rules, and finish with an ordinary Q store.
- This layout follows the checked Thor core tables rather than assuming every structured access is
  beneficial. Ordinary pair loads/stores are covered on Cortex-X3 page 23, Cortex-A715 page 26,
  Cortex-A710 page 39, and Cortex-A510 page 32. The `TBL2` and byte `LD4` entries are on X3 page 34,
  A715 page 37, A710 page 56, and A510 pages 46-47. The D-form `LD4` is retained because the next
  horizontal tile row is not contiguous in memory; the output side uses no `ST3` or `ST4`.
- Final production ThinLTO keeps four outlined format helpers at 188/304/208/192 bytes for
  RGBA8/RGB8/RGB5A1/RGB565. Their repeated bodies are respectively 10 instructions per eight
  pixels, 11 per sixteen, 15 per eight, and 13 per eight. Including the removed prior 32-byte row
  writer, the corresponding old repeated bodies were 14 per eight, 20 per sixteen, 31 per sixteen,
  and 29 per sixteen. The static repeated-instruction reductions are therefore 28.6%, 45.0%, 3.2%,
  and 10.3%; they are not cycle, whole-game FPS, or wattage measurements. `PerformConversion()`
  grows from `0x29e0` to `0x2aa8` (200 bytes) to select the fused route.
- Independent Catch2 coverage checks all four formats; zero, odd, and even tile counts 0/1/2/3/5;
  heights 0/1/2/7/8; alpha 0/0x80/0xff; one- and eight-pixel transfer units; sixteen channel-edge
  values; exact address/image-size progression; and 32-byte guards. The production and test Y2R
  objects compiled, and the complete ELF64/AArch64 Catch2 executable plus `libcitra-android.so`
  linked with ThinLTO in 1 minute 32 seconds. The test name is present in the test ELF while the
  hidden test wrapper is absent from the production library. The ARM64 tests were not run because
  device execution remains excluded.
- The final `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` recovery build passed
  with JDK 17 in 2 minutes 53 seconds. The resulting package contains only `arm64-v8a`, is
  28,969,511 bytes, and has SHA-256
  `DDE1A7EEF6B6DD2A0C7415E1263C584C9C685A48A36958F9BF6DF7DB0A8468F2`.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake cache while removing
  2,497,952,233 logical bytes of native/JNI staging, the test/tools executables, mappings, symbols,
  Kotlin/R8 output, and local Gradle state from the final build peak. Reported C: free space
  increased by 2,058,395,648 bytes to 109,470,556,160. The retained `.cxx` cache is 2,786,752,349
  bytes and `app/build` contains only the 28,969,511-byte APK. No device, ADB, install, launch,
  game, FPS, battery, wattage, temperature, or visual run was used.

## 2026-08-17 Allocation-Free Fully Direct Y2R Route

- After direct 8-bit input borrowing and direct zero-gap output packing landed, `PerformConversion()`
  still unconditionally allocated `input_line_width * 8 * 4` bytes for the shared CDMA strip and
  freed it at return. The fully direct route never read or wrote that storage. At 400-pixel width
  this was a 12,800-byte transient reservation per conversion; the valid 1024-pixel maximum is
  32,768 bytes.
- The buffer is now omitted only when output is zero-gap, unrotated, and linear and the active input
  is either YUV422/YUV420 8-bit planar with all three active gaps zero or interleaved YUYV with its
  active gap zero. Inactive-plane gaps deliberately do not matter. Both 16-bit formats, every active
  input gap, any output gap, rotation, and Block8x8 output retain the original staging behavior.
  Fixed Y/U/V staging partitions are also calculated once outside the strip loop.
- The null pointer is safe by construction: every active receive on the bypass route enters
  `PrepareInputData8()` with `gap == 0`, advances the same address and image-size fields, and returns
  guest memory before touching its compact-output argument. Conversion consumes those borrowed
  inputs before direct output starts. The output formats, tile storage, conversion math, CDMA state,
  and all fallback data paths are unchanged.
- No-LTO AArch64 codegen exposed and rejected a tempting regression during review: array
  `make_unique` value-initialized the fallback buffer and emitted a full `memset`. The retained
  `new[]` allocation is uninitialized exactly like the baseline. The baseline `PerformConversion()`
  was `0x2aa8` bytes and unconditionally called two `new[]` functions, deleted tile storage, then
  tail-called the strip-buffer `delete[]`. The candidate is `0x2c18` bytes: its eligibility gate
  branches around the first `new[]`, leaves the tile allocation, and uses `CBZ` after deleting tiles
  to skip the matching strip-buffer delete. Final production ThinLTO retains the same `0x2c18`
  size and contains no `memset` in this function.
- Independent Catch2 coverage exercises all four output formats, both 8-bit planar formats, YUYV,
  both 16-bit formats, every active and inactive input-gap distinction, all rotations, both block
  layouts, and output gap. Existing zero-gap input coverage now also passes a null staging pointer
  across transfer units 1/7/16/31 and zero/one/two/five units while checking exact returned pointer
  and CDMA state. Both modified ARM64 objects compiled, and the complete test ELF plus production
  shared library linked with ThinLTO in 1 minute 39 seconds. The test name is present in the test ELF
  while the hidden predicate hook is absent from production. The ARM64 tests were not run because
  device execution remains excluded.
- This removes allocator and lifetime work rather than a pixel-loop instruction, so the Arm core
  manuals do not provide a defensible cycle estimate. It is a bounded Y2R video/camera win, not a
  measured FPS or wattage result.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed with JDK 17 in 2 minutes
  38 seconds. The package contains only `arm64-v8a`, is 28,969,947 bytes, and has SHA-256
  `D2E7BFE0351144F646529B7DBCB0E607E29F2D2CEA3DC26B99D80CBEBBC94C49`.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake cache while removing
  2,499,957,480 logical bytes of native/JNI staging, the test/tools executables, codegen audit
  objects, mappings, symbols, Kotlin/R8 output, and local Gradle state. The deletion pass reported
  2,060,390,400 bytes recovered, with final C: free space at 109,464,788,992 bytes. The retained
  `.cxx` cache is 2,786,934,832 bytes and `app/build` contains only the 28,969,947-byte APK. No
  device, ADB, install, launch, game, FPS, battery, wattage, temperature, or visual run was used.

## 2026-08-17 Layout-Aware Right-Eye Presentation Elision

- Command-line Git over SSH fetched upstream Azahar `fb1c1a710` (`video_core: Fix some pica
  command handling issues (#2412)`) and merged it directly into `master` as `d60d706e8`. The
  upstream fix hardens PICA LUT wrapping, out-of-range float-uniform writes, and sequential-register
  batches. The fork's AArch64 PICA paths survived the merge, and the post-merge ARM64 native build
  passed before this optimization began.
- OpenGL and Vulkan previously prepared all three presentation images whenever any frame,
  screenshot, or OpenGL frame dump was due. Normal mono-left output cannot sample the top-right
  image, and a bottom-only layout cannot sample either top image, yet the renderer still performed
  the right framebuffer surface lookup and `AccelerateDisplay()`/fallback upload path every
  qualifying presentation.
- Presentation now forms the union of the main, secondary, screenshot, and frame-dump layouts.
  Top stereo modes and explicit mono-right retain the real right-eye resolve; mono-left and
  bottom-only output alias the current left presentation image and texture coordinates into the
  still-valid right descriptor slot and skip the right-eye load. Additional-top layouts count as a
  top consumer. The right fallback texture remains configured so a later mode change is safe.
- `RightEyeDisabler` records whether the completed frame actually blocked its right-eye transfer.
  Presentation consumes that fact once, only when a render target is actually prepared, and aliases
  left even if a stereo layout is selected. A throttled/non-presented VBlank leaves it pending, so a
  later presentation cannot sample the deliberately stale right buffer. This deliberately does not
  key off the checkbox alone because the hack's per-title detection may turn itself off.
- Permanent Catch2 coverage exercises mono-left, mono-right, all six stereo modes, disabled top,
  additional top, and additional bottom layouts. The full ARM64 native target compiled both
  renderers and the new test translation unit, then linked the ELF64/AArch64 test executable and
  production `libcitra-android.so` successfully after the final lifecycle hardening in 1 minute
  29 seconds. The ARM64 tests were not run
  because device execution remains excluded.
- Final AArch64 ThinLTO disassembly retains the boolean branch in both
  `PrepareRendertarget(bool)` implementations. On the false branch, OpenGL copies the left texture
  handle plus coordinates and bypasses `LoadFBToScreenInfo`; Vulkan copies the left image view plus
  coordinates and bypasses its corresponding load. `RightEyeDisabler::ReportEndFrame()` lowers to
  compact byte tests and an OR/store for the consumed presentation state.
- This removes one right-eye surface lookup/resolve per qualifying presented frame. A CPU fallback
  would also avoid its flush and texture upload. Exact time and power saved depend on whether the
  framebuffer is accelerated and on the active display layout, so this is not a defensible
  whole-game FPS or wattage percentage without a controlled Thor A/B run.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed with JDK 17 in 1 minute
  49 seconds. The package contains only `arm64-v8a`, is 28,969,603 bytes, and has SHA-256
  `7DC1E017576271CE78E71644AD7290062D7EF09D6B5544DA1AB9D23CFFBC0129`.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake cache while removing
  2,469,127,412 logical bytes of APK/JNI/native staging, the 446,510,544-byte ARM64 test ELF, helper
  executables, mappings, symbols, Kotlin/R8 output, and local Gradle state. Reported C: free space
  increased by 2,029,428,736 bytes to 109,022,683,136. The retained `.cxx` cache is
  2,781,397,898 bytes and `app/build` contains only the final APK. No device, ADB, install, launch,
  game, FPS, battery, wattage, temperature, or visual run was used.

## 2026-08-17 Asynchronous Skipped-Frame Vulkan Submission

- Upstream duplicate-frame suppression commit `8c4e8b77b` added a fallback
  `scheduler.Finish()` whenever Vulkan rendered no host screen. That condition now also covers
  Eco Turbo presentation throttling. `Finish()` submits the current command buffer and then waits
  for its pre-submit timeline tick, so every duplicate or Eco-Turbo-skipped VBlank serialized the
  emulation thread with GPU completion even though the CPU did not consume a rendered result.
- The no-presentation fallback now uses `scheduler.Flush()`. It records the same render-pass end,
  advances the same timeline value, dispatches the same command chunk, and submits the same Vulkan
  command buffer, but does not call `MasterSemaphore::Wait()`. The graphics queue therefore keeps
  guest work ordered while the CPU and Adreno can overlap it.
- Lifetime safety does not depend on the removed wait. Command buffers and descriptor sets are
  tagged with `CurrentTick()` and reused only after known GPU completion; stream-buffer wrap calls
  `Scheduler::Wait()` for the exact recorded watch; and rasterizer garbage collection deletes a
  sentenced surface only after the completed tick advances beyond its sentence tick. Stale-low
  completion merely delays reuse or deletion. Explicit `Finish()` calls remain for screenshot CPU
  readback, resized render-frame recreation, presentation-window destruction, and renderer teardown.
- The full `arm64-v8a` native target passed in 1 minute 43 seconds, compiling the changed Vulkan
  renderer and linking both the ELF64/AArch64 Catch2 executable and production
  `libcitra-android.so`. The final ThinLTO `RendererVulkan::SwapBuffers()` is `0x408` bytes. Its
  `screenRendered == false` branch calls `Scheduler::SubmitExecution()` directly (inlined
  `Flush()`), with no call to `Scheduler::Finish()` or `MasterSemaphore::Wait()`. The later
  `WaitWorker()` drains CPU command recording only; it is not a GPU-completion wait.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed with JDK 17 in 2 minutes
  46 seconds. The APK contains only `arm64-v8a`, is 28,970,191 bytes, and has SHA-256
  `C41B14B848561271869A977748E248B8653C0782E23064179777BFCA606520C5`.
- This removes exactly one GPU-completion wait from every Vulkan `SwapBuffers()` that performs no
  host presentation. The actual saved time ranges from nearly zero when Adreno is already caught
  up to the outstanding GPU backlog when it is not. No device, ADB, install, launch, game, FPS,
  battery, wattage, temperature, or visual run was used, so this is a proven synchronization
  removal and a strong power/overlap candidate rather than a measured whole-game percentage.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake/Ninja cache while removing
  2,471,517,253 logical bytes of Gradle staging, the ARM64 test ELF, and native helper executables.
  R8 held one generated `classes.dex` open until the Gradle daemons were stopped; the exact second
  pass removed it. Reported C: free space increased by 2,030,428,160 bytes to 109,023,707,136.
  The retained `.cxx` cache is 2,781,525,815 bytes and `app/build` contains only the final APK.

## 2026-08-17 Native Vulkan Frame-Worker Overlap

- `RasterizerVulkan::TickFrame()` inherited an unconditional `scheduler.WaitWorker()` from
  Azahar commit `e8c75b410` (`libretro: vulkan: wait before ticking`). At that time rasterizer-cache
  garbage collection used frame age, so an old surface could be destroyed while a lagging worker
  command still referenced it. The later upstream completion-tick conversion in `b34de55b5`, plus
  this fork's corrected strict comparison in `dcd3a58a0`, removed that lifetime dependency:
  sentenced resources are retained while `completed_tick <= retirement_tick` and deleted only
  after completion advances beyond their tick.
- Native threaded presentation had one additional ordering dependency on the frame-end join. It
  records the present-queue callback after `Flush()` dispatches the render submission. The callback
  is now explicitly dispatched behind that submission in the scheduler's FIFO, so the presentation
  thread is notified after `vkQueueSubmit` without forcing the emulation thread to wait for worker
  completion. The synchronous presentation fallback keeps its explicit `WaitWorker()`, and LibRetro
  keeps the original wait before its cache tick.
- Removing the join lets the producer start the next frame while prior worker lambdas execute.
  Presentation clear color is therefore captured by value rather than read through mutable
  renderer state. Descriptor updates still finish on the producer before queuing; command and
  descriptor pools remain timeline-tagged; stream-buffer wrap waits its exact watch tick; resize,
  readback, window destruction, and renderer teardown retain explicit synchronization.
- A focused constexpr regression covers completion older than, equal to, and newer than a resource
  retirement tick. The full `arm64-v8a` native build compiled and linked that test plus production
  `libcitra-android.so` successfully in 1 minute 29 seconds. The ARM64 test executable was not run
  on this x64 host because device use remains forbidden.
- Linked AArch64 inspection shows `RasterizerVulkan::TickFrame()` is 12 bytes and branches directly
  to `RasterizerCache::TickFrame()` with no `Scheduler::WaitWorker()` call. Threaded
  `PresentWindow::Present()` ends in a tail call to `Scheduler::DispatchWork()`; its only linked
  `WaitWorker()` branch is the preserved non-threaded fallback.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed with JDK 17 in 2 minutes
  23 seconds. The APK contains only `arm64-v8a`, is 28,969,839 bytes, and has SHA-256
  `A2DC7360808C53E871C66D9CCF35E0A2C0D570B7761DE7C13F33C86154C9D8E1`.
- This removes exactly one CPU-side Vulkan worker join from every normal native threaded frame and
  permits command recording/submission to overlap the next emulation-frame setup. The saved time
  depends on worker backlog and the title's CPU/GPU balance. No device, ADB, install, launch, game,
  FPS, battery, wattage, temperature, or visual run was used, so no whole-game percentage is
  claimed.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake/Ninja cache while removing
  2,471,565,068 logical bytes of Gradle staging, the ARM64 test ELF, and native helper executables.
  Reported C: free space increased by 2,025,144,320 bytes to 100,816,732,160. The retained `.cxx`
  cache is 2,781,916,118 bytes and `app/build` contains only the final APK.

## 2026-08-17 Single-Dispatch Vulkan Presentation Handoff

- After the native frame-end join was removed, normal threaded presentation still used two worker
  chunks per frame. `RenderToWindow()` called `Flush(frame->render_ready)`, which recorded and
  dispatched the Vulkan submission; `PresentWindow::Present()` then recorded a second frame-queue
  callback and called `DispatchWork()` again. That second dispatch repeated the descriptor
  `on_dispatch` callback, scheduler queue push/pop, `queue_mutex`, `event_cv.notify_all()`, reserve
  chunk acquisition/recycling, and worker execution-lock handoff without recording more GPU work.
- `Scheduler::FlushWithCallback()` now records one typed command containing both operations. It
  performs the exact existing timeline-tick preparation and `MasterSemaphore::SubmitWork()` first,
  releases `submit_mutex`, then runs the supplied callback before the submitted chunk is recycled.
  `PresentWindow` owns both the threaded combined route and the synchronous `Flush()` plus
  `WaitWorker()` route, so callers cannot accidentally separate submission from presentation.
- The ordering remains strict: the present callback cannot expose `frame->render_ready` before its
  signal submission has reached `vkQueueSubmit`; the presentation thread's copy waits on that
  binary semaphore; descriptor updates still flush on the producer before the combined chunk is
  queued; and the chunk retains its submit marker so the worker allocates a fresh render command
  buffer afterward. If the current 32 KiB command chunk lacks 56 bytes for the combined command,
  the unchanged capacity fallback first dispatches prior work and records the combined operation
  in a fresh chunk.
- Both the worker-to-present frame notification and the present-to-render free-frame notification
  now release their predicate mutex before `notify_one()`. The queue mutation remains protected,
  and predicate testing under the same mutex prevents lost wakes, while the awakened thread no
  longer immediately contends on a lock deliberately held by the notifier. The separate
  queue-to-swapchain lock exchange in `PresentThread()` is ordering-sensitive and remains unchanged.
- The final full `arm64-v8a` native rebuild compiled and ThinLTO-linked the test ELF and production
  `libcitra-android.so` successfully in 1 minute 23 seconds. Linked AArch64 has one 56-byte
  `FlushWithCallback` typed command whose `0x118`-byte execute body locks/submits/unlocks, then
  locks/pushes/unlocks and tail-notifies. The capacity-fit threaded `Present()` path has one final
  `Scheduler::DispatchWork()`; `WaitWorker()` remains only on its synchronous branch.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed with JDK 17 in 1 minute
  50 seconds. The APK contains only `arm64-v8a`, is 28,970,707 bytes, and has SHA-256
  `43873337BB20AB212810E8536ECD275EF7469872875B98806E172532AD6745EA`.
- This removes one complete CPU scheduler dispatch and two notify-under-lock handoffs from every
  normal native threaded Vulkan frame. It should reduce scheduler CPU time, wakeup/lock traffic,
  and presentation handoff latency, but no device, ADB, install, launch, game, FPS, battery,
  wattage, temperature, or visual run was used, so no whole-game percentage is claimed.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake/Ninja cache while removing
  2,471,604,766 logical bytes of Gradle staging, the ARM64 test ELF, and native helper executables.
  Reported C: free space increased by 2,025,955,328 bytes to 88,974,700,544. The retained `.cxx`
  cache is 2,782,171,675 bytes and `app/build` contains only the final APK.

## 2026-08-17 Vulkan Isotropic Sampler Fidelity and Power

- The initial Vulkan backend commit `dfa2fd0e0` enabled the physical device's maximum supported
  anisotropy on every guest texture sampler, even though PICA sampler state exposes only
  nearest/linear magnification, nearest/linear minification, nearest/linear mip selection, LOD
  bounds/bias, and wrap modes. There is no guest anisotropy control in `regs_texturing.h` or
  `SamplerParams`, and the OpenGL backend does not add anisotropy. Vulkan therefore changed the
  emulated filter and potentially added texture work that the game never requested.
- Vulkan's two final-screen samplers also requested device-maximum anisotropy for both the linear
  and nearest choices. The current Khronos
  [Vulkan sampling specification](https://docs.vulkan.org/spec/latest/chapters/textures.html) makes
  nearest filtering with anisotropy implementation-dependent, so the old nearest screen choice did
  not have deterministic nearest semantics across drivers.
- Guest and final-screen Vulkan samplers now set `anisotropyEnable = false` and
  `maxAnisotropy = 1.0f`. Guest nearest/linear, mip, LOD, wrap, border-color, and comparison state is
  otherwise unchanged. The device feature remains enabled when supported, so a future explicit and
  measured option can still use it without changing device creation.
- Qualcomm Adreno Game Developer Guide 80-78185-2 AL page 84 recommends minimizing texture fetches
  and cache misses and notes that bilinear or nearest can outperform trilinear or anisotropic
  filtering. Its upper bound is sixteen samples for a 16x anisotropic lookup on an affected
  fragment, while adaptive behavior usually makes average application cost much lower and commonly
  under twice isotropic filtering. This change removes that unrequested adaptive work; it does not
  assume every old sample consumed sixteen taps.
- A complete `arm64-v8a` native rebuild passed in 1 minute 40 seconds and linked both the ARM64 test
  ELF and production `libcitra-android.so`. Final AArch64 for `Vulkan::Sampler::Sampler` writes zero
  to the `VkSamplerCreateInfo::anisotropyEnable` word and `0x3f800000` (`1.0f`) to
  `maxAnisotropy`. `RendererVulkan::CompileShaders()` emits the same field pair for both final
  presentation samplers, including the nearest sampler.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed with JDK 17 in 3 minutes
  48 seconds. The APK contains only `arm64-v8a`, is 28,970,223 bytes, and has SHA-256
  `6C21171E5534618D9D96DB5E3E47E9B2F114912F4B8E1563C918C46C9EE188FE`.
- This affects every Vulkan guest texture sampler plus the final screen samplers, so it is a broader
  texture-pipe and power candidate than a one-per-frame CPU synchronization micro-optimization.
  Exact benefit depends on texture angle, minification, cache behavior, scene, and driver. No
  device, ADB, install, launch, game, FPS, battery, wattage, temperature, or visual run was used, so
  no whole-game percentage is claimed. A future allowed matched Thor A/B should hold title, scene,
  save, caches, driver, resolution, layout, performance/fan mode, brightness, and duration fixed;
  compare image correctness and record GPU texture-pipe utilization, frametimes, battery power,
  temperature, and thermal slope.
- Cleanup retained that APK and the active ARM64 RelWithDebInfo CMake/Ninja cache while removing
  2,471,546,637 logical bytes of Gradle staging, the ARM64 test/helper bin directory, and native
  helper tools. Reported C: free space increased by 2,024,697,856 bytes to 88,759,250,944. The
  retained `.cxx` cache is 2,782,216,533 bytes and `app/build` contains only the final APK.

## 2026-08-17 AArch64 PICA Vertex-Cache Lookup

- `PicaCore::LoadVertices()` keeps a fully associative 64-entry circular cache for indexed draws
  that reach CPU-side vertex processing. The baseline AArch64 ThinLTO loop tested a valid byte and
  then one `u16` ID at a time, advancing through 256-byte attribute records. A fully valid miss
  repeated about nine instructions for each of 64 entries, roughly 579 lookup instructions with
  setup and exit. Draws that succeed through hardware vertex acceleration bypass this path.
- The replacement-state proof allows the valid-byte array to disappear. Before the cache fills,
  misses insert sequentially and `[0, vertex_cache_count)` is exactly the valid prefix; hits do not
  advance either count or position. Once count reaches 64, every slot is valid and the original
  circular replacement order continues. Searching only that prefix therefore preserves the old
  fully associative behavior, including the first matching slot if duplicate IDs ever occur.
- AArch64 now compares sixteen IDs per band. Final ThinLTO uses `LDP Q`, two `.8h` `CMEQ`, `UZP1`
  to form the byte mask, `ORN` to select lane indices or `0xff`, and one `.16b` `UMINV` to recover
  the first match. The compiler's `UZP1`/`ORN` are exact simplifications of the source
  `XTN`/`XTN2` and `BSL`; there is no lookup spill. A complete 64-entry miss executes 74 lookup
  instructions including setup and exit instead of about 579, an 87.2% path-local reduction.
  `LoadVertices()` grows from 1,344 to 1,436 bytes, a 92-byte or 6.8% code-size tradeoff.
- This shape follows the checked Snapdragon 8 Gen 2 core manuals. Ordinary vector loads are listed
  on Cortex-X3 page 23, A715 page 26, A710 page 39, and A510 page 32; integer reductions on pages
  26, 29, 43, and 36 respectively; and the select/narrow operations on X3 pages 31-32, A715 pages
  34-35, A710 pages 52-53, and A510 pages 43-44. One reduction covers a broad sixteen-ID band,
  avoiding repeated horizontal dependencies on the reduction-sensitive A510 cores.
- Permanent Catch2 coverage compares the helper with an independent scalar first-match loop for
  all 65 valid-prefix lengths and all 65,536 `u16` values, then checks duplicate and miss cases.
  The full ARM64 build compiled and ThinLTO-linked both the test ELF and production library. A
  stripped 25,849,176-byte test executable was pushed over USB to AYN Thor `c3ca0370`; the focused
  test passed all two assertions in one case in about 250 ms of host-observed time. The device and
  host temporary binaries were removed immediately afterward.
- `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 10 minutes 58 seconds.
  This was an accidentally clean 2,199-action native rebuild after two local Ninja versions reset
  the generated build log, not evidence that the source change itself requires a full rebuild.
  After commit `0bcb6a8e8`, an incremental version-stamped build passed in 1 minute 32 seconds. The
  final APK is ARM64-only, 28,970,267 bytes, v2-signed with the test certificate, reports
  `0bcb6a8e8-vanilla-thor`, and has SHA-256
  `6C30FC4673EE74E3C6A5236D27EA2C8E519EEF05C5001B700E4E83C026488A63`.
- The final APK installed successfully over `org.azahar_emu.azahar.debug` on USB Thor `c3ca0370`.
  Package inspection reports `primaryCpuAbi=arm64-v8a` and the expected version; the package was
  explicitly force-stopped afterward. No game or app UI was launched and no FPS, battery power,
  temperature, or sustained-speed measurement was taken. A matched A/B should use an indexed,
  CPU-vertex-fallback-heavy scene and hold title, save, caches, renderer, driver, resolution,
  layout, performance/fan mode, brightness, and duration fixed before assigning a whole-game or
  wattage benefit.

## 2026-08-17 Per-Game Cached-Data Manager

- Android's game long-press sheet now exposes **Manage Cached Data** instead of a backend picker
  labeled only as shader-cache deletion. The manager is keyed by the selected title ID and reports
  separate human-readable Vulkan and OpenGL shader-cache totals before offering either action.
- Native size accounting covers the same persistent per-title files as the existing deletion
  paths: OpenGL separable/conventional precompiled binaries and its transferable binary; Vulkan
  vertex, fragment, geometry, and pipeline transferable files plus matching pipeline-cache files.
  Filesystem work runs on `Dispatchers.IO`, not the Android UI thread.
- Each backend opens a second confirmation naming the game, backend, and measured size and warns
  that the next run may stutter while the cache rebuilds. Downloaded custom textures remain user
  content managed through **Open › Textures Folder** and are not included in or deleted by this
  manager.
- The dialog explains the distinct lifetime and cost models: a texture-filter result stays in its
  rasterizer surface's scaled GPU image until guest invalidation or upload and clears with the game
  session, while screen-filter Anime4K processes the changing final presentation each frame. This
  avoids promising a disk cache whose hashing, I/O, synchronization, VRAM duplication, and stale
  invalidation costs have not shown a Thor power or speed benefit.
- The dirty-tree release-style ARM64 build passed in 1 minute 37 seconds. After commit
  `38cecd56c`, the incremental version-stamped build passed in 1 minute 23 seconds. The final APK is
  ARM64-only, 28,975,708 bytes, v2-signed with the test certificate, reports
  `38cecd56c-vanilla-thor`, and has SHA-256
  `C86C1A5B4CCA4A061D8FC6C4D7CFF1B5BD3246ECA9363186874F626DDDC22395`.
- The final APK installed successfully on USB Thor `c3ca0370`; package inspection reports
  `primaryCpuAbi=arm64-v8a` and the expected version. In the live library UI, long-pressing 7th
  Dragon III exposed the manager and reported a 1.41 MB Vulkan cache and 225 kB OpenGLES cache.
  Opening the Vulkan action showed the expected title/backend/size warning; it was canceled without
  confirming deletion. Azahar was force-stopped and all temporary UI hierarchy dumps were removed.
  No game was launched and no cache or custom texture was deleted.
- A follow-up through wireless ADB `192.168.1.33:5555` identified the same device as `AYN Thor`
  and reconfirmed the installed ARM64 ABI and `38cecd56c-vanilla-thor` version before force-stop.
  `dumpsys battery` reported AC powered true, USB and wireless powered false, charging status, and
  21% battery. Use Wi-Fi ADB for subsequent Thor work as requested; do not interpret wall-powered
  runs as battery-discharge watt measurements.
- Post-verification cleanup removed 2,470,040,421 logical bytes of Gradle intermediates, native/JNI
  staging, reports, local Gradle state, and the 445,571,808-byte test ELF. The final APK plus its
  476-byte metadata and the active 2,785,959,354-byte ARM64 RelWithDebInfo CMake/Ninja cache remain;
  the final C: free-space check reported 87,739,273,216 bytes.

## 2026-08-17 AArch64 PICA Matching-Lane Compare

- PICA `CMP` writes two conditional-code booleans from the X and Y components. The old AArch64 JIT
  always emitted two scalar `FCMP`/`CSET` pairs plus two lane moves: six instructions after source
  swizzling, even when both lanes selected the same comparison operation.
- Matching operations now use one vector `FCMEQ`, `FCMGT`, or `FCMGE`, move the low 64-bit mask to
  a general register, and extract lane-zero and lane-one sign bits. Equal, less/less-equal, and
  greater/greater-equal therefore use four instructions; `NotEqual` uses five because it inverts
  the equality mask. Mixed operators retain the old scalar path.
- This preserves PICA's prior ordered comparison behavior. An unordered/NaN operand produces false
  for equality and ordered relational operations; inverted equality makes `NotEqual` true. New
  regression coverage exercises all six operations with less, greater, equal, and NaN inputs in
  both the interpreter and AArch64 JIT.
- The checked Cortex-X3, A715, A710, and A510 manuals list AdvSIMD floating compares on pages 28,
  30, 46, and 39 respectively. The operation is available across all Thor core classes rather than
  relying on an optional X3-only extension. Documented compare latency is two cycles on X3/A715/A710
  and three on A510.
- A clean 2,199-action ARM64 native build passed and linked the test ELF and production library.
  Over Wi-Fi ADB `192.168.1.33:5555`, AYN Thor ran the exact `PICA State Access` interpreter and JIT
  cases with 39/39 assertions passing in each. A broader `[shader]` run had 49 of 50 cases pass and
  exposed the existing unrelated `LG2 - ShaderJitTest` mismatch; this change's focused suites pass.
  The stripped device test binary was removed from both host and Thor immediately afterward.
- Commit `2e26caa9f` was pushed before the final build. The release-style
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build passed in three minutes.
  The ARM64-only, v2-signed APK is 28,975,540 bytes, reports
  `2e26caa9f-vanilla-thor`, and has SHA-256
  `3BA2EFB83034ECF1770F5F27E7C1D26CD9D28846EE1FBD909C68EC409167B903`.
- The APK installed successfully over `org.azahar_emu.azahar.debug` on the same Wi-Fi Thor and
  reports `primaryCpuAbi=arm64-v8a`. The device was AC-powered at 52% battery; Azahar was
  force-stopped before and after installation, and no app UI or game was launched. This is a local
  generated-instruction reduction, not a measured whole-game FPS or battery-watt improvement.
- Post-verification cleanup removed 2,022,138,323 logical bytes of Gradle/native staging, test
  output, and rendered manual-page PNGs. `app/build` now contains only the 28,975,540-byte APK and
  its 476-byte metadata; the 3,229,579,753-byte active ARM64 RelWithDebInfo CMake/Ninja cache is
  retained. C: reported 87,276,048,384 bytes free after cleanup.

## 2026-08-17 AArch64 PICA LG2 Signed Exponent Repair

- The complete ARM64 shader suite exposed an old x86-to-AArch64 porting error in the PICA `LG2`
  helper. After extracting the IEEE-754 exponent and subtracting bias 127, AArch64 moved the signed
  bits into a SIMD lane and executed unsigned `UCVTF`. For `0.5`, exponent `-1` was therefore
  interpreted as `0xffffffff` and converted to `4294967296.0`; x64 correctly uses signed
  `cvtsi2ss`.
- AArch64 now converts the unbiased 32-bit GPR exponent directly with scalar `SCVTF`. This restores
  negative exponents and replaces the old `MOV` plus `UCVTF` pair with one instruction, removing
  one generated instruction from every normal positive-input `LG2` helper execution. Polynomial
  coefficients, Horner/FMA order, mantissa reduction, and NaN/zero/negative branches are unchanged.
- The Cortex-X3, A715, A710, and A510 software optimization guides list signed and unsigned FP
  conversion forms in the conversion tables spanning pages 28-29, 30-31, 46-47, and 39-40
  respectively. This is baseline hardware available on every Thor core class; the signed form is
  required by the algorithm rather than an optional X3-only acceleration.
- Permanent regression coverage now checks every exact power-of-two exponent from `-32` through
  `+32`, plus the existing NaN, negative, zero, fractional-mantissa, and large-value cases. On the
  wall-powered Wi-Fi Thor, the exact interpreter and JIT cases each passed 70 assertions, and the
  complete `[shader]` suite passed all 2,276 assertions across 50 test cases. The prior LG2 failure
  is gone; the 25,852,440-byte stripped test ELF was removed from host and device immediately.
- The ARM64 native compile/link passed in 1 minute 17 seconds. Commit `7dd086fad` was pushed before
  the final `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build, which passed in
  2 minutes 1 second. The ARM64-only, v2-signed APK is 28,974,880 bytes, reports
  `7dd086fad-vanilla-thor`, and has SHA-256
  `55404C7006CAD4AEDF225C5AE641BC03BF31E1CA8B1C595BBB4318B27EC97242`.
- The APK installed successfully over `org.azahar_emu.azahar.debug` through Wi-Fi ADB
  `192.168.1.33:5555` and reports `primaryCpuAbi=arm64-v8a`. The device reported AC power, no USB
  power, and 71% battery. Azahar was force-stopped before and after installation; no UI or game was
  launched, so this is correctness plus a local one-instruction reduction, not a whole-game FPS or
  wattage measurement.
- Post-verification cleanup removed 2,022,514,627 logical bytes of Gradle/native staging and manual
  render PNGs. `app/build` now contains only the APK and its 476-byte metadata; the
  3,229,693,469-byte active ARM64 RelWithDebInfo CMake/Ninja cache remains. C: reported
  87,055,941,632 bytes free after cleanup.

## 2026-08-17 AArch64 PICA Reciprocal-Square-Root Refinement

- The x64 PICA shader backend implements `RCP` and `RSQ` with approximate host instructions, while
  AArch64 used exact scalar `FDIV` for `RCP` and exact `FSQRT` followed by `FDIV` for `RSQ`. The
  Cortex-X3, A715, and A710 timing tables list F32 divide/square-root latency at 7-10 cycles,
  reciprocal estimates at three, and refinement steps at four; A510 lists divide at 13, square root
  at 12, and estimate/refinement/multiply at four cycles.
- A temporary ARM64 Android benchmark compared exact operations with one- and two-step hardware
  estimate sequences over 8,388,608 operations, taking the best of five runs with `FPCR=0`. Pinned
  results for exact versus one-step `RSQ` were 8.1428 versus 5.9943 ns/op on CPU 0 (26.4% faster),
  1.4515 versus 0.8241 on CPU 3 (43.2%), 1.3848 versus 0.8300 on CPU 5 (40.1%), and 0.6619 versus
  0.5547 on CPU 7 (16.2%). One-step `RCP` was slower on all four cores, and two-step sequences were
  slower than exact, so `RCP` stays exact and `RSQ` uses exactly one refinement.
- Over 1,000,000 positive-normal values, the selected `RSQ` sequence had maximum relative error
  `1.6128e-5` and maximum error 195 ULP. Positive and negative zero, positive infinity, negative
  infinity, NaN, and negative finite inputs retained the prior result classes and bit patterns.
  Squaring the estimate before `FRSQRTS` is intentional: rearranging the operand as input times
  estimate would disturb the architecture's infinity-times-zero special handling.
- The AArch64 JIT now emits scalar `FRSQRTE`, squares the estimate, applies `FRSQRTS` with the
  original source, and performs the final `FMUL` before broadcasting the lane. Permanent tests cover
  zero plus 8,000 dense positive-normal inputs across exponents `-62..62`; both template backends
  therefore execute 16,000 dense assertions.
- A fresh 2,199-action ARM64 native build passed in 11 minutes 12 seconds. Over Wi-Fi ADB
  `192.168.1.33:5555`, the focused `RSQ*` suite passed all 16,028 assertions in two cases, and the
  complete `[shader]` suite passed all 18,278 assertions in 50 cases. The stripped test executable
  was removed from both host and device. Source commit `10a238446` was pushed to `master`.
- Final release-style packaging passed in 2 minutes 54 seconds. The ARM64-only, v2-signed APK is
  28,975,596 bytes, reports `10a238446-vanilla-thor`, and has SHA-256
  `1A9BD9C26782526D7F5D39FD8EDF8E8F432226EB99D93D8AB4241F82FDABA028`. It installed successfully
  over `org.azahar_emu.azahar.debug` and reports `primaryCpuAbi=arm64-v8a`; Azahar was force-stopped
  before and after installation, and no app UI or game was launched. The Thor reported USB power,
  no AC or wireless power, 80% battery, 4.214 V, and 25.0 C. USB power is not a battery-discharge
  watt measurement.
- Post-verification cleanup removed 2,024,213,566 logical bytes of Gradle/native staging, native
  helper binaries, and the local Gradle cache. `app/build` now contains only the 28,975,596-byte APK
  and its 476-byte metadata; the 3,224,935,167-byte active ARM64 RelWithDebInfo CMake/Ninja cache
  remains. C: reported 86,472,650,752 bytes free after cleanup.
- These are local operation timings and correctness results, not a whole-game FPS or power claim.
  A matched game A/B is still required with title, save, caches, renderer, driver, resolution,
  layout, performance/fan mode, brightness, and duration held constant.

## 2026-08-17 AArch64 PICA DP3 and MOVA Narrowing

- The remaining arithmetic audit found two x64-originated AArch64 costs. `MOVA` converted all four
  float lanes with Q-form `FCVTZS` even though the PICA instruction can consume only X/Y. `DP3`
  zeroed W through a general-register-to-vector lane insertion, then serialized two pairwise adds.
  The x64 backend instead groups the live dot product as `(X + Y) + Z`.
- The Cortex-X3 guide lists normal/pairwise FP add at two-cycle latency and D-form F32 versus Q-form
  F32 conversion at three versus four cycles on pages 28-29. A715 lists normal `FADD` at two versus
  pairwise `FADDP` at three and D/Q conversion at three/four on pages 30-31. A710 lists add forms at
  two and D/Q conversion at three/four on pages 46-47. A510 lists add forms at four on page 39; its
  conversion table spans pages 39-40. These are baseline AdvSIMD operations on every Thor core.
- A self-contained ARM64 benchmark executed 67,108,864 independent conversions and 33,554,432
  four-way interleaved DP3 reductions per sample, taking the best of five runs. Q-form versus D-form
  `FCVTZS` measured 0.501534 versus 0.250587 ns/op on CPU 0, 0.741592 versus 0.370985 on CPU 3,
  0.741922 versus 0.370354 on CPU 5, and 0.339053 versus 0.169341 on CPU 7: essentially 2.00x
  throughput on every core class. Current versus dependency-shortened `DP3` measured 6.948113
  versus 5.391890 ns/op on CPU 0 (22.4% faster), 1.112044 versus 0.926644 on CPU 3 (16.7%),
  1.233326 versus 0.926003 on CPU 5 (24.9%), and 0.612165 versus 0.452924 on CPU 7 (26.0%). The
  benchmark executable was removed from host and device, and rendered manual pages were removed
  from the host.
- AArch64 `MOVA` now emits D-form `.2S` `FCVTZS`, extracts the low 64-bit pair once, and preserves
  the existing per-lane sign extension and destination masks. `DP3` keeps the four-lane sanitized
  multiply, forms X+Y in a scratch scalar while broadcasting Z independently, performs one scalar
  add, and broadcasts the result. W is never part of the reduction, and no FMA or reassociation is
  introduced. New tests cover both MOVA lanes, negative/fractional truncation, ignored exceptional
  Z/W inputs, untouched loop state, DP3 result broadcast, and NaN W inputs.
- The complete ARM64 native target compiled and linked the test ELF plus production library in
  1 minute 48 seconds. On the Wi-Fi Thor, `DP3*` passed 8 assertions in two interpreter/JIT cases,
  `PICA State Access*` passed 96 assertions in two cases, and the full `[shader]` suite passed all
  18,298 assertions in 50 cases. Every earlier zero-match filter invocation was explicitly discarded
  rather than counted. Source commit `20687daae` was pushed to `master`.
- Final release-style packaging passed in 3 minutes 19 seconds. The ARM64-only, v2-signed APK is
  28,975,592 bytes, reports `20687daae-vanilla-thor`, and has SHA-256
  `7F2FA912E6F4DAD6EFBC25417A0E858C2A5B8E956D9712FE9EA8C0B117315A3A`. It installed successfully
  over `org.azahar_emu.azahar.debug`, reports `primaryCpuAbi=arm64-v8a`, and remained force-stopped;
  no Azahar UI or game was launched. At final install the Thor reported USB power, no AC/wireless
  power, 80% battery, 4.228 V, and 22.0 C, which is not a battery-discharge watt measurement.
- Post-verification cleanup removed 2,023,799,217 logical bytes of Gradle/JNI/native staging, native
  helper binaries, and the local Gradle cache. `app/build` retains only the 28,975,592-byte APK and
  its 476-byte metadata; the 3,225,184,280-byte active ARM64 RelWithDebInfo CMake/Ninja cache
  remains. C: reported 86,234,206,208 bytes free after cleanup.
- These are isolated JIT-operation measurements, not whole-game FPS or wattage. A matched title,
  scene, cache, renderer, driver, resolution, layout, mode, fan, brightness, and duration A/B is
  still required before attributing a sustained system-level gain.

## 2026-08-17 AArch64 PICA Partial MOVA Extraction

- The narrowed AArch64 `MOVA` still converted X/Y with one D-form `.2S` `FCVTZS`, but partial
  X-only and Y-only masks then transferred the complete low 64-bit pair to a GPR before a separate
  `SXTW` or `ASR`. A signed element transfer can select either 32-bit lane and sign-extend it into
  the destination GPR in one instruction, removing one generated instruction from every partial
  `MOVA` while preserving the existing truncating conversion.
- The Cortex-X3, A715, and A710 guides list element-to-GPR `UMOV`/`SMOV` at two-cycle latency and
  one-per-cycle throughput on pages 32, 35, and 53 respectively. The A510 guide lists three-cycle
  latency and one-per-cycle throughput on page 44. This is baseline AdvSIMD functionality on all
  Snapdragon 8 Gen 2 core classes, not an X3-only extension.
- A disassembly-checked ARM64 benchmark compared the exact generated sequences over 67,108,864
  partial conversions per sample, alternating A/B order across ten rounds and taking each best
  result. Current versus direct-`SMOV` X extraction measured 2.392305 versus 0.879373 ns/op on CPU 0
  (63.24% faster), 0.463409 versus 0.463997 on CPU 3 (effectively tied), 0.463285 versus 0.370489 on
  CPU 5 (20.03%), and 0.338756 versus 0.338707 on CPU 7 (tied). Y results were 2.399199 versus
  0.882965 (63.20%), 0.463819 versus 0.463686 (tied), 0.463347 versus 0.370460 (20.05%), and
  0.338737 versus 0.338700 ns/op (tied).
- Replacing the packed XY extraction with two `SMOV`s was rejected. Although it helped A510, it was
  26.10% slower on CPU 3, 71.45% slower on CPU 5, and 97.46% slower on CPU 7 because two element
  transfers contend for the documented transfer path. The shipped hybrid therefore uses one
  `SMOV` for X-only or Y-only and retains one packed transfer plus `SXTW`/`ASR` for XY.
- Correctness is exact for the selected path: `SMOV Xd, Vn.S[lane]` produces the same signed
  32-to-64-bit value as the removed packed `UMOV` plus lane extraction. Disabled address registers
  remain untouched. Permanent coverage now includes an explicit negative Y-only case alongside
  X-only preservation, XY truncation, exceptional ignored Z/W inputs, and initial-state checks.
- The full ARM64 native build passed in 1 minute 37 seconds. On Wi-Fi ADB `192.168.1.33:5555`, the
  focused `PICA State Access*` suite passed all 102 assertions in two interpreter/JIT cases and the
  complete `[shader]` suite passed all 18,304 assertions in 50 cases. The 25,862,424-byte stripped
  test ELF and the 8,536-byte benchmark were removed from both host and Thor immediately. Source
  commit `ef555210d` was pushed to `master` before packaging.
- Release-style packaging passed in 2 minutes 55 seconds. The ARM64-only, v2-signed APK is
  28,975,900 bytes, reports `ef555210d-vanilla-thor`, and has SHA-256
  `7474747FA816752AD669E2E7017AFE55759CAC0EEC2A39ADB8623F5D06558EE3`. It installed successfully
  over `org.azahar_emu.azahar.debug`, reports `primaryCpuAbi=arm64-v8a`, and remains force-stopped;
  no UI or game was launched. The Thor reported USB power, no AC/wireless power, 80% battery,
  4.211 V, and 23.0 C, so this is not a battery-discharge watt measurement.
- Post-verification cleanup removed about 2.02 GB of Gradle/JNI/native staging plus the repo-local
  Gradle cache and rendered manual pages. `app/build` retains only the APK and its 476-byte metadata;
  the 3,225,378,046-byte active ARM64 RelWithDebInfo CMake/Ninja cache remains. C: reported
  86,221,090,816 bytes free after cleanup.
- This is an isolated generated-instruction and throughput improvement, not a whole-game FPS or
  wattage result. A matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/
  duration A/B remains required before claiming a sustained system-level gain.

## 2026-08-17 AArch64 PICA Conditional Flow

- The AArch64 PICA condition evaluator still inherited an x64-shaped boolean-materialization
  strategy. OR inverted up to two canonical condition flags into scratch registers, combined them,
  then compared with zero, taking two to four generated instructions. AND used one to three.
  Every caller then assumed that guest-true was host `NE`, even though the truth tables can be
  represented directly by other AArch64 condition codes.
- `COND0` and `COND1` are canonical zero/one values: shader entry loads their C++ `bool` bytes,
  same-operation `CMP` extracts single bits, and mixed comparisons use `CSET`. The sixteen OR/AND
  reference/input combinations therefore reduce exactly to one flag-setting instruction. OR uses
  `CMN/NE` for refs 1/1, `TST/EQ` for 0/0, and `CMP/GE` or `CMP/LE` for mixed refs. AND uses
  `TST/NE`, `CMN/EQ`, or `CMP/GT/LT`. JustX/JustY compare the selected flag with its reference and
  return `EQ`. IFC and CALLC branch on the inverse returned condition; BREAKC and JMPC use it
  directly. Uniform-controlled flow retains its prior zero/nonzero `CMP` behavior.
- The Cortex-X3, A715, A710, and A510 basic arithmetic/logical timing tables on pages 15, 17, 17,
  and 14 respectively list the relevant flag-setting operations as one-cycle-latency instructions.
  A disassembly-checked ARM64 benchmark executed eight evaluations per loop, alternated A/B order
  across seven rounds, and took the best result on every Thor core class. Current versus selected
  one-instruction sequences measured, in ns/evaluation:
  - A510 CPU 0: OR11 0.560538 -> 0.373591 (33.35%), OR00 0.902216 -> 0.372051
    (58.76%), OR10 0.747175 -> 0.372039 (50.21%), AND00 0.750218 -> 0.373479
    (50.22%), and AND10 0.561308 -> 0.373529 (33.45%).
  - A715 CPU 3: 0.292621 -> 0.196484 (32.85%), 0.509576 -> 0.196484 (61.44%),
    0.385394 -> 0.196484 (49.02%), 0.382662 -> 0.196497 (48.65%), and
    0.294664 -> 0.196484 (33.32%).
  - A710 CPU 5: 0.302723 -> 0.212689 (29.74%), 0.476968 -> 0.212677 (55.41%),
    0.387151 -> 0.212739 (45.05%), 0.384922 -> 0.212689 (44.74%), and
    0.302618 -> 0.212739 (29.70%).
  - X3 CPU 7: 0.243131 -> 0.158492 (34.81%), 0.429650 -> 0.156431 (63.59%),
    0.353369 -> 0.159157 (54.96%), 0.341156 -> 0.156394 (54.16%), and
    0.232713 -> 0.161858 (30.45%).
- Permanent IFC coverage already exhausts all sixteen `refx/refy/COND0/COND1` combinations for
  JustX, JustY, OR, and AND in both the interpreter and JIT. New control-flow coverage deliberately
  uses a `CMP/GE` condition whose equal true case would fail an old hard-coded EQ/NE assumption,
  and checks both true and false CALLC, JMPC, and BREAKC paths. The focused `Conditional*` device
  run passed all 140 assertions in four cases; the complete `[shader]` run passed all 18,316
  assertions in 52 cases. The ARM64 native build passed in 1 minute 34 seconds. Temporary test and
  benchmark executables were removed from Thor and host, and rendered manual pages were removed.
  Source commit `341bfc574` was pushed to `master` before packaging.
- Release-style packaging passed in 2 minutes 51 seconds. The ARM64-only, v2-signed APK is
  28,976,928 bytes, reports `341bfc574-vanilla-thor`, and has SHA-256
  `09E2459C73B18A7C70096DF56F200D5BD203ADEC889EFE7651735E0E513E9680`. It installed successfully
  over `org.azahar_emu.azahar.debug`, reports `primaryCpuAbi=arm64-v8a`, and remains force-stopped;
  no app UI or game was launched. Thor reported USB power, no AC/wireless power, 79% battery,
  4.188 V, and 23.0 C. Charging telemetry is not a battery-discharge watt measurement.
- Post-verification cleanup removed 2,052,971,095 logical bytes of Gradle/JNI/native staging,
  native helper binaries, and the repo-local Gradle cache. `app/build` retains only the APK and
  metadata; the 3,225,922,442-byte active ARM64 RelWithDebInfo CMake/Ninja cache remains. C:
  reported 86,168,379,392 bytes free after cleanup.
- This is an isolated generated-condition speedup of 29.7-63.6% in the measured cases, not a
  whole-game FPS or wattage claim. A matched title/scene/cache/renderer/driver/resolution/layout/
  mode/fan/brightness/duration A/B remains required before attributing sustained system-level gain.

## 2026-08-17 Dynarmic Direct A32 NZCV Capture

- Command-line Git over SSH refreshed Azahar `upstream/master` to `fb1c1a710` and found the fork
  133 commits ahead with no upstream-only commit. The official Azahar Dynarmic `master` was fetched
  directly into this repository's object store and still resolves to
  `e77b1ba0b7da7cbe93021b01a663acfe7c4dd516` from 2026-06-24. It retains six ARM64 `RMIF` TODOs;
  the vendored Oaknut has no `RMIF` mnemonic and Dynarmic exposes no FlagM host capability gate, so
  emitting that optional instruction globally would be incorrect.
- `A32SetCpsrNZCV` previously asked the allocator for a temporary GPR. For a flags-backed arithmetic
  result this generated `MRS Xtemp, NZCV` followed by `MOV W23, Wtemp`, even though the fork already
  reserves callee-saved `W23` for guest NZCV. `ReadIntoFixedRegister()` now copies an IR argument
  directly into a fixed register through the existing immediate/GPR/FPR/spill/flags loader. It
  asserts that the destination is absent from the allocator order; the A32 call site targets only
  reserved `X23`. Normal argument-use accounting and `SpillFlags()` behavior remain intact.
- The A710, A715, and X3 special-purpose-register tables on pages 86, 63, and 60 say NZCV is fully
  renamed and its read is neither non-speculative, in-order, nor flush-producing. The A510 guide
  does not publish the comparable table, so all four real core classes were measured. A
  disassembly-checked ARM64 benchmark evaluated 16,777,216 flag results per case, consumed each with
  `TBZ`, alternated current/direct order across nine rounds, and took the best result:
  - A510 CPU 0: 3.087629 -> 2.586969 ns/evaluation, 16.22% faster.
  - A715 CPU 3: 0.504820 -> 0.403201 ns/evaluation, 20.13% faster.
  - A710 CPU 5: 0.516344 -> 0.416782 ns/evaluation, 19.28% faster.
  - X3 CPU 7: 0.389681 -> 0.380737 ns/evaluation, 2.30% faster.
- A nearby two-instruction idea was deliberately rejected. Replacing uniform `LDRB; CMP; B.cond`
  with `LDRB; CBZ/CBNZ` removed an instruction but made the two taken patterns 24.18%/22.71% slower
  on A510 and 45.99%/38.93% slower on A715. A510 fallthrough improved about 44%; A715 fallthrough
  tied, and A710/X3 tied or moved only within noise. The manual throughput rows did not capture the
  branch-direction cost, so the production PICA sequence remains unchanged.
- Permanent `[core][arm][dynarmic]` coverage runs real guest `ADDS R0, R0, #1` followed by MI, VS,
  CS, and EQ conditional moves across linked A32 blocks. N/V, Z/C, no-flags, and N-only inputs passed
  all 24 assertions. The broader `[core]~[file_sys]` device run passed 2,891 assertions in 16 cases;
  the excluded Android path-parser case is a pre-existing host-filesystem fixture that expects a
  generated `get_build_flavor` file. Native ARM64 builds linked the test ELF and
  `libcitra-android.so`; source commit `2d39584f4` was pushed to `origin/master` before packaging.
- A complete 2,200-action ARM64 native rebuild and release-style package passed in 14 minutes 3
  seconds. The ARM64-only, v2-signed APK is 28,976,840 bytes, reports
  `2d39584f4-vanilla-thor`, and has SHA-256
  `141F9311E5B9910031674508F4A1BE1269A8F54EEBC28837F9E6FB08969683E6`. It installed over
  `org.azahar_emu.azahar.debug` by wireless ADB, reports `primaryCpuAbi=arm64-v8a`, and remains
  force-stopped; no UI or game was launched. The Thor reported USB power, no AC/wireless power,
  78% battery, 4.154 V, and 23.0 C, so this is not battery-discharge watt evidence.
- Post-verification cleanup removed 2,056,113,736 logical bytes of Gradle/JNI/native staging, the
  repo-local Gradle cache, benchmark/test helpers, and rendered manual pages. C: free space rose by
  1,608,404,992 bytes. `app/build` retains only the 28,976,840-byte APK and 476-byte metadata; the
  3,230,823,924-byte active ARM64 RelWithDebInfo CMake/Ninja cache remains for incremental work.
- This is optimization 79 in the current Thor work tally and a 2.3%-20.1% win for one recurring
  generated sequence, not the whole emulator. The 79 changes overlap, trigger in different games,
  and include rejected/UX/power-oriented work; their percentages must not be added. Whole-game FPS,
  sustained power, frametime, and thermal gains still require a matched title/scene A/B.

## 2026-08-17 Dynarmic ARM64 Final-Use Read/Write Coalescing

- Dynarmic's x64 allocator already updates an eligible final-use read/write operand in its current
  host register, but the ARM64 allocator always allocated a new output register and copied the old
  value first. The ARM64 path now transfers that physical location to the output when the input is
  non-immediate, has the same host-register class, has exactly one remaining IR use and one active
  lock, and the output has not already been realized. Shared or otherwise ineligible values retain
  the original allocate-and-copy path.
- `HostLocInfo::ReplaceLastUseWith()` changes only the final value owner. `RAReg` records a reused
  read location so its destructor unlocks the output value without erasing the physical location it
  now owns. This removes copies from eligible SHA-256, saturating vector accumulate, vector-element
  insertion, VTBX default, vector FMA, and FP16 absolute-value lowerings without changing their
  emitted operation or the allocator's spill fallback.
- A disassembly-checked benchmark compared the old explicit full-vector copy with the coalesced
  form over 16,777,216 useful operations, alternated order for nine rounds, and used the best sample.
  An initial version accidentally serialized four nominal chains through one temporary and its
  numbers were discarded. The corrected benchmark kept four independent chains. Nanoseconds per
  useful read/write operation, throughput multiple, and time reduction were:

  | Thor core | FMLA old -> new | FMLA gain | BIC old -> new | BIC gain |
  | --- | --- | --- | --- | --- |
  | A510 CPU 0 | 2.756224 -> 1.250332 | 2.204x; 54.6% less time | 2.501638 -> 1.001578 | 2.498x; 60.0% less time |
  | A715 CPU 3 | 0.543852 -> 0.189565 | 2.869x; 65.1% less time | 0.353695 -> 0.190552 | 1.856x; 46.1% less time |
  | A710 CPU 5 | 0.648188 -> 0.189363 | 3.423x; 70.8% less time | 0.389752 -> 0.187444 | 2.079x; 51.9% less time |
  | X3 CPU 7 | 0.592520 -> 0.169103 | 3.504x; 71.5% less time | 0.338578 -> 0.169240 | 2.001x; 50.0% less time |

- A complete 2,200-action ARM64 native rebuild and release-style package passed in 14 minutes 8
  seconds. A permanent A32 VTBX regression then rebuilt in 38 seconds and passed on Thor together
  with the existing linked-block flag case: 32 assertions in two `[core][arm][dynarmic]` cases. The
  broader `[core]~[file_sys]` run passed 2,899 assertions in 17 cases. Source commit `a9aada95d`
  and test commit `6ca666b71` were pushed to `origin/master` through command-line Git over SSH.
- The ARM64-only, v2-signed APK is 28,979,556 bytes, reports
  `a9aada95d-vanilla-thor`, and has SHA-256
  `54EB796EE6854BDD3FB4AD1623A79706F7E0E7D5FA295314D64011489D00AC09`. It installed over
  `org.azahar_emu.azahar.debug` by wireless ADB, reports `primaryCpuAbi=arm64-v8a`, and remains
  force-stopped; no app UI or game was launched. Thor reported USB power, no AC/wireless power,
  78% battery, 4.126 V, and 23.0 C, so this is not battery-discharge watt evidence.
- Temporary benchmark/test executables were removed from host and device. Post-verification cleanup
  removed 2,017,514,496 logical bytes of reproducible Gradle/JNI/native staging and increased C:
  free space by 1,579,499,520 bytes. `app/build` retains only the APK and 476-byte metadata; the
  3,243,275,791-byte active ARM64 RelWithDebInfo CMake/Ninja cache remains for incremental work.
- This is optimization 80 in the current Thor tally. The measured result is a 1.86x-3.50x synthetic
  throughput gain, or 46.1%-71.5% less time, only when these recurring read/write sequences and
  final-use lifetimes occur. It cannot be added to the other 79 items. Whole-game FPS, sustained
  watts, frametimes, and thermals still require a matched title/scene A/B.

## 2026-08-17 Dynarmic ARM64 Packing and Select Move Elimination

- Three remaining ARM64 lowerings still paid for x86-shaped result materialization after final-use
  coalescing landed. `Pack2x32To1x64` copied its low word before `BFI`, `LeastSignificantWord`
  copied the low 32 bits of an existing 64-bit value, and `PackedSelect` copied its GE mask before
  destructive `BSL`. The first and third now use the conservative final-use read/write allocator;
  the low-word operation is a zero-code `DefineAsExisting()` alias. Shared or otherwise ineligible
  values retain the allocate-and-copy fallback.
- The Arm manuals confirm that the surviving operations use baseline pipelines on every Thor core.
  `BFM`/`BFI` is documented on X3 page 18, A715 page 20, A710 page 27, and A510 page 22; `BSL` is
  on X3 page 31, A715 page 34, A710 page 52, and A510 page 43. Removing the preceding move avoids
  real dependency/issue work and assumes no optional ISA extension.
- A disassembly-checked AArch64 benchmark ran 16,777,216 useful operations over four independent
  chains, alternated old/new order across nine rounds, took the best sample, and verified equal
  checksums. An initial packed-select version violated AAPCS64 by clobbering callee-saved SIMD
  registers; those numbers were discarded, the helper was corrected to caller-saved registers,
  and final disassembly verified the exact old and new sequences. Results were:

  | Thor core | Pack 32x2 old -> new | Low word old -> new | Packed SEL old -> new |
  | --- | --- | --- | --- |
  | A510 CPU 0 | 0.625756 -> 0.249660 ns/op; 2.506x | 0.624926 -> 0.294328; 2.123x | 2.763567 -> 2.008003; 1.376x |
  | A715 CPU 3 | 0.225835 -> 0.159822; 1.413x | 0.244629 -> 0.165706; 1.476x | 0.539208 -> 0.370929; 1.454x |
  | A710 CPU 5 | 0.208508 -> 0.184927; 1.128x | 0.209658 -> 0.162218; 1.292x | 0.556094 -> 0.370510; 1.501x |
  | X3 CPU 7 | 0.178597 -> 0.169376; 1.054x | 0.148312 -> 0.102176; 1.452x | 0.254074 -> 0.231546; 1.097x |

- Permanent tests execute real A32 `UMLAL` to cover packed low/high results and real A32 `SEL`
  across all 16 GE masks, including GE preservation. The focused device suite passed 66 assertions
  in four cases; the broader `[core]~[file_sys]` run passed 2,933 assertions in 19 cases. Two
  incremental ARM64 native builds passed in 1 minute 28 seconds and 1 minute 7 seconds. Temporary
  benchmark/test sources and binaries were removed from both host and Thor. Source/test commit
  `fecae1a30` was pushed directly to `origin/master` over command-line Git SSH before packaging.
- The JDK 17 release-style package passed in 2 minutes 36 seconds. The ARM64-only, v2-signed APK is
  28,976,828 bytes, reports `fecae1a30-vanilla-thor`, and has SHA-256
  `DE4D278DCD87BB81056990CEC6FEED366CEB4C2261EA3A8FBC79838130293157`. Wireless ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reports `primaryCpuAbi=arm64-v8a`, `stopped=true`, and
  no process ID. No UI or game was launched. Thor reported USB power, no AC/wireless power, 79%
  battery, 4.145 V, and 22.0 C, so this charging snapshot is not battery-discharge watt evidence.
- Post-verification cleanup removed 2,017,554,358 logical bytes from `app/build` and increased C:
  free space by 1,589,624,832 bytes. The build directory retains only the APK and its 476-byte
  metadata; the 3,243,488,163-byte active ARM64 RelWithDebInfo CMake/Ninja cache remains for
  incremental work.
- This is optimization 81 in the current Thor work tally. Its measured 1.05x-2.51x result applies
  only to these exact generated sequences. The 81 items overlap, trigger in different workloads,
  and include rejected, UX, and power-oriented work; their gains cannot be added. Whole-game FPS,
  sustained watts, frametimes, and thermals still require a matched title/scene A/B.

## 2026-08-18 Dynarmic ARM64 Signed-Narrow Fusion

- The A32 frontend contains 133 byte/halfword low-part construction sites and at least 59 direct
  textual narrow-plus-sign-extension expressions. Guest `SXTB`, `SXTH`, signed DSP operations, and
  halfword multiplies could therefore lower to `UXTB; SXTB` or `UXTH; SXTH`: the unsigned narrow
  canonicalized a value whose sole next consumer immediately discarded the same upper bits again.
- The ARM64 emitter now aliases the narrow result only when it has exactly one use, the immediately
  following IR instruction is the matching byte/halfword signed extension, and argument zero points
  directly to that producer. `SXTB`/`SXTH` then performs truncation and sign extension in one
  instruction. This O(1) check adds no block scan or lookup allocation. Shared, non-adjacent, zero-
  extension, store, shift, and unknown-consumer paths retain `UXTB`/`UXTH`.
- The X3 page 18, A715 page 20, A710 pages 27-28, and A510 pages 22-23 tables cover the baseline
  `UBFM`/`SBFM` family underlying these aliases. Removing one bitfield operation reduces a true
  dependency and integer-pipeline work on every Thor core without an optional ISA assumption.
- A disassembly-checked benchmark compared four independent old and fused chains over 16,777,216
  iterations, alternated order for nine rounds, selected the best samples, and verified equal
  checksums:

  | Thor core | Byte `UXTB; SXTB` -> `SXTB` | Half `UXTH; SXTH` -> `SXTH` |
  | --- | --- | --- |
  | A510 CPU 0 | 0.627656 -> 0.250822 ns/op; 2.502x; 60.04% less time | 1.132231 -> 0.251382; 4.504x; 77.80% less time |
  | A715 CPU 3 | 0.236033 -> 0.141632; 1.667x; 39.99% | 0.235977 -> 0.141353; 1.669x; 40.10% |
  | A710 CPU 5 | 0.273183 -> 0.155802; 1.753x; 42.97% | 0.276155 -> 0.152790; 1.807x; 44.67% |

  A710 CPU 6 independently measured 1.856x/1.778x for byte/halfword. CPU 7 reported online but
  rejected both the benchmark and `/system/bin/true` with a single-bit affinity mask as `EINVAL`;
  no X3 measurement is claimed for this run.
- Permanent guest coverage executes A32 `SXTB`, `SXTH`, and `SMULBB` with dirty upper bits, plus a
  register-controlled `LSL` whose shift register is `0xffff0001`. That last result proves narrowing
  remains on the non-sign-extension path. Thor passed 70 assertions in five focused cases and 2,937
  assertions in 20 broader core cases. The ARM64 native build passed in 1 minute 45 seconds;
  temporary opcode-check, test, and benchmark files were removed from host and device. Source/test
  commit `fc067c02f` was pushed directly to `origin/master` through command-line Git SSH.
- JDK 17 release packaging passed in 2 minutes 28 seconds. The ARM64-only, v2-signed APK is
  28,977,844 bytes, reports `fc067c02f-vanilla-thor`, and has SHA-256
  `CBF8900D2E85268BA4AB19713C55F9E7D4FC08C5880986A493E754B95D9D9894`. It installed over
  `org.azahar_emu.azahar.debug`; Android reports `primaryCpuAbi=arm64-v8a`, `stopped=true`, and no
  process ID. No UI or game was launched. Thor reported USB power, no AC/wireless power, 80%
  battery, 4.156 V, and 21.0 C, so this is not a matched battery-discharge watt measurement.
- Cleanup removed 2,017,571,882 logical bytes from `app/build` and raised C: free space by
  1,581,228,032 bytes. Only the APK and 476-byte metadata remain there; the 3,244,305,971-byte
  active ARM64 RelWithDebInfo CMake/Ninja cache remains for incremental work.
- This is optimization 82 in the Thor work tally. The 1.67x-4.50x figures apply only to the exact
  fused sequences when those IR patterns occur. The 82 items overlap and cannot be added; matched
  title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness A/B runs remain necessary
  for whole-game FPS, sustained watts, frametime, or thermal claims.

## 2026-08-18 Dynarmic ARM64 Register-Shift Mask Elision

- A32 register-controlled data processing constructs its shift count with
  `LeastSignificantByte(GetRegister(...))` at 21 ARM/Thumb translation sites. On ARM64 that
  producer emits `UXTB`, but the no-carry LSL, LSR, and ASR lowerings immediately masked the same
  host register again with `AND #0xff`. The second operation could not change the value.
- The emitter now recognizes only a shift argument that resolves through identities to
  `LeastSignificantByte` and uses that register directly for the variable shift and range compare.
  This is deliberately not a general U8 invariant: callback-returned bytes and future producers
  retain the old mask. Carry-producing shift paths also remain unchanged. A shift consumer cannot
  trigger the adjacent signed-extension fusion, so the recognized producer necessarily emitted
  its canonicalizing `UXTB`.
- The checked Cortex tables put the removed logical operation and surviving variable shift on the
  same integer resources. `AND` has latency/throughput 1/6 on X3 page 15, 1/4 on A715/A710 page 17,
  and 1/3 on A510 page 14. `LSLV`/`LSRV`/`ASRV` have 1/6 on X3 page 18, 1/4 on A715 page 20 and A710
  page 27, and 1/3 on A510 page 22. Removing the duplicate therefore saves one dependency and one
  integer issue without assuming an optional extension.
- A disassembly-checked benchmark retained the frontend `UXTB` and compared the exact old and new
  LSL and clamped-ASR sequences over four independent chains and 16,777,216 iterations. Nine
  rounds alternated old/new order, selected each best sample, and required equal nonzero checksums:

  | Thor core | LSL old -> new | ASR old -> new |
  | --- | --- | --- |
  | A510 CPU 0 | 2.136812 -> 1.510237 ns/op; 1.415x; 29.32% less time | 2.640345 -> 2.135542; 1.236x; 19.12% |
  | A715 CPU 3 | 0.535285 -> 0.417087; 1.283x; 22.08% | 0.499109 -> 0.388098; 1.286x; 22.24% |
  | A710 CPU 5 | 0.501662 -> 0.422252; 1.188x; 15.83% | 0.485471 -> 0.390124; 1.244x; 19.64% |

  CPU 6 and CPU 7 reported online but rejected harmless single-bit affinity probes during the
  final run, so no second-A710 or X3 measurement is claimed.
- Permanent guest coverage executes non-flags LSL, LSR, ASR, and ROR plus carry-producing LSLS for
  dirty-upper-bit shift registers whose low bytes are 0, 1, 31, 32, 33, and 255. It verifies the
  complete result and carry semantics at every ARM edge. Thor passed 106 assertions in six focused
  Dynarmic cases and 2,973 assertions in 21 broader core cases. The ARM64 native build passed in
  85.49 seconds. Temporary opcode, test, benchmark, disassembly, and rendered-manual files were
  removed from host and device. Source/test commit `169306159` was pushed directly to
  `origin/master` through command-line Git SSH.
- JDK 17 release packaging passed in 2 minutes 33 seconds. The ARM64-only, v2-signed APK is
  28,977,464 bytes, reports `169306159-vanilla-thor`, and has SHA-256
  `EBD13F4D4493F8415BF4358242B413CBC733AA0B0221EA0367EBA04D24851619`. It installed over
  `org.azahar_emu.azahar.debug`; Android reports `primaryCpuAbi=arm64-v8a`, `stopped=true`, and no
  process ID. No UI or game was launched. Thor reported USB power, no AC/wireless power, 80%
  battery, 4.155 V, and 21.0 C, so this is not battery-discharge watt evidence.
- Cleanup removed 2,017,617,383 logical bytes from `app/build` and raised C: free space by
  1,576,497,152 bytes. Only the APK and 476-byte metadata remain there; the 3,244,522,777-byte
  active ARM64 RelWithDebInfo CMake/Ninja cache remains for incremental work.
- This is optimization 83 in the Thor work tally. Its 1.19x-1.42x LSL and 1.24x-1.29x ASR results
  apply only to these exact generated sequences. The 83 items overlap, trigger in different title
  workloads, and cannot be added. Whole-game FPS, sustained watts, frametime, and thermal claims
  still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness
  A/B run.

## 2026-08-18 Dynarmic ARM64 Sole-Consumer Shift-Byte Fusion

- Optimization 83 still materialized the frontend `LeastSignificantByte` as `UXTB`. For an A32
  register shift, that byte value normally has exactly one eventual use as the shift instruction's
  count, with only register/flag reads between producer and consumer. The ARM64 allocator can keep
  the raw source live through those reads, so the byte result can alias it without a host
  instruction when the sole consumer is 32-bit LSL, LSR, or ROR.
- AArch64 variable shifts consume only bits 4:0. That directly matches A32 ROR's low-byte count
  modulo 32. For no-carry LSL/LSR, `TST Wcount, #0xe0` examines only low-byte bits 7:5: EQ means
  the A32 count is 0..31 and the variable-shift result is valid; non-EQ means 32..255 and selects
  zero. Dirty source bits above bit 7 affect neither operation. Existing carry paths retain their
  low-byte zero/range checks, while their variable shifts also need only bits 4:0.
- The gate requires one use, finds that eventual consumer, accepts only its shift-count argument,
  and recognizes only LSL/LSR/ROR. Shared values, stores, extensions, unknown consumers, and generic
  U8 producers retain `UXTB` and their masks. ASR deliberately retains optimization 83's canonical
  path: a candidate `MOV 31; TST #0xe0; CSEL; ASRV` sequence helped A510 by roughly 23%, but repeated
  A715 runs were 0.9%-4.9% slower and A710 improved by only 0.6%-1.1%.
- The checked manual tables list basic/flag-setting logical operations on X3 page 15, A715/A710
  page 17, and A510 page 14. `UBFM`/`UXTB` and variable shifts are on X3 page 18, A715 page 20,
  A710 page 27, and A510 page 22. These are real integer/ALU operations on every Thor core class;
  removing one reduces instruction fetch/decode/issue work without an optional ISA feature.
- A disassembly-checked benchmark used four independent chains, 16,777,216 iterations, nine
  alternating-order rounds, best samples, and equal nonzero checksums. It compared optimization
  83's exact sequence with the accepted raw-count sequence:

  | Thor core | LSL old -> new | LSR old -> new | ROR old -> new |
  | --- | --- | --- | --- |
  | A510 CPU 0 | 1.509530 -> 0.752534 ns/op; 2.006x; 50.15% less time | 1.506653 -> 0.627943; 2.399x; 58.32% | 1.132371 -> 0.250024; 4.529x; 77.92% |
  | A715 CPU 3 | 0.392147 -> 0.314307; 1.248x; 19.85% | 0.392278 -> 0.314244; 1.248x; 19.89% | 0.209037 -> 0.160285; 1.304x; 23.32% |
  | A710 CPU 5 | 0.388550 -> 0.320123; 1.214x; 17.61% | 0.388614 -> 0.320318; 1.213x; 17.57% | 0.204095 -> 0.150064; 1.360x; 26.47% |

  CPU 6 and CPU 7 reported online but rejected harmless single-bit affinity probes with `EINVAL`,
  so no second-A710 or X3 result is claimed.
- Permanent guest coverage now checks carry-producing LSLS, LSRS, ASRS, and RORS separately for
  dirty-upper-bit count registers whose low bytes are 0, 1, 31, 32, 33, and 255, in addition to
  the no-flags coverage. The final ARM64 build passed in one minute. Thor passed 154 assertions in
  seven focused Dynarmic cases and 3,021 assertions in 22 broader core cases. Temporary test,
  benchmark, disassembly, and rendered-manual files were removed from host and device. Source/test
  commit `e9aa683d4` was pushed directly to `origin/master` through command-line Git SSH.
- JDK 17 release packaging passed in 2 minutes 28 seconds. The 28,977,696-byte APK is ARM64-only,
  v2-signed, reports `e9aa683d4-vanilla-thor`, and has SHA-256
  `F3EA150A076C0682D70A7D24DE37EC3559D29CF360433550DC2E6C7927F34A50`. It installed over
  `org.azahar_emu.azahar.debug` via Wi-Fi and was force-stopped with no process ID; no UI or game
  was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.155 V, and 20.0 C,
  so this is not a battery-discharge watt measurement.
- Cleanup removed 2,017,658,292 logical bytes from `app/build` and raised C: free space by
  1,577,951,232 bytes. Only the APK and 476-byte metadata remain there; the 3,244,726,683-byte
  active ARM64 RelWithDebInfo CMake/Ninja cache remains for incremental work.
- This is optimization 84 in the Thor work tally. Its 1.21x-4.53x figures apply only to these exact
  generated sequences when the sole-consumer gate fires. The 84 items overlap and cannot be added.
  Whole-game FPS, sustained watts, frametime, and thermal claims still require a matched title,
  scene, cache state, renderer, driver, resolution, layout, performance mode, fan, and brightness
  A/B run.

## 2026-08-18 Dynarmic A32 Scalar Long-Multiply Lane Broadcast

- A32 scalar `VMULL`, `VMLAL`, and `VMLSL` previously constructed their scalar operand as
  `VectorGetElement()` followed immediately by `VectorBroadcast()`. On the ARM64 backend that
  crossed from SIMD to a general register with `UMOV` and then crossed back with the general-
  register form of `DUP`. The other scalar NEON multiply families already used the direct
  `VectorBroadcastElement()` form.
- The long-multiply translator now uses that direct element broadcast too. The selected 16-bit or
  32-bit lane and the replicated vector are bit-identical; the signed/unsigned widening multiply
  and optional vector add/subtract remain unchanged. Generated preparation falls from
  `UMOV; DUP (general register)` to one `DUP (element)`, eliminating a cross-register-bank
  dependency and one host instruction without an optional ISA feature.
- The complete relevant timing-table pages were rendered and visually checked. X3 pages 31-32,
  A715 pages 34-35, and A710 pages 52-53 list element `DUP` at two-cycle latency, while the old
  route adds a two-cycle `UMOV` and uses the three-cycle, one-per-cycle general-register `DUP`.
  A510 pages 43-44 list three cycles for element `DUP`, `UMOV`, and general-register `DUP`, with
  the direct element form also avoiding the second instruction and general-register handoff.
- A disassembly-checked benchmark executed eight independent broadcasts per loop for 16,777,216
  iterations, alternated old/new order across nine rounds, selected each best sample, and required
  equal nonzero checksums. Nanoseconds per useful broadcast were:

  | Thor core | 16-bit `UMOV; DUP` -> element `DUP` | 32-bit `UMOV; DUP` -> element `DUP` |
  | --- | --- | --- |
  | A510 CPU 0 | 3.016938 -> 0.502268; 6.007x; 83.35% less time | 3.019914 -> 0.503150; 6.002x; 83.34% |
  | A715 CPU 3 | 0.358605 -> 0.179209; 2.001x; 50.03% | 0.358586 -> 0.179214; 2.001x; 50.02% |
  | A715 CPU 4 | 0.358444 -> 0.179187; 2.000x; 50.01% | 0.358347 -> 0.179237; 1.999x; 49.98% |

  CPUs 5 and 7 reported online at 2.8032 and 3.1872 GHz but rejected both direct and Android
  `taskset` single-bit affinity with `EINVAL`; no A710 or X3 benchmark number is claimed for this
  run.
- Permanent guest coverage executes `VMULL.S16`, `VMLAL.U16`, `VMLSL.S32`, and `VMULL.U32` with
  different scalar-lane indices, signed extremes, unsigned accumulator wrap, subtraction, and
  complete 64-bit results. Thor passed 166 assertions in eight focused `[core][arm][dynarmic]`
  cases and 3,033 assertions in 23 broader `[core]~[file_sys]` cases. The ARM64 native build linked
  the test ELF and `libcitra-android.so` in 1 minute 21 seconds. Source/test commit `f63697ee0` was
  pushed directly to `origin/master` over command-line Git SSH before packaging.
- JDK 17 release packaging passed in 2 minutes 32 seconds. The ARM64-only, v2-signed APK is
  28,976,576 bytes, reports `f63697ee0-vanilla-thor`, and has SHA-256
  `0132573765AAAB8E4D188AE3FE43F836137300D5EEAD79213270406D58AD5FAF`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and was force-stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.154 V, and
  20.0 C, so this charging snapshot is not battery-discharge watt evidence.
- Temporary test/benchmark binaries and rendered manual pages were removed from host and Thor.
  Post-verification cleanup removed 2,017,682,102 logical bytes of Gradle/JNI/native staging and
  raised C: free space by 1,608,740,864 bytes. `app/build` retains only the 28,976,576-byte APK and
  476-byte metadata; the 3,238,891,722-byte active ARM64 RelWithDebInfo CMake/Ninja cache remains
  for incremental work.
- This is optimization 85 in the Thor work tally. Its 2.00x-6.01x result applies only to the scalar
  lane-broadcast preparation used by these guest long multiplies. The 85 items overlap, trigger in
  different workloads, and cannot be added. Whole-game FPS, sustained watts, frametime, and thermal
  gains still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/
  brightness A/B.

## 2026-08-18 Dynarmic A32 VZIP D-Register SIMD Retention

- A32 D-register `VZIP.8` and `VZIP.16` already formed both results in one U128
  `VectorInterleaveLower()`, but then extracted its two 64-bit halves with `VectorGetElement()` and
  wrote them with `SetExtendedRegister()`. On ARM64, each extraction emitted an element-to-GPR
  `UMOV`; each D-register write immediately reconstructed the same SIMD value with a GPR-to-D
  `FMOV`. Four cross-register-bank transfers surrounded values that never needed to leave SIMD.
- The D form now writes the interleave result's low half with `SetVector()` and uses
  `VectorRotateWholeVectorRight(..., 64)` before writing the high half. The ARM64 backend consumes
  the first D value directly and lowers the rotation to one `EXT #8`. Guest lane order is unchanged,
  the existing Q-register path is untouched, and the decoder's existing rejection of the undefined
  D-form 32-bit size remains intact. Result preparation falls from
  `ZIP1; UMOV; UMOV; FMOV; FMOV` to `ZIP1; EXT`: five host instructions to two, with no optional
  ISA feature.
- The complete relevant timing-table pages were rendered and visually checked. X3 pages 31-32 list
  `EXT` at two-cycle latency and throughput four, versus throughput one for element-to-general-
  register `UMOV`. A715 pages 34-35 and A710 pages 52-53 list `EXT` at two-cycle latency and
  throughput two, again versus throughput one for `UMOV`. A510 pages 43-44 place `EXT`, `UMOV`, and
  the unzip/zip family on its VALU paths; the new sequence removes the two `UMOV`s and the two
  reverse-bank `FMOV`s there as well.
- A disassembly-checked benchmark compared eight independent old and new result paths, retained two
  identical D-width consumers per operation, ran 8,388,609 iterations, alternated order over nine
  rounds, selected each best sample, and required equal nonzero `0x81` checksums:

  | Thor core | Old -> new | Local result-path gain |
  | --- | --- | --- |
  | A510 CPU 0 | 6.665553 -> 4.020711 ns/op | 1.658x; 39.68% less time |
  | A715 CPU 3 | 1.066110 -> 0.728358 ns/op | 1.464x; 31.68% less time |
  | A715 CPU 4 | 1.066017 -> 0.733805 ns/op | 1.453x; 31.16% less time |

  Only the currently usable CPU 0/3/4 single-bit affinity masks were measured; no A710 or X3
  number is claimed for this run.
- Permanent guest coverage executes low-register `VZIP.8 D0, D1` and high-register
  `VZIP.16 D30, D31` with non-repeating lanes and verifies all four complete 64-bit outputs. Thor
  passed 174 assertions in nine focused `[core][arm][dynarmic]` cases and 3,041 assertions in 24
  broader `[core]~[file_sys]` cases. The ARM64 native build passed in 1 minute 18 seconds. Temporary
  assembler, benchmark, stripped-test, disassembly, and rendered-manual files were removed from
  both host and device. Source/test commit `18b35d600` was made directly on `master`.
- JDK 17 release packaging passed in 2 minutes 39 seconds. The ARM64-only, v2-signed APK is
  28,977,540 bytes, reports `18b35d600-vanilla-thor`, and has SHA-256
  `5FD34C294FA02032BB21C5B83FB4CDABF97C7BBD2BD8FCF3E43A644CF93A713A`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and was force-stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.153 V, and
  20.0 C, so this charging snapshot is not battery-discharge watt evidence.
- Post-verification cleanup removed 2,017,695,829 logical bytes from `app/build` and raised C: free
  space by 1,576,329,216 bytes. `app/build` retains only the 28,977,540-byte APK and 476-byte
  metadata; the 3,245,100,241-byte active ARM64 RelWithDebInfo CMake/Ninja cache remains for
  incremental work.
- This is optimization 86 in the Thor work tally. Its 1.45x-1.66x result applies only when an A32
  guest executes the D-register VZIP forms and only to this exact result path. The 86 items overlap,
  trigger at different frequencies, and cannot be added. Whole-game FPS, sustained watts,
  frametime, and thermal gains still require a matched title/scene/cache/renderer/driver/resolution/
  layout/mode/fan/brightness A/B run.

## 2026-08-18 Dynarmic Native Widening Absolute Difference

- A32 guest `VABDL`/`VABAL` first extracted each 64-bit source from its SIMD register with
  `VectorGetElement(64)`, reconstructed a D value with `ZeroExtendToQuad()`, and widened it with
  `VectorZeroExtend()` before taking the absolute difference. The ARM64 emitter therefore paid
  `UMOV; FMOV; UXTL` for each source plus `SABD`/`UABD`: seven host instructions with two round
  trips across the SIMD/GPR register banks. The equivalent A64 guest path used two separate
  extensions plus the difference.
- Dynarmic now has signed and unsigned 8/16/32-bit widening-absolute-difference IR operations. A32
  feeds the original D-register values directly; A64 feeds the selected low/high 64-bit part; and
  accumulation remains a widened-lane `VectorAdd()` after the difference. The ARM64 backend emits
  one baseline `SABDL`/`UABDL`. x64 expands the new operation back into the established same-width
  difference plus zero extension, preserving portability without requiring an x64-specific ISA.
- The actual Snapdragon core guides were rendered and visually checked. The low-half
  `SABDL`/`UABDL` form is listed at latency 2 / throughput 4 on Cortex-X3 issue 4.0 page 25,
  latency 2 / throughput 2 on Cortex-A715 issue 5.0 page 28 and Cortex-A710 issue 4.0 page 42, and
  latency 3 with the table's low/high-form throughput notation `2,1` on Cortex-A510 issue 6.0 page
  35. The implementation uses the faster low-half form because the selected guest D value is
  already in the low 64 bits.
- A disassembly-checked benchmark compared eight independent repetitions of the exact old
  `UMOV; FMOV; UXTL; UMOV; FMOV; UXTL; UABD` path with eight native `UABDL` instructions. It ran
  2,000,000 loop iterations per sample (16,000,000 operations), alternated A/B order over seven
  rounds, selected each best sample, and required identical nonzero `0x00fe000000fe0001` results:

  | Thor core | Old -> new | Local widening-difference gain |
  | --- | --- | --- |
  | A510 CPU 0 | 7.998265 -> 0.500492 ns/op | 15.981x; 93.74% less time |
  | A715 CPU 3 | 1.030244 -> 0.178369 ns/op | 5.776x; 82.69% less time |
  | A715 CPU 4 | 1.020482 -> 0.178372 ns/op | 5.721x; 82.52% less time |
  | A710 CPU 6 | 1.457829 -> 0.178369 ns/op | 8.173x; 87.76% less time |

  The device MIDRs identified CPU 0 as part `0xd46`, CPUs 3/4 as `0xd4d`, CPU 6 as `0xd47`, and
  CPU 7 as `0xd4e`. The current ADB-shell scheduler mask rejected single-bit affinity for CPU 7,
  so no X3 benchmark number is claimed.
- Permanent A32 guest tests cover `VABDL.S8`, `VABDL.U16`, `VABDL.S32`, `VABAL.U8`, `VABAL.S16`,
  and `VABAL.U32` with signed extremes, every widening size, complete destination lanes, and
  accumulator wraparound. Thor passed 198 assertions in 11 focused `[core][arm][dynarmic]` cases
  and 3,065 assertions in 26 broader `[core]~[file_sys]` cases. The full ARM64 native build passed
  in 1 minute 46 seconds. Source/test commit `1907b5129` was pushed directly to `origin/master`
  over command-line Git SSH before packaging.
- JDK 17 release packaging passed in 1 minute 54 seconds. The ARM64-only, v2-signed APK is
  28,980,344 bytes, reports `1907b5129-vanilla-thor`, and has SHA-256
  `B678724C5811203E83E64EEF9377E7615017748092A086FBED75D58971D46223`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and was force-stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.153 V, and
  20.0 C, so the charging snapshot is not battery-discharge watt evidence.
- Temporary test/benchmark binaries and rendered manual pages were removed from host and Thor.
  Post-verification cleanup removed 2,021,390,819 logical bytes of Gradle/JNI/native staging and
  raised C: free space by 1,580,883,968 bytes to 82,019,672,064. `app/build` retains only the
  28,980,344-byte APK and 476-byte metadata; the 3,246,345,900-byte active ARM64 RelWithDebInfo
  CMake/Ninja cache remains for incremental work.
- This is optimization 87 in the Thor work tally. Its 5.72x-15.98x result applies only to the
  widening-absolute-difference host sequence used by guest `VABDL`/`VABAL`; instruction frequency
  is title-dependent. The 87 items overlap and cannot be added. Whole-game FPS, sustained watts,
  frametime, and thermal gains still require a matched title/scene/cache/renderer/driver/resolution/
  layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Long/Wide Add and Subtract

- A32 guest `VADDL`/`VSUBL` and A64 `SADDL`/`UADDL`/`SSUBL`/`USUBL` widened both 64-bit narrow
  inputs separately and then used generic vector add/sub IR. The ARM64 host therefore emitted two
  `SXTL`/`UXTL` instructions plus `ADD`/`SUB`. A32 `VADDW`/`VSUBW` and A64
  `SADDW`/`UADDW`/`SSUBW`/`USUBW` kept one input wide but still extended the narrow input before a
  generic add/sub, costing two host instructions.
- Dynarmic now retains these guest operations as signed/unsigned, long/wide add/sub IR. The ARM64
  backend emits one baseline `SADDL`/`UADDL`/`SSUBL`/`USUBL` for the long forms and one
  `SADDW`/`UADDW`/`SSUBW`/`USUBW` for the wide forms. The selected A64 high half is already placed
  in the IR value's low 64 bits. x64 polyfills the new IR back to the established extension plus
  add/sub sequence, and RISC-V retains its existing unimplemented vector-backend status.
- The actual Snapdragon core guides were rendered and visually checked again. Cortex-X3 issue 4.0
  page 26 lists all eight instructions in basic ASIMD arithmetic at latency 2 / throughput 4.
  Cortex-A715 issue 5.0 page 28 and Cortex-A710 issue 4.0 page 42 list latency 2 / throughput 2.
  Cortex-A510 issue 6.0 page 35 lists the long/basic group at latency 3 with the table's `2,1`
  throughput notation. All use the normal vector arithmetic pipeline; no optional ISA extension or
  unsafe whole-binary core targeting is required.
- A disassembly-checked benchmark compared eight independent exact old/new sequences over
  2,000,000 loop iterations per sample, alternated order for seven rounds, and selected the best
  samples:

  | Thor core | Long: `2x extend + add` -> `UADDL` | Wide: `extend + add` -> `UADDW` |
  | --- | --- | --- |
  | A510 CPU 0 | 2.256113 -> 0.500260 ns/op; 4.510x | 2.005768 -> 0.500319; 4.009x |
  | A715 CPU 3 | 0.716790 -> 0.178369 ns/op; 4.019x | 0.357243 -> 0.178372; 2.003x |
  | A715 CPU 4 | 0.716087 -> 0.178844 ns/op; 4.004x | 0.357285 -> 0.178369; 2.003x |
  | A710 CPU 6 | 0.715846 -> 0.178747 ns/op; 4.005x | 0.357139 -> 0.178372; 2.002x |

  CPU 7/X3 still rejects the ADB shell's single-bit affinity request, so its manual throughput is
  recorded but no direct X3 timing is claimed. These results measure only the fused host sequences;
  their guest frequency and whole-game impact remain title/scene dependent.
- Permanent A32 guest tests cover signed and unsigned `VADDL`, `VADDW`, `VSUBL`, and `VSUBW`
  across 8/16/32-bit sizes, signed extremes, full-width results, and modular lane wraparound. Thor
  passed 206 assertions in 13 focused `[core][arm][dynarmic]` cases and 3,073 assertions in 28
  broader `[core]~[file_sys]` cases. The final ARM64 native rebuild passed in 1 minute 28 seconds.
  Source/test commit `852e7ef8e` was pushed directly to `origin/master` over command-line Git SSH.
- JDK 17 release packaging passed in 2 minutes 41 seconds. The ARM64-only, v2-signed APK is
  28,985,020 bytes, reports `852e7ef8e-vanilla-thor`, and has SHA-256
  `648F3286CFD5F8A471B3F9E582E4E40E6BFD1A8B164BE72D161F17403B351717`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and was force-stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.154 V, and
  20.0 C, so this charging snapshot is not battery-discharge watt evidence.
- Temporary benchmark/test binaries and rendered manual pages were removed from host and Thor.
  Post-verification cleanup removed 2,018,486,183 logical bytes of Gradle/JNI/native staging and
  raised C: free space by 1,576,734,720 bytes to 81,981,497,344. `src/android/app/build` retains
  only the 28,985,020-byte APK and 476-byte metadata; the 3,248,592,186-byte active ARM64
  RelWithDebInfo CMake/Ninja cache remains for bounded incremental work.
- This is optimization 88 in the Thor work tally. Its 4.00x-4.51x long-form and 2.00x-4.01x
  wide-form results apply only to these exact recurring host sequences. The 88 items overlap and
  cannot be added. Whole-game FPS, sustained watts, frametime, and thermal gains still require a
  matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Widening Multiply-Accumulate

- A32 vector and scalar-by-lane `VMLAL`/`VMLSL`, plus the corresponding A64 vector and indexed
  forms, previously produced a signed/unsigned widening multiply IR followed by generic vector
  add/sub IR. The ARM64 host therefore executed `SMULL`/`UMULL` and `ADD`/`SUB`. Scalar-by-lane
  forms also retain their already optimized direct SIMD `DUP`, but paid the same two-instruction
  arithmetic cost after that broadcast.
- Dynarmic now retains signed/unsigned widening multiply-accumulate/subtract as one IR operation.
  ARM64 consumes the accumulator with `ReadWriteQ()` and emits one baseline
  `SMLAL`/`UMLAL`/`SMLSL`/`UMLSL` for 8-, 16-, and 32-bit narrow lanes. x64 polyfills the new IR
  to the established extend, multiply, and modular add/sub operations; direct Windows-target
  syntax checks covered the x64 emitter and A32/A64 interface configuration. RISC-V keeps explicit
  unimplemented handlers consistent with its existing vector backend. Both modified A64 frontend
  files also passed direct Android-target syntax checks even though Azahar builds only A32 guest
  support.
- The Snapdragon core manuals were rendered and visually checked rather than assuming fewer
  instructions always meant more throughput. Cortex-X3 issue 4.0 page 27 lists long multiply at
  latency 3 / throughput 2 and long multiply-accumulate at latency `4(1)` / throughput 2, with page
  28 explaining late forwarding of the accumulate operand. Cortex-A715 issue 5.0 page 29 and
  Cortex-A710 issue 4.0 page 43 list long multiply at latency 3 / throughput 2 but long
  multiply-accumulate at latency `4(1)` / throughput 1. Cortex-A510 issue 6.0 page 36 lists both at
  latency 4 with the table's `2,1` throughput notation on VMAC. Those A710/A715 issue-rate tables
  made a real-device regression check mandatory.
- A disassembly-checked benchmark compared eight independent exact old `SMULL; ADD` chains with
  eight native `SMLAL` chains. It ran 2,000,000 loop iterations per sample, alternated order for
  seven rounds, selected the best sample, and required the same nonzero checksum (`400420`):

  | Thor core | `SMULL + ADD` -> `SMLAL` | Result |
  | --- | --- | --- |
  | A510 CPU 0 | 2.510622 -> 0.500417 ns/op | 5.017x; 80.07% less time |
  | A715 CPU 3 | 0.357171 -> 0.357210 ns/op | 1.000x; tied |
  | A715 CPU 4 | 0.358467 -> 0.357314 ns/op | 1.003x; tied |
  | A710 CPU 5 | 0.357298 -> 0.357217 ns/op | 1.000x; tied (1.003x repeat) |
  | X3 CPU 7 | 0.157848 -> 0.156878 ns/op | 1.006x; tied |

  CPU 6 rejected the harmless single-bit affinity request during this run. The fused path has no
  measured throughput regression and halves recurring arithmetic instructions on all core classes;
  reduced decode/rename/temporary-register work is a credible power-efficiency direction, but no
  watt reduction is claimed without a matched game-scene battery-discharge test.
- Permanent A32 tests cover signed and unsigned full-vector `VMLAL`/`VMLSL` across every widening
  size, extremes, and modular wraparound. Existing scalar-by-lane tests cover unsigned accumulate
  and signed subtract. Thor passed 228 assertions in 14 focused `[core][arm][dynarmic]` cases and
  3,095 assertions in 29 broader `[core]~[file_sys]` cases. The initial full ARM64 release build
  passed in 3 minutes 30 seconds, and the exact committed revision rebuilt in 1 minute 37 seconds.
  Source/test commit `edeb3bb7c` was pushed directly to `origin/master` over command-line Git SSH.
- The ARM64-only, v2-signed APK is 28,985,156 bytes, reports `edeb3bb7c-vanilla-thor`, and has
  SHA-256 `70556050B64F810CAFFC365F8C1E27186635A8DB739E5CD1541B21008C42BDDE`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and was force-stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.153 V, and
  20.0 C, so this charging snapshot is not battery-discharge watt evidence.
- Post-build cleanup preserved that hash-verified APK plus `output-metadata.json` and retained the
  reusable `.cxx` compiler cache, while reducing `app/build` from 2,047,775,686 to 28,985,632
  logical bytes. That removed 2,018,790,054 logical bytes of intermediates and Windows reported
  1,576,771,584 additional free bytes on C:.
- This is optimization 89 in the Thor work tally. Its 5.017x figure applies only to the exact A510
  multiply-accumulate sequence; the measured larger cores were ties. The 89 items overlap and
  cannot be added. Whole-game FPS, sustained watts, frametime, and thermal gains still require a
  matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Mixed Halving Add/Subtract

- A32 ARM11 `SHASX`/`SHSAX`/`UHASX`/`UHSAX` previously widened both halfword inputs, exchanged the
  second operand with `EXT`, synthesized add-versus-subtract signs with an immediate mask plus
  `EOR`/`SUB`, performed a 32-bit subtraction, shifted for halving, and narrowed. The recurring
  ARM64 path was nine instructions per guest operation.
- The ARM64 backend now exchanges halfwords with `REV32`, computes both exact lane-wise candidates
  with native `SHADD`/`SHSUB` or `UHADD`/`UHSUB`, and inserts the required low halfword. This is four
  baseline AdvSIMD instructions, preserves signed floor rounding and unsigned underflow, and leaves
  x64 and other backend fallbacks unchanged.
- The complete relevant manual pages were rendered and visually checked. Cortex-X3 issue 4.0 pages
  26 and 31-32 list halving arithmetic, element `INS`, and `REV32` at latency 2 / throughput 4.
  Cortex-A715 issue 5.0 pages 28 and 34 and Cortex-A710 issue 4.0 pages 42 and 52 list the same
  operations at latency 2 / throughput 2. Cortex-A510 issue 6.0 pages 35 and 43 list latency 3 with
  the guide's `2,1` throughput notation. All are normal AdvSIMD operations available on every Thor
  core class; no global X3 target or optional ISA feature is used.
- A disassembly-checked benchmark reproduced eight recurring exact old/new sequences, ran 2,000,000
  loop iterations per sample, alternated order over seven rounds, selected the best sample, and
  required identical nonzero checksum `0040003f`:

  | Thor core | Nine instructions -> four instructions | Result |
  | --- | --- | --- |
  | A510 CPU 0 | 10.061357 -> 4.015094 ns/op | 2.506x; 60.09% less time |
  | A715 CPU 3 | 1.671787 -> 0.715713 ns/op | 2.336x; 57.19% less time |
  | A715 CPU 4 | 1.672887 -> 0.722370 ns/op | 2.316x; 56.82% less time |
  | A710 CPU 6 | 1.670498 -> 0.715609 ns/op | 2.334x; 57.16% less time |

  CPUs 5 and 7 rejected the harmless single-bit affinity request, so no second-A710 or X3 timing is
  claimed. The source commit is `118b6beaa`, pushed directly to `origin/master` over command-line
  Git SSH.
- Thor passed 232 assertions in 15 focused `[core][arm][dynarmic]` cases and 3,099 assertions in 30
  broader `[core]~[file_sys]` cases. The permanent edge-case test covers both ASX/SAX layouts,
  negative signed halving, and unsigned subtraction underflow. The initial JDK 17 release build
  passed in 2 minutes 53 seconds; the exact committed revision rebuilt in 1 minute 31 seconds.
- The ARM64-only, v2-signed APK is 28,985,040 bytes, reports `118b6beaa-vanilla-thor`, and has
  SHA-256 `19EC345F297656964C3866F096EE5AE326929BEE303912319A5997F64500350A`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and was force-stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.153 V, and 20.0 C,
  so this charging snapshot is not battery-discharge watt evidence.
- Cleanup preserved only that hash-verified APK plus its 476-byte metadata in `app/build`, retained
  the 3,234,326,241-byte active ARM64 `.cxx` cache, and removed 2,018,784,591 logical bytes of
  reproducible Gradle/JNI staging. Windows reported 1,576,751,104 additional free bytes on C:, to
  81,986,957,312 bytes. Temporary benchmark/test binaries and rendered manual pages were deleted
  from both host and Thor.
- This is optimization 90 in the Thor work tally. Its 2.316x-2.506x result applies only to this exact
  recurring host sequence. The 90 items overlap and cannot be added. Whole-game FPS, sustained
  watts, frametime, and thermal gains still require a matched title/scene/cache/renderer/driver/
  resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Mixed Saturated Add/Subtract

- ARM and Thumb-2 `QASX`/`QSAX`/`UQASX`/`UQSAX` previously expanded into four halfword extracts,
  signed or unsigned extensions, scalar add/subtract, two generic saturation clamps, shifts,
  masks, and repacking. The recurring signed ARM64 result path was 21 instructions before any
  one-time guest-flag spill required by its scalar `CMP` operations.
- Four packed IR operations now preserve the exchanged-halfword semantics through the backends.
  ARM64 emits `REV32`, both signed `SQADD`/`SQSUB` or unsigned `UQADD`/`UQSUB` candidates, and one
  element insert: four recurring instructions. Lazy host FPSR state is spilled before native
  saturating arithmetic so its QC side effect cannot contaminate guest FP state. The x64 backend
  uses saturated SSE word arithmetic plus `PBLENDW`, with an SSE2 `PEXTRW`/`PINSRW` fallback.
- The complete relevant manual pages were rendered and visually checked. Cortex-X3 issue 4.0 page
  26 lists these saturating AdvSIMD operations at latency 2 / throughput 4. Cortex-A715 issue 5.0
  page 28 and Cortex-A710 issue 4.0 page 42 list latency 2 / throughput 2. Cortex-A510 issue 6.0
  page 35 lists the complex saturated group at latency 4 with the guide's `2,1` throughput
  notation. The A510 latency explains why the dependency-chain improvement is smaller there.
- A disassembly-checked benchmark compared the 21-instruction signed scalar clamp/repack sequence
  with the four-instruction native operation, used an identical loop-carried dependency, ran
  8,000,000 operations per sample over four alternating-order rounds, selected the best samples,
  and required equal nonzero checksum `7fff8000`:

  | Thor core | 21 scalar instructions -> four AdvSIMD instructions | Result |
  | --- | --- | --- |
  | A510 CPU 0 | 5.018444 -> 4.523125 ns/op | 1.110x; 9.87% less time |
  | A715 CPU 3 | 3.124603 -> 1.472897 ns/op | 2.121x; 52.86% less time |
  | A715 CPU 4 | 3.125534 -> 1.459720 ns/op | 2.141x; 53.30% less time |
  | A710 CPU 5 | 2.796231 -> 1.546693 ns/op | 1.808x; 44.69% less time |

  CPU 6 and X3 CPU 7 rejected the harmless single-bit affinity request despite `0-7` being online,
  so no timing is claimed for those cores. The source/test commit is `5c8820635`, pushed directly
  to `origin/master` over command-line Git SSH.
- Thor passed all 237 assertions in 16 focused `[core][arm][dynarmic]` cases. The new permanent test
  saturates both directions for signed and unsigned ASX/SAX layouts and confirms guest NZCV, Q,
  and GE flags remain unchanged. The full native binary executed 187,784 assertions: 187,780 passed;
  the four unrelated device-environment failures were three missing build-flavor/DSP hooks and the
  existing Vulkan resource-pool device mismatch. An x86_64 Android syntax compile also accepted the
  new SSE backend. The full ARM64 compile/link passed in 1 minute 47 seconds.
- The JDK 17 release build passed in 2 minutes 47 seconds. Its ARM64-only APK is 28,984,420 bytes,
  reports `5c8820635-vanilla-thor`, and has SHA-256
  `91D496D5898718597AD73EB993E426C289D3422988AA6981D7787B6FA172ABBA`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and was force-stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.151 V, and 20.0 C,
  so this charging snapshot is not battery-discharge watt evidence.
- Cleanup retained the hash-verified APK and active 2,793,703,011-byte ARM64 `.cxx` cache, removed
  2,018,782,277 logical bytes of reproducible Gradle/JNI staging plus the 447,513,344-byte test ELF,
  and deleted temporary benchmark/test binaries and rendered manual pages from host and Thor.
  `app/build` now contains only the 28,984,420-byte APK and its 476-byte metadata.
- This is optimization 91 in the Thor work tally. Its 1.110x-2.141x result applies only to the
  recurring mixed-saturation host sequence and cannot be added to the other 90 items. Whole-game
  FPS, sustained watts, frametimes, and thermals still require a matched title/scene/cache/
  renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Mixed Add/Subtract With GE

- ARM and Thumb-2 `SASX`/`SSAX`/`UASX`/`USAX` already reached packed mixed-halfword IR, but ARM64
  widened both inputs, exchanged 32-bit lanes, synthesized per-lane add/sub signs with an immediate
  mask, subtracted, generated GE in the wide lanes, and narrowed. The recurring path was 10
  instructions for signed operations and 11 for unsigned operations when GE was live.
- ARM64 now uses `REV32`, narrow `ADD` and `SUB` candidates, and one halfword insert for the wrapped
  result. Signed GE uses the sign of `SHADD`/`SHSUB`, which matches the sign of the full mathematical
  result; unsigned addition uses `CMHI` for carry and unsigned subtraction uses the sign of `UHSUB`
  for no-borrow. The live-GE path is eight instructions for signed and unsigned operations. If GE
  is dead, the result-only path returns after four instructions.
- A first seven-instruction widening candidate was compiled, correctness-tested, disassembled, and
  rejected. It measured 1.384x faster on A510 CPU 0 but regressed A715 CPU 3/4 by 5.4%/6.1% and
  A710 CPU 5 by 10.9%. Its lane insert followed by `XTN` lengthened the loop-carried dependency.
  The retained eight-instruction path keeps the recurring result in halfword lanes and removes that
  final narrow.
- The complete relevant manual pages were rendered and visually checked. Cortex-X3 issue 4.0 page
  26 lists basic arithmetic and compare at latency 2 / throughput 4; pages 31-32 list element insert
  and `REV32` at latency 2 / throughput 4. Cortex-A715 issue 5.0 pages 28 and 34 list these groups at
  latency 2 / throughput 2. Cortex-A710 issue 4.0 pages 42-43 and 52 likewise list latency 2 /
  throughput 2. Cortex-A510 issue 6.0 pages 35-36 and 43 list the basic arithmetic, compare, insert,
  and reverse groups at latency 3 with the guide's `2,1` throughput notation, while `XTN` is latency
  4; this supports retaining the narrow result path and explains the rejected widening candidate.
- A disassembly-checked benchmark compared the exact old 10-instruction signed sequence with the
  retained eight-instruction sequence, unrolled eight dependency-linked operations per loop, ran
  8,000,000 operations per sample over four alternating-order rounds, selected the best samples,
  and required equal nonzero low-32-bit checksum `92009200`:

  | Thor core | 10 old instructions -> eight narrow/GE instructions | Result |
  | --- | --- | --- |
  | A510 CPU 0 | 10.063119 -> 7.542331 ns/op | 1.334x; 25.05% less time |
  | A715 CPU 3 | 2.407200 -> 1.858789 ns/op | 1.295x; 22.78% less time |
  | A715 CPU 4 | 2.365501 -> 1.842767 ns/op | 1.284x; 22.10% less time |
  | A710 CPU 5 | 2.327070 -> 2.085534 ns/op | 1.116x; 10.38% less time |
  | A710 CPU 6 | 2.327468 -> 2.272181 ns/op | 1.024x; 2.38% less time |
  | X3 CPU 7 | 2.064089 -> 1.819824 ns/op | 1.134x; 11.83% less time |

  This measures the signed live-GE sequence. The unsigned path has the same eight recurring
  instructions but different flag operations, so no unmeasured unsigned speed ratio is claimed.
  The source/test commit is `01a24248f`, pushed directly to `origin/master` over command-line Git
  SSH.
- Thor passed all 301 assertions in 17 focused `[core][arm][dynarmic]` cases. The permanent test
  covers all four instructions across 32 zero, signed-extreme, unsigned carry/borrow, and mixed-bit
  input combinations, checking wrapped results, every GE pair, and unchanged NZCV/Q. The full
  native binary executed 187,848 assertions: 187,844 passed; the same four unrelated device-
  environment failures remain (three missing build-flavor/DSP hooks and the Vulkan resource-pool
  device mismatch). The final ARM64 compile/link passed in 1 minute 2 seconds.
- The exact committed JDK 17 release build passed in 1 minute 43 seconds. Its ARM64-only APK is
  28,984,020 bytes, reports `01a24248f-vanilla-thor`, and has SHA-256
  `F7F18F9D42E8FB8A2011BD916313009193182D9C5782CCE5D4177EE0330BCA7D`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and was force-stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.155 V, and 20.0 C,
  so this charging snapshot is not battery-discharge watt evidence.
- Cleanup retained only the hash-verified APK and its 476-byte metadata in `app/build`, retained the
  2,793,887,598-byte active ARM64 `.cxx` cache, removed 2,018,862,040 logical bytes of reproducible
  Gradle/JNI staging plus the 447,537,968-byte test ELF, and deleted temporary benchmark/test
  binaries and rendered manual pages from host and Thor. Windows reported 82,422,992,896 free bytes
  on C: afterward.
- This is optimization 92 in the Thor work tally. Its 1.024x-1.334x result applies only to the
  recurring signed mixed add/subtract host sequence and cannot be added to the other 91 items.
  Whole-game FPS, sustained watts, frametimes, and thermals still require a matched title/scene/
  cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Signed Dual Multiply-Long

- ARM and Thumb-2 `SMLALD`/`SMLALDX`/`SMLSLD`/`SMLSLDX` previously expanded each guest operation
  into four signed-halfword extracts, two 32-bit `MUL`, two `SXTW`, a product add/subtract, and a
  separate 64-bit accumulator add. The recurring arithmetic path was ten AArch64 instructions.
  Dynarmic now retains signed multiply-add-long and multiply-subtract-long in IR. ARM64 emits the
  same four extracts followed by two `SMADDL`/`SMSUBL` operations: six instructions, no SIMD/GPR
  transfers, and no intermediate product materialization. The portable x64 lowering keeps the
  exact modular semantics with signed extension, multiply, and add/subtract.
- A packed AdvSIMD candidate was implemented in a temporary benchmark and rejected. `FMOV`,
  `SMULL`, `SADDLV` or `REV64`/`SUB`, and the result transfer passed 400,128 edge/random comparisons,
  but measured only 0.184x-0.840x the existing scalar speed across accessible Thor cores. The SIMD
  register crossing and horizontal reduction cost more than the removed scalar instructions.
- The complete relevant manual pages were extracted, rendered, and visually checked. Cortex-X3
  issue 4.0 page 16, Cortex-A715 issue 5.0 page 18, Cortex-A710 issue 4.0 page 21, and Cortex-A510
  issue 6.0 page 18 all list scalar `SMADDL`/`SMSUBL` at latency 2 and throughput 1. The X3/A715/A710
  tables show accumulator latency 1 in parentheses, and the notes describe late forwarding; A510
  also documents a dedicated accumulator-forwarding path. This supports two dependent widening
  MACs while avoiding the non-free general/SIMD transfers documented elsewhere in the same guides.
- The retained benchmark compared the exact old ten-instruction arithmetic sequence with the new
  six-instruction sequence. It kept four independent 64-bit accumulator chains, ran 8,000,000
  operations per sample over nine alternating-order rounds, selected the best samples, required
  equal nonzero checksums, and passed 1,001,536 signed-edge and random candidate comparisons:

  | Thor core | SMLALD | SMLALDX | SMLSLD | SMLSLDX |
  | --- | --- | --- | --- | --- |
  | A510 CPU 0 | 5.143698 -> 2.635684 ns/op; 1.952x | 4.629154 -> 2.127480; 2.176x | 5.147044 -> 2.632943; 1.955x | 4.636250 -> 2.125957; 2.181x |
  | A715 CPU 3 | 1.168119 -> 0.837760; 1.394x | 1.162038 -> 0.833157; 1.395x | 1.167949 -> 0.840397; 1.390x | 1.162246 -> 0.844349; 1.376x |
  | A715 CPU 4 | 1.165501 -> 0.839030; 1.389x | 1.158522 -> 0.840540; 1.378x | 1.167936 -> 0.837031; 1.395x | 1.162383 -> 0.838600; 1.386x |
  | A710 CPU 5 | 0.918275 -> 0.714225; 1.286x | 0.916009 -> 0.714245; 1.282x | 0.916348 -> 0.714245; 1.283x | 0.916465 -> 0.715606; 1.281x |

  CPUs 6 and 7 rejected the harmless single-bit affinity request, so no timing is claimed for
  those cores. The source/test commit is `e78d99f8c`, pushed directly to `origin/master` over
  command-line Git SSH.
- Thor passed all 685 assertions in 18 focused `[core][arm][dynarmic]` cases. The new permanent test
  contributes 384 assertions across ARM and Thumb add/subtract/exchange forms, six signed-extreme
  and 64-bit-wrap inputs, unchanged NZCV/Q/GE state, and source/destination accumulator aliasing.
  The exact committed JDK 17 ARM64 release build passed in 3 minutes 34 seconds.
- The ARM64-only APK is 28,987,612 bytes, reports `e78d99f8c-vanilla-thor`, and has SHA-256
  `3E88E73E9E93C557DB00C97F89E4886F38D6496435BA7B943291971FAF3FD307`. It installed over
  `org.azahar_emu.azahar.debug` by Wi-Fi ADB and remained stopped with no process ID; no app UI or
  game was launched. Thor reported USB power, no AC/wireless power, 80% battery, 4.153 V, and
  20.0 C, so this charging snapshot is not battery-discharge watt evidence.
- Cleanup retained only the hash-verified APK and its 476-byte metadata in `app/build`, retained the
  2,794,713,569-byte active ARM64 `.cxx` cache, removed 2,018,871,738 logical bytes of reproducible
  Gradle/JNI staging plus the 447,574,992-byte test ELF and 55,154,008 bytes of temporary host
  benchmarks/manual renders, and deleted all temporary benchmark/test binaries from Thor. Windows
  recovered 2,079,604,736 physical bytes and reported 82,166,231,040 bytes free on C: afterward.
- This is optimization 93 in the Thor work tally. Its 1.281x-2.181x result applies only to the
  affected signed dual multiply-long host sequence and cannot be added to the other 92 items.
  Whole-game FPS, sustained watts, frametimes, and thermals still require a matched title/scene/
  cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Signed Multiply-Accumulate-Long

- ARM and Thumb-2 plain `SMLAL` previously expanded its signed 32x32-bit accumulation into two
  `SXTW`, one 64-bit `MUL`, and one 64-bit `ADD`. Dynarmic now retains the operation as generic
  signed multiply-add-long IR, which ARM64 emits as one `SMADDL`. ARM and Thumb-2
  `SMLALBB`/`SMLALBT`/`SMLALTB`/`SMLALTT` retain their two required signed-halfword extracts but
  replace `MUL`, product `SXTW`, and accumulator `ADD` with `SMADDL`: five arithmetic instructions
  become three. The generic backend continues to preserve exact modulo-64-bit accumulation.
- The complete relevant manual pages were text-extracted, rendered, and visually checked.
  Cortex-X3 issue 4.0 page 16, Cortex-A715 issue 5.0 page 18, Cortex-A710 issue 4.0 page 21, and
  Cortex-A510 issue 6.0 page 18 all list `SMADDL` at latency 2 and throughput 1. X3/A715/A710 show
  accumulator latency 1 and describe late forwarding. A510 documents a dedicated MAC accumulator
  forwarding path; its table also lists X-form `MUL` at latency 4 and throughput 1/2, making removal
  of the old 64-bit multiply especially valuable on the efficiency cluster.
- `llvm-objdump` verified the temporary benchmark's exact repeated bodies: plain baseline had
  `SXTW`, `SXTW`, `MUL`, `ADD`, while the candidate had one `SMADDL`; halfword baseline had `SXTH`,
  `SXTH`, `MUL`, `SXTW`, `ADD`, while the candidate had `SXTH`, `SXTH`, `SMADDL`. Each sample used
  four independent accumulators and 5,000,000 loop iterations, or 20,000,000 affected guest
  operations, over nine alternating-order rounds. Baseline and fused checksums matched before
  timing. Median results were:

  | Thor core | Plain `SMLAL` | Halfword `SMLALxy` |
  | --- | --- | --- |
  | A510 CPU 0 | 2.648721 -> 0.505107 ns/op; 5.244x | 3.146227 -> 1.509789 ns/op; 2.084x |
  | A715 CPU 3 | 0.449122 -> 0.358919 ns/op; 1.251x | 0.554815 -> 0.358698 ns/op; 1.547x |
  | A715 CPU 4 | 0.448365 -> 0.358456 ns/op; 1.251x | 0.558607 -> 0.358576 ns/op; 1.558x |
  | A710 CPU 5 | 0.381510 -> 0.358453 ns/op; 1.064x | 0.471432 -> 0.358344 ns/op; 1.316x |

  CPUs 6 and 7 rejected the single-bit affinity request with `EINVAL`, so no X3 timing is claimed.
  Thor reported USB power, no AC/wireless power, 80% battery, 4.153 V, and 20.0 C; this is not a
  battery-discharge watt measurement.
- A permanent regression covers ARM and Thumb plain and BB/TT halfword forms, ARM flag-setting,
  source/destination aliases, six signed-extreme and accumulator-wrap inputs, and unchanged
  C/V/Q/GE state. It passed 282 assertions on Thor. The complete focused `[core][arm][dynarmic]`
  run passed 967 assertions in 19 cases. The source/test commit is `5afbf2dc6`, pushed directly to
  `origin/master` over command-line Git SSH. The exact JDK 17 ARM64 release build passed in 1 minute
  32 seconds.
- The installed ARM64 APK is 28,986,836 bytes, reports `5afbf2dc6-vanilla-thor`, and has SHA-256
  `4D72233FA3DB0BBD04D0639049556E843BC6F76D6C44E5A95E4A78CF45314D58`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; the app remained stopped and no game was launched.
- Cleanup removed the temporary manual renders, benchmark source/binary, stripped Thor test copy,
  device copies, 447,592,144-byte native test ELF, and reproducible Gradle/JNI/R8/native-symbol
  staging. It retained the APK plus its 476-byte metadata and the 2,788,792,017-byte active ARM64
  CMake/Ninja cache. Total logical removal was 2,493,334,236 bytes; C: recovered 2,051,178,496
  physical bytes and reported 82,094,518,272 bytes free afterward.
- This is optimization 94 in the Thor work tally. Its 1.064x-5.244x figures apply only to the
  affected signed multiply-accumulate-long host sequence and cannot be added to the other 93 items.
  Whole-game FPS, sustained watts, frametimes, and thermals still require a matched title/scene/
  cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Unsigned Widening Multiply

- ARM and Thumb-2 `UMULL` and `UMLAL` previously zero-extended two 32-bit inputs into 64-bit IR and
  then used generic `Mul64`. ARM64 consequently emitted X-form `MUL`, even though the guest
  operation is exactly a 32x32-to-64-bit unsigned widening multiply. The new generic
  `UnsignedMultiplyLong(U32, U32) -> U64` IR operation emits native `UMULL Xd, Wn, Wm` on ARM64.
  The x64 backend zeroes both 32-bit scratch registers before its 64-bit `IMUL` polyfill. ARM and
  Thumb `UMLAL` add the packed accumulator after `UMULL`; `UMULL` consumes the result directly.
  `UMAAL` was deliberately left unchanged.
- The complete relevant manual pages were text-extracted, rendered, and visually checked.
  Cortex-X3 issue 4.0 page 16, Cortex-A715 issue 5.0 page 18, and Cortex-A710 issue 4.0 page 21 list
  `MUL`/the `UMULL` alias at throughput 2 while `UMADDL` has throughput 1 and accumulator latency 1.
  Cortex-A510 issue 6.0 page 18 lists X-form `MUL` at latency 4 and throughput 1/2, versus latency 2
  and throughput 1 for the widening long-multiply form. That predicts the selected lowering's large
  A510 win and big-core parity.
- `llvm-objdump` verified the exact temporary benchmark bodies: the baseline used four independent
  X-form `MUL` results; the candidate used four independent `UMULL X, W, W` results. `UMLAL` added
  one 64-bit `ADD` to each chain, while the rejected `UMAAL` candidate added two. Each sample ran
  20,000,000 loop iterations, or 80,000,000 affected operations, for nine alternating-order rounds;
  all baseline/candidate checksums matched. Median accepted-path ratios were:

  | Thor core | `UMULL`: X-form `MUL` -> native `UMULL` | `UMLAL`: X-form `MUL` + `ADD` -> `UMULL` + `ADD` |
  | --- | --- | --- |
  | A510 CPU 0 | 1.9965x | 1.7940x |
  | A715 CPU 3 | 1.0004x | 0.9987x |
  | A715 CPU 4 | 1.0007x | 0.9977x |
  | A710 CPU 6 | 1.0003x | 1.0007x |
  | X3 CPU 7 | 0.9998x | 1.0005x |

  A510's raw medians were 80,629,271 -> 40,385,781 ns for `UMULL` and 90,624,532 ->
  50,515,729 ns for `UMLAL`. The sub-0.31% movements on accepted big-core paths are treated as
  parity/noise, not speed claims.
- Direct `UMADDL` was rejected despite being about 2.24x faster than the old `UMLAL` sequence on
  A510: it measured only about 0.766x-0.769x on A715, 0.664x on A710, and 0.516x on X3. Fused
  `UMAAL` variants had the same asymmetric problem. Merely reassociating `UMAAL` helped A510 by
  about 44%, A715 by 5.7%-6.9%, and A710 by 9.4%, but a longer X3 run measured 0.9498x. Applying
  native `UMULL` to `UMAAL` itself measured 0.9776x on X3, so `UMAAL` remains unchanged.
- The permanent regression covers ARM `UMLAL`/`UMLALS`/`UMULL`/`UMULLS`, Thumb-2
  `UMLAL`/`UMULL`, source/destination aliases, six unsigned-extreme and 64-bit-wrap inputs, ARM N/Z
  updates, and unchanged C/V/Q/GE state. The complete Thor `[core][arm][dynarmic]` run passed 1,217
  assertions in 19 cases. The source/test commit is `dd02d1b5b`, pushed directly to
  `origin/master` over command-line Git SSH. The clean JDK 17 release-style build passed in 3 minutes
  24 seconds; the exact post-commit rebuild passed in 1 minute 42 seconds.
- The installed ARM64 APK is 28,985,276 bytes, reports `dd02d1b5b-vanilla-thor`, and has SHA-256
  `FBB82CB04CF865E2F31B9F395501C3FBA5A1036B22AC72C46B9E3E4A821D69AF`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, and no game was launched.
- Cleanup removed the temporary benchmark source/binary, four manual renders, stripped Thor test
  copy, both device copies, 447,611,496-byte native test ELF, and reproducible Gradle/JNI/R8/native-
  symbol staging. It retained the 28,985,276-byte APK plus 476-byte metadata and the
  2,795,665,346-byte active ARM64 CMake/Ninja cache. Total logical removal was 2,493,397,879 bytes;
  C: recovered 2,053,197,824 physical bytes and reported 82,027,966,464 bytes free afterward.
- This is optimization 95 in the Thor work tally. Its 1.794x-1.997x figures apply only to the
  affected unsigned widening-multiply host sequences on A510; the accepted big-core paths were
  parity. It cannot be added to the other 94 items. Whole-game FPS, sustained watts, frametimes,
  and thermals still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/
  brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Signed Widening Multiply

- ARM and Thumb-2 `SMULL` previously sign-extended both 32-bit operands into 64-bit IR and then
  used generic `Mul64`. ARM64 emitted `SXTW`, `SXTW`, and X-form `MUL` for each guest operation.
  The new generic `SignedMultiplyLong(U32, U32) -> U64` IR operation emits one native
  `SMULL Xd, Wn, Wm`. The x64 backend preserves the same semantics with two `MOVSXD` operations
  and 64-bit `IMUL`; the generic modulo-64-bit result remains unchanged.
- The complete relevant manual pages were rendered and visually checked. Cortex-X3 issue 4.0 page
  16 and Cortex-A710 issue 4.0 page 21 list multiply-long `SMULL` at latency 2 and throughput 2 on
  the M pipelines. Cortex-A715 issue 5.0 page 18 states that the `SMULL` zero-accumulator alias can
  execute on M at throughput 2. Cortex-A510 issue 6.0 page 18 lists X-form `MUL` at latency 4 and
  throughput 1/2, versus latency 2 and throughput 1 for the long multiply-accumulate form that
  encodes `SMULL` when its accumulator is zero.
- `llvm-objdump` verified the exact temporary loop bodies. The baseline repeated four independent
  groups of `SXTW`, `SXTW`, `MUL`; the candidate repeated four independent `SMULL X, W, W`
  instructions. Each sample ran 20,000,000 iterations, or 80,000,000 signed products, for nine
  alternating-order rounds. All warmup and timed checksums matched. Median results were:

  | Thor core | Baseline -> native `SMULL` | Result |
  | --- | --- | --- |
  | A510 CPU 0 | 141,038,021 -> 40,297,917 ns | 3.499884x |
  | A715 CPU 3 | 30,355,469 -> 16,418,489 ns | 1.848859x |
  | A715 CPU 4 | 30,369,948 -> 16,633,959 ns | 1.825780x |
  | A710 CPU 6 | 23,290,677 -> 14,327,969 ns | 1.625539x |
  | X3 CPU 7 | 20,128,438 -> 12,582,187 ns | 1.599757x |

  MIDRs were re-read immediately before the run: CPU 0 `0x411fd461`, CPUs 3-4 `0x411fd4d0`, CPU 6
  `0x412fd470`, and CPU 7 `0x411fd4e0`. Thor reported USB power, no AC/wireless power, 80% battery,
  4.160 V, and 21.0 C. That is useful thermal context, not a battery-discharge watt measurement.
- The permanent regression covers ARM `SMULL`/`SMULLS`, Thumb-2 `SMULL`, source/destination aliases,
  seven signed-extreme/zero inputs, ARM N/Z updates, unchanged C/V/Q/GE, and complete 64-bit
  results. The complete Thor `[core][arm][dynarmic]` run passed 1,364 assertions in 20 cases. The
  source/test commit is `f511c52f8`, pushed directly to `origin/master` over command-line Git SSH.
  The clean JDK 17 release-style build passed in 3 minutes 7 seconds; the exact post-commit rebuild
  passed in 1 minute 35 seconds.
- The installed ARM64 APK is 28,984,800 bytes, reports `f511c52f8-vanilla-thor`, and has SHA-256
  `2E68C3E83CF13AB444C7321DE749B98D8DE9617EFD84983B9DE107FD57944F99`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, and no game was launched.
- Cleanup removed the benchmark/encoding source and binaries, four manual renders, stripped Thor
  test copy, both device copies, 447,640,208-byte native test ELF, and reproducible Gradle/JNI/R8/
  native-symbol staging. It retained the 28,984,800-byte APK plus 476-byte metadata and the
  2,796,499,773-byte active ARM64 CMake/Ninja cache. Total logical removal was 2,493,530,904 bytes;
  C: recovered 2,053,332,992 physical bytes and reported 81,954,062,336 bytes free afterward.
- This is optimization 96 in the Thor work tally. Its 1.600x-3.500x result applies only to the
  affected signed widening-multiply host sequence and cannot be added to the other 95 items.
  Whole-game FPS, sustained watts, frametimes, and thermals still require a matched title/scene/
  cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Signed High-Word Multiply

- ARM and Thumb-2 `SMMUL{R}`, `SMMLA{R}`, and `SMMLS{R}` still expanded signed 32-bit operands to
  64-bit IR and used generic `Mul64`. ARM64 emitted `SXTW`, `SXTW`, and X-form `MUL`. The
  accumulate/subtract forms also built `(Ra << 32)` with a zero register plus `BFI` before a
  generic 64-bit `ADD`/`SUB`. The rounding forms then consumed intermediate bit 31 as before.
- The frontend now keeps these operations native without adding backend-specific semantics.
  `SMMUL` uses `SignedMultiplyLong(U32, U32) -> U64`; `SMMLA` and `SMMLS` zero-extend `Ra`, shift it
  left 32 bits, and use the established `SignedMultiplyAddLong`/`SignedMultiplySubtractLong` IR.
  ARM64 consequently emits `SMULL`, or `LSL` plus `SMADDL`/`SMSUBL`, before the existing high-word
  extraction. x64 retains the established exact signed-extend/multiply/add-subtract polyfill.
- `llvm-objdump` verified all exact temporary loop bodies. Each function repeated four independent
  guest-equivalent operations for 10,000,000 iterations, or 40,000,000 affected operations per
  sample, across nine rotating-order rounds. Warmup and timed checksums matched for every
  baseline, split, and fused form. Median speedups for the selected native paths were:

  | Thor core | `SMMUL`: old -> `SMULL` | `SMMLA`: old -> fused | `SMMLS`: old -> fused |
  | --- | --- | --- | --- |
  | A510 CPU 0 | 2.000438x | 2.120779x | 2.129672x |
  | A715 CPU 3 | 1.745992x | 1.588502x | 1.589313x |
  | A715 CPU 4 | 1.748796x | 1.608305x | 1.597712x |
  | A710 CPU 5 | 1.586090x | 1.573429x | 1.575366x |

  Fused `SMADDL`/`SMSUBL` also beat the native split `SMULL` plus `ADD`/`SUB` candidate by
  6.0%-6.4% on A510, 9.4% on A710, and 14.6%-16.1% on A715. CPU 5's MIDR was re-read as
  `0x412fd470`, confirming Cortex-A710. CPU 6 and X3 CPU 7 rejected the harmless single-bit
  affinity request during this run despite being online and clocked, so no fused timing is claimed
  for them. The prior isolated native-`SMULL` sprint already measured its exact component 1.600x
  on X3.
- The permanent regression covers ARM and Thumb plain/rounded multiply, accumulate, and subtract,
  source/destination aliases, six signed-extreme and wrap inputs, exact intermediate-bit-31
  rounding with 32-bit result wrap, and unchanged NZCV/Q/GE. Thor passed all 1,580 assertions in 21
  focused `[core][arm][dynarmic]` cases. The source/test commit is `78639ae10`, pushed directly to
  `origin/master` over command-line Git SSH. The initial JDK 17 ARM64 release build passed in 2
  minutes 50 seconds; the exact post-commit rebuild passed in 1 minute 38 seconds.
- The installed ARM64 APK is 28,985,496 bytes, reports `78639ae10-vanilla-thor`, and has SHA-256
  `71F66CD938E55CD13C674548EA9AFEBE86BD74ED751D05E3E0D420171E78B93E`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; the app remained stopped and no game was launched. Thor
  reported USB power, no AC/wireless power, 80% battery, 4.160 V, and 21.0 C. That charging context
  is not a battery-discharge watt measurement.
- Cleanup removed the 25,983,736-byte stripped Thor test copy, 11,568-byte device benchmark, local
  benchmark/encoding/test artifacts, 447,654,976-byte native test ELF, `.cxx` tool metadata, and
  reproducible Gradle/JNI/R8/native-symbol staging. It retained the 28,985,496-byte APK plus
  476-byte metadata and the 2,790,604,448-byte active ARM64 CMake/Ninja cache. Total logical host
  removal was 2,498,682,926 bytes; C: recovered 2,056,519,680 physical bytes and reported
  81,892,618,240 bytes free afterward.
- This is optimization 97 in the Thor work tally. Its 1.573x-2.130x figures apply only when the
  guest executes these signed high-word multiply instructions; they cannot be added to the other
  96 items. Whole-game FPS, sustained watts, frametimes, and thermals still require a matched
  title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Signed Word-by-Halfword Multiply

- ARM and Thumb-2 `SMULWB`/`SMULWT` previously sign-extended the full-word operand and selected
  signed halfword into 64-bit IR, multiplied them with generic `Mul64`, shifted right 16 bits, and
  kept the low word. ARM64 emitted `SXTH`, two `SXTW`, X-form `MUL`, and `LSR` for the bottom form;
  the top form also needed `LSR` plus `SXTH` to select its halfword. The frontend now keeps the
  selected halfword as a signed `U32` and uses established `SignedMultiplyLong(U32, U32) -> U64`.
  ARM64 emits `SXTH + SMULL + LSR` for `SMULWB` and `ASR + SMULL + LSR` for `SMULWT`, removing two
  or three recurring host instructions without adding backend-specific semantics. The portable
  x64 signed-extend/multiply polyfill remains exact.
- The recorded Cortex manuals support testing this candidate rather than assuming it: X3 and A710
  list multiply-long `SMULL` at latency 2 and throughput 2, A715 lists its zero-accumulator alias at
  throughput 2, and A510 lists X-form `MUL` at latency 4/throughput 1/2 versus latency 2/throughput
  1 for the long form. `llvm-objdump` then verified every exact baseline and candidate loop body.
  Each sample ran four independent guest-equivalent operations for 10,000,000 iterations, or
  40,000,000 affected operations, over nine alternating-order rounds. Warmup and timed checksums
  matched. Median results were:

  | Thor core | `SMULWB`: old -> native | `SMULWT`: old -> native |
  | --- | --- | --- |
  | A510 CPU 0 | 171,532,917 -> 105,741,511 ns; 1.622191x | 191,485,833 -> 85,609,896 ns; 2.236725x |
  | A715 CPU 3 | 22,748,073 -> 15,600,573 ns; 1.458156x | 26,480,625 -> 15,606,146 ns; 1.696807x |
  | A710 CPU 5 | 18,885,781 -> 12,078,125 ns; 1.563635x | 22,650,260 -> 12,087,500 ns; 1.873858x |
  | X3 CPU 7 | 17,290,261 -> 10,059,115 ns; 1.718865x | 20,440,364 -> 10,067,187 ns; 2.030395x |

- The same candidate was benchmarked but deliberately rejected for `SMLAWB`/`SMLAWT`. Its full
  path included the architectural `ADDS`, overflow extraction, guest-Q load/OR/store, and the same
  checksum lock. It improved A510 by 1.393200x/1.704470x but repeated high-sample medians were
  0.997959x/0.998901x on A715 and 0.994340x/0.989918x on X3; A710 was effectively tied at
  1.001064x/1.000844x. The accumulate forms therefore retain their existing generic lowering
  instead of trading an efficiency-core win for a measurable larger-core regression.
- The permanent regression covers ARM and Thumb-2 top/bottom forms, destination/source aliases,
  six signed-extreme and distinct-halfword inputs, exact product bits 16-47, unchanged NZCV/Q/GE,
  and untouched source registers. Thor passed all 1,748 assertions in 22 focused
  `[core][arm][dynarmic]` cases. The source/test commit is `50e746101`, pushed directly to
  `origin/master` over command-line Git SSH. The initial JDK 17 ARM64 release build passed in 2
  minutes 50 seconds; the exact post-commit rebuild passed in 1 minute 39 seconds.
- The installed ARM64 APK is 28,984,936 bytes, reports `50e746101-vanilla-thor`, and has SHA-256
  `594854C8486FD4AF6A9CB8F9E8B8B96E880AC5442E854FA805926FE8E2449D31`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; the empty process-ID check confirmed that it remained stopped,
  and no app UI or game was launched. Thor reported USB power, no AC/wireless power, 80% battery,
  4.158 V, and 21.0 C. That charging context is not a battery-discharge watt measurement.
- Cleanup deleted both device binaries, all local benchmark/encoding/stripped-test artifacts, the
  447,676,816-byte native test ELF, and reproducible Gradle/JNI/R8/native-symbol staging. It
  retained the APK plus its 476-byte metadata and the 2,796,808,993-byte active ARM64 CMake/Ninja
  cache. Total logical host removal was 2,466,676,050 bytes; C: recovered 2,020,790,272 physical
  bytes and reported 81,818,632,192 bytes free afterward.
- This is optimization 98 in the Thor work tally. Its 1.458x-2.237x figures apply only when the
  guest executes these signed word-by-halfword multiply instructions; they cannot be added to the
  other 97 items. Whole-game FPS, sustained watts, frametimes, and thermals still require a matched
  title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Vector Widening Shift

- A32 NEON `VSHLL.S/U8`, `.S/U16`, and `.S/U32` previously became a signed or unsigned vector
  extension followed by a generic logical left shift. ARM64 therefore emitted `SXTL`/`UXTL`
  (the zero-shift `SSHLL`/`USHLL` aliases) and then `SHL`, even though AArch64 directly encodes the
  complete operation as one `SSHLL`/`USHLL` with the guest immediate.
- The recorded Cortex manuals support the candidate without substituting manual tables for device
  evidence. The X3 guide lists the basic AdvSIMD immediate-shift family, including `SHL`, `SHLL`,
  `SSHLL`, `SXTL`, `USHLL`, and `UXTL`, at latency 2 and throughput 2. A715 and A710 list latency 2
  and throughput 1; A510 lists latency 3 with its dual throughput notation. This made instruction
  fusion plausible on every Thor core class, but the exact sequences were still benchmarked on the
  physical device before source changed.
- The ARM64 backend now aliases an extension to its narrow source only when it has exactly one use,
  the immediately following matching-width logical shift consumes it as argument zero, and the
  immediate is smaller than the original narrow element width. The shift consumer then emits one
  signed or unsigned native widening shift. The extension-side and consumer-side predicates are
  deliberately symmetrical: shared, non-adjacent, mismatched-width, non-immediate, or out-of-range
  IR retains `SXTL`/`UXTL` plus `SHL`, so a rejected fusion can never feed raw narrow data into the
  generic wide-shift fallback.
- `llvm-objdump` verified twelve exact loop bodies: signed/unsigned 8-to-16 at shift 3,
  signed/unsigned 16-to-32 at shift 11, and signed/unsigned 32-to-64 at shift 19. Each timed sample
  ran four independent operations for 5,000,000 iterations, or 20,000,000 affected operations,
  across nine alternating-order rounds. Warmup and every timed baseline/candidate checksum matched;
  the final checksum lock was nonzero for all three widths. Median speedups were:

  | Guest-equivalent form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `VSHLL.S8` | 4.020344x | 2.004667x | 2.003952x | 2.804748x |
  | `VSHLL.U8` | 4.136257x | 1.999144x | 1.997490x | 2.802131x |
  | `VSHLL.S16` | 4.011369x | 1.999516x | 2.000597x | 2.805524x |
  | `VSHLL.U16` | 4.026792x | 2.001173x | 2.000801x | 2.795824x |
  | `VSHLL.S32` | 4.015906x | 2.002867x | 2.000681x | 2.805015x |
  | `VSHLL.U32` | 4.034313x | 2.001538x | 2.000752x | 2.804717x |

- The permanent A32 regression covers all six signed/unsigned widths, shifts 1 through the maximum
  legal immediate, low and high D/Q registers, complete destination/source overlap, partial overlap,
  preserved non-overlapping sources, untouched unrelated SIMD state, and unchanged CPSR N/C/Q/GE.
  Thor passed all 1,760 assertions in 23 focused `[core][arm][dynarmic]` cases. The source/test
  commit is `5a538cee2`, pushed directly to `origin/master` over command-line Git SSH. The first
  release-style source/test build passed in 3 minutes; the final refined native build passed in 1
  minute 21 seconds; and the exact post-commit release rebuild passed in 1 minute 41 seconds.
- The installed ARM64 APK is 28,986,288 bytes, reports `5a538cee2-vanilla-thor`, and has SHA-256
  `EEB75684F0F965AFDAE95C7043CD7CAD09298DA6A828D2AB628713964440A01F`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was empty,
  and no app UI or game was launched. Thor reported USB power, no AC/wireless power, 79% battery,
  4.150 V, and 23.0 C. That charging context is not a battery-discharge watt measurement.
- Cleanup deleted the local benchmark/encoding/stripped-test artifacts, four rendered manual pages,
  both temporary device binaries, the 447,734,920-byte native test ELF, `.cxx` tool metadata, and
  reproducible Gradle/JNI/R8/native-symbol staging. It retained the 28,986,288-byte APK plus its
  476-byte metadata and the 2,791,133,813-byte active ARM64 CMake/Ninja cache. Total logical host
  removal was 2,499,799,755 bytes; C: recovered 2,051,805,184 physical bytes and reported
  81,704,198,144 bytes free afterward.
- This is optimization 99 in the Thor work tally, not 78. Its 1.997x-4.136x figures apply only when
  the guest executes these widening-shift forms and cannot be added to the other 98 items. Whole-game
  FPS, sustained watts, frametimes, and thermals still require a matched title/scene/cache/renderer/
  driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Maximum-Width Vector Shift-Long

- A32 `VSHLL.I8`, `VSHLL.I16`, and `VSHLL.I32`, plus A64 `SHLL`, use the maximum shift equal to the
  original element width. Their frontend IR is an adjacent zero extension followed by a logical
  shift by 8, 16, or 32. Optimization 99 deliberately required a smaller immediate because native
  `SSHLL`/`USHLL` only encode 0 through width-minus-one, so these maximum forms still emitted
  `UXTL` plus `SHL`.
- The recorded Cortex-X3, A715, A710, and A510 software optimization guides explicitly group
  `SHLL` with the basic AdvSIMD immediate-shift family. They list latency/throughput as 2/2 on X3,
  2/1 on A715 and A710, and latency 3 with A510's `2,1` throughput notation. Exact AArch64 assembly
  and `llvm-objdump` then verified that the candidate loop contained one `SHLL` where the baseline
  contained `UXTL` plus `SHL`, with otherwise identical loop control.
- The benchmark ran four independent vector operations per loop for 5,000,000 iterations, or
  20,000,000 affected operations per sample, across nine alternating-order rounds. Warmup and timed
  checksums matched and remained nonzero. Median results were:

  | Guest form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `VSHLL.I8 #8` | 43,724,323 -> 10,239,271 ns; 4.270257x | 2.001835x | 1.999951x | 2.803131x |
  | `VSHLL.I16 #16` | 43,645,052 -> 10,329,427 ns; 4.225312x | 1.998722x | 2.000696x | 2.801387x |
  | `VSHLL.I32 #32` | 43,650,364 -> 10,739,583 ns; 4.064438x | 2.000654x | 1.999606x | 2.801009x |

- ARM64 Dynarmic now accepts equality only for the adjacent, sole-use zero-extension shape and emits
  native `SHLL`; signed extension remains limited to a smaller immediate, and larger, shared,
  non-adjacent, mismatched, or non-immediate forms retain the generic path. The extension alias and
  consumer predicates use the same rule so a rejected fusion cannot expose an unextended operand.
  Permanent A32 cases cover all three encodings, exact results, source preservation, unrelated SIMD
  state, CPSR state, high registers, and partial source/destination overlap.
- The release build passed and Thor passed all 1,766 assertions in 23 focused
  `[core][arm][dynarmic]` cases. Source/test commit `03e97ef1e` was pushed directly to
  `origin/master` over command-line Git SSH. The initial release build passed in 2 minutes 57 seconds;
  the exact post-commit release rebuild passed in 1 minute 42 seconds.
- The installed ARM64 APK is 28,986,332 bytes, reports `03e97ef1e-vanilla-thor`, and has SHA-256
  `26A636CBCB7532E0B40D6EEC4FF3A864C382B01DA5FAB3D639B344EFC661FA46`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was empty,
  and no app UI or game was launched. Thor reported USB power, no AC/wireless power, 78% battery,
  4.126 V, and 23.0 C. That charging context is not a battery-discharge watt measurement.
- Cleanup deleted both device binaries, all local benchmark/object/stripped-test artifacts, four
  rendered manual pages, the 447,741,832-byte native test ELF, `.cxx` tool metadata, and reproducible
  Gradle/JNI/R8/native-symbol staging. It retained the 28,986,332-byte APK plus its 476-byte metadata
  and the 2,791,288,080-byte active ARM64 CMake/Ninja cache. Total logical host removal was
  2,499,901,607 bytes; C: recovered 2,059,575,296 physical bytes and reported 81,604,730,880 bytes
  free afterward.
- This is optimization 100 in the overlapping Thor work tally. Its 1.999x-4.270x figures apply only
  to these maximum-width widening shifts and cannot be added to the other 99 items. Whole-game FPS,
  sustained watts, frametimes, and thermals still require a matched title/scene/cache/renderer/
  driver/resolution/layout/mode/fan/brightness/duration A/B run.

## 2026-08-18 Dynarmic Native Shift-Right Narrowing

- Before this sprint, `upstream/master` was fetched and merged. The only new upstream commit was
  `db15d78fe`, a runtime-neutral copyright-header sweep across 764 files; merge commit `fe9136656`
  was pushed to `origin/master`. It changes no emulator hot path, so no speed or power result is
  attributed to the sync.
- A32 NEON `VSHRN`, `VQSHRN.S`, `VQSHRN.U`, and `VQSHRUN.S` for 16-to-8, 32-to-16, and 64-to-32-bit
  elements previously lowered to a vector right shift followed by a separate narrow. ARM64 emitted
  `USHR + XTN`, `SSHR + SQXTN`, `USHR + UQXTN`, or `SSHR + SQXTUN`. AArch64 directly represents
  these exact non-rounding pairs as `SHRN`, `SQSHRN`, `UQSHRN`, and `SQSHRUN`.
- The recorded Cortex manuals support measuring this fusion on each Thor core class. A510 lists
  basic and saturating fused shift-narrow families at latency 4 with `2,1` throughput notation.
  A710 and A715 list basic `SHRN` at latency 2/throughput 1 and the saturating family at latency
  4/throughput 1. X3 lists the basic family at latency 2/throughput 2 and the saturating family at
  latency 4/throughput 2. These tables motivated the experiment; the physical-device results below
  determine the claim.
- `llvm-objdump` verified all 24 exact loop bodies: the baseline has the expected shift plus narrow,
  the candidate has one fused instruction, and loop control is otherwise identical. Each timed
  sample ran four independent vector operations for 5,000,000 iterations, or 20,000,000 affected
  operations, across nine alternating-order rounds. Warmup and timed checksums matched and remained
  nonzero. Median speedups were:

  | Guest form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `VSHRN.I16/I32/I64` | 4.029602x / 4.002076x / 3.988138x | 0.999242x / 1.002312x / 0.999635x | 1.000478x-1.001940x | 0.999432x-1.000630x |
  | `VQSHRN.S16/S32/S64` | 4.009519x / 3.994325x / 4.178109x | 1.9977x-2.0031x | 1.995907x-2.002566x | 2.798938x-2.804810x |
  | `VQSHRN.U16/U32/U64` | 3.731329x / 4.357201x / 3.858466x | 1.9977x-2.0031x | 1.995907x-2.002566x | 2.798938x-2.804810x |
  | `VQSHRUN.S16/S32/S64` | 3.773881x / 4.001312x / 4.404425x | 1.9977x-2.0031x | 1.995907x-2.002566x | 2.798938x-2.804810x |

  The grouped ranges are retained where the individual captured values were not needed to select
  the same all-width lowering. Plain `VSHRN` is throughput-neutral on A715/A710/X3 but still halves
  its vector instruction count; its roughly 4x A510 result makes it worthwhile without sacrificing
  larger-core throughput.
- ARM64 Dynarmic now aliases the shift to its raw source only when it has one use, the immediately
  following exact-width narrow consumes it as argument zero, and the constant shift is 1 through
  half the source width. The consumer emits the matching fused instruction. Saturating forms load
  FPSR before emission so the guest sticky FPSCR.QC behavior remains intact. Shared, non-adjacent,
  mismatched, non-immediate, zero, or out-of-range IR retains the original two-instruction path.
  Rounding forms are deliberately excluded because their frontend includes a different rounding-
  correction DAG and needs its own correctness proof.
- The permanent regression covers all four instruction families and all three widths, low and high
  registers, partial destination/source overlap at D31/Q15, unrelated-register preservation, exact
  results, CPSR preservation, initial FPSCR state, and sticky QC. The release build passed in 7
  minutes 58 seconds after the broad upstream header rebuild, Thor passed all 1,823 assertions in
  24 focused `[core][arm][dynarmic]` cases, and the exact post-commit rebuild passed in 2 minutes 14
  seconds. Source/test commit `83483cbbd` was pushed directly to `origin/master` with command-line
  Git over SSH.
- The installed ARM64 APK is 28,988,724 bytes, reports `83483cbbd-vanilla-thor`, and has SHA-256
  `CA833539D408CD92631115F6F16E86328ACD4D1E8A8A793F642F08B4E1810992`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor reported USB power, no AC/wireless power, 77%
  battery, 3.982 V, and 23.0 C. That charging context is not a battery-discharge watt measurement.
- Cleanup deleted both temporary device binaries, the 26,007,784-byte stripped host test copy,
  local benchmark/object/source scratch, eight rendered manual pages, the 447,866,752-byte native
  test ELF, `.cxx` tool metadata, and reproducible Gradle/JNI/R8/native-symbol staging. It retained
  the 28,988,724-byte APK plus 476-byte metadata and the 2,794,679,241-byte active ARM64 CMake/Ninja
  cache. Total logical host removal was 2,500,714,051 bytes; C: recovered 2,060,034,048 physical
  bytes and reported 81,544,265,728 bytes free afterward.
- This is optimization 101 in the overlapping Thor work tally. The approximately 1.00x-4.40x
  figures apply only when the guest executes these exact shift-right-narrow forms and cannot be
  added to the other 100 items. Whole-game FPS, sustained watts, frametimes, and thermals still
  require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/
  duration A/B run.

## ARM64 Rounding Shift-Right Narrow Fusion (2026-08-18)

- A32 NEON `VRSHRN`, `VQRSHRN.S`, `VQRSHRN.U`, and `VQRSHRUN.S` for 16-to-8, 32-to-16, and
  64-to-32-bit elements previously used an overflow-safe rounding correction before narrowing.
  The frontend emitted a right shift, broadcast rounding bit, AND, equality mask, subtract-as-add,
  and the selected narrow. ARM64 materialized that as `MOV + DUP + USHR/SSHR + AND + CMEQ + SUB`
  followed by `XTN`, `SQXTN`, `UQXTN`, or `SQXTUN`. AArch64 directly represents the exact operation
  as `RSHRN`, `SQRSHRN`, `UQRSHRN`, or `SQRSHRUN`.
- The recorded Cortex manuals explicitly list the native rounding forms. A510 lists the A64
  complex immediate-shift family at latency 4 with `2,1` throughput notation; its A32 table lists
  `VRSHRN` at latency 3 and the saturating rounding forms at latency 4. A710 and A715 list the A64
  complex family at latency 4/throughput 1, while X3 lists latency 4/throughput 2. These tables
  justified an exact all-core experiment; they are not substitutes for the physical result.
- `llvm-objdump` verified every baseline and candidate body. Each sample ran four independent
  vector operations for 1,000,000 iterations, or 4,000,000 affected guest operations, across nine
  alternating-order rounds. Warmup and timed checksums matched and remained nonzero. Median
  speedups were:

  | Guest form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `VRSHRN.I16` | 13.885256x | 3.001884x | 3.472421x | 3.598723x |
  | `VRSHRN.I32` | 14.116638x | 3.004390x | 3.435994x | 3.507308x |
  | `VRSHRN.I64` | 14.715745x | 2.911880x | 3.310005x | 3.520017x |
  | `VQRSHRN.S16` | 14.225325x | 3.343469x | 3.587415x | 3.771264x |
  | `VQRSHRN.S32` | 14.667758x | 3.419528x | 3.472222x | 3.823051x |
  | `VQRSHRN.S64` | 14.499985x | 3.160713x | 3.250409x | 3.959625x |
  | `VQRSHRN.U16` | 13.547762x | 3.316225x | 3.398665x | 3.778699x |
  | `VQRSHRN.U32` | 13.654684x | 3.528960x | 3.560860x | 3.847900x |
  | `VQRSHRN.U64` | 13.132014x | 3.541497x | 3.389130x | 3.712468x |
  | `VQRSHRUN.S16` | 13.846615x | 3.097625x | 3.475383x | 3.904464x |
  | `VQRSHRUN.S32` | 14.809991x | 3.456508x | 3.234291x | 3.695996x |
  | `VQRSHRUN.S64` | 14.697800x | 2.807415x | 3.491155x | 3.776349x |

- Dynarmic now represents the four rounding modes with first-class, exact-width IR operations.
  ARM64 lowers them directly to the matching native instruction and loads FPSR for saturating
  forms. x64 and RISC-V request a polyfill that reconstructs the established overflow-safe DAG,
  preserving non-ARM64 behavior without making the ARM64 backend recognize a fragile multi-node
  pattern.
- Permanent A32 coverage executes all four families and all three source widths. It covers low and
  high registers, partial destination/source overlap at D31/Q15, exact positive and negative
  rounding, unrelated SIMD state, CPSR/FPSCR preservation, saturation, and sticky QC. The first
  release-style ARM64 build passed in 3 minutes 36 seconds; Thor then passed all 1,880 assertions
  in 24 focused `[core][arm][dynarmic]` cases. Source/test commit `596a28aab` was pushed directly to
  `origin/master` over SSH, and the exact post-commit APK build passed in 1 minute 43 seconds.
- The ARM64-only APK is 28,992,796 bytes, reports `596a28aab-vanilla-thor`, and has SHA-256
  `DC5E4F03165E7F7161CD66123468B5E4DFEE85682B476AD6B6846926AD23EF4D`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Both temporary Thor test/benchmark binaries were
  removed immediately after use.
- Cleanup removed 2,469,307,555 logical bytes: the 448,016,304-byte native test ELF, benchmark and
  A32-encoding scratch, eight rendered manual pages, copied validation layers, and reproducible
  Gradle/JNI/R8/native-symbol staging. It retained the 28,992,796-byte APK plus 476-byte metadata
  and the 2,802,108,385-byte active ARM64 CMake/Ninja cache. C: recovered 2,026,508,288 physical
  bytes and reported 81,532,805,120 bytes free afterward.
- This is optimization 102 in the overlapping Thor work tally. The 2.81x-14.81x measurements apply
  only while executing these exact rounding shift-right-narrow forms. They cannot be added to the
  other 101 items or treated as a whole-game FPS, battery-watt, or thermal result; those require a
  matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/duration A/B.

## ARM64 Vector Rounding Shift-Right Fusion (2026-08-18)

- A32/A64 vector `VRSHR`/`SRSHR`/`URSHR` previously became an overflow-safe right shift, rounding-
  bit broadcast, AND, equality mask, and subtract-as-add. `VRSRA`/`SRSRA`/`URSRA` then appended a
  separate modular vector add. ARM64 can express the exact operations as one `SRSHR`/`URSHR` or
  `SRSRA`/`URSRA`, without touching FPSR.
- The Cortex guides made this a per-core measurement question rather than an automatic fusion.
  A510 lists A64 `SRSHR`/`URSHR` at latency 4 and A32 `VRSRA` at latency 7, while A710/A715 list
  basic immediate shifts at latency 2, rounding immediate shifts and shift-accumulates at latency
  4, and X3 lists the same latency classes at higher throughput. `llvm-objdump` verified all 24
  baseline/candidate bodies. Each sample ran four independent vector operations for 1,000,000
  iterations, or 4,000,000 affected operations, across nine alternating-order rounds. Warmup and
  timed checksums matched and remained nonzero.

  | Guest-equivalent form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `VRSHR.S8` | 10.168814x | 2.503628x | 2.717059x | 3.644689x |
  | `VRSHR.S16` | 10.113846x | 2.504299x | 2.708425x | 4.464165x |
  | `VRSHR.S32` | 9.983189x | 2.504528x | 2.714135x | 4.767936x |
  | `VRSHR.S64` | 10.595018x | 2.512617x | 2.713818x | 4.721130x |
  | `VRSHR.U8` | 10.433867x | 2.505041x | 2.710081x | 3.758589x |
  | `VRSHR.U16` | 10.605283x | 2.510857x | 2.714629x | 3.518261x |
  | `VRSHR.U32` | 10.049815x | 2.505534x | 2.718222x | 3.793438x |
  | `VRSHR.U64` | 9.879887x | 2.504598x | 2.713218x | 3.624434x |
  | `VRSRA.S8` | 5.579025x | 2.991158x | 3.440428x | 3.327763x |
  | `VRSRA.S16` | 5.311777x | 3.005532x | 3.491094x | 3.342980x |
  | `VRSRA.S32` | 5.076427x | 3.004477x | 3.486183x | 3.354109x |
  | `VRSRA.S64` | 5.183191x | 3.005112x | 3.496335x | 2.543372x |
  | `VRSRA.U8` | 5.149252x | 3.008707x | 3.471114x | 3.372724x |
  | `VRSRA.U16` | 5.646550x | 3.004865x | 3.481740x | 3.176134x |
  | `VRSRA.U32` | 5.412307x | 2.991859x | 3.503312x | 3.304029x |
  | `VRSRA.U64` | 5.173864x | 3.004616x | 3.386183x | 3.285604x |

- Plain non-rounding `VSRA` was measured in the same harness and rejected. Native `SSRA`/`USRA`
  improved A510 by 3.94x-4.10x and was effectively neutral on A715 (0.996033x-1.000846x) and A710
  (0.996907x-1.003349x), but X3 fell to 0.777230x-0.942683x, a 5.7%-22.3% regression. The frontend
  deliberately retains its existing shift plus add for this family.
- Dynarmic now carries signed/unsigned, 8/16/32/64-bit rounding right shift and rounding right-
  shift-accumulate operations in first-class IR. ARM64 emits the matching native instruction.
  x64 and RISC-V request a polyfill that reconstructs the prior overflow-safe DAG plus optional
  modular add, so non-ARM64 behavior is unchanged.
- Permanent A32 coverage executes all 16 signed/unsigned forms across every lane width. It includes
  D/Q widths, low and high registers, maximum legal shifts, full source/destination overlap, exact
  signed and unsigned rounding, accumulator wraparound, unrelated SIMD state, and unchanged
  CPSR/FPSCR. The release-style ARM64 test binary built successfully and Thor passed all 1,928
  assertions in 25 focused `[core][arm][dynarmic]` cases. Source/test commit `f8dfcb115` was pushed
  directly to `origin/master` over command-line Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build
  passed with JDK 17 in 2 minutes 42 seconds. Its ARM64-only APK is 28,997,240 bytes, reports
  `f8dfcb115-vanilla-thor`, and has SHA-256
  `8B3649C5E6E5F0CC1AA57CD9E2424D9C672C1800B0BE61EABB074402658F246A`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 52%, 3.754 V, and 25.0 C, so
  this is not battery-discharge watt evidence. Both temporary device binaries were removed.
- Cleanup removed 2,503,006,450 logical bytes: the 448,224,232-byte native test ELF, stripped test
  copy, benchmark/encoding scratch, eight rendered manual pages, copied validation layers, tool
  metadata, and reproducible Gradle/JNI/R8/native-symbol staging. It retained the 28,997,240-byte
  APK plus 476-byte metadata and the 2,797,637,092-byte active ARM64 CMake/Ninja cache. C: recovered
  2,059,730,944 physical bytes and reported 81,490,550,784 bytes free afterward.
- This is optimization 103 in the overlapping Thor work tally. The 2.50x-10.61x measurements apply
  only while executing these exact rounding shift-right or rounding shift-right-accumulate forms;
  they cannot be added to the other 102 items or treated as a whole-game FPS, sustained battery-
  watt, frametime, or thermal result. Those still require a matched title/scene/cache/renderer/
  driver/resolution/layout/mode/fan/brightness/duration A/B run.

## ARM64 Vector Shift-Insert Fusion (2026-08-18)

- A32/A64 `VSLI`/`SLI` and `VSRI`/`SRI` previously expanded on ARM64 to five host instructions: a
  vector shift, scalar immediate materialization, `DUP`, `BIC`, and `ORR`. AArch64 expresses the
  exact destination-preserving operation in one native `SLI` or `SRI`, reducing this affected path
  by four instructions, or 80%.
- The complete Arm guide pages were rendered and visually checked before measurement. A510 lists
  A64 `SLI`/`SRI` and A32 `VSLI`/`VSRI` at latency 3 and throughput `2,1` on VALU; A710 lists both
  forms at latency 2 and throughput 1 on V1; A715 lists the A64 forms at latency 2 and throughput 1
  on V1; X3 lists the A64 forms at latency 2 and throughput 2 on V13. These tables supported testing
  every Thor core class rather than assuming the five-to-one instruction reduction scaled equally.
- `llvm-objdump` verified the intended five-instruction baseline and one-instruction candidate for
  every lane width and direction, with identical loop control. Each sample used four independent
  vector chains for 1,000,000 iterations, or 4,000,000 affected operations, over nine alternating-
  order rounds. Warmup and timed checksums matched and remained nonzero.

  | Guest-equivalent form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `VSLI.8` | 6.939407x | 2.005499x | 2.203525x | 2.420771x |
  | `VSLI.16` | 7.102336x | 2.007047x | 2.210151x | 2.421235x |
  | `VSLI.32` | 7.011041x | 2.006873x | 2.191220x | 2.426934x |
  | `VSLI.64` | 8.322861x | 1.998352x | 2.174261x | 2.421775x |
  | `VSRI.8` | 7.838822x | 1.993065x | 2.204195x | 2.419850x |
  | `VSRI.16` | 7.043788x | 2.006766x | 2.195724x | 2.420463x |
  | `VSRI.32` | 7.107420x | 2.009022x | 2.195096x | 2.422545x |
  | `VSRI.64` | 7.040607x | 2.005004x | 2.202777x | 2.425394x |

- Dynarmic now carries left and right vector shift-insert as first-class IR. ARM64 emits the native
  instruction, while x64 and RISC-V request an exact polyfill, preserving their established output.
  Permanent A32 coverage executes all 16 min/max-immediate operations across 8/16/32/64-bit lanes.
  It covers D/Q forms, low/high registers, source/destination overlap, preserved destination bits,
  unrelated SIMD state, and unchanged CPSR/FPSCR.
- The full native ARM64 build completed successfully in 13 minutes 33 seconds, including the
  production emitter and library. Thor then passed all 1,976 assertions in 26 focused
  `[core][arm][dynarmic]` cases. Source/test commit `ce1500209` was pushed directly to
  `origin/master` over command-line Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build
  passed with JDK 17 in 3 minutes 6 seconds. Its ARM64-only APK is 28,998,432 bytes, reports
  `ce1500209-vanilla-thor`, and has SHA-256
  `2E66A94B4F804ED795BAA0CF360158C6E4AB8E51BCB276B688079D53A186C444`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the corrected process-ID
  check was empty, and no app UI or game was launched. Thor was USB-powered at 49%, 3.828 V, and
  22.0 C, so this is not battery-discharge watt evidence. Both temporary device binaries were
  removed.
- Cleanup removed 2,498,961,084 logical bytes: the 447,340,912-byte native test ELF, stripped test
  copy, benchmark/encoding scratch, six rendered manual pages, copied JNI dependencies, and
  reproducible Gradle/JNI/R8/native-symbol staging. It retained the 28,998,432-byte APK plus
  476-byte metadata and the 2,784,158,328-byte active ARM64 CMake/Ninja cache. C: recovered about
  2,057,101,312 physical bytes and reported 81,502,978,048 bytes free immediately afterward. No
  PDF or rendered manual artifact was committed.
- This is optimization 104 in the overlapping Thor work tally. The 1.99x-8.32x measurements apply
  only while executing these exact vector shift-insert forms. They cannot be added to the other 103
  items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal result. Those
  still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/
  duration A/B run.

## ARM64 Packed Unsigned Byte Difference Sum (2026-08-18)

- A32 ARMv6 `USAD8` and `USADA8` are part of the actual 3DS ARM11 guest instruction set, unlike
  guest AdvSIMD-only experiments. Dynarmic's ARM64 `PackedAbsDiffSumU8` lowering previously emitted
  `MOVI` for a four-byte lane mask, `UABD`, `AND`, and `UADDLV`. It now emits `UABDL H8` followed by
  `UADDLV H4`, reducing the affected guest operation from four host instructions to two, or 50%.
- The semantic shortcut is exact: `UABDL` widens all eight unsigned byte differences, placing the
  four defined guest lanes in the low four halfwords. Reducing only `H4` ignores the packed
  operand's undefined upper word without a mask. The largest sum is 4 * 255 = 1020, and `USADA8`
  retains its normal 32-bit modular accumulator addition in the surrounding IR.
- The complete Arm guide pages were rendered and visually checked before measurement. A510 lists
  `UABDL` at latency 3 and throughput `2,1` on VALU and `UADDLV 4H` at latency 4 and throughput 1
  on VALU. A710 lists latency/throughput 2/2 on V for `UABDL` and 2/1 on V1 for `UADDLV 4H`.
  A715 lists 2/2 on V and 3/1 on V1 respectively. X3 lists 2/4 on V and 2/2 on V13 respectively.
  These differences supported measuring every Thor core class instead of extrapolating from X3.
- `llvm-objdump` verified the intended four-instruction baseline and two-instruction candidate with
  identical loop control. The harness used four independent packed operations for 1,000,000
  iterations, or 4,000,000 affected operations, over nine alternating-order rounds per core.
  Warmup and timed checksums matched and remained nonzero at 1432.

  | Guest-equivalent form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `USAD8` four-byte difference sum | 1.759435x | 2.515585x | 2.505252x | 2.806593x |

- Permanent A32 tests execute ARM and Thumb `USAD8`/`USADA8`, including normal and source/
  accumulator-alias encodings, maximum byte differences, an accumulator at `UINT32_MAX`, patterned
  edge values, unrelated-register preservation, and unchanged NZCV/Q/GE flags. The complete native
  ARM64 build passed in 11 minutes 42 seconds, and Thor passed all 2,176 assertions in 27 focused
  `[core][arm][dynarmic]` cases. Source/test commit `928eae934` was pushed directly to
  `origin/master` over command-line Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build
  passed with JDK 17 in 2 minutes 48 seconds. Its ARM64-only APK is 28,997,788 bytes, reports
  `928eae934-vanilla-thor`, and has SHA-256
  `DCF8B4F89B683FD45F70A986E6773B122C7F048284880ED2878E993BCC6F3B57`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 49%, 3.849 V, and 21.0 C, so
  this is not battery-discharge watt evidence. Both temporary device binaries were removed.
- Cleanup removed 2,496,891,355 logical bytes: the 448,367,400-byte native test ELF, stripped test
  copy, benchmark/encoding scratch, five rendered manual pages, copied JNI dependencies, and
  reproducible Gradle/JNI/R8/native-symbol staging. It retained the 28,997,788-byte APK plus
  476-byte metadata and the 2,787,840,399-byte active ARM64 CMake/Ninja cache. C: recovered
  2,054,905,856 physical bytes and reported 81,254,731,776 bytes free immediately afterward. No
  PDF or rendered manual artifact was committed.
- This is optimization 105 in the overlapping Thor work tally. The 1.76x-2.81x measurements apply
  only while executing these exact packed byte-difference forms. They cannot be added to the other
  104 items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal result.
  Those still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/
  brightness/duration A/B run.

## ARM64 A32 Halfword Packing (2026-08-18)

- A32 ARMv6 `PKHBT` and `PKHTB` are part of the 3DS ARM11 guest instruction set. The ARM and Thumb
  frontends previously expanded each operation into an immediate shift, two masks, and an OR.
  Dynarmic now retains `PackHalfwordBottom` and `PackHalfwordTop` as first-class IR operations.
  ARM64 emits one `BFXIL` for bottom shift 0, one `BFI` for bottom shift 16, and `LSL` plus `BFXIL`
  for other bottom shifts. Top shifts 1-16 use one `BFXIL`; shifts 17-32 use `ASR` plus `BFXIL`,
  with ASR #32 represented exactly by ASR #31. This changes the measured forms from three or four
  host instructions to one or two. x64 and RISC-V reconstruct the established exact DAG through a
  polyfill.
- The semantic shortcut is exact. `BFXIL` replaces only the low destination halfword, while `BFI`
  at bit 16 replaces only the high destination halfword. For a top shift no greater than 16, the
  low 16 bits of arithmetic shift right are the source field beginning at that shift, so sign
  extension is irrelevant. Larger shifts first materialize the sign-extended low field; shift 32
  is all copies of bit 31. Dynarmic's read-write allocator copies a source when it is still live,
  so reusing it as the result does not alter shared IR values.
- The complete instruction-characteristics pages in the Cortex-A510, A710, A715, and X3 software
  optimization guides were rendered and visually checked before measurement. A510 lists bitfield
  moves at latency 2 and throughput 3, with immediate LSL/ASR aliases at latency 1 and throughput
  3. A710 lists bitfield moves at 2/2 on M and immediate shifts at 1/4 on I. A715 lists both at
  latency 1 and throughput 4 on I. X3 lists bitfield moves at 2/2 on M and immediate shifts at 1/6
  on I. These different pipelines supported measuring every Thor core class instead of
  extrapolating from the prime core.
- `llvm-objdump` verified identical loop control around the intended candidates: `PKHBT` shift 0
  changed 3 -> 1 instructions, shift 7 changed 4 -> 2, and shift 16 changed 4 -> 1;
  `PKHTB` shifts 7 and 16 changed 4 -> 1, while shifts 24 and 32 changed 4 -> 2. The harness used
  four independent operations for 1,000,000 iterations, or 4,000,000 affected operations, over
  nine alternating-order rounds per core. Every warmup and timed checksum matched and remained
  nonzero.

  | Guest-equivalent form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `PKHBT`, LSL #0 | 2.545486x | 1.884437x | 1.803578x | 1.489624x |
  | `PKHBT`, LSL #7 | 1.209718x | 1.750912x | 1.344066x | 1.499972x |
  | `PKHBT`, LSL #16 | 3.014697x | 2.460514x | 2.162188x | 2.232581x |
  | `PKHTB`, ASR #7 | 2.969528x | 2.443599x | 2.155619x | 2.005063x |
  | `PKHTB`, ASR #16 | 2.825604x | 2.438770x | 2.163796x | 2.003154x |
  | `PKHTB`, ASR #24 | 1.221914x | 1.861854x | 1.912342x | 1.756898x |
  | `PKHTB`, ASR #32 | 1.196462x | 1.861727x | 1.905815x | 1.776214x |

- Permanent tests execute both ARM and Thumb encodings at bottom shifts 0, 1, 7, 15, 16, 17,
  and 31 and top shifts 1, 7, 15, 16, 17, 24, 31, and 32. They cover distinct registers,
  destination equal to either source, all three registers equal, positive and negative sources,
  unrelated-register preservation, and unchanged NZCV/Q/GE flags. The complete native ARM64 build
  passed with JDK 17 in 2 minutes 2 seconds, and Thor passed all 3,646 assertions in 28 focused
  `[core][arm][dynarmic]` cases. Source/test commit `f016be8b3` was pushed directly to
  `origin/master` over command-line Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build
  passed with JDK 17 in 3 minutes 17 seconds. Its ARM64-only APK is 28,997,592 bytes, reports
  `f016be8b3-vanilla-thor`, and has SHA-256
  `0DF0F881D9F2E06F45FC487A65430FFFA64471878B303ACB8D4F74EBE97D2252`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 50%, 3.861 V, and 21.0 C, so
  this is not battery-discharge watt evidence. Both temporary device binaries were removed.
- Cleanup removed 2,502,513,652 logical bytes: the 448,422,216-byte native test ELF, stripped test
  copy, benchmark/encoding scratch, four rendered manual pages, copied JNI dependencies, and
  reproducible Gradle/JNI/R8/native-symbol staging. It retained the 28,997,592-byte APK plus
  476-byte metadata and the 2,789,489,413-byte active ARM64 CMake/Ninja cache. C: recovered
  2,059,714,560 physical bytes and reported 81,224,990,720 bytes free immediately afterward. No
  PDF or rendered manual artifact was committed.
- This is optimization 106 in the overlapping Thor work tally. The 1.20x-3.01x measurements apply
  only while executing these exact halfword-pack forms. They cannot be added to the other 105
  items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal result. Those
  still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/
  duration A/B run.

## ARM64 A32 Scalar Extend-And-Add (2026-08-18)

- A32 ARM/Thumb-2 `SXTAB`, `SXTAH`, `UXTAB`, and `UXTAH` are part of the 3DS ARM11 guest ISA. With
  rotation zero, Dynarmic previously built a narrow, sign/zero extension, and generic add. It now
  retains `SignedExtendAndAdd32` or `UnsignedExtendAndAdd32` as first-class IR, and ARM64 emits one
  extended-register `ADD` using `SXTB`, `SXTH`, `UXTB`, or `UXTH`. x64 and RISC-V polyfill the new
  IR back into the established exact DAG. Nonzero guest rotations deliberately keep their previous
  IR path.
- The complete arithmetic/extend/shift pages in the Cortex-A510, A710, A715, and X3 software
  optimization guides were rendered and visually checked before measurement. A510 lists extended
  `ADD`/`SUB` at latency 1 and throughput 3, with latency 2 when the dependency is on `Rm`. A710,
  A715, and X3 list latency 2 and throughput 2 on the M pipeline. This supported measuring each
  accessible Thor core class instead of extrapolating from one core.
- `llvm-objdump` verified the intended sequence change for every width and signedness: rotation-zero
  forms changed from `SXTB`/`SXTH`/`UXTB`/`UXTH` plus `ADD` to one extended-register `ADD`.
  Nonzero controls changed from `ROR` plus extension plus add to `ROR` plus extended add. The
  standalone harness used four independent chains for 4,000,000 iterations, or 16,000,000 affected
  operations per sample, over nine alternating-order rounds. Every warmup and timed checksum
  matched and remained nonzero.

  | Guest-equivalent form | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `SXTAB`, ROR #0 | 1.330541x | 1.165637x | 1.129608x | not measurable |
  | `SXTAH`, ROR #0 | 1.332116x | 1.171669x | 1.135539x | not measurable |
  | `UXTAB`, ROR #0 | 1.329924x | 1.171331x | 1.126554x | not measurable |
  | `UXTAH`, ROR #0 | 1.340523x | 1.204479x | 1.126133x | not measurable |

- A510 rotation-zero medians changed from 0.504124 to 0.378887 ns/op for `SXTAB`, 0.502028 to
  0.376865 for `SXTAH`, 0.503691 to 0.378737 for `UXTAB`, and 0.504987 to 0.376709 for `UXTAH`.
  The nonzero controls measured 1.000410x, 1.000475x, 0.994994x, and 0.998904x respectively on
  A510. Although those controls improved by 1.017667x-1.052110x on A715 and
  1.469712x-1.474514x on A710, the A510 results were neutral and `UXTAB` ROR #24 repeated a 0.50%
  regression. The optimization is therefore limited to rotation zero.
- X3 is intentionally not reported as a physical result. Android exposed CPU 7 as online, but
  `/sys/devices/system/cpu/cpu7/core_ctl/active_cpus` remained zero, single-bit CPU 6/7 affinity
  masks returned `EINVAL`, and a short seven-load helper did not unpark it. The X3 guide informed
  the implementation review only; it cannot substitute for a benchmark.
- Permanent coverage executes 40 ARM/Thumb encodings over six input patterns. It covers all four
  operations, rotation-zero optimized forms, nonzero fallback controls, destination/addend/value
  aliases, high registers, signed and unsigned edge values, every unrelated GPR, and unchanged
  NZCV/Q/GE flags. The final native ARM64 build passed with JDK 17 in 1 minute 32 seconds, and Thor
  passed all 7,486 assertions in 29 focused `[core][arm][dynarmic]` cases. Source/test commit
  `1741a60c2` was pushed directly to `origin/master` over command-line Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build
  passed with JDK 17 in 3 minutes 4 seconds. Its ARM64-only APK is 28,999,604 bytes, reports
  `1741a60c2-vanilla-thor`, and has SHA-256
  `21F28D5C29BB3E26A5FD7B0FA4EE2CAA000272668080D1BC7EF39E6994C3DC56`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 55%, 3.880 V, and 21.0 C, so
  this is not battery-discharge watt evidence. All temporary device helpers were removed.
- Cleanup removed 2,469,674,954 logical bytes: the 448,479,264-byte native test ELF plus
  reproducible Gradle/JNI/R8/native-symbol/mapping staging. It retained the 28,999,604-byte APK,
  476-byte metadata, and 2,797,132,682-byte active ARM64 CMake/Ninja cache. C: recovered
  2,058,350,592 physical bytes and reported 81,383,247,872 bytes free immediately afterward.
  Benchmark source/binaries, encoding scratch, stripped tests, helper scripts, and rendered manual
  pages had already been removed; no PDF or rendered manual artifact was committed.
- This is optimization 107 in the overlapping Thor work tally. The 1.13x-1.34x measurements apply
  only while executing these exact rotation-zero extend-and-add forms. They cannot be added to the
  other 106 items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal
  result. Those still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/
  fan/brightness/duration A/B run.

## Rejected ARM64 A32 MLA/MLS MADD/MSUB Fusion (2026-08-18)

- A tempting A32 Dynarmic change was to replace the current split `MUL` plus `ADD`/`SUB` lowering
  for `MLA`/`MLS` with native AArch64 `MADD`/`MSUB`. The complete integer multiply tables in the
  Cortex-A510, A710, A715, and X3 software optimization guides were reviewed first. A510 documents
  W-form multiply-add/subtract latency 3, throughput 1, and typical accumulator forwarding every
  two cycles; the three larger cores document latency 2 with accumulator forwarding 1 and
  throughput 1. The tables made dependency structure a required measurement dimension rather than
  a reason to assume the fused instruction was universally better.
- `llvm-objdump` verified exact split and fused sequences. A standalone harness measured both four
  independent chains and a sequential accumulator chain for 16,000,000 affected operations per
  sample over nine alternating-order rounds. Every checksum matched and remained nonzero. Ratios
  below are fused divided by the retained split path; values below 1.0 are regressions.

  | Form and dependency pattern | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `MLA`, independent | 1.241811x | 0.690163x | 0.625852x | 0.554212x |
  | `MLA`, dependent | 0.613736x | 0.739660x | 0.997213x | 1.001076x |
  | `MLS`, independent | 1.235381x | 0.668825x | 0.624810x | 0.554066x |
  | `MLS`, dependent | 0.595157x | 0.726579x | 1.001818x | 1.000144x |

- The fused lowering was rejected and the split `MUL` plus `ADD`/`SUB` path remains unchanged.
  This experiment does not increment the optimization tally. Its source, binaries, encodings, and
  device helper were removed after measurement; no manual PDF or rendered page was copied into the
  repository.

## ARM64 A32 Packed Halfword Saturation (2026-08-18)

- A32 ARM/Thumb-2 `SSAT16` and `USAT16` are ARM11 guest instructions. Dynarmic previously extracted
  and sign-extended both halfwords, invoked the scalar saturation operation twice, repacked them,
  derived two overflow results, and updated sticky `CPSR.Q` twice. The frontend now emits
  `PackedSignedSaturation16` or `PackedUnsignedSaturation16` plus one overflow pseudo-result and one
  `A32OrQFlag` call.
- ARM64 sign-extracts both lanes, shares the min/max constants, clamps with scalar `CMP`/`CSEL`,
  packs with `BFI`, and compares the packed result against the input once. Signed saturation to 16
  bits aliases the input and reports no overflow; unsigned saturation to zero bits returns zero and
  performs the required comparison. AdvSIMD `SQSHL`/`SQSHLU` were deliberately not used because
  their host `FPSR.QC` side effect could incorrectly alter guest VFP `FPSCR.QC`; this guest
  instruction updates only ARM11 `CPSR.Q`. x64 and RISC-V polyfill the new IR back into the exact
  established two-lane scalar DAG.
- `llvm-objdump` verified the intended old and new sequences. The standalone harness measured
  representative signed and unsigned 8-bit saturation, with the sticky-Q load/OR/store included.
  It used four independent operations and four sequential dependent operations per loop,
  4,000,000 affected operations per sample, and nine alternating-order rounds. Every checksum
  matched and remained nonzero.

  | Guest-equivalent path | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `SSAT16`, independent | 1.314113x | 1.999041x | 2.024448x | 2.031681x |
  | `SSAT16`, dependent | 1.207003x | 1.999299x | 1.966196x | 1.505940x |
  | `USAT16`, independent | 1.092654x | 2.000018x | 2.007952x | 2.031970x |
  | `USAT16`, dependent | 1.124863x | 2.000474x | 1.928901x | 1.495144x |

- Permanent coverage generates all 128 operation/immediate/encoding/alias combinations: ARM and
  Thumb, signed immediates 1-16, unsigned immediates 0-15, and aliased or distinct source and
  destination registers. Ten mixed-lane inputs and both initial Q states verify exact output,
  every unrelated GPR, unchanged NZCV/GE and FPSCR, and sticky CPSR.Q. The final native ARM64 build
  passed with JDK 17 in 1 minute 56 seconds. Thor passed the full 51,007-assertion, 30-case focused
  suite; the new test passed all 43,521 assertions when pinned separately to CPU 0/A510,
  CPU 3/A715, CPU 5/A710, and CPU 7/X3. Source/test commit `aac808826` was pushed directly to
  `origin/master` over command-line Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` cold
  build passed with JDK 17 in 13 minutes 9 seconds. Its ARM64-only APK is 28,999,820 bytes, reports
  `aac808826-vanilla-thor`, and has SHA-256
  `8BE9CA081B05BA8589AF2EE5C080563D01EB19B1C4C9CDE2C014C7A4C7439A41`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 55%, 3.907 V, and 22.0 C, so
  this is not battery-discharge watt evidence.
- Cleanup removed 2,493,164,811 logical bytes: the 447,536,016-byte native test ELF, benchmark and
  encoding scratch, rendered manual pages, and reproducible Gradle/JNI/R8/native-symbol/mapping
  staging. It retained the 28,999,820-byte APK, 476-byte metadata, and 2,784,972,401-byte active
  ARM64 CMake/Ninja cache. C: recovered 2,051,076,096 physical bytes and reported 80,769,478,656
  bytes free immediately afterward. Both temporary device helpers were removed; no PDF, manual
  page, benchmark binary, or scratch note was committed.
- This is optimization 108 in the overlapping Thor work tally. The 1.09x-2.03x measurements apply
  only while executing these exact packed-saturation forms. They cannot be added to the other 107
  items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal result. Those
  still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/
  duration A/B run.

## ARM64 A32 Packed Byte Sign Extension (2026-08-18)

- A32 ARM/Thumb-2 `SXTB16` is part of the 3DS ARM11 guest ISA. Dynarmic previously rotated the
  source when requested, selected bytes 0 and 2 with `0x00FF00FF`, selected their sign bits with
  `0x00800080`, multiplied the sign bits by `0x1FE`, and ORed the pieces together. On ARM64 that
  exact IR became `AND`, `AND`, constant materialization, `MUL`, and `ORR` after any rotate.
- The frontend now emits first-class `PackedSignExtendByteToHalf` IR for rotations 0, 8, 16, and
  24. ARM64 uses `SBFX` to extract and sign-extend the selected upper byte into a scratch register,
  `SXTB` for the selected low byte, and `BFI` to insert the upper halfword. Disassembly review caught
  the required alias order before implementation: `SBFX` must read the upper byte before `SXTB`
  writes a final-use register that may alias the source. x64 and RISC-V polyfill the operation back
  into the established portable mask/multiply DAG.
- Local Cortex-A510, A710, A715, and X3 optimization-guide tables were reviewed before selecting
  the sequence. They document the relevant `SBFM`/`SBFX`, `SXTB`, and `BFM`/`BFI` latency and
  throughput characteristics, but the mixed results predicted by those tables were treated only as
  a reason to benchmark every Thor core class. No manual PDF or rendered page entered the repo.
- `llvm-objdump` verified the exact five-instruction old body and three-instruction new body. A
  standalone ARM64 harness measured rotation-zero and rotation-eight forms with four independent
  chains and a sequential dependent chain. Each sample executed 16,000,000 affected operations
  (32,000,000 in the final A510 rerun), nine rounds alternated old/new order, and every checksum
  matched and remained nonzero. Lower nanoseconds per operation are better; ratios are old/new.

  | Guest-equivalent path | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | ROR 0, independent | 1.332951x | 1.856836x | 1.291697x | 1.615659x |
  | ROR 0, dependent | 1.248735x | 2.042547x | 1.331349x | 1.336929x |
  | ROR 8, independent | 1.000965x | 1.541483x | 1.235306x | 1.301351x |
  | ROR 8, dependent | 1.198771x | 1.638419x | 1.249064x | 1.240429x |

- Permanent coverage generates all 16 encoding/rotation/alias combinations: ARM and Thumb,
  rotations 0/8/16/24, and aliased or distinct source and destination. Ten mixed inputs cover both
  sign edges and ignored-byte garbage while verifying exact output, every unrelated GPR, unchanged
  NZCV/Q/GE, and unchanged FPSCR. The new test passed all 2,721 assertions when pinned separately
  to CPU 0/A510, CPU 3/A715, CPU 5/A710, and CPU 7/X3. The final focused suite passed 53,728
  assertions in 31 cases. The final native verification build passed with JDK 17 in 1 minute 5
  seconds. Source/test commit `e58d8e1c3` was pushed directly to `origin/master` over command-line
  Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build
  passed with JDK 17 in 2 minutes 23 seconds. Its ARM64-only APK is 29,001,332 bytes, reports
  `e58d8e1c3-vanilla-thor`, and has SHA-256
  `E89B15D0B7AFE42D2B44FE9F44B9904BD9CDBA68C68550E26036BA87BBD0AF11`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 57%, 3.877 V, and 22.0 C, so
  this is not battery-discharge watt evidence.
- Cleanup removed 2,493,001,646 logical host bytes: the 447,569,976-byte native test ELF,
  standalone benchmark and stripped test copy, rendered manual pages, and reproducible Gradle/JNI/
  R8/native-symbol/mapping staging. It retained the 29,001,332-byte APK, 476-byte metadata, and
  2,785,534,773-byte active ARM64 CMake/Ninja cache. C: recovered 2,053,423,104 physical bytes and
  reported 80,681,345,024 bytes free immediately afterward. The 86,360-byte benchmark and
  26,090,904-byte stripped test helper were also removed from the Thor; no PDF, manual page,
  benchmark binary, or scratch note was committed.
- This is optimization 109 in the overlapping Thor work tally. The 1.00x-2.04x measurements apply
  only while executing these exact packed sign-extension forms. They cannot be added to the other
  108 items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal result.
  Those still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/
  brightness/duration A/B run.

## ARM64 A32 Rotated Packed Sign-Extend-and-Add (2026-08-18)

- A32 ARM/Thumb-2 `SXTAB16` sign-extends bytes 0 and 2 of an optionally rotated source, adds them
  independently to the two destination halfwords, and wraps each lane modulo 16 bits. Dynarmic's
  old frontend rebuilt the same two-mask, sign-mask-times-`0x1FE`, and OR DAG used by the former
  `SXTB16` lowering, then crossed into SIMD for `PackedAddU16`. With a nonzero guest rotation the
  complete ARM64 body was ten host instructions: one `ROR`, five sign-extension instructions, two
  GPR-to-SIMD `FMOV`s, one halfword `ADD`, and one SIMD-to-GPR `FMOV`.
- Three disassembly-checked bodies were measured. The accepted composition routes rotations
  8/16/24 through `PackedSignExtendByteToHalf`, replacing the five-instruction sign-extension body
  with `SBFX`, `SXTB`, and `BFI`; the required `ROR` plus shared packed-add transfers remain, so the
  full nonzero path falls from ten instructions to eight. The unmodified x64 and RISC-V polyfill
  expands the first-class operation back into the same portable DAG.
- A more aggressive five-instruction rotation-zero AdvSIMD body used GPR-to-SIMD `FMOV`, byte
  `UZP1`, another `FMOV`, signed widening `SADDW`, and a final `FMOV` (six instructions with a
  rotation). It was rejected: the doubled X3 run measured 0.894154x for independent rotation zero
  and 0.978046x for independent ROR8, regressions of 10.6% and 2.2%. Applying the scalar
  composition to rotation zero was also rejected after its doubled X3 independent result repeated
  at 0.979993x, a 2.0% regression. Rotation zero therefore retains the old lowering.
- The Cortex-A510/A710 AArch32 tables document native `SXTAB16` latency/throughput differences,
  while all four AArch64 tables document materially different `SBFM`/`BFM` costs. That manual
  evidence correctly warned against accepting instruction count alone. No manual PDF or rendered
  page was copied into the repository.
- `llvm-objdump` verified the exact old, composed, and rejected fused bodies. The standalone
  benchmark used four independent source-alias chains and one source-alias chain repeated four
  times sequentially per loop. Each sample executed 16,000,000 affected operations; the final X3
  confirmation used 32,000,000. Nine rounds rotated the order of all three candidates, and every
  checksum matched and remained nonzero. The accepted ROR8 old/composed median ratios were:

  | ROR8 dependency pattern | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | Four independent chains | 1.065097x | 1.159406x | 1.120201x | 1.067154x |
  | Sequential chain | 1.050745x | 1.159727x | 1.089628x | 1.076240x |

- Permanent coverage generates all 40 encoding/rotation/alias combinations: ARM and Thumb,
  rotations 0/8/16/24, all-distinct operands, destination/addend alias, destination/source alias,
  addend/source alias, and all operands aliased. Ten addend/source pairs cover positive and negative
  byte edges, ignored-byte garbage, carry and borrow wrap, and mixed lanes while verifying exact
  output, every unrelated GPR, unchanged NZCV/Q/GE, and unchanged FPSCR. The new test passed all
  6,801 assertions when pinned separately to CPU 0/A510, CPU 3/A715, CPU 5/A710, and CPU 7/X3.
  The final focused suite passed 60,529 assertions in 32 cases. The retained ARM64 Ninja graph built
  both the native tests and `libcitra-android.so`; source/test commit `624534787` was pushed directly
  to `origin/master` over command-line Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` build
  passed with JDK 17 in 3 minutes 23 seconds after the pinned Khronos validation-layer ZIP was
  restored to its expected build-temp path. Its ARM64-only APK is 29,002,140 bytes, reports
  `624534787-vanilla-thor`, and has SHA-256
  `B440EE1C11C3883D7558953442DDAB3371CE0F6AF35AC44888BEFD09DBEA3494`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 57%, 3.911 V, and 23.0 C, so
  this is not battery-discharge watt evidence.
- Cleanup removed 2,493,587,500 logical host bytes: the 447,603,224-byte native test ELF,
  standalone benchmark/source and stripped test copy, rendered manual pages, pinned validation
  ZIP, and reproducible Gradle/JNI/R8/native-symbol/mapping staging. It retained the 29,002,140-byte
  APK, 476-byte metadata, and 2,785,715,434-byte active ARM64 CMake/Ninja cache. C: recovered
  2,053,017,600 physical bytes and reported 79,950,155,776 bytes free immediately afterward. The
  629,032-byte benchmark and 26,095,448-byte stripped test helper were also removed from the Thor;
  no PDF, manual page, benchmark binary, or scratch note was committed.
- This is optimization 110 in the overlapping Thor work tally. The 1.05x-1.16x measurements apply
  only while executing these exact nonzero-rotation packed sign-extend-and-add forms. They cannot
  be added to the other 109 items or treated as a whole-game FPS, sustained battery-watt,
  frametime, or thermal result. Those still require a matched title/scene/cache/renderer/driver/
  resolution/layout/mode/fan/brightness/duration A/B run.

## ARM64 A32 Native Bit Reversal (2026-08-18)

- A32 ARM/Thumb-2 `RBIT` previously expanded in the frontend to a portable mask/shift/OR network:
  two AND-and-shift pairs plus OR, followed by four AND-and-shift pairs plus three ORs. The ARM64
  backend consequently emitted 17 host instructions for a guest operation with a native scalar
  instruction.
- Dynarmic now retains the operation as first-class `ReverseBits32` IR. ARM64 emits one native
  `RBIT`; the unmodified x64 and RISC-V semantics are preserved by polyfilling the IR operation
  back to the exact old network. The A32 A64-guest frontend was intentionally left unchanged.
- The locally reviewed Cortex manuals list AArch64 `RBIT` at latency/throughput 2/3 on A510 page
  22, 1/4 on A710 page 27, 1/4 on A715 page 20, and 1/6 on X3 page 18. This made the semantic IR
  route a strong candidate, but the Thor benchmark remained the acceptance test. No manual PDF or
  rendered page was copied into the repository.
- Standalone disassembly verified the exact old 17-instruction loop and new single-`RBIT` loop.
  The benchmark ran four independent operations or one sequential dependent chain repeated four
  times per loop. Each sample executed 16,000,000 affected operations; 11 samples alternated
  old/new order, and medians are reported below.

  | Dependency pattern | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | Four independent chains | 17.584485x | 12.871236x | 11.306165x | 15.101148x |
  | Sequential chain | 7.024895x | 9.056788x | 9.644375x | 9.113942x |

- Permanent ARM and Thumb-2 coverage checks nine values, distinct operands, source/destination
  aliases, every unrelated GPR, NZCV/Q/GE, and FPSCR. The new test passed 612 assertions when
  pinned separately to CPU 0/A510, CPU 3/A715, CPU 5/A710, and CPU 7/X3. The full focused
  `[core][arm][dynarmic]` suite passed 61,141 assertions in 33 cases. Source/test commit
  `ff4994c54` was pushed directly to `origin/master` using command-line Git SSH.
- The exact post-source-commit `:app:assembleVanillaRelWithDebInfoLite` build with
  `--no-configuration-cache` passed with JDK 17. The retained ARM64-only APK is 28,998,524
  bytes, reports `ff4994c54-vanilla-thor`, and has SHA-256
  `D3D965EC21CC0D3FF5E94B4D1B8842AF00E2102FC84036E8F59FA38F5CBF3CAF`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 57%, 3.847 V, and 23.0 C, so
  this is not battery-discharge watt evidence.
- Cleanup removed 2,492,895,929 logical host bytes of temporary benchmark/test and reproducible
  Gradle/JNI/R8/native-symbol/mapping staging while retaining the APK, its 476-byte metadata, and
  the active ARM64 CMake/Ninja cache. C: recovered 2,051,411,968 physical bytes and reported
  79,644,041,216 bytes free immediately afterward. Temporary device helpers were also removed; no
  PDF, manual page, benchmark binary, or scratch note was committed.
- This is optimization 111 in the overlapping Thor work tally. The 7.02x-17.58x measurements apply
  only while executing this exact bit-reversal path. They cannot be added to the other 110 items
  or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal result. Those
  still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/brightness/
  duration A/B run.

## ARM64 A32 Native Halfword Byte Reversal (2026-08-18)

- A32 ARM and Thumb-2 `REV16` previously expanded into five recurring ARM64 instructions: `LSR`,
  mask, `LSL`, mask, and `ORR`. Thumb-16 used an even longer frontend graph that separately
  extracted, byte-reversed, zero-extended, shifted, and recombined both halfwords.
- Dynarmic now retains all three guest encodings as first-class `ByteReverseHalfwords32` IR. ARM64
  emits one native `REV16`; x64 and RISC-V preserve exact behavior by polyfilling the operation
  back to the established shift/mask/OR network.
- Visual inspection of the local Cortex manuals found AArch64 `REV16` latency/throughput 1/3 on
  A510 page 22, 1/4 on A710 page 27, 1/4 on A715 page 20, and 1/6 on X3 page 18. The pages were
  rendered only for review and immediately deleted; no manual PDF or rendered page was copied into
  the repository.
- `llvm-objdump` verified the exact five-instruction old loop and single-`REV16` new loop. The
  benchmark ran four independent operations or one sequential dependency chain repeated four
  times per loop. Each sample executed 16,000,000 affected operations; 11 samples alternated
  old/new order, and every final checksum matched at nonzero `0x000643d4`.

  | Dependency pattern | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | Four independent chains | 4.184574x | 3.789630x | 3.605787x | 4.435562x |
  | Sequential chain | 5.122643x | 3.096017x | 3.293049x | 3.268359x |

- Permanent coverage checks ARM, Thumb-16, and Thumb-2 encodings, nine values, distinct operands,
  source/destination aliases, every unrelated GPR, NZCV/Q/GE, and FPSCR. The new test passed all
  918 assertions when pinned separately to CPU 0/A510, CPU 3/A715, CPU 5/A710, and CPU 7/X3. The
  full focused `[core][arm][dynarmic]` suite passed 62,059 assertions in 34 cases. Source/test
  commit `a3c723d8b` was pushed directly to `origin/master` using command-line Git SSH.
- The exact post-commit `:app:assembleVanillaRelWithDebInfoLite` build with
  `--no-configuration-cache` passed with JDK 17 in 2 minutes 51 seconds. The retained ARM64-only APK
  is 28,998,496 bytes, reports `a3c723d8b-vanilla-thor`, and has SHA-256
  `73BEFC0F27839AE7E411A5B840A6E587013C4406C403E8DB4BBF6A6BDF972EF4`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 42%, 3.685 V, and 27.0 C, so
  this is not battery-discharge watt evidence.
- Cleanup removed 2,492,142,451 logical host bytes of temporary benchmark/test and reproducible
  Gradle/JNI/R8/native-symbol/mapping staging while retaining the 28,998,496-byte APK, its 476-byte
  metadata, and the 2,793,288,818-byte active ARM64 CMake/Ninja cache. C: recovered 2,052,165,632
  physical bytes and reported 79,188,664,320 bytes free immediately afterward. The 7,816-byte
  benchmark and 26,103,304-byte stripped test helper were also removed from the Thor; no PDF,
  manual page, benchmark binary, or scratch note was committed.
- This is optimization 112 in the overlapping Thor work tally. The 3.10x-5.12x measurements apply
  only while executing this exact halfword-byte-reversal path. They cannot be added to the other
  111 items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal result.
  Those still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/
  brightness/duration A/B run.

## ARM64 A32 Native Signed Halfword Byte Reversal (2026-08-18)

- A32 ARM, Thumb-16, and Thumb-2 `REVSH` previously emitted three recurring ARM64 instructions:
  `UXTH`, `REV16`, and `SXTH`. The high half of the input must remain irrelevant, and bit 15 of the
  byte-reversed low half must still sign-extend through the destination word.
- Dynarmic now retains all three guest encodings as first-class `ByteReverseSignedHalf32` IR.
  ARM64 emits `REV; ASR #16`; x64 and RISC-V preserve exact behavior by polyfilling the operation
  back to `LeastSignificantHalf`, `ByteReverseHalf`, and `SignExtendHalfToWord`.
- Visual inspection of the local Cortex manuals found AArch64 `REV`/`REV16` latency/throughput 1/3
  on A510 page 22, 1/4 on A710 page 27, 1/4 on A715 page 20, and 1/6 on X3 page 18. The `SBFM`
  group containing `SXTH` is 2/3 on A510 and 1/4 on the other three cores; the A510 guide notes
  immediate `ASR` as a latency-1 alias. That evidence required benchmarking both two-instruction
  candidates rather than choosing by instruction count. Rendered review pages were immediately
  deleted, and no manual PDF or page was copied into the repository.
- `llvm-objdump` verified the exact three-instruction old sequence and both two-instruction
  candidates. The benchmark ran four independent operations or one sequential dependency chain
  repeated four times per loop. Each sample executed 16,000,000 affected operations; 15 samples
  rotated old/`REV; ASR`/`REV16; SXTH` order. Checksums matched at nonzero `0xfffff63b` for the
  independent pattern and `0xffffd3a7` for the dependency pattern.

  | Dependency pattern | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | Four independent chains | 2.621212x | 1.570994x | 1.550576x | 1.605491x |
  | Sequential chain | 2.631136x | 1.499708x | 1.499609x | 1.499462x |

- `REV; ASR #16` won acceptance. `REV16; SXTH` was close on the larger cores and about 1.1% faster
  for A510 independent work, but it took 1.644622375 ns/op on the A510 dependency chain versus
  1.079433625 ns/op for `REV; ASR`. A temporary actual-JIT code dump produced raw words
  `5ac00a74 13107e94`, decoded as `rev w20, w19; asr w20, w20, #16`. The diagnostic hook and
  helper were removed; the final clean stripped test executable was byte-identical to the fully
  tested clean binary (SHA-256 `39CDFB99607D88BEF5E72DEA4600CA5770BFBB12354CDCB92CF85C438DF9FC38`).
- Permanent coverage checks ARM, Thumb-16, and Thumb-2 encodings, nine dirty-upper/sign-boundary
  values, distinct operands, source/destination aliases, every unrelated GPR, NZCV/Q/GE, and
  FPSCR. The new test passed all 918 assertions when pinned separately to CPU 0/A510, CPU 3/A715,
  CPU 5/A710, and CPU 7/X3. The full focused `[core][arm][dynarmic]` suite passed 62,977 assertions
  in 35 cases on CPU 3/A715. The clean JDK 17 ARM64 native build passed 2,200 Ninja steps in 14
  minutes 42 seconds. Source/test commit `cd95d873f` was pushed directly to `origin/master` using
  command-line Git SSH.
- Exact source-commit packaging with `:app:assembleVanillaRelWithDebInfoLite`, ordinary Gradle build
  caching, and `--no-configuration-cache` passed in 3 minutes 49 seconds. The retained APK is
  28,999,048 bytes, reports `cd95d873f-vanilla-thor`, and has SHA-256
  `EC2E530DC6E1AFEA2E5349C1934588E59AD02E3891182C1CD820331E62C1D7A3`. Wi-Fi ADB installed it over
  `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was empty,
  and no app UI or game was launched. Thor was USB-powered at 33%, 3.734 V, and 25.0 C, so this is
  not battery-discharge watt evidence.
- Cleanup removed the 448,682,880-byte native test ELF and reproducible Gradle/JNI/R8/native-symbol/
  mapping staging while retaining the APK, its 476-byte metadata, and the 2,788,591,339-byte active
  ARM64 CMake/Ninja cache. C: recovered 2,029,408,256 physical bytes and reported 78,889,033,728
  bytes free immediately afterward. Four temporary device helpers totaling 78,331,256 bytes were
  removed from `/data/local/tmp`; no PDF, manual page, benchmark binary, or scratch note was
  committed.
- This is optimization 113 in the overlapping Thor work tally. The 1.50x-2.63x measurements apply
  only while executing this exact signed-halfword byte-reversal path. They cannot be added to the
  other 112 items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal
  result. Those still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/
  fan/brightness/duration A/B run.

## ARM64 A32 Native Bitfield Extraction (2026-08-18)

- A32 ARM and Thumb-2 `UBFX` previously expanded to recurring `LSR; AND`; `SBFX` expanded to
  `LSL; ASR`. Dynarmic now retains them as first-class `UnsignedBitFieldExtract32` and
  `SignedBitFieldExtract32` IR. ARM64 emits one native `UBFX` or `SBFX`, and the legal full-width
  `lsb=0,width=32` identity aliases the source without emitting an instruction. x64 and RISC-V
  polyfill the new operations back to the exact established shift/mask graphs.
- Visual inspection of the local Cortex guides found the AArch64 basic `SBFM`/`UBFM` bitfield group
  at latency/throughput 2/3 on A510 page 22, 1/4 on A710 page 27, 1/4 on A715 page 20, and 1/6 on
  X3 page 18. The A510 footnote lists the simple immediate `LSL`/`LSR`/`ASR` aliases at latency 1,
  predicting a throughput win but a possible dependency tie against the old two-operation graph.
  Rendered review pages were deleted immediately; no manual PDF or rendered page was copied into
  the repository.
- A temporary actual-JIT trace captured raw words `53083e74`, `53054674`, `13083e74`, and
  `13054674`. Host `llvm-objdump` decoded them as `ubfx w20,w19,#8,#8`,
  `ubfx w20,w19,#5,#13`, `sbfx w20,w19,#8,#8`, and `sbfx w20,w19,#5,#13`. The trace hook was
  removed, the final stripped binary contained no trace marker, and its focused test passed all
  8,161 assertions.
- The standalone benchmark was disassembly-checked for the exact old and new instruction bodies.
  It ran four independent chains or one sequential dependency chain, 32,000,000 affected
  operations per sample, 15 samples, and alternating old/new order. Every old/new checksum matched
  and remained nonzero.

  | Guest operation and dependency pattern | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `UBFX`, four independent chains | 1.5054x | 2.0185x | 2.0231x | 2.0579x |
  | `SBFX`, four independent chains | 2.1327x | 2.0176x | 2.0218x | 2.0581x |
  | `UBFX`, sequential chain | 1.0342x | 1.9992x | 1.9997x | 2.0001x |
  | `SBFX`, sequential chain | 1.0209x | 1.9999x | 2.0006x | 1.9994x |

- Permanent coverage checks signed and unsigned ARM/Thumb-2 forms; fields `{0,1}`, `{0,32}`,
  `{31,1}`, `{8,8}`, `{5,13}`, and `{16,16}`; ten boundary/dirty inputs; distinct and
  source/destination-alias operands; every unrelated GPR; NZCV/Q/GE; and FPSCR. The 8,161-assertion
  case passed separately on CPU 0/A510, CPU 3/A715, CPU 5/A710, and CPU 7/X3. The full focused
  `[core][arm][dynarmic]` suite passed 71,138 assertions in 36 cases on CPU 3/A715. A clean native
  ARM64 build passed 2,200 Ninja steps in 13 minutes 18 seconds; the final trace-free incremental
  rebuild passed four steps in 1 minute 24 seconds. Source/test commit `f4bc8cae9` was pushed
  directly to `origin/master` using command-line Git SSH.
- Exact source-commit packaging with `:app:assembleVanillaRelWithDebInfoLite`, JDK 21, ordinary
  Gradle caching, and `--no-configuration-cache` passed in 3 minutes 54 seconds. The retained APK
  is 29,000,672 bytes, reports `f4bc8cae9-vanilla-thor`, and has SHA-256
  `FDD4B07D78AE38C5E7E68CFF544529453253FABC354F6543E39D519AC6C9376C`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported `stopped=true`, the process-ID check was
  empty, and no app UI or game was launched. Thor was USB-powered at 23%, 3.702 V, and 26.0 C, so
  this is not battery-discharge watt evidence.
- Cleanup removed 2,549,202,191 logical host bytes of the native test ELF, benchmark/test helpers,
  and reproducible Gradle/JNI/R8/native-symbol/mapping staging while retaining the APK, its
  476-byte metadata, and the 2,794,765,804-byte active ARM64 CMake/Ninja cache. C: recovered
  2,104,160,256 physical bytes and reported 78,131,527,680 bytes free immediately afterward. The
  four exact temporary device helpers were also removed; no PDF, manual page, benchmark binary, or
  scratch note was committed.
- This is optimization 114 in the overlapping Thor work tally. The 1.02x-2.13x measurements apply
  only while executing these exact bitfield-extract paths. They cannot be added to the other 113
  items or treated as a whole-game FPS, sustained battery-watt, frametime, or thermal result.
  Those still require a matched title/scene/cache/renderer/driver/resolution/layout/mode/fan/
  brightness/duration A/B run.

## ARM64 A32 Native Bitfield Insertion (2026-08-18)

- A32 ARM and Thumb-2 `BFI` previously expanded into four recurring ARM64 instructions: clear the
  destination field with `AND`, shift the source with `LSL`, mask the inserted field with another
  `AND`, and combine with `ORR`. Dynarmic now retains the operation as `BitFieldInsert32` or the
  single-input `BitFieldInsertSelf32` form. ARM64 emits one native `BFI`; x64 and RISC-V polyfill
  the operations back to the exact established graph. A full-width `lsb=0,width=32` replacement
  aliases the source without code, and a self insertion at `lsb=0` is also an identity. `BFC`
  deliberately remains unchanged because its ARM64 logical-immediate clear is already one
  instruction.
- The self opcode is a code-generation requirement, not just an IR naming distinction. The
  distinct lowering consumes a read/write destination and a separate source. The self lowering
  consumes one read/write value and emits `BFI` with the same physical register twice, preventing
  the allocator from materializing a hidden copy before the instruction.
- Visual inspection of the complete local Cortex manual pages found AArch64 `BFM` at
  latency/throughput 2/3 on A510 page 22, 2/2 on A710 page 27, 1/4 on A715 page 20, and 2/2 on X3
  page 18. That predicts large issue-throughput savings everywhere, a true distinct dependency win
  on A715, and possible distinct dependency ties on A510/A710/X3 because the old destination
  `AND` to `ORR` critical path can overlap the source shift/mask work. The rendered pages were
  deleted after review; no PDF or rendered manual page entered the repository.
- A temporary emitter-span trace captured raw words `331b3293` and `331b3273`. Host
  `llvm-objdump` decoded the complete spans as exactly `bfi w19,w20,#5,#13` and
  `bfi w19,w19,#5,#13`, proving both distinct and self forms are one instruction with no hidden
  copy. The diagnostic was removed. The final stripped binary contained no `BFI115` marker and
  was byte-identical to the earlier clean binary: 26,122,328 bytes with SHA-256
  `7244AB37E03937C440C0D75070A74DFE21A36E189AFD737DF3C088DE1B559309`.
- The standalone benchmark's old and new bodies were disassembly-checked. Each invocation used 15
  samples, alternated old/new order, and executed 32,000,000 affected operations per sample.
  Four complete all-core invocations were run; every old/new checksum matched and was nonzero.
  The table reports the median of the four per-invocation medians so one X3 DVFS outlier cannot
  inflate the accepted result.

  | Operand/dependency pattern | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | Distinct, independent chains | 2.8367x | 2.5211x | 2.0470x | 2.0028x |
  | Self alias, independent chains | 2.8142x | 3.8745x | 2.6386x | 2.1808x |
  | Distinct, sequential chain | 0.9979x | 2.0011x | 1.0000x | 1.0002x |
  | Self alias, sequential chain | 1.5152x | 3.1851x | 1.5004x | 1.5037x |

- Permanent coverage checks ARM and Thumb-2 encodings; fields `{0,1}`, `{0,32}`, `{31,1}`,
  `{8,8}`, `{5,13}`, and `{16,16}`; ten boundary/dirty input pairs; distinct and destination/
  source-alias operands; every unrelated GPR; NZCV/Q/GE; and FPSCR. The 4,081-assertion case
  passed separately on CPU 0/A510, CPU 3/A715, CPU 5/A710, and CPU 7/X3. The final clean focused
  case again passed 4,081 assertions, and the complete `[core][arm][dynarmic]` suite passed 75,219
  assertions in 37 cases on CPU 3/A715. Source/test commit `f13065b0f` was pushed directly to
  `origin/master` using command-line Git SSH.
- Exact source-commit packaging with JDK 17, `:app:assembleVanillaRelWithDebInfoLite`, ordinary
  Gradle caching, and `--no-configuration-cache` passed in 3 minutes 43 seconds. The retained
  ARM64-only APK is 29,001,628 bytes, reports `f13065b0f-vanilla-thor`, and has SHA-256
  `9C84256BFF6FBC7CBAA91C504944CC8C07B9D75A33BB9884B6DC887F6764E6AB`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; Android reported no process after a final force-stop, and no
  app UI or game was launched. Thor reported AC power at 27%, 4.031 V, and 30.0 C, so the timing
  results are not battery-discharge watt evidence.
- Cleanup removed 2,575,854,404 logical host bytes of the native test ELF, benchmark/test/manual-
  render helpers, and reproducible Gradle/JNI/R8/native-symbol/mapping staging. C: recovered
  2,130,010,112 physical bytes and reported 77,687,324,672 bytes free. The retained active ARM64
  CMake/Ninja cache is 2,795,713,152 bytes; retained build output is only the 29,001,628-byte APK
  and its 476-byte metadata. Five exact device helpers totaling 104,500,104 bytes were removed
  from `/data/local/tmp`; no PDF, rendered manual page, benchmark binary, or scratch note was
  committed.
- This is optimization 115 in the overlapping Thor work tally. The 0.9979x-3.8745x measurements
  apply only while executing these exact bitfield-insert patterns; the 0.9979x A510 distinct-chain
  median is an effectively neutral 0.21% difference consistent with the manual-predicted tie. The
  values cannot be added to the other 114 items or treated as whole-game FPS, sustained battery-
  watt, frametime, or thermal results. Those still require a matched title/scene/cache/renderer/
  driver/resolution/layout/mode/fan/brightness/duration A/B run. A32 `MOVT` to native ARM64 `MOVK`
  is the next scalar JIT candidate, but it needs the same alias, disassembly, correctness, and
  all-core measurement gates before implementation.

## ARM64 A32 Native Move Top Half (2026-08-18)

- A32 ARM and Thumb-2 `MOVT` previously built a generic low-half `AND` plus shifted-immediate `OR`
  graph. Dynarmic now retains nonzero forms as `MoveTopHalf32`. ARM64 reads and writes the same
  allocation and emits the exact architectural match, `MOVK Wd,#imm,LSL#16`; x64 and RISC-V
  polyfill back to the established graph. The central emitter retains immediate zero as
  `AND Wd,Wd,#0xffff`, because identity removal already made that old path one instruction.
- The complete local Cortex optimization-guide pages were used, not instruction-count intuition.
  The move-wide family containing `MOVN`/`MOVZ`/`MOVK` is latency/throughput 1/3 on A510 page 22,
  1/4 on A710 page 27, 1/4 on A715 page 20, and 1/6 on X3 page 18. The Arm architecture semantics
  also match exactly: MOVK retains every destination bit outside the selected 16-bit halfword and
  does not update flags. No PDF or rendered manual page entered Git.
- A temporary emitter trace captured raw JIT word `72a24693`; host `llvm-objdump` decoded it as
  exactly `movk w19,#0x1234,lsl #16`, proving a one-instruction span with no hidden register copy.
  The diagnostic was removed. The final stripped test binary contains no `THOR_MOVT116` marker,
  is 26,128,600 bytes, and has SHA-256
  `179AA28540897777CAA0EC5D3C4D332300FF3843944CD824E7B7B512F81EAD0B`.
- The standalone old/new bodies were disassembly-checked. The representative `0x1234` old body is
  `AND; MOVZ; ORR`, an OR-encodable `0xffff` old body is `AND; ORR`, and the identity-reduced zero
  body is one `AND`; each candidate is one MOVK. Each invocation used 15 samples, alternated
  old/new order, and executed 32,000,000 affected guest operations per sample. Three complete
  invocations ran on each Thor core class; the table reports the median of those three per-run
  medians.

  | Immediate/dependency pattern | A510 CPU 0 | A715 CPU 3 | A710 CPU 6 | X3 CPU 7 |
  | --- | ---: | ---: | ---: | ---: |
  | `0x1234`, independent chains | 2.6231x | 2.8808x | 2.9015x | 2.7145x |
  | `0x1234`, sequential chain | 2.0075x | 2.0001x | 1.9990x | 1.9993x |
  | `0xffff`, independent chains | 2.0936x | 1.9489x | 1.9424x | 1.8513x |
  | `0xffff`, sequential chain | 2.0037x | 2.0000x | 1.9996x | 2.0001x |

- The initial `MOVT #0` candidate was not accepted blindly. Its independent A510 result repeatedly
  regressed by 7.1%-9.0%, while A715/A710 were effectively tied and X3 was DVFS-sensitive. The
  final zero guard emits the identical old one-AND path, so it cannot take that regression while
  every nonzero immediate keeps native MOVK.
- Permanent coverage checks ARM and Thumb-2 encodings; destination registers 0/4/8/12; zero, one,
  boundary, alternating, and dirty immediates; ten boundary/dirty register inputs; every unrelated
  GPR; NZCV/Q/GE; and FPSCR. The final 12,241-assertion case passed on CPU 0/A510, CPU 3/A715,
  CPU 6/A710, and CPU 7/X3. The complete `[core][arm][dynarmic]` suite passed 87,460 assertions in
  38 cases on CPU 3/A715. The diagnostic-free Android ARM64 native build passed in 1 minute 33
  seconds. Source/test commit `31968b954` was pushed directly to `origin/master` with command-line
  Git SSH.
- Exact source-commit packaging with JDK 17, ordinary Gradle caching,
  `:app:assembleVanillaRelWithDebInfoLite`, and `--no-configuration-cache` passed in 3 minutes 10
  seconds. The retained ARM64-only APK is 29,003,004 bytes, reports
  `31968b954-vanilla-thor`, and has SHA-256
  `3948DEE4659E8C91DF1E077604E0493DF1C259C3077B2EBC38066438CABFFAF4`. Wi-Fi ADB installed it
  over `org.azahar_emu.azahar.debug`; a final force-stop left no process, and no app UI or game was
  launched. Thor reported AC power at 59%, 4.226 V, and 35.0 C, so the timing results are not
  battery-discharge watt evidence.
- Cleanup removed 2,549,786,788 logical host bytes of stripped/unstripped tests, trace/benchmark
  helpers, and reproducible Gradle/JNI/R8/native-symbol/mapping staging. C: recovered
  2,108,264,448 physical bytes and reported 75,939,856,384 bytes free. The retained active ARM64
  CMake/Ninja cache is 2,790,551,470 bytes; retained build output is only the 29,003,004-byte APK
  and its 476-byte metadata. Four exact device helpers totaling 79,132,088 bytes were removed from
  `/data/local/tmp`; no PDF, rendered manual page, benchmark binary, or scratch note was committed.
- This is optimization 116 in the overlapping Thor work tally. The accepted 1.85x-2.90x
  independent and about 2.00x dependency results apply only while executing nonzero MOVT paths.
  They cannot be added to the other 115 items or treated as whole-game FPS, sustained battery-
  watt, frametime, or thermal results. Those still require a matched title/scene/cache/renderer/
  driver/resolution/layout/mode/fan/brightness/duration A/B run.

## High-Value Optimization Places

1. Data-driven Thor game profiles

   `ApplyAndroidGameProfile()` currently hardcodes E.X. Troopers. Move this toward a small data-driven loader or generated map from `src/android/app/src/main/assets/game_profiles/*.ini` so per-title settings can be added without expanding native `if` blocks.

   Useful profile knobs: resolution cap, custom texture disable/preload disable, shader settings, frame limit, GPU timing simulation, render-thread delay, and title-specific compatibility hacks.

2. Adreno 740 Vulkan driver testing

   The fork can now fetch and install recent generic Turnip builds, Turnip variants, and a Qualcomm fallback, but this is not the same as a fully tested Thor driver matrix. Keep tracking which package works best for 3DS workloads on Thor Base/Pro/Max, and avoid silently forcing a driver without user action.

3. Shader stutter testing

   `async_shader_compilation` defaults to off. On Adreno 740/Vulkan it is worth A/B testing per title, especially for games with shader compilation hitching. Do not flip it globally until visual correctness is checked.

4. Resolution and texture guardrails

   The Thor 8 Gen 2 can handle more than native resolution in many titles, but 3x+ can still be a bad default for heavy games or dual-screen presentation. Keep default 1x, cap problem titles at 2x, and avoid preload/custom textures unless a title is proven stable.

5. Compatibility-cost toggles

   `simulate_3ds_gpu_timings` improves correctness but can cost performance in some games. `delay_game_render_thread_us` is available for dynamic-framerate edge cases. These should be per-title profile toggles, not global Thor defaults.

6. Crypto workload profiling

   The false-negative CRC32/PMULL configure probes are repaired and the optional units retain
   runtime feature gates. Profile actual 3DS CRC, GCM, and GF(2) workloads before treating those
   latent hardware paths as a gameplay optimization; AES/SHA content paths were already hardware
   accelerated and crypto setup is not currently a sustained-game-FPS premise.

7. Future preprocessed-texture cache evidence

   Texture-filter results already live in the owning rasterizer surface's scaled GPU image until a
   guest write invalidates or uploads the region, so a separate disk cache would add hashing, I/O,
   synchronization, and storage without evidence of a power win. The final screen Anime4K filter
   is different and normally runs once per presented frame because its input changes each frame.

   The per-game manager now reports and deletes the real persistent Vulkan/OpenGL shader caches.
   Add a preprocessed/decoded texture-cache category only after a prototype demonstrates a warm-run
   time or energy win greater than hashing, storage I/O, synchronization, invalidation, and extra
   storage/VRAM costs on Thor. Keep texture dumps and downloaded packs visibly separate; a future
   pack-uninstall action must be labeled as deletion of user content, not cache cleanup.

## Benchmark Checklist

- Test with the release-style Thor APK: `:app:assembleVanillaRelWithDebInfoLite`.
- Use the same Thor Control Center performance mode, fan mode, brightness, and driver before comparing.
- Capture FPS, frametime stability, speed percentage, battery temperature, and whether audio crackles.
- Run one cold-cache pass and one warm-cache pass.
- Record the title ID, region, ROM revision, cheat preset, renderer, internal resolution, secondary display layout, and GPU driver.

## ARM64 A32 Narrow-Store Extension Elision (2026-08-18)

- Ordinary A32 byte and halfword stores previously materialized
  `LeastSignificantByte`/`LeastSignificantHalf` as ARM64 `UXTB`/`UXTH`, then immediately used
  `STRB`/`STRH`, whose architectural write width discarded the upper bits again. The ARM64 emitter
  now aliases the raw word only when the narrow value has exactly one use and that use is matching
  `A32WriteMemory8`/`A32WriteMemory16`. Shared values, other U8/U16 consumers, exclusive stores,
  mismatched widths, and endian-reversal paths retain canonical narrowing.
- The local Cortex software-optimization manuals identify `UXTB`/`UXTH` as aliases in the baseline
  `UBFM` group on X3 page 18, A715 page 20, A710 pages 27-28, and A510 pages 22-23. `STRB` and
  `STRH` consume the low byte/halfword by definition. This proves that the extension is redundant
  for the gated shape and that removing it saves an integer instruction; it does not prove a
  store-pipeline throughput or battery-power gain. No PDF or rendered manual page entered Git.
- Temporary emitter traces captured raw JIT words `38334b34` and `78334b34`. Capstone decoded the
  complete wrapper spans as exactly `strb w20, [x25, w19, uxtw]` and
  `strh w20, [x25, w19, uxtw]`, with no hidden extension or register copy. The diagnostic was
  removed. The final trace-free stripped test binary is 26,136,392 bytes, contains no
  `THOR_NARROW117` marker, and has SHA-256
  `295DDE99220E6B7BD7651A87EB348370AAFDB70F6F65E185890210A7F341F9FE`.
- A disassembly-checked standalone helper compared four independent old `UXTB/UXTH; STRB/STRH`
  chains against four direct stores. Each of 15 alternating-order samples executed 32,000,000
  affected stores and required matching byte/halfword checksums of 510/131070. The values below
  are old time divided by new time; A510 uses five invocation medians, A715 and A710 use three,
  and X3 reports the one valid invocation before Android `core_ctl` intermittently rejected later
  CPU 6/7 affinity masks with `EINVAL`:

  | Thor core | Direct `STRB` | Direct `STRH` |
  | --- | ---: | ---: |
  | A510 CPU 0 | 0.999368x | 1.002850x |
  | A715 CPU 3 | 0.999791x | 0.999637x |
  | A710 CPU 5 | 1.000054x | 1.000341x |
  | X3 CPU 7, one accepted invocation | 1.000024x | 1.000072x |

- The store-saturated loop is therefore throughput-neutral within noise even though the generated
  path falls from two host instructions to one. The accepted benefit is lower generated-code size
  and less fetch/decode/integer-issue work when these guest stores execute; a watt reduction is a
  plausible hypothesis, not a measured result.
- Permanent ARM and Thumb-16 coverage exercises `STRB`/`STRH`, distinct data/base operands and
  data-equals-base aliases, eight dirty/boundary inputs, callback and fastmem paths, all unrelated
  GPRs, NZCV/Q/GE, and FPSCR. The focused case passed 2,592 assertions on A510 CPU 0, A715 CPU 3,
  A710 CPU 6, and X3 CPU 7. The clean CPU-3 ARM64 Dynarmic suite passed 90,052 assertions in 39
  cases. The source change is commit `8bb915e32` and is pushed to `origin/master`.
- The exact post-commit JDK 17 `:app:assembleVanillaRelWithDebInfoLite`
  `--no-configuration-cache` build passed. The 29,003,652-byte APK has SHA-256
  `6241014BD33858C1A3BB37FC017C28968167398B02DC4E598B2E7B870D6AC58F`, package
  `org.azahar_emu.azahar.debug`, and version `8bb915e32-vanilla-thor`. It was installed over Wi-Fi
  ADB at `192.168.1.33:5555`, then verified `stopped=true` with no PID; the app/game was not
  launched.
- Cleanup removed 2,575,311,320 logical bytes: the 104,561,839-byte local scratch tree, the
  448,896,104-byte unstripped native test executable, and all reproducible Gradle staging. The
  reusable ARM64 CMake/Ninja cache is 2,796,842,775 bytes; retained Gradle output is only the
  29,003,652-byte APK and its 476-byte metadata. C: free space increased by 1,909,010,432 physical
  bytes from the pre-clean audit. All five exact `/data/local/tmp` helpers were removed.
- This is optimization 117 in the overlapping Thor work tally. It is not additive with the other
  116 entries and does not establish whole-game FPS, frametime, thermal, or wattage gains. Those
  claims still require a controlled matched title/scene/cache/renderer/driver/resolution/layout/
  performance-mode/fan/brightness/duration A/B run, which was intentionally not performed because
  the current instruction is not to launch the app.

## ARM64 A32 Signed Narrow-Load Fusion (2026-08-18)

- A32 `LDRSB`/`LDRSH` previously reached the ARM64 backend as an unsigned `A32ReadMemory8`/
  `A32ReadMemory16` followed by `SignExtendByteToWord`/`SignExtendHalfToWord`. Direct fastmem and
  page-table hits therefore emitted `LDRB; SXTB` or `LDRH; SXTH`. Dynarmic now emits one native
  `LDRSB`/`LDRSH` only when the extension is the load's sole immediately following consumer. The
  extension aliases the load result without code. Shared, non-adjacent, mismatched, ordered/
  acquire, exclusive, endian-reversed, A64, and unrelated shapes retain the established lowering;
  callback and fastmem/page-table fault fallbacks still sign-extend their narrow return explicitly.
- The complete external Cortex optimization-guide tables were inspected directly. Basic register-
  offset `LDRB`/`LDRSB` and `LDRH`/`LDRSH` share latency/throughput 4/3 on X3 page 19, 4/3 on A715
  page 21, 4/3 on A710 page 29, and 2/2 on A510 page 24. The same-cost signed load removes a real
  `SBFM` alias and dependency without enabling an optional ISA extension. No PDF or rendered manual
  page entered Git.
- A temporary emitter-boundary trace captured raw JIT words `38f34b34`, `78f34b34`, `38f54b33`,
  and `78f54b33`. Independent Capstone decoding identified exactly
  `ldrsb w20,[x25,w19,uxtw]`, `ldrsh w20,[x25,w19,uxtw]`,
  `ldrsb w19,[x25,w21,uxtw]`, and `ldrsh w19,[x25,w21,uxtw]`. The diagnostic and the unavailable
  Android disassembly experiment were removed before the final build; no trace marker remains.
- The standalone helper was disassembly-checked so each split body contained eight
  `LDRB/LDRH; SXTB/SXTH` pairs and each fused body contained eight `LDRSB/LDRSH` instructions,
  with the same eight adds, loop control, and nonzero checksums (`ffc2f700` byte, `7ff8f700`
  halfword). Each of eight alternating-order samples executed 4,000,000 iterations, or 32,000,000
  affected loads. The table reports median nanoseconds per affected load and old/fused speedup.

  | Thor core and path | Split ns/load | Fused ns/load | Old/fused |
  | --- | ---: | ---: | ---: |
  | A510 CPU 0, byte | 0.686038 | 0.556078 | 1.2337x |
  | A510 CPU 0, halfword | 1.203090 | 0.556183 | 2.1631x |
  | A715 CPU 3, byte | 0.370259 | 0.364127 | 1.0168x |
  | A715 CPU 3, halfword | 0.370347 | 0.363895 | 1.0177x |
  | A710 CPU 6, byte | 0.357950 | 0.357806 | 1.0004x |
  | A710 CPU 6, halfword | 0.357780 | 0.357963 | 0.9995x |

- The A510 loop therefore used 18.9% less median affected-path time for bytes and 53.8% less for
  halfwords; A715 used about 1.7% less, while A710 was neutral within 0.1%. Android intermittently
  rejected the CPU 5 and CPU 7 affinity masks with `EINVAL` despite listing them online, so no X3
  number is invented. The manuals still establish no extra signed-load cost on X3. These are
  load/accumulate-loop results, not an instruction-frequency-weighted emulator estimate.
- Permanent ARM and Thumb coverage checks byte/halfword loads, distinct and destination-equals-
  base forms, ten signed boundaries, callback and fastmem paths, every unrelated GPR, NZCV/Q/GE,
  and FPSCR. The focused case passed 3,040 assertions on CPU 3/A715. The full suite initially
  exposed and prevented two integration mistakes in unrelated producer handling and fallback
  register-allocation lifetime; after correction, the clean trace-free `[core][arm][dynarmic]`
  suite passed 93,092 assertions in 40 cases. Source/test commit `3f76c7440` was pushed directly to
  `origin/master` with command-line Git SSH.
- Exact post-commit packaging with JDK 17,
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache`, and ordinary Gradle caching
  passed in 3 minutes 41 seconds. The ARM64-only APK is 29,006,964 bytes, has SHA-256
  `44AF95AEB8A26DB45FE48F5C3464191A54DABEB97DD80522FF475C257CE972B8`, and reports package
  `org.azahar_emu.azahar.debug` version `3f76c7440-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no PID; neither the app nor a game was launched. Thor
  reported AC power at 80%, 4.273 V, and 25.0 C, so this is not battery-discharge watt evidence.
- Cleanup removed 2,575,457,579 logical host bytes: the 104,589,660-byte scratch set, the
  448,949,152-byte unstripped test ELF, and reproducible Gradle staging. C: recovered
  2,133,901,312 physical bytes and reported 56,508,461,056 bytes free. The retained active ARM64
  CMake/Ninja cache is 2,797,119,953 bytes; retained build output is only the 29,006,964-byte APK
  and 476-byte metadata. Five exact device helpers totaling 104,582,416 bytes were removed from
  `/data/local/tmp`; no PDF, benchmark, test binary, rendered manual page, or scratch note was
  committed.
- This is optimization 118 in the overlapping Thor work tally. The 0.9995x-2.1631x exact-loop
  results apply only while executing these signed narrow-load shapes and cannot be added to the
  other 117 items. Removing one generated instruction reduces code-cache, fetch/decode, and
  integer-issue work, so lower energy is plausible, but whole-game FPS, frametime, thermal, and
  wattage claims still require a controlled matched title/scene/cache/renderer/driver/resolution/
  layout/performance-mode/fan/brightness/duration A/B run.

## ARM64 Chained Narrow-to-Long Sign Extension (2026-08-18)

- Dynarmic can form `SignExtendByteToWord -> SignExtendWordToLong` or
  `SignExtendHalfToWord -> SignExtendWordToLong`. ARM64 previously emitted two dependent baseline
  bitfield aliases: `SXTB/SXTH Wd,Wn`, then `SXTW Xd,Wd`. AArch64 defines direct
  `SXTB/SXTH Xd,Wn` forms with the same result: sign-extending bits 7 or 15 directly to 64 bits is
  identical to first sign-extending them to 32 bits and then sign-extending bit 31 to 64 bits.
- The accepted emitter gate requires a non-immediate narrow source, exactly one use, and an
  immediately following `SignExtendWordToLong` consumer using the narrow extension as argument
  zero. The word extension aliases its input without code, and the long extension emits direct
  `SXTB X` or `SXTH X`. Immediate, shared, non-adjacent, mismatched, word-only, and unrelated forms
  retain the original lowering. Producer and consumer predicates are derived from the same helper
  so a fallback cannot consume an unextended value.
- ARM/Thumb-2 `SMLAWB`/`SMLAWT` currently produce the halfword chain while preserving their exact
  multiply, shift, modular add, overflow extraction, and sticky-Q graph. This optimization removes
  only the redundant intermediate sign extension; it does not apply the previously rejected
  fused/reassociated SMLAW multiply-accumulate candidate that regressed A715 and X3.
- Complete Cortex software-optimization tables were inspected directly. Baseline `SBFM` aliases
  are latency/throughput 1/6 on X3 page 18, 1/4 on A715 page 20, and 1/4 on A710 page 27. A510 page
  22 lists the basic family at 2/3, with latency 1 for the `SXTB` alias. The manuals therefore
  predict a dependency and issue reduction from deleting the second operation without requiring
  an optional extension or a core-specific build. No PDF or rendered manual page entered Git.
- A temporary emitter-boundary trace captured raw JIT words `93403e75`, `93403eb3`, and
  `93403eb6`. LLVM decoded them as `sxth x21,w19`, `sxth x19,w21`, and `sxth x22,w21` respectively.
  There was no preceding word-form `SXTH` or trailing `SXTW`. The diagnostic and temporary opcode
  object were removed before the final build.
- The standalone helper was disassembly-checked so its split independent bodies contained eight
  `SXTB/SXTH W` plus eight `SXTW X` instructions and its direct bodies contained eight
  `SXTB/SXTH X` instructions. Dependent bodies repeated the same shapes on one register. Each of
  eight alternating-order samples executed 4,000,000 iterations, or 32,000,000 affected
  operations, with matching nonzero split/direct checksums. The table reports old/direct median
  speedup.

  | Thor core | Byte independent | Half independent | Byte dependent | Half dependent |
  | --- | ---: | ---: | ---: | ---: |
  | A510 CPU 0 | 4.343224x | 4.259255x | 3.053356x | 1.986687x |
  | A715 CPU 3 | 1.815167x | 1.839695x | 1.999006x | 1.998513x |
  | A710 CPU 5 | 1.891211x | 1.891639x | 2.001035x | 1.999644x |
  | X3 CPU 7 | 2.001748x | 2.002068x | 2.000773x | 1.999879x |

- Android initially rejected CPU 7 affinity with `EINVAL` despite reporting CPUs 0-7 online and
  permitted. Brief load on the accessible big-core cluster let `core_ctl` accept the X3 run; all
  helpers were stopped afterward. Thor reported AC power, 80%, 4.271 V, and 25.0 C. These are
  instruction-sequence timings under wall power, not battery-discharge watt measurements.
- Permanent ARM and Thumb coverage exercises SMLAW bottom/top forms, distinct operands,
  destination aliases with each source role and the all-alias form, positive/negative overflow,
  initial sticky Q, unchanged NZCV/GE, every unrelated GPR, and unchanged FPSCR. The focused case
  passed 4,080 assertions. The clean final `[core][arm][dynarmic]` suite passed 97,172 assertions
  in 41 cases on Thor. Source/test commit `a8611bf76` was pushed directly to `origin/master` with
  command-line Git SSH.
- Exact JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 32 seconds.
  The ARM64-only APK is 29,006,484 bytes,
  has SHA-256 `ECE6AA41909B2571C6161A3513B6AEAD8794C9BE561516371E8A2B1D6468D4FA`, and reports package
  `org.azahar_emu.azahar.debug` version `a8611bf76-vanilla-thor`. It was installed over Wi-Fi ADB,
  immediately force-stopped, and verified `stopped=true` with no PID; neither the app nor a game
  was launched.
- Cleanup removed 2,497,115,187 logical host bytes: the 26,167,732-byte scratch tree, the
  448,982,400-byte unstripped native test executable, and reproducible Gradle staging. C: recovered
  2,070,192,128 physical bytes and reports 56,486,883,328 bytes free. The retained active ARM64
  CMake/Ninja cache is 2,797,255,579 bytes; retained Gradle output is only the 29,006,484-byte APK
  and 476-byte metadata. Both exact device helpers were removed; no PDF, benchmark, test binary,
  rendered manual page, or scratch note was committed.
- This is optimization 119 in the overlapping Thor work tally. The 1.815167x-4.343224x figures
  apply only to the exact removed instruction chain and cannot be added to the other 118 entries.
  Lower code-cache, fetch/decode, dependency, and integer-issue work makes lower energy plausible,
  but whole-game FPS, frametime, temperature, thermal slope, and watts still require a controlled
  matched title/scene/cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/
  duration A/B run.

## ARM64 Small Shifted-ADD Folding and VABA Rejection (2026-08-18)

- The first candidate was the obvious AArch64 replacement for same-width A32/A64 `VABA`:
  `SABD/UABD` followed by `ADD` can be expressed as one native accumulating `SABA/UABA`. The
  complete Cortex tables warned that this was not uniformly cheaper. X3 page 25 lists
  `SABD/UABD` latency/throughput 2/4 and `SABA/UABA` 4(1)/2; A715 pages 27-28 and A710 page 42
  list 2/2 versus 4(1)/1; A510 page 35 lists `SABD/UABD` latency 3 with split `2,1` throughput but
  `SABA/UABA` latency 6 with `1/2,1/4` throughput. No PDF or rendered page entered Git.
- A disassembly-checked helper compared eight independent and eight true accumulator-chain
  `SABD/UABD; ADD` operations with matching `SABA/UABA` operations. Each of nine alternating-order
  samples executed 4,000,000 iterations, or 32,000,000 affected operations, with matching
  checksums. Old/fused medians on A510 were:

  | Form | Independent | Accumulator dependency |
  | --- | ---: | ---: |
  | `SABA.8` | 2.005866x | 0.670348x |
  | `SABA.16` | 1.868680x | 0.670527x |
  | `SABA.32` | 1.972331x | 0.688968x |
  | `UABA.8` | 2.051850x | 0.671513x |
  | `UABA.16` | 1.971682x | 0.661446x |
  | `UABA.32` | 1.999445x | 0.659546x |

- The same six forms were 1.0168x-1.0337x independent and 1.9991x-2.0005x dependent on A715,
  0.9994x-1.0012x independent and 1.6656x-1.6671x dependent on A710, and
  1.9237x-2.0002x independent and 1.7325x-1.7339x dependent on X3. The native accumulator looked
  excellent on most big-core patterns but made the A510 accumulator chain 31.1%-34.0% slower.
  Global fusion was therefore rejected and no VABA source change was retained.
- The accepted candidate instead targets ordinary no-flags A32 adds whose second operand is a
  sole-use, immediately adjacent `LogicalShiftLeft32` by an immediate 1 through 4. ARM64 aliases
  the shift result to its input without emitting code and uses one
  `ADD Wd,Wbase,Windex,LSL #shift`. Flags/carry, shared/non-adjacent shifts, immediate sources,
  variable/zero shifts, shifts 5 through 31, subtraction, and unrelated consumers retain the old
  lowering. The producer and consumer use the same eligibility helper so a fallback cannot read an
  unshifted alias.
- The exact helper compared eight independent, eight base-dependent, and eight index-dependent
  old `LSL; ADD` chains with one-instruction shifted ADD chains. The main run used 4,000,000
  iterations and nine alternating-order samples per shape. A longer A510 confirmation used
  8,000,000 iterations, or 64,000,000 affected operations, and 15 samples. Old/fused A510 results
  for the retained gate were:

  | Shift | Independent | Base dependency | Index dependency |
  | ---: | ---: | ---: | ---: |
  | 1 | 1.286088x | 1.001042x | 1.007412x |
  | 2 | 1.475840x | 0.981775x | 0.997746x |
  | 3 | 1.335876x | 0.989126x | 0.987284x |
  | 4 | 1.492759x | 1.012459x | 1.006867x |

- For shifts 1 through 4, A715 independent/base/index results were 1.8124x-1.8351x,
  1.008x-1.011x, and 1.9985x-2.0005x; A710 was 1.890x-1.893x, about 1.000x, and
  1.999x-2.000x; X3 was 1.8477x-2.0012x, about 1.000x, and about 2.000x. The bounded gate trades
  large independent/front-end and big-core shifted-index wins for A510 dependency results close to
  one. It does not claim every synthetic dependency shape improves.
- Wider shifts were explicitly rejected. For shifts 16/31 the base-dependent form fell to about
  0.505x on A715, 0.500x on A710, and 0.500x on X3 even though independent forms sometimes won.
  The permanent gate therefore stops at four instead of assuming that fewer instructions always
  means better heterogeneous-core scheduling.
- Temporary emitter instrumentation captured 128 fused JIT emissions: 32 each for shifts 1, 2,
  3, and 4. The unique words were `0b130674`, `0b130a74`, `0b130e74`, and `0b131274`; their opcode
  and immediate fields validate as single 32-bit shifted-register ADD instructions with immediate
  1, 2, 3, and 4. The trace was removed, the emitter and production library were rebuilt, and both
  the 449,025,552-byte unstripped and 26,153,672-byte stripped clean test binaries contained no
  trace marker.
- Permanent ARM and Thumb-2 coverage exercises shifts 1/2/3/4 plus unfused 5/16/31, distinct
  operands, destination/base/index aliases, base-equals-index and all-alias forms, dirty/boundary
  inputs, 32-bit wrap, every unrelated GPR, NZCV/Q/GE, and FPSCR. The clean focused case passed
  9,520 assertions, and the final Thor `[core][arm][dynarmic]` suite passed 106,692 assertions in
  42 cases. Source/test commit `e984ad250` was pushed directly to `origin/master` with command-line
  Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 7 seconds.
  The ARM64-only APK is 29,007,336 bytes, has SHA-256
  `039E69AF41858A704545E4ABB3BCB4610C0F99C038B9F8DDBC95A25757FD0B6A`, and reports package
  `org.azahar_emu.azahar.debug` version `e984ad250-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no PID; neither app nor game was launched. Thor reported
  AC power, 80%, 4.271 V, and 25.0 C, so this is not battery-discharge watt evidence.
- Cleanup removed 2,550,451,023 logical host bytes: 79,360,075 bytes of scratch, the
  449,025,552-byte unstripped test ELF, and reproducible Gradle/JNI/R8/native-symbol/mapping
  staging. C: recovered 2,106,241,024 physical bytes and reports 56,474,251,264 bytes free. The
  retained active ARM64 CMake/Ninja cache is 2,797,552,350 bytes; retained build output is only the
  29,007,336-byte APK and 476-byte metadata. Three present device helpers totaling 52,669,048
  bytes were removed from `/data/local/tmp`; the already-absent VABA helper was also included in
  the exact cleanup command. Existing unrelated screenshots were left untouched. No PDF,
  benchmark, test binary, rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 120 in the overlapping Thor ledger: one bounded shifted-ADD
  optimization shipped and one unsafe global VABA fusion was permanently rejected. The exact-loop
  ratios cannot be added to the other 119 entries or treated as whole-game FPS, sustained watts,
  frametime, or thermal improvement. Those still require a controlled matched title/scene/cache/
  renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B run, which was
  intentionally not performed because the app/game was not to be launched.

## ARM64 Right-Shifted ADD Folding (2026-08-18)

- Command-line Git fetched authoritative `upstream/master` at
  `db15d78feb97ed19b6fc0354481e74694d339594`; the fork was 221 commits ahead and zero behind before
  this work, so no upstream merge was needed. Work remained on `master` and used the configured SSH
  remotes.
- Dynarmic previously materialized a sole A32 `LogicalShiftRight32` or
  `ArithmeticShiftRight32` before an immediately following flag-free `Add32`, even though AArch64
  can encode the shift directly in `ADD`. The retained gate requires one use, immediate adjacency,
  a non-immediate source, an immediate 1..31, no shift carry pseudo, no ADD flag/overflow pseudo,
  and carry-in false. Shared, non-adjacent, immediate-source, variable, zero/32, flag/carry,
  subtraction, and unrelated forms retain the old lowering. The separately measured LSL gate stays
  at 1..4.
- A disassembly-checked helper compared eight independent, eight base-dependent, and eight
  index-dependent old `LSR/ASR; ADD` bodies with one shifted-register `ADD`. Representative shifts
  1/2/3/4/8/16/31 used 4,000,000 iterations, or 32,000,000 affected operations per body, and nine
  alternating-order samples. Every old/fused checksum matched. Old-over-fused median ranges were:

  | Thor core | Independent | Base dependency | Index dependency |
  | --- | ---: | ---: | ---: |
  | A510 CPU 0 | 1.381850x-1.771904x | 1.437729x-1.872461x | 1.379672x-1.826679x |
  | A715 CPU 3 | 1.072932x-1.077689x | 1.087407x-1.093244x | 1.094345x-1.100728x |
  | A710 CPU 6 | 1.061272x-1.064909x | 1.059394x-1.064169x | 1.166458x-1.170341x |
  | X3 CPU 7 | 0.999391x-1.002055x | 1.015065x-1.124654x | 1.057515x-1.337368x |

  Thor reported AC power, 80% charge, 4.271 V, and 25.0 C at the start. These are wall-powered
  instruction-kernel timings, not battery-discharge watt measurements.

- The first X3 confirmation was discarded because the temporary core-wake workers were still
  eligible to run on CPU 7. The corrected protocol killed and reaped those workers before timing.
  Its first LSR#1 row still caught frequency settling, so a preconditioned 20,000,000-iteration,
  21-sample confirmation replaced it: 0.999391x independent, 1.024746x base-dependent, and
  1.337368x index-dependent. The table conservatively excludes one benefit-inflating LSR#2
  frequency-settling outlier. No contaminated result controls the gate.
- Temporary actual-emitter tracing plus `llvm-objdump` proved the generated words. LSR shifts
  1/2/3/4/5/16/31 decoded from `0b530674`, `0b530a74`, `0b530e74`, `0b531274`, `0b531674`,
  `0b534274`, and `0b537e74`; ASR used `0b930674`, `0b930a74`, `0b930e74`, `0b931274`,
  `0b931674`, `0b934274`, and `0b937e74`. Each is one
  `add w20,w19,w19,lsr/asr #shift`. Negative gates decoded as standalone LSL for shifts 5/16/31,
  `mov w20,wzr` for LSR32, and standalone `asr w20,w19,#31` for ASR32. The trace hook was removed;
  the 449,035,480-byte clean unstripped and 26,154,632-byte stripped test binaries contained zero
  trace markers and the clean stripped SHA-256 returned to
  `579B9E94800A20B8FA32FB5CC465104A672981BE242537CFF28F3B852EBDB8E7`.
- Permanent ARM and Thumb-2 coverage now exercises LSL/LSR/ASR; encoded shifts
  0/1/2/3/4/5/16/31; distinct operands; destination/base/index, base/index, and all-way aliases;
  signed ASR boundaries; modular wrap; every unrelated GPR; NZCV/Q/GE; and FPSCR. The clean focused
  case passed 32,640 assertions separately on A510 CPU 0, A715 CPU 3, A710 CPU 6, and X3 CPU 7.
  The clean full `[core][arm][dynarmic]` suite passed 129,812 assertions in 42 cases on A715.
  Source/test commit `752115dc9` was pushed directly to `origin/master` over command-line Git SSH.
- Exact JDK 17 packaging from source commit `752115dc9` with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 1 second.
  The ARM64-only APK is 29,008,576 bytes, has SHA-256
  `69937CA0CF18154A214FB3525ED5556169625A0493D310D0DB38F216219460AE`, and reports package
  `org.azahar_emu.azahar.debug` version `752115dc9-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no PID. Neither app nor game was launched.
- Exact cleanup removed 2,497,807,188 logical host bytes: 26,712,598 bytes of bounded scratch, the
  449,035,480-byte unstripped test ELF, and 2,022,059,110 bytes of reproducible Gradle/JNI/R8/
  native-symbol/mapping staging. C: recovered 2,027,413,504 physical bytes and reports
  56,242,896,896 bytes free. The retained active ARM64 CMake/Ninja cache is 2,797,658,936 bytes;
  retained build output is only the 29,008,576-byte APK and 476-byte metadata. Both exact device
  helpers totaling 26,697,560 bytes were removed from `/data/local/tmp`. No PDF, benchmark, test
  binary, rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 121 in the overlapping Thor ledger. It removes one host
  instruction only when matching guest shifts execute; its kernel ratios cannot be added to the
  other 120 entries or converted into emulator-wide FPS or watts. Lower code-cache, fetch/decode,
  dependency, and integer-issue work makes lower energy plausible, but whole-game frametime,
  temperature, thermal slope, and battery watts still require a controlled matched title/scene/
  cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B run.

## ARM64 Shifted SUB Folding (2026-08-19)

- Command-line Git fetched authoritative `upstream/master` at
  `db15d78feb97ed19b6fc0354481e74694d339594`; the fork was 223 commits ahead and zero behind before
  this work, so no merge was needed. Work remained on `master`, and both source and documentation
  commits used the configured command-line Git SSH remote. RPCS3's current
  [`Avoid redundant XFloat normalization in SELB`](https://github.com/RPCS3/rpcs3/commit/82164a5),
  [`Avoid redundant copy when writing to mip level or Z layer`](https://github.com/RPCS3/rpcs3/commit/ad059d0),
  and earlier [`Shorten FI dependency chain`](https://github.com/RPCS3/rpcs3/commit/27f0e87)
  changes reinforced the transferable pattern of removing redundant materialization only behind
  an exact semantic predicate; no RPCS3 code was copied into this 3DS emulator.
- Dynarmic previously emitted a standalone immediate LSL/LSR/ASR and then `SUB` for ordinary
  no-flags A32 subtraction. ARM64 now aliases a sole immediately adjacent shift producer to its
  non-immediate input and emits one shifted-register `SUB` when the immediate is 1..31. The shared
  gate proves normal `Sub32` carry-in true, no associated arithmetic or shift pseudo-operation,
  one producer use, immediate adjacency, and a non-immediate shift source. Shared, non-adjacent,
  immediate-source, variable, zero/32, flag/carry, borrow/reverse-subtract, and unrelated cases
  retain the established split lowering. ADD keeps its independently measured LSL 1..4 gate.
- A disassembly-checked helper compared four independent, eight base-dependent, and eight
  shifted-index-dependent `LSL/LSR/ASR; SUB` bodies with one shifted-register `SUB`. Shifts
  1/2/3/4/5/8/16/31 used 1,000,000 iterations and nine alternating-order samples in the all-core
  run, with matching checksums throughout. Old-over-fused median ranges were:

  | Thor core | Independent | Base dependency | Index dependency |
  | --- | ---: | ---: | ---: |
  | A510 CPU 0 | 1.9168x-2.1679x | 1.4962x-1.7710x | 1.4865x-1.7664x |
  | A715 CPU 3 | 1.0837x-1.4598x | 1.1102x-1.7645x | 1.1414x-1.7961x |
  | A710 CPU 6 | 1.1343x-1.7994x | 1.0582x-1.6535x | 1.1678x-1.8219x |
  | X3 CPU 7, LSL #1..#4 | 1.934x-2.000x | 2.196x-2.238x | 2.178x-2.222x |
  | X3 CPU 7, other forms | approximately neutral | 1.016x-1.125x | 1.067x-1.117x |

  Thor began the run on AC power at 80%, 4.271 V, and 25.0 C. These are wall-powered
  instruction-kernel timings, not battery-discharge watt measurements.
- The first long X3 confirmation was discarded because its temporary core-wake workers had not
  yet been reaped. Corrected 20,000,000-iteration, 21-sample independent runs killed and reaped the
  helpers before timing: LSL #1..#4 measured 1.9566x-1.9814x, while wider LSL and every LSR/ASR
  form were approximately neutral at 0.9996x-1.0024x. A doubled-work 40,000,000-iteration,
  21-sample ASR check measured 0.9997x-1.0024x, rejecting one isolated ASR #2 loss as
  non-repeatable. No contaminated result controls the shipped gate.
- Temporary actual-emitter tracing captured final Dynarmic words for LSL, LSR, and ASR shifts
  1/2/3/4/5/16/31. `llvm-objdump` decoded the `4b13....`, `4b53....`, and `4b93....` families as
  exactly one `sub w20,w19,w19,lsl/lsr/asr #shift`. The trace hook was removed before the final
  build. The clean unstripped test ELF was 449,051,840 bytes; its temporary stripped copy was
  26,156,296 bytes with SHA-256
  `D11DF48D8C9621CF8F26288AFF5338A57254F9569740AB38767F896A3C6D7482`.
- Permanent ARM and Thumb-2 coverage exercises both ADD and SUB, LSL/LSR/ASR, encoded shifts
  0/1/2/3/4/5/16/31, distinct operands, destination/base/index and all-way aliases, dirty and
  signed-boundary inputs, modular 32-bit wrap, every unrelated GPR, NZCV/Q/GE, and FPSCR. The clean
  focused case passed 65,280 assertions separately on A510 CPU 0, A715 CPU 3, A710 CPU 6, and X3
  CPU 7 during the verification sequence; the final trace-free build passed it again on A715. The
  final full `[core][arm][dynarmic]` suite passed 162,452 assertions in 42 cases on A715. Source and
  test commit `88b4da62d` was pushed directly to `origin/master` over command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 2 minutes 11 seconds.
  The ARM64-only APK is 29,008,048 bytes, has SHA-256
  `E1B145275CA80454EB2DAB5A9A9C405BF640498D70E2059430DEF89F4AF6B246`, and reports package
  `org.azahar_emu.azahar.debug` version `88b4da62d-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no PID. Neither app nor game was launched.
- Exact cleanup removed 2,497,380,018 logical host bytes: 26,222,307 bytes of bounded scratch, the
  449,051,840-byte unstripped test ELF, and reproducible Gradle/JNI/R8/native-symbol/mapping
  staging. C: recovered 2,055,483,392 physical bytes and reports 56,370,929,664 bytes free. The
  retained active ARM64 CMake/Ninja cache is 2,791,814,967 bytes; retained build output is only the
  29,008,048-byte APK and 476-byte metadata. Both exact device helpers totaling 26,210,120 bytes
  were removed from `/data/local/tmp`. No PDF, benchmark, test binary, rendered manual page, or
  scratch note was committed.
- This is optimization/candidate entry 122 in the overlapping Thor ledger. It removes one host
  instruction only when matching guest shifted-subtract paths execute; its exact-loop ratios
  cannot be added to the other 121 entries or converted into whole-emulator FPS or watts. Lower
  code-cache, fetch/decode, dependency, and integer-issue work makes lower energy plausible, but
  whole-game frametime, thermal slope, and battery watts still require a controlled matched
  title/scene/cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B.

## ARM64 Shifted Logical Folding (2026-08-19)

- Command-line Git fetched authoritative `upstream/master` at
  `db15d78feb97ed19b6fc0354481e74694d339594`; the fork was 225 commits ahead and zero behind before
  this work, so no merge was needed. Work remained on `master`, and the source commit was pushed
  directly to the configured command-line Git SSH remote. RPCS3's current
  [`Avoid redundant XFloat normalization in SELB`](https://github.com/RPCS3/rpcs3/commit/82164a54c18e8255f8db86c4d7fe18a587b3a36d)
  and
  [`Stop inverting float-to-int saturation on ARM64`](https://github.com/RPCS3/rpcs3/commit/6161ecd7aaf631fdbaf78bc7714db839e15771c8)
  changes reinforce the transferable rule: remove redundant ARM64 materialization only when the IR
  representation and an exact semantic predicate prove it safe. No RPCS3 code was copied.
- Dynarmic previously emitted a standalone immediate LSL/LSR/ASR/ROR and then AND/EOR/ORR for
  ordinary no-flags A32 logical operations. ARM64 now aliases a sole immediately adjacent shift
  producer to its non-immediate source and encodes that shift directly in operand 1 of `And32`,
  `Eor32`, or `Or32` for immediates 1..31. The single helper shared by producer and consumer proves
  that neither has an associated pseudo-operation, the producer has one use, adjacency is exact,
  the shifted value is the consumer's second operand, and the source is not immediate. Flag/carry,
  shared, non-adjacent, immediate-source, variable, zero/32/RRX, wrong-operand, and unrelated forms
  retain the established split lowering. This logical-family gate is independent of ADD's measured
  LSL #1..#4 limit.
- A disassembly-checked helper compared four independent, eight base-dependent, and eight
  shifted-index-dependent old/fused bodies for all three logical operations, all four shift kinds,
  and shifts 1/2/3/4/5/8/16/31. Each of the 288 rows per core used 100,000 iterations and nine
  alternating-order median rounds; every checksum matched. Old-over-fused median ranges were:

  | Thor core | Independent | Base dependency | Shifted-index dependency |
  | --- | ---: | ---: | ---: |
  | A510 CPU 0 | 1.6663x-2.1421x | 0.9999x-1.0001x | 0.9999x-1.0573x |
  | A715 CPU 3 | 1.3833x-1.4687x | 1.0104x-1.0151x | 1.9998x-2.0000x |
  | A710 CPU 6 | 1.7897x-1.8011x | 0.9998x-1.0002x | 1.9996x-2.0002x |
  | X3 CPU 7 | 1.8106x-2.0620x | 0.9998x-1.0002x | 2.0000x-2.0002x |

  Thor reported AC power, no USB or wireless charging, 80% charge, 4.271 V, and 25.0 C at the
  start. These are wall-powered instruction-kernel timings, not battery-discharge watt results.
- The A710 and X3 runs used corrected core wake, kill, and reap sequencing before timing. Isolated
  losses from an earlier narrow run did not repeat in the expanded matrix. Preconditioned
  10,000,000-iteration, 21-round confirmations measured 1.003857x for A510 AND/ROR #31
  base-dependent, 1.000026x for A710 ORR/LSL #5 base-dependent, and 1.000111x for X3 EOR/LSR #5
  base-dependent. The expanded corrected matrix's overall minima were 0.999866x on A510,
  1.010399x on A715, 0.999824x on A710, and 0.999808x on X3; no row fell below 0.995x.
- Temporary actual-emitter tracing captured all 84 accepted operation/shift-kind/representative-
  amount combinations for shifts 1/2/3/4/5/16/31. `llvm-objdump` decoded representative shift-5
  words as exactly one `and`, `eor`, or `orr w20,w19,w19,lsl/lsr/asr/ror #5`; the raw instruction
  families were `0a13/0a53/0a93/0ad3`, `4a13/4a53/4a93/4ad3`, and
  `2a13/2a53/2a93/2ad3`. No shift-zero case entered the fused path. The trace hook was removed
  before the final build. The clean unstripped test ELF was 449,136,576 bytes; its temporary
  stripped copy was 26,164,680 bytes with SHA-256
  `D9351899F9A8A2D99C790A6706985DF3F0ABF021C6E2C394F5B2DE0ED04ACF34`.
- Permanent ARM and Thumb-2 coverage exercises AND/EOR/ORR, LSL/LSR/ASR/ROR, encoded shifts
  0/1/2/3/4/5/16/31, all five destination/source alias layouts, full-width and signed-boundary
  inputs, flag-setting fallbacks including RRX and shift-32 semantics, every unrelated GPR,
  NZCV/Q/GE, and FPSCR. The final focused case passed 156,672 assertions independently on A510 CPU
  0, A715 CPU 3, A710 CPU 6, and X3 CPU 7. The final trace-free full `[core][arm][dynarmic]` suite
  passed 319,124 assertions in 43 cases on A715. Source/test commit `633537612` was pushed directly
  to `origin/master` over command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 11 seconds
  with ThinLTO and ARMv8 NEON enabled. The ARM64-only APK is 29,008,424 bytes, has SHA-256
  `E7DDBD34AA999F105B2BF1C6B6251D81441EDF87A43B4F74A775E9560EB54576`, and reports package
  `org.azahar_emu.azahar.debug` version `633537612-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no PID. Neither app nor game was launched.
- Exact cleanup removed 2,497,710,884 logical host bytes: 26,291,501 bytes of bounded scratch, the
  449,136,576-byte unstripped test ELF, and 2,022,282,807 bytes of reproducible Gradle/JNI/R8/
  native-symbol/mapping staging. C: recovered 2,055,376,896 physical bytes and reports
  56,363,606,016 bytes free. The retained active ARM64 CMake/Ninja cache is 2,792,154,322 bytes;
  retained build output is only the 29,008,424-byte APK and 476-byte metadata. Both exact device
  helpers totaling 26,278,248 bytes were removed from `/data/local/tmp`. No PDF, benchmark, test
  binary, rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 123 in the overlapping Thor ledger. It removes one host
  instruction only when matching guest shifted-logical paths execute; its exact-loop ratios cannot
  be added to the other 122 entries or converted into whole-emulator FPS or watts. Lower code-cache,
  fetch/decode, dependency, and integer-issue work makes lower energy plausible, but whole-game
  frametime, thermal slope, and battery watts still require a controlled matched title/scene/cache/
  renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B.

## ARM64 Shifted MVN Folding (2026-08-19)

- Command-line Git fetched authoritative `upstream/master` at
  `db15d78feb97ed19b6fc0354481e74694d339594`; the fork was 227 commits ahead and zero behind before
  this work, so no merge was needed. Work remained on `master`, and source was pushed directly to
  the configured SSH remote. RPCS3's current
  [`Avoid redundant XFloat normalization in SELB`](https://github.com/RPCS3/rpcs3/commit/82164a54c18e8255f8db86c4d7fe18a587b3a36d)
  and
  [`Stop inverting float-to-int saturation on ARM64`](https://github.com/RPCS3/rpcs3/commit/6161ecd7aaf631fdbaf78bc7714db839e15771c8)
  changes reinforced the transferable rule: remove redundant ARM64 materialization only behind an
  exact semantic predicate. No RPCS3 code was copied into Azahar.
- A shifted-`BIC` candidate was tested first and rejected. A disassembly-checked helper covered
  independent, base-dependent, and shifted-index-dependent `LSL`/`LSR`/`ASR`/`ROR` forms at eight
  immediate amounts: 96 rows per core, 100,000 iterations, and nine alternating-order samples,
  with matching checksums. Old-over-fused median ranges were 1.989558x-1.999476x independent,
  0.999736x-1.002232x base-dependent, and 0.969527x-1.049735x index-dependent on A510;
  1.530501x-1.635051x, 1.010393x-1.015156x, and 1.999814x-2.000007x on A715;
  1.786136x-1.789721x, 0.986581x-1.000182x, and 1.999648x-2.014596x on A710; and
  1.507809x-1.590860x, 1.000000x-1.000192x, and 1.999996x-2.000000x on X3. Longer A510
  confirmations measured 1.013477x for base-dependent ROR #16 but only 0.985695x for ASR #5 and
  0.992624x for LSL #16; index-dependent ROR #1 was 1.003267x. The binary base dependency makes a
  global fold unsafe despite the attractive independent and big-core results, so production BIC
  lowering was not changed.
- Dynarmic previously emitted a standalone immediate shift followed by `MVN` for ordinary
  no-flags A32 ARM/Thumb-2 operations. ARM64 now aliases a sole immediately adjacent
  LSL/LSR/ASR/ROR producer to its non-immediate source and emits one shifted-register `MVN` for
  immediates 1..31. The producer and consumer use the same eligibility helper. Flag/carry pseudos,
  shared/non-adjacent producers, immediate sources, variable shifts, zero/32/RRX forms, and
  unrelated consumers retain the established lowering.
- A disassembly-checked MVN helper compared independent and input-dependency chains for all four
  shift kinds and eight representative amounts: 64 rows per core, 1,000,000 iterations, and nine
  alternating-order medians. Every checksum matched. Old-over-fused median ranges were:

  | Thor core | Independent | Input dependency |
  | --- | ---: | ---: |
  | A510 CPU 0 | 1.939666x-2.206506x | 0.916487x-1.070433x |
  | A715 CPU 3 | 1.531232x-1.592283x | 1.988148x-2.006246x |
  | A710 CPU 6 | 1.786884x-1.798057x | 1.991944x-2.003972x |
  | X3 CPU 7 | 1.999233x-2.016784x | 1.991326x-2.005043x |

  The short A510 per-row dependency spread was DVFS noise around a 1.000647x mean. Corrected
  10,000,000-iteration, 21-sample confirmations measured 1.000880x for LSR #8, 1.005599x for
  ROR #16, and 0.996854x for LSL #4, so no repeated row crossed the 0.995 acceptance floor. Thor
  began the benchmark on AC power with USB and wireless charging false, 80% charge, 4.271 V, and
  25.0 C. These are wall-powered instruction-kernel timings, not battery-discharge watt results.
- The checked core manuals list basic and shifted no-flags logical operations at
  latency/throughput 1/3 ALU on A510 page 14, 1/4 I on A710 and A715 page 17, and 1/6 I on X3
  page 15. Those rows support removing a real issued logical/shift instruction but do not erase the
  measured dependency-shape distinction between accepted unary MVN and rejected binary BIC.
- Temporary actual-emitter tracing captured 28 accepted opcode/amount groups, each 32 times across
  the ARM/Thumb/layout/input matrix, and no shift-zero entry. `llvm-objdump` decoded representative
  words `2a3317f4`, `2a7317f4`, `2ab317f4`, and `2af317f4` as exactly
  `mvn w20,w19,lsl/lsr/asr/ror #5`. The trace hook was removed before the final build. The clean
  unstripped test ELF was 449,185,640 bytes; its stripped copy was 26,171,144 bytes with SHA-256
  `240AA4F4D795922CC3639CF6459F8F1F21893E721348CBE59746B5010207756F`.
- Permanent ARM and Thumb-2 coverage exercises all four shift kinds, encoded amounts
  0/1/2/3/4/5/16/31, distinct and source/destination-alias layouts, full-width boundary inputs,
  flag-setting fallbacks including RRX and shift-32 carry semantics, every unrelated GPR,
  NZCV/Q/GE, and FPSCR. The clean focused case passed 26,112 assertions independently on A510 CPU
  0, A715 CPU 3, A710 CPU 6, and X3 CPU 7, then passed again on A715 after trace removal. The final
  full `[core][arm][dynarmic]` suite passed 345,236 assertions in 44 cases on A715. Source/test
  commit `54844eca7` was pushed directly to `origin/master` over command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 1 second
  with LTO and ARMv8 NEON enabled. The ARM64-only APK is 29,010,304 bytes, has SHA-256
  `DE02C7732EDD18A890B8D031EEDE32953750054BC8404D4C84A0820C6354AC22`, and reports package
  `org.azahar_emu.azahar.debug` version `54844eca7-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no PID. Neither app nor game was launched.
- Exact cleanup removed 2,524,660,844 logical host bytes: 53,107,224 bytes of bounded scratch, the
  449,185,640-byte unstripped test ELF, and 2,022,367,980 bytes of reproducible Gradle/JNI/R8/
  native-symbol/mapping staging. C: recovered 2,082,582,528 physical bytes and reports
  56,356,532,224 bytes free. The retained active ARM64 CMake/Ninja cache is 2,798,391,708 bytes;
  retained build output is only the 29,010,304-byte APK and 476-byte metadata. Three exact device
  helpers totaling 26,246,344 bytes were removed from `/data/local/tmp`. No PDF, benchmark, test
  binary, rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 124 in the overlapping Thor ledger: one bounded shifted-MVN
  optimization shipped and one unsafe global shifted-BIC fusion was permanently rejected. The
  exact-loop ratios cannot be added to the other 123 entries or converted into whole-emulator FPS
  or watts. Lower code-cache, fetch/decode, dependency, and integer-issue work makes lower energy
  plausible only when this guest path executes; whole-game frametime, thermal slope, and battery
  watts still require a controlled matched title/scene/cache/renderer/driver/resolution/layout/
  performance-mode/fan/brightness/duration A/B run.

## ARM64 Flag-Setting Small-LSL Arithmetic Folding (2026-08-19)

- Command-line Git refreshed authoritative `upstream/master` at
  `db15d78feb97ed19b6fc0354481e74694d339594`; the fork was 229 commits ahead and zero behind before
  this source change, so no merge was needed. RPCS3 master was also refreshed at
  `ddd82ecada385db436f77bed21ffca46da5d008b`. Its recent
  [`Additional constant folding for intrinsics`](https://github.com/RPCS3/rpcs3/commit/502ea1f43624)
  change is x86-intrinsic-specific and was not copied, but it reinforces the transferable rule:
  eliminate redundant materialization only behind an exact semantic and target-performance gate.
- Dynarmic already folded selected no-flags shifted ADD/SUB forms, but rejected every arithmetic
  instruction with an associated pseudo-result. A32 ADDS/SUBS/CMN/CMP create `Add32` or normal-
  carry-in `Sub32` plus `GetNZCVFromOp`; their arithmetic NZCV does not consume the shift's carry.
  The new gate therefore requires exactly that sole NZCV pseudo-operation, a sole-use immediately
  adjacent non-immediate `LogicalShiftLeft32` producer with no carry pseudo-operation, and an
  immediate amount from 1 through 4. Overflow/carry/other pseudo users, shared or non-adjacent
  shifts, immediate sources, variable/zero shifts, every flag-setting LSR/ASR, and LSL 5..31 retain
  the established split lowering. Existing no-flags gates are unchanged.
- The initial disassembly-checked helper covered ADDS and SUBS, LSL/LSR/ASR amounts
  1/2/3/4/5/8/16/31, and independent/base-dependent/shifted-index-dependent shapes: 144 rows per
  core, 100,000 iterations, and nine alternating-order samples. Checksums included the result and
  final carry and matched for every old/fused pair. A global flag-setting fold was rejected:
  base-dependent right-shift or wide-LSL rows fell to 0.516636x on A715, 0.532947x on A710, and
  0.512581x on X3, while X3 independent right shifts were also slightly negative around 0.981x.
- The accepted LSL 1..4 subset received a 1,000,000-iteration, 21-alternating-sample confirmation.
  The 24 rows per core all matched and measured the following old-over-fused median ranges:

  | Thor core | ADDS independent | ADDS base dependency | ADDS index dependency | SUBS independent | SUBS base dependency | SUBS index dependency |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | A510 CPU 0 | 1.386344x-1.411093x | 0.999987x-1.002197x | 0.997101x-1.000758x | 1.753822x-1.783091x | 0.997249x-1.003986x | 0.996813x-0.999658x |
  | A715 CPU 3 | 1.785637x-1.790345x | 1.030837x-1.036421x | 1.994028x-2.000236x | 1.770285x-1.789932x | 1.030944x-1.037272x | 1.996517x-1.999474x |
  | A710 CPU 6 | 1.387422x-1.408469x | 1.044024x-1.052955x | 1.984874x-1.988536x | 1.387526x-1.393477x | 1.044189x-1.052473x | 1.987025x-1.989183x |
  | X3 CPU 7 | 1.121517x-1.162937x | 1.022735x-1.026055x | 2.004439x-2.016717x | 1.153209x-1.155187x | 1.021842x-1.026588x | 2.009119x-2.016168x |

  No accepted row crossed the conservative 0.995 floor. Android had parked CPU7, so the X3 runner
  used temporary workers only to schedule a waiting shell on CPU7, pinned that shell, then killed
  and reaped every worker before releasing the benchmark. Thor began on AC power with USB and
  wireless charging false, 80% charge, 4.269 V, and 22.0 C. These are wall-powered instruction-
  kernel timings, not a battery-discharge watt measurement.
- The checked Cortex-X3, A715, A710, and A510 basic arithmetic/flag-setting tables are on pages 15,
  17, 17, and 14. The A-profile architecture and those core guides establish that shifted ADDS/SUBS
  are real native operations, but the physical heterogeneous-core results above control the narrow
  1..4 gate and the right/wide-shift no-go.
- Temporary actual-emitter tracing captured words `2b130674`, `2b130a74`, `2b130e74`, and
  `2b131274` for ADD and `6b130674`, `6b130a74`, `6b130e74`, and `6b131274` for SUB.
  `llvm-objdump` decoded them as exactly one `adds/subs w20,w19,w19,lsl #1/#2/#3/#4`. No rejected
  shift entered the traced fused branch. The trace was removed; the clean 449,230,928-byte
  unstripped test ELF and its 26,176,904-byte stripped copy contained no trace marker. The stripped
  SHA-256 is `FA8C7B9DFDA8B4B688EAC274B4D410C4AF0EEF59498977F9C6C7CC80800D9E3D`.
- Permanent ARM and Thumb-2 coverage exercises ADDS/SUBS/CMN/CMP; LSL/LSR/ASR encodings at
  0/1/2/3/4/5/16/31; every destination/base/index alias layout; full-width carry and signed-overflow
  boundaries; comparison no-write behavior; every unrelated GPR; NZCV/Q/GE; and FPSCR. The clean
  focused case passed 91,392 assertions independently on A510 CPU 0, A715 CPU 3, A710 CPU 6, and
  X3 CPU 7. The final trace-free full `[core][arm][dynarmic]` suite passed 436,628 assertions in
  45 cases on A715. Source/test commit `8c4f938d1` was pushed directly to `origin/master` over
  command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 5 seconds
  with LTO and ARMv8 NEON enabled. The ARM64-only, v2-signed APK is 29,010,196 bytes, has SHA-256
  `51A6D46D303C84F1BD3B120D5D4B5D189A37DB1D3BC7607672021F6C9E2CDA55`, and reports package
  `org.azahar_emu.azahar.debug` version `8c4f938d1-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no device PID. Neither app nor game was launched.
- Exact bounded cleanup removed 2,462,888,344 logical host bytes: 80,829,781 bytes of scratch,
  the 449,230,928-byte unstripped test ELF, and 1,932,827,635 bytes of reproducible Gradle/JNI/R8/
  symbol/mapping staging. C: recovered 2,019,205,120 physical bytes and reports 56,021,700,608 bytes
  free. The retained active ARM64 CMake/Ninja cache is 2,793,216,649 bytes; retained build output is
  only the 29,010,196-byte APK and 476-byte metadata. Four exact device helpers totaling 54,642,608
  bytes were removed from `/data/local/tmp`. No PDF, benchmark, test binary, rendered manual page,
  or scratch note was committed.
- This is optimization/candidate entry 125 in the overlapping Thor ledger. It ships one bounded
  flag-setting small-LSL fold and permanently rejects a tempting global flag-setting shifted-
  arithmetic fold. The exact-loop ratios cannot be added to the other 124 entries or converted into
  whole-emulator FPS or watts. Lower code-cache, fetch/decode, dependency, and integer-issue work
  makes lower energy plausible only when this guest path executes; whole-game frametime, thermal
  slope, and battery watts still require a controlled matched title/scene/cache/renderer/driver/
  resolution/layout/performance-mode/fan/brightness/duration A/B run.

## AArch64 PICA DP4/DPH Pairwise Reduction (2026-08-19)

- Command-line Git refreshed authoritative `upstream/master` at
  `db15d78feb97ed19b6fc0354481e74694d339594`; the fork was 231 commits ahead and zero behind before
  this source change, so no merge was needed. RPCS3 master remained
  `ddd82ecada385db436f77bed21ffca46da5d008b`. Its recent x86-only intrinsic folding was not copied;
  the transferable lesson was to remove redundant materialization behind exact semantic and
  target-performance gates.
- The x64 PICA shader JIT reduces DP4 with two `HADDPS` operations, but the AArch64 port used vector
  `FADDP`, scalar `FADDP`, then `DUP`. After the first same-source vector operation, the lanes are
  `[X+Y, Z+W, X+Y, Z+W]`. Repeating that vector operation computes the identical ordered
  `(X+Y)+(Z+W)` result in all four lanes. DP4, DPH, and DPHI therefore keep sanitized multiplication,
  x64's grouping, and result replication while dropping one recurring host instruction. DPH/DPHI
  still replace source one's W component with one before multiplication.
- A static Android 35 AArch64 helper disassembled to the intended old three-instruction and new
  two-instruction bodies. Eight raw-bit edge rows covered finite asymmetry, cancellation, both zero
  signs, infinities, quiet/signaling NaNs, maximum finite values, normals, and subnormals; all four
  output words and FPSR matched exactly. Each timing used 500,000 iterations, eight reductions per
  iteration, and 31 alternating-order samples. Independent work used V0-V7; the dependency case
  repeatedly reduced V0. Wall-powered Thor medians were:

  | Thor core | Independent old -> new | Speedup | Dependent old -> new | Speedup |
  | --- | ---: | ---: | ---: | ---: |
  | A510 CPU 0 | 3.776562 -> 2.528034 ns/reduction | 1.493873x | 5.534492 -> 4.029700 | 1.373425x |
  | A715 CPU 3 | 0.537057 -> 0.375000 ns/reduction | 1.432153x | 3.232266 -> 2.153347 | 1.501043x |
  | A710 CPU 6 | 0.539961 -> 0.356758 ns/reduction | 1.513523x | 3.227213 -> 2.148958 | 1.501757x |
  | X3 CPU 7 | 0.399349 -> 0.253724 ns/reduction | 1.573950x | 2.831549 -> 1.887903 | 1.499838x |

  No accepted row approached the conservative 0.995 regression floor. CPU7 required the established
  staging method because Android had parked it: temporary workers made the X3 schedulable, a waiting
  shell was pinned, and every worker was killed before the benchmark began. The Thor was on AC power,
  with USB/wireless charging false, at 80% and 22.0 C before measurements. These are instruction-
  kernel results, not battery-discharge watts.
- Temporary actual-emitter tracing captured `6e21d421 6e21d421` for both DP4 and DPH;
  `llvm-objdump` decoded each word as `faddp v1.4s,v1.4s,v1.4s`. The trace was removed, and the clean
  stripped test contained no marker. Permanent finite/asymmetric DP4 and DPH coverage now checks
  every broadcast result lane and proves DPH ignores source one's original W. The clean
  `[video_core][shader]` suite passed 18,320 assertions in 52 cases independently on A510 CPU 0,
  A715 CPU 3, A710 CPU 6, and X3 CPU 7. Source/test commit `163471c4c` was pushed directly to
  `origin/master` over command-line Git SSH.
- A broader `[video_core]` audit exposed one separate existing failure: after a resource-pool
  refresh, `CommitResource()` updates its local `gpu_tick` but its search lambda still holds the
  pre-refresh value captured by copy, so the exact Vulkan test grows from four to eight resources
  instead of reusing index zero. This did not affect the shader acceptance result; it was queued as
  a separate memory/allocation/power candidate and is resolved by entry 127 below.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 1 minute 39 seconds
  with LTO and ARMv8 NEON enabled. The ARM64-only, v2-signed APK is 29,010,440 bytes, has SHA-256
  `6EDC9A0A5A98D8BDA6F4791F2CF50334A9A5DD465545B9CAF3D4A3000D3736D0`, and reports package
  `org.azahar_emu.azahar.debug` version `163471c4c-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no device PID. Neither app nor game was launched.
- Exact bounded cleanup removed 2,526,315,856 logical host bytes: 54,600,271 bytes of scratch, the
  449,237,936-byte unstripped test ELF, and 2,022,477,649 bytes of reproducible Gradle/JNI/R8/
  symbol/mapping staging. C: recovered 2,082,492,416 physical bytes and reports 55,870,803,968 bytes
  free. The retained active ARM64 CMake/Ninja cache is 2,793,371,765 bytes; retained build output is
  only the 29,010,440-byte APK and 476-byte metadata. Four exact device helpers totaling 54,592,218
  bytes were removed from `/data/local/tmp`. No PDF, benchmark, test binary, rendered manual page,
  or scratch note was committed.
- This is optimization/candidate entry 126 in the overlapping Thor ledger. It ships one bounded
  PICA reduction improvement and identifies, but does not yet count, the Vulkan stale-capture
  follow-up. The exact-loop ratios cannot be added to the other 125 entries or converted into
  whole-emulator FPS or watts. Lower code-cache, SIMD-issue, and dependency work makes lower energy
  plausible only when these guest shader operations execute; whole-game frametime, thermal slope,
  and battery watts still require a controlled matched title/scene/cache/renderer/driver/
  resolution/layout/performance-mode/fan/brightness/duration A/B run.

## Vulkan Refreshed Resource-Pool Reuse (2026-08-19)

- The full video-core verification after optimization 126 exposed an inherited upstream resource-
  pool bug. `CommitResource()` loaded `gpu_tick`, captured that value by copy in its search lambda,
  called `Refresh()`, updated the local variable, and then unknowingly searched with the original
  snapshot. This dates to the 2024 descriptor-management rewrite and remains present in current
  `upstream/master`; it was not introduced by this fork's later timeline-polling cadence change.
- Resource searches now take `completed_tick` as an explicit argument. The initial hinted-tail
  search uses cached completion; after refresh, both the tail retry and the wrapped-prefix search
  use the newly loaded monotonic `KnownGpuTick()`. Circular first-free ordering, one refresh on
  exhaustion, resource tick assignment, and true overflow behavior remain unchanged.
- The natural four-entry regression advances submission ticks 1-4 while completion stays cached at
  zero, then refreshes completion to two. The inherited code returned index four and recorded an
  allocation from `[4,8)`; the fixed code returns reusable index zero and leaves the sole `[0,4)`
  allocation intact. A second seeded case places the only newly reusable entry before a nonzero
  hint, proving the refreshed snapshot reaches the wrapped-prefix search without another growth.
  In production, each false command-pool miss allocates four command buffers; a descriptor-heap
  miss grows by a 64-set batch and can create another Vulkan descriptor pool if existing capacity
  is exhausted. Avoided allocation frequency and bytes remain title/workload dependent.
- The clean `[video_core][vulkan]` selection passed 45 assertions in four cases independently on
  A510 CPU 0, A715 CPU 3, A710 CPU 6, and X3 CPU 7. The previously failing full `[video_core]`
  selection then passed 135,030 assertions in 71 cases on A715. The stripped ARM64 test was
  26,180,296 bytes with SHA-256
  `3811D876C645C486575703021F02616C9BEA885FBCDCC6CC8F9C72D4D1195D27`. Source/test commit
  `18bb05bf0` was pushed directly to `origin/master` over command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 2 minutes 58 seconds
  with LTO and ARMv8 NEON enabled. The ARM64-only, v2-signed APK is 29,010,496 bytes, has SHA-256
  `6C46D8762F36773B3B04573DD9A0AE83022DF7F4D8004A7693A1FC0C07B9E7D2`, and reports package
  `org.azahar_emu.azahar.debug` version `18bb05bf0-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no device PID. Neither app nor game was launched; Thor
  remained on AC power at 80% and 21.0 C.
- Exact bounded cleanup removed 2,497,884,433 logical host bytes: 26,181,517 bytes of scratch, the
  449,252,384-byte unstripped test ELF, and 2,022,450,532 bytes of reproducible Gradle/JNI/R8/
  symbol/mapping staging. C: recovered 2,055,311,360 physical bytes and reports 55,878,307,840 bytes
  free. The retained active ARM64 CMake/Ninja cache is 2,793,544,395 bytes; retained build output is
  only the 29,010,496-byte APK and 476-byte metadata. Three exact device helpers totaling 26,182,412
  bytes were removed from `/data/local/tmp`; the generalized affinity helper was corrected to reap
  every temporary worker, and a process audit found no survivor. No PDF, benchmark, test binary,
  rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 127 in the overlapping Thor ledger. It prevents unnecessary
  Vulkan allocation and persistent pool growth when a refresh proves resources reusable. It does
  not claim an Arm instruction-kernel speedup, an additive whole-emulator FPS percentage, or
  measured watt savings. Lower CPU/driver allocation work and lower command/descriptor memory are
  expected only on affected exhaustion paths; matched title/scene/cache/renderer/driver/resolution/
  layout/performance-mode/fan/brightness/duration A/B remains necessary for whole-game evidence.

## Vulkan Cached Full-Ring Reuse Before Refresh (2026-08-19)

- Optimization 127 fixed refreshed completion but retained an avoidable query order: a cached miss
  in the hinted tail called `MasterSemaphore::Refresh()` before checking the wrapped prefix. When
  cached progress already proved a prefix object reusable, that path still crossed the Vulkan
  driver timeline-counter boundary even though no newer completion value was needed.
- `ResourcePool::CommitResource()` now treats cached and refreshed completion as two complete
  circular-search snapshots. It searches the hinted tail and wrapped prefix using cached
  `KnownGpuTick()` first. Only if both ranges fail does it refresh once, reload completion, and
  search both ranges again before growing. `KnownGpuTick()` is monotonic and may only be stale-low,
  so cached reuse cannot select an unfinished object. First-free circular ordering is preserved
  within each snapshot, resource tick assignment is unchanged, and a true full pool still grows by
  its existing step.
- A permanent seeded regression sets ticks `{1,5,5,5}`, hint two, and cached completion one. It
  proves index zero is reused, `refresh_count` stays zero, known completion remains one, and no
  allocation occurs. The prior natural-refresh and refreshed-wrapped-prefix cases remain, covering
  the query path and post-refresh full-ring retry. Clean `[video_core][vulkan]` runs passed 49
  assertions in five cases independently on A510 CPU 0, A715 CPU 3, A710 CPU 6, and X3 CPU 7. The
  full `[video_core]` selection passed 135,034 assertions in 72 cases on A715. The temporary
  stripped ARM64 test was 26,181,896 bytes. Source/test commit `d2e224ac9` was pushed directly to
  `origin/master` over command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 7 seconds
  with LTO and ARMv8 NEON enabled. The ARM64-only, v2-signed APK is 29,010,268 bytes, has SHA-256
  `A8E8BA8332BD88E0739FA614B08BD54A40833B5314475932E9F1A98921BEB9CC`, and reports package
  `org.azahar_emu.azahar.debug` version `d2e224ac9-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no device PID. Neither app nor game was launched; the
  Thor remained wall-powered.
- Exact bounded cleanup removed 2,497,907,677 logical host bytes: the 26,181,896-byte stripped test,
  the 449,264,128-byte unstripped test ELF, and 2,022,461,653 bytes of reproducible Gradle/JNI/R8/
  symbol/mapping staging. C: recovered 2,055,766,016 physical bytes and reports 55,866,826,752 bytes
  free. The retained active ARM64 CMake/Ninja cache is 2,793,691,776 bytes; retained build output is
  only the 29,010,268-byte APK and 476-byte metadata. The exact 26,181,896-byte device test helper
  was removed from `/data/local/tmp`, and no helper or app PID remained. No PDF, benchmark, test
  binary, rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 128 in the overlapping Thor ledger. It removes one Vulkan
  timeline-counter driver query only when the cached full-ring search already finds reusable work;
  workloads without that ring state see no direct gain. It is not an additive whole-emulator FPS
  percentage or measured watt reduction. Lower driver-call, synchronization, and CPU wakeup work
  makes lower energy plausible on the affected path, but whole-game frametime, thermal slope, and
  battery watts still require a controlled matched title/scene/cache/renderer/driver/resolution/
  layout/performance-mode/fan/brightness/duration A/B run.

## HLE Partial PCM16 Suffix Decode (2026-08-19)

- `Source::ParseConfig()` handled a partial embedded PCM16 update by decoding all `config.length`
  frames from sample zero into a full `StereoBuffer16`, then erasing the first
  `current_sample_number` frames. That repeated decoding and allocation for already-consumed guest
  audio and moved every surviving deque element. The cost grows with historical buffer length and
  can recur as a title extends its embedded buffer.
- `DecodePCM16FromSample()` now advances the read-only PCM input by
  `first_sample * channels * sizeof(s16)` and delegates only the suffix length to the unchanged
  ordinary PCM16 decoder. The partial-update caller passes its established current sample. This
  still re-reads all unconsumed bytes, so a game may modify retained audio; it is not an append-only
  assumption. If declared length shrinks behind the position, the caller resets position to zero
  before decoding exactly as before. Equal position/length produces an empty buffer, and normal
  PCM16 dequeue callers retain the original API/body with no added branch.
- Permanent codec coverage compares suffix results with the corresponding full decode across mono
  and stereo, zero length, 0/mid/end offsets, 1023/1024/1025 deque block boundaries, and multi-block
  data. It passed 44,736 assertions in two cases independently on A510 CPU 0, A715 CPU 3, A710 CPU
  6, and X3 CPU 7. An end-to-end `Source` regression uses emulated FCRAM and covers zero, beginning,
  middle, end, and shrink positions for both channel counts; it passed 30 assertions on every core
  class. The broader `[audio_core]~*LLE*` selection passed 49,267 assertions in 20 cases on A715;
  three firmware-dependent cases skipped. The two LLE-named Android harness cases were excluded
  because that standalone test process lacks the existing JNI `get_build_flavor` function.
- A static Android 26 AArch64 benchmark used a 4096-frame buffer, 2,000 calls per sample, 15
  alternating-order samples, and identical old/new checksums. It measured full decode plus prefix
  erase against direct suffix allocation/decode at 25%, 50%, and 75% consumed:

  | Thor core | Mono 25% | Mono 50% | Mono 75% | Stereo 25% | Stereo 50% | Stereo 75% |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | A510 CPU 0 | 1.447320x | 2.032726x | 3.644807x | 2.016496x | 2.410324x | 3.674203x |
  | A715 CPU 3 | 1.895424x | 2.348434x | 3.712791x | 2.464604x | 2.900488x | 4.462811x |
  | A710 CPU 6 | 1.848034x | 2.363830x | 3.115541x | 1.884939x | 2.412377x | 3.314921x |
  | X3 CPU 7 | 2.169913x | 2.283686x | 3.397582x | 1.905733x | 2.418110x | 3.502966x |

  The temporary 626,928-byte benchmark had SHA-256
  `958A474E74FAB711BEF614EF81C64CDE12498A1E546B70395C697212CBF0E7B5`. CPU6/CPU7 used the
  established unpark-and-pin helper; every staging worker was killed and reaped before timing.
  These were wall-powered path timings, not battery-discharge watts.
- The final stripped ARM64 test was 26,187,896 bytes with SHA-256
  `D50F2DD9783829A9EAA2D7FEB015BE0827030CB1C44449D20C96289E5884B32F`. Source/test commit
  `539b32b3f` was pushed directly to `origin/master` over command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 5 seconds
  with LTO and ARMv8 NEON enabled. The ARM64-only, v2-signed APK is 29,010,560 bytes, has SHA-256
  `7ED7AA572E0E04A848ED8CC98F05C1747632FB6A2B7F34C8E62D58F7C32C9D11`, and reports package
  `org.azahar_emu.azahar.debug` version `539b32b3f-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop verified `stopped=true` with no device PID. Neither app nor game was launched; the
  Thor remained wall-powered.
- Exact bounded cleanup removed 2,498,659,759 logical host bytes: 26,821,860 bytes of scratch, the
  449,318,952-byte unstripped test ELF, and 2,022,518,947 bytes of reproducible Gradle/JNI/R8/
  symbol/mapping staging. C: recovered 2,056,220,672 physical bytes and reports 55,862,116,352 bytes
  free. The retained active ARM64 CMake/Ninja cache is 2,794,006,691 bytes; retained build output is
  only the 29,010,560-byte APK and 476-byte metadata. Three exact device helpers totaling
  26,815,981 bytes were removed from `/data/local/tmp`, and no affinity marker, helper, or app PID
  remained. No PDF, benchmark, test binary, rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 129 in the overlapping Thor ledger. The 1.45x-4.46x ratios
  apply only to the measured partial-update mechanism and scale with the consumed fraction; they
  are not additive whole-emulator FPS or watt percentages. Lower decode, allocator, memory-copy,
  cache, and deque-movement work makes lower energy plausible only for titles using this command.
  Whole-game frametime, thermal slope, and battery watts still require a controlled matched title/
  scene/cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B run.

## PICA LG2 NZCV Edge Classification (2026-08-19)

- Before entering the positive-input polynomial, the AArch64 PICA `LG2` helper classified edge
  values by producing a four-lane floating equality mask, transferring its low lane to a GPR,
  comparing that integer with zero, and branching around separate zero/sign handling. The helper
  now issues `FCMP input,#0.0`, branches on VS for unordered/NaN, then branches on LE for zero,
  negative finite values, and negative infinity. This shrinks the recurring classifier from seven
  generated instructions to three while keeping the existing NaN and negative-infinity result
  vectors and the positive polynomial unchanged.
- Actual-emitter tracing captured words `1e202028`, `54000306`, and `54fffeed`, corresponding to
  scalar `FCMP S1,#0.0`, `B.VS`, and `B.LE`. Ordered positive values and positive infinity continue
  into the polynomial. NaN reaches the existing broadcast return; negative finite values and
  negative infinity return the default NaN vector; both positive and negative zero select the
  signed negative-infinity vector exactly as before. Permanent shader cases now cover negative
  infinity, both signed zeros and their result signs, plus existing NaN, finite, infinity, and
  power-of-two behavior.
- A static Android 26 AArch64 helper measured the exact old and new classifier loops for 10,000,000
  iterations across 21 alternating-order samples. Checksums and all tested edge classifications
  matched. Median old-over-new ratios were:

  | Thor core | Old ns/iteration | New ns/iteration | Old/new |
  | --- | ---: | ---: | ---: |
  | A510 CPU 0 | 7.538797 | 1.510547 | 4.990773x |
  | A715 CPU 3 | 0.577713 | 0.565037 | 1.022436x |
  | A710 CPU 6 | 0.814411 | 0.536854 | 1.517007x |
  | X3 CPU 7 | 0.648901 | 0.472203 | 1.374199x |

- Two attractive alternatives were measured and rejected. A self-compare for NaN followed by a
  zero compare won on A510/A710 but regressed A715/X3 by roughly 7-10%. In `EX2`, `FRINTN` plus
  `FCVTZS` preserved tested tie behavior but regressed A710 about 2.9%; direct GPR-destination
  `FCVTNS` regressed it about 20%. The original three-instruction `EX2` range-reduction sequence
  remains. Cortex timing tables directed these experiments, but exact heterogeneous-core results
  controlled acceptance.
- The final trace-free ARM64 shader selection passed 18,332 assertions in 52 cases independently
  on A510 CPU 0, A715 CPU 3, A710 CPU 6, and X3 CPU 7. Full `[video_core]` passed 135,046 assertions
  in 72 cases on A715. The stripped test was 26,191,288 bytes with SHA-256
  `40A879685FFCF1F21FE85E1DA5D9C5FE072DFD5A2F7F7E0445E23902851AEF49`. Source/test commit
  `55683b8ea` was pushed directly to `origin/master` over command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 12 minutes 25 seconds
  after a full native rebuild. The ARM64-only, v2-signed APK is 29,010,488 bytes, has SHA-256
  `22A39576D5F2443006EFD1DD03E03042E5228C6BD88EFE18B32018ED056E438B`, and reports package
  `org.azahar_emu.azahar.debug` version `55683b8ea-vanilla-thor`. Wi-Fi ADB installed it, then a
  force-stop left no app PID. Neither app nor game was launched; Thor remained AC-powered at 80%
  and 21.0 C.
- Bounded cleanup removed 2,170,863,616 physical host bytes and left 55,606,501,376 bytes (51.79
  GiB) free on C:. The retained active ARM64 CMake/Ninja cache is 2,812,947,309 bytes; build output
  contains only the 29,010,488-byte APK and 476-byte metadata. Temporary test/benchmark/trace
  helpers were removed from the host and device. No PDF, test binary, rendered manual page,
  benchmark helper, or scratch note was committed.
- This is optimization/candidate entry 130 in the overlapping Thor ledger. The 1.02x-4.99x figures
  apply only to the recurring `LG2` edge classifier; they cannot be added to the other 129 entries
  or treated as whole-emulator FPS or battery-watt gains. Lower instruction, mask, transfer, and
  branch-dependency work makes lower energy plausible only when guest shaders execute `LG2`.
  Whole-game frametime, thermal slope, and battery watts still require a controlled matched title/
  scene/cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B run.

## AArch64 PICA Guest Math Link Register (2026-08-19)

- The AArch64 PICA shader JIT's `EX2` and `LG2` helpers use a local `BL`. When either instruction
  ran inside a guest shader `CALL`, `X30` already held the guest-subroutine return. The inherited
  ARM64 port therefore emitted `STR X30,[SP],#-16; BL helper; LDR X30,[SP,#16]!` around every math
  invocation. The store/load forwarded correctly, but each helper still touched the stack and
  updated `SP` twice.
- The accepted lowering reserves AAPCS intra-procedure scratch register `X16` for this bounded live
  range: `MOV X16,X30; BL helper; MOV X30,X16`. The local math helpers do not call an external ABI
  or use X16. Their returns and the guest-subroutine return all remain ordinary `RET X30`, retaining
  the CPU return predictor. Math calls outside guest subroutines retain their original one-
  instruction direct `BL`; the native caller save, 48-byte guest root layout, guest return records,
  helper arithmetic, swizzles, and destination writes are unchanged. `Compile_MathCall()` takes its
  target by reference because Oaknut records a forward branch's writeback on that exact unbound
  `Label` object.
- A temporary Android 29 AArch64 assembly helper executed the exact old and accepted sequences for
  10,000,000 iterations across 21 alternating-order samples. Both paths produced identical
  checksums. Median results while the Thor was AC-powered were:

  | Thor core | Old ns/call | New ns/call | Time saved | Old/new |
  | --- | ---: | ---: | ---: | ---: |
  | A510 CPU 0 | 6.513641 | 5.559792 | 14.644% | 1.171562x |
  | A715 CPU 3 | 3.981469 | 3.222146 | 19.071% | 1.235658x |
  | A710 CPU 5 | 2.864568 | 2.177630 | 23.980% | 1.315452x |
  | X3 CPU 7 | 2.516255 | 1.886776 | 25.017% | 1.333627x |

  CPU5 and CPU7 used the established unpark-and-pin helper; every staging worker was killed and
  reaped before timing. These are wall-powered exact-path timings, not battery-discharge watts.
- Measurement rejected three broader-looking variants. Keeping the native caller in `X17` and
  returning through `RET X17` changed the A510 root median from 4.032760 to 4.537854 ns, a 12.5%
  regression. Returning from the nested math helper through `RET X16` changed its A510 median from
  6.545078 to 17.594229 ns. Separately compacting the established guest root frame from 48 to 32
  bytes changed an isolated A510 median from 6.958635 to 8.705635 ns, a 25.1% regression. Folding
  only the root X30 push/pop into pre/post-indexed loads and stores measured neutral/noisy on A510
  and was removed as unproven. The committed change contains none of those variants.
- The final trace-free stripped ARM64 test was 26,191,288 bytes with SHA-256
  `B6CB41544D983E3DC9730F09C105D4F74C2A4AEAD794921C8975750D262C8711`. Exact nested `CALL`,
  `EX2`, and `LG2` cases passed 1, 13, and 74 assertions on A510. The complete shader selection
  passed 18,332 assertions in 52 cases independently on A510 CPU 0, A715 CPU 3, A710 CPU 5, and X3
  CPU 7. Full `[video_core]` passed 135,046 assertions in 72 cases on A715. Source commit
  `f8f2166e0` was pushed directly to `origin/master` over command-line Git SSH.
- Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 3 seconds
  with LTO and ARMv8 NEON enabled. The ARM64-only, v2-signed APK is 29,010,440 bytes, has SHA-256
  `21630E21DC9F7889D6C6F07500C98A687F6998C3CBB67D2BE859C394F3A69249`, and reports package
  `org.azahar_emu.azahar.debug` version `f8f2166e0-vanilla-thor`. Wi-Fi ADB installed it, then an
  immediate force-stop left no app PID. Neither app nor game was launched. The Thor stayed
  AC-powered, and its temporary stay-awake value was restored from one to the original zero.
- Exact bounded cleanup removed 2,471,858,956 logical host bytes: the 449,332,056-byte native test
  ELF and 2,022,526,900 bytes of reproducible Gradle/JNI/R8/symbol/mapping staging. C: recovered
  2,029,510,656 physical bytes and reports 55,010,025,472 bytes (51.23 GiB) free. The retained
  active ARM64 CMake/Ninja cache is 2,797,896,308 bytes; retained build output is only the
  29,010,440-byte APK and 476-byte metadata. The 26,191,288-byte test and 10,400-byte benchmark
  helpers were removed from `/data/local/tmp`; repo-local benchmark scratch was deleted. No PDF,
  benchmark, test binary, rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 131 in the overlapping Thor ledger. It removes two stack
  accesses and two `SP` updates each time `EX2`/`LG2` executes inside a guest shader subroutine; it
  does not speed ordinary direct math calls or hardware-vertex-shader draws. Its 1.17x-1.33x exact-
  path ratios cannot be added to the prior 130 entries or converted into whole-emulator FPS or
  battery watts. Lower data-cache and address-generation work makes lower energy plausible only on
  this route; whole-game frametime, thermal slope, and battery power still require a controlled
  matched title/scene/cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/
  duration A/B run.

## Upstream Android 17 / Gradle 8.14.5 Integration (2026-08-19)

- A final upstream audit after optimization 131 found three newly published commits. `d53c1e8fb`
  updates the Android wrapper from Gradle 8.13 to 8.14.5; `0821ff1ff` sets target SDK 37 and adapts
  emulation UI insets to enforced edge-to-edge behavior; `f6a3e3aa5` makes the margin-layout access
  null-safe. They merged without conflict as `d491accd4`, preserving this fork's ARM64-only Thor
  flavor, `.debug` package slot, dual-display work, screen filters, profiles, and optimization 131.
- The integrated layout gives the emulation drawer a black background and names its coordinator.
  One display-cutout listener now applies all four safe-inset margins to both that coordinator and
  the in-game menu, and it tolerates a null `layoutParams`. The old API-35 theme that opted out of
  edge-to-edge enforcement is removed. This was compile/package checked only under the standing
  no-launch restriction; visual cutout placement still requires a later permitted app run.
- JDK 17 packaging with the freshly downloaded Gradle 8.14.5 wrapper and
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 4 minutes 4 seconds.
  Native configuration retained AArch64, LTO, and Crypto++ NEON. The ARM64-only, v2-signed APK is
  29,006,752 bytes with SHA-256
  `F0ADE434226DA2FF740EE3287A6FFEFA36EB24AAAF65F6492E9656768E032B32`; it reports package
  `org.azahar_emu.azahar.debug`, version `d491accd4-vanilla-thor`, minimum SDK 29, and target SDK 37.
  Wi-Fi ADB installed it and an immediate force-stop left no app PID. Neither app nor game was
  launched.
- The exact second cleanup removed 2,471,852,144 logical bytes: the 449,332,056-byte native test
  ELF and 2,022,520,088 bytes of reproducible Gradle/JNI/R8/symbol/mapping staging. It recovered
  2,030,022,656 physical bytes and left 53,960,597,504 bytes (50.25 GiB) free on C:. Retained repo
  build output is only the 29,006,752-byte APK and 476-byte metadata; the active ARM64 CMake/Ninja
  cache is 2,797,957,681 bytes. The Gradle 8.14.5 wrapper distribution remains as the current
  reusable tool cache rather than being redownloaded on every build.
- This synchronization is not optimization entry 132 and adds no FPS or watt claim. It keeps the
  fork current with upstream Android packaging and cutout safety while entry 131 remains the latest
  measured performance change.

## AArch64 Exact-Six PICA Output Vertices (2026-08-19)

- The generic CPU output-vertex constructor initializes a 32-`f24` scratch array to one, loops over
  `vs_output_total & 7`, scatters each four-component shader output through the guest mapping,
  copies the first 96 bytes into `OutputVertex`, and applies the established absolute-value and
  one-saturating color clamp. That exact overflow-map behavior matters: guest mappings 24-31 may
  receive writes even though they are outside the visible vertex, and later outputs must overwrite
  earlier outputs when mappings alias.
- AArch64 now keeps the generic source and linked constructor unchanged for counts 0-5 and 7. When
  the configured count is exactly six, `PicaCore` selects a stored handler that calls a dedicated
  constructor with six unconditional mappings. The selection is reconciled before CPU vertex
  processing and rewires both the geometry pipeline and geometry-shader emitter only when the mode
  changes. The exact-six constructor still uses the same 32-slot default array, mapping order,
  96-byte copy, and color clamp; it removes only the generic loop's five recurring count checks.
- The final ThinLTO-linked test ELF retains separate constructors. The generic form remains 496
  bytes with its threshold ladder. The exact-six form is 368 bytes, contains the six mapped output
  groups, and has no output-count branch; its only conditional branch is the compiler's stack-
  canary failure check.
- A static Android AArch64 benchmark used the real constructors, one million vertices per sample,
  seven alternating-order median samples, and identical output checks. The Thor was AC-powered,
  so these are wall-powered exact-path timings rather than battery-discharge watts:

  | Thor core | Generic ns/vertex | Exact-six ns/vertex | Time saved | Generic/exact-six |
  | --- | ---: | ---: | ---: | ---: |
  | A510 CPU 0 | 45.053021 | 38.758854 | 13.970% | 1.162393x |
  | A715 CPU 3 | 11.787291 | 11.559375 | 1.934% | 1.019717x |
  | A710 CPU 5 | 15.082084 | 15.103438 | -0.142% | 0.998586x |
  | X3 CPU 7 | 12.940365 | 12.665625 | 2.123% | 1.021692x |

  The A710 result is a tie inside the ledger's 0.995 floor rather than a claimed gain.
- Broader-looking variants were rejected. A direct-write stackless constructor regressed the A510
  realistic five-output case by 3.1% and a dense seven-output case by 4.1%. A hybrid looked good in
  a simplified clone but the real constructor made counts zero through five slower. A per-vertex
  count specialization added about 1.5 ns to common low counts. An unrolled seven-output variant
  regressed A710 by 1.34%. None of those variants is in the committed code.
- Permanent tests compare every byte of the visible vertex with an independent 32-slot reference
  for counts 0-7 and 4,096 randomized maps per count, then compare 100,000 randomized exact-six
  vertices against the generic constructor on AArch64. The focused `PICA*` selection passed 17,953
  assertions in 11 cases independently on A510 CPU 0, A715 CPU 3, A710 CPU 5, and X3 CPU 7. Full
  `[video_core]` passed 135,046 assertions in 74 cases on A715. The complete native ARM64 build,
  including both tests and the ThinLTO Android library, passed all 2,201 steps in 11 minutes.
- Source/test commit `afee0ebd9` was pushed directly to `origin/master` with command-line Git over
  SSH. JDK 17 packaging with `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache`
  passed in 2 minutes 54 seconds. The ARM64-only, v2-signed APK is 29,007,556 bytes with SHA-256
  `5B53E28E0EC3A2C8502E595970467E86BFA3170D35E49AB41FAA81B9378737A2`; it reports package
  `org.azahar_emu.azahar.debug`, version `afee0ebd9-vanilla-thor`, minimum SDK 29, and target SDK 37.
  Wi-Fi ADB installed it, then an immediate force-stop left no app PID. Neither app nor game was
  launched, and the device's original `stay_on_while_plugged_in=0` remained unchanged.
- Exact bounded cleanup removed 2,472,386,503 logical bytes: the 449,476,144-byte native test ELF
  plus reproducible Gradle/JNI/R8/symbol/mapping staging. It recovered 2,034,024,448 physical bytes
  and left 53,729,521,664 bytes (50.04 GiB) free on C:. Retained repo build output is only the
  29,007,556-byte APK and 476-byte metadata; the active ARM64 CMake/Ninja cache is 2,805,175,558
  bytes. The stripped test, benchmark binary, CPU7 helper, and repo-local benchmark scratch were
  removed. No PDF, benchmark, test binary, rendered manual page, or scratch note was committed.
- This is optimization/candidate entry 132 in the overlapping Thor ledger. It accelerates only CPU
  output-vertex construction when the guest configures exactly six shader outputs; hardware vertex
  shaders and every other output count see no constructor change. The 1.162393x A510 exact-path
  result cannot be added to the prior 131 entries or converted into whole-emulator FPS or battery
  watts. Fewer recurring control instructions make lower energy plausible only on this route;
  whole-game frametime, thermal slope, and battery power still require a controlled matched title/
  scene/cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B.

## AArch64 Predecoded PICA Vertex Attributes (2026-08-19)

- `VertexLoader::LoadVertex()` previously switched on a draw-invariant PICA format for every
  non-default attribute of every uncached vertex. A second runtime-counted loop converted one to
  four components and a third filled missing components. Format and component count are fixed when
  the loader is constructed, so that recurring decode/control work was inherited x86-oriented C++
  structure rather than guest behavior.
- The constructor now predecodes the four formats and four legal component counts into one-byte
  `AttributeLoader` values. The recurring path dispatches through one compact 16-way jump table to
  compile-time-unrolled bodies. One- through three-component bodies retain scalar conversions and
  exact missing `(0,0,0,1)` defaults. AArch64 BYTE4 and UBYTE4 load four bytes, widen twice with
  signed `SSHLL` or unsigned `USHLL`, convert four lanes with `SCVTF` or `UCVTF`, and store one Q
  vector. SHORT4 uses one D load, `SSHLL`, `SCVTF`, and one Q store; FLOAT4 is an exact Q load/store.
  Invalid zero/>4 component descriptors retain the existing vertex-retention error route, and
  default attributes, guest addresses, strides, and cache behavior are unchanged.
- The exact Android 29 standalone helper was compiled and disassembled before timing. The old
  dynamic helper occupied `0x470` bytes; the predecoded helper occupied `0x218` bytes. Each run
  first made 500,000 randomized byte-for-byte comparisons across all 16 legal shapes, then timed
  four independent attribute outputs for 300,000 loop iterations across 11 alternating-order
  median samples. Final old/new ratios were:

  | Format/count | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 |
  | --- | ---: | ---: | ---: |
  | BYTE1 | 2.2910x | 1.8115x | 1.5968x |
  | BYTE2 | 1.5250x | 1.8512x | 1.6278x |
  | BYTE3 | 1.3322x | 1.8929x | 1.6645x |
  | BYTE4 | 2.1967x | 1.8381x | 1.5415x |
  | UBYTE1 | 2.2988x | 1.8198x | 1.5920x |
  | UBYTE2 | 1.4685x | 1.8519x | 1.6376x |
  | UBYTE3 | 1.2266x | 1.6575x | 1.4308x |
  | UBYTE4 | 1.5754x | 1.9553x | 1.6298x |
  | SHORT1 | 2.3440x | 1.8301x | 1.6072x |
  | SHORT2 | 1.5550x | 1.6292x | 1.6307x |
  | SHORT3 | 1.3563x | 1.8985x | 2.1109x |
  | SHORT4 | 2.8787x | 1.8905x | 1.5433x |
  | FLOAT1 | 3.0161x | 2.6831x | 1.5864x |
  | FLOAT2 | 2.4746x | 1.7840x | 1.5865x |
  | FLOAT3 | 2.1257x | 1.7895x | 1.6096x |
  | FLOAT4 | 2.3760x | 2.6991x | 1.4988x |

  Android's core control exposed masks through CPU5 during the final run but parked CPU6/CPU7, so
  A710 CPU5 was measured and X3 was not. No power setting was changed to force the X3 online. At
  the final evidence capture, `dumpsys battery` reported AC powered, charge-limited/status 3 at
  80%, and 23.0 C. These are exact-path wall-powered timings, not battery-discharge watts.
- Final ThinLTO keeps `Pica::VertexLoader::LoadVertex()` as a 780-byte (`0x30c`) AArch64 function
  with the 16-way descriptor table. Linked disassembly contains the intended Q load/store,
  `SSHLL`/`USHLL`, and vector `SCVTF`/`UCVTF` bodies rather than reconstructing scalar runtime
  loops. The permanent test independently converts 4,096 random inputs for every legal shape:
  65,536 byte-for-byte comparisons plus invalid zero/five-count checks. Its focused test passed 18
  assertions on A510 CPU0, A715 CPU3, and A710 CPU5. Full `[video_core]` passed 135,064 assertions
  in 75 cases on A715. The exact stripped test was 26,202,744 bytes with SHA-256
  `7BDF5990E62877A617D38E27D61E5E4F8AA87FF070B37E39BC71DB4B5A6D6283`.
- The adjacent scalar-DSP audit did not inflate the accepted count. `UXTB16` already compiled to
  one AArch64 mask, while changing `SMUSD`/`SMUSDX` from four extracts, two multiplies, and a
  subtraction to four extracts plus `SMULL`/`SMSUBL` shortened static code but failed the all-core
  gate. With both loops aligned to 64 bytes, A510 `SMUSD` changed from 2.599973 to 2.702699 ns/op
  (0.961991x) and `SMUSDX` from 2.141290 to 2.141473 ns/op (0.999915x). The big-core wins were
  rejected because the little-core regression matters to Thor efficiency and sustained power.
- Source/test commit `2fa9ca8ce` was pushed directly to `origin/master` with command-line Git over
  SSH. Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 1 minute 36 seconds.
  The ARM64-only, v2-signed APK is 29,007,260 bytes with SHA-256
  `66A3FE227C613E27A70B34106C7796DD43B777EFD16CFEDBFE8237A38D6F5A7F`; its signer certificate
  SHA-256 is `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `2fa9ca8ce-vanilla-thor`, minimum SDK
  29, and target SDK 37. Wi-Fi ADB installed it, an immediate force-stop left no app PID, and the
  original `stay_on_while_plugged_in=0` remained unchanged. Neither app nor game was launched.
- Bounded cleanup removed 2,498,906,352 logical host bytes and recovered 2,056,413,184 physical
  bytes, leaving 53,492,310,016 bytes free on C:. The retained active ARM64 CMake/Ninja cache is
  2,805,610,423 bytes; retained build output is only the 29,007,260-byte APK and 476-byte metadata.
  The 449,552,336-byte native test ELF, stripped test, both standalone benchmarks/sources,
  Gradle/JNI/R8/symbol/mapping staging, and all three `/data/local/tmp` helpers were removed. No
  PDF, benchmark, test binary, rendered manual page, APK, or scratch note was committed.
- This is optimization/candidate entry 133 in the overlapping Thor ledger. It accelerates vertex
  attribute conversion only when CPU-side PICA loading occurs; hardware-vertex-shader draws and
  cache hits can bypass it. The 1.23x-3.02x kernel ratios cannot be added to the prior 132 entries
  or converted into whole-emulator FPS or battery watts. Less recurring branch, integer, and
  conversion work makes lower energy plausible on this path; whole-game frametime, thermal slope,
  and battery power still require a controlled matched title/scene/cache/renderer/driver/
  resolution/layout/performance-mode/fan/brightness/duration A/B.

## Draw-Lifetime PICA Vertex Stream Pointers (2026-08-19)

- After entry 133 predecoded format/count, the remaining CPU-side recurring path still called
  `MemorySystem::GetPhysicalPointer()` for every non-default attribute of every uncached vertex.
  That repeated physical-region classification, backing-object dereference, virtual `GetPtr()`,
  and region-offset addition even though the physical base, loader offset, format, and stride are
  fixed for one `VertexLoader`/draw.
- `VertexLoader` now resolves a separate direct backing pointer for every configured attribute in
  its constructor. `LoadVertex()` performs only the established descriptor dispatch after loading
  that pointer and adding `stride * vertex`. FCRAM, VRAM, DSP, and New-3DS backing allocations are
  stable for the `MemorySystem` lifetime, so writes still update the storage seen by the cached
  pointer. A null constructor lookup changes the descriptor to `Invalid`, preserving the existing
  retention/error route without null pointer arithmetic. Default attributes remain pointer-free.
- A temporary Android 29 ARM64 benchmark compared the exact old recurring address lookup with the
  new pointer-plus-stride operation while keeping the same `VertexLoaderUtils::LoadAttribute()`
  work. It used four independent BYTE4/UBYTE4/SHORT4/FLOAT4 streams, 256 source vertices, two
  million loop iterations, 11 alternating-order median samples, and equal nonzero checksums. The
  Thor was AC-powered, charge-limited/status 3 at 80%, and 23.0 C, so these are wall-powered exact-
  path timings rather than battery-discharge watts:

  | Thor core | Old ns/attribute | Cached ns/attribute | Old/cached |
  | --- | ---: | ---: | ---: |
  | A510 CPU 0 | 28.906693 | 14.861517 | 1.945070x |
  | A715 CPU 3 | 10.614219 | 4.828092 | 2.198429x |
  | A710 CPU 6 | 9.793997 | 2.897591 | 3.380048x |

  Android's shell affinity mask excluded CPU7 during this run, so no X3 timing is claimed.
- Final ThinLTO shrank `Pica::VertexLoader::LoadVertex()` from entry 133's 780 bytes (`0x30c`) to
  728 bytes (`0x2d8`). Linked AArch64 disassembly loads the cached pointer, multiplies stride by
  vertex, and adds the two directly; it contains no `GetPhysicalPointer()` or
  `GetPhysMemRegionInfo()` call. The corresponding physical-region and backing `GetPtr()` work is
  visible once in the constructor.
- The permanent test uses the actual `Core::System`, `MemorySystem`, register descriptors, and
  `VertexLoader`. It combines BYTE4 and SHORT3 streams with a default attribute, loads two
  vertices, mutates guest FCRAM after loader construction, and byte-compares the resulting `f24`
  attributes against an independent scalar reference. Focused `[video_core][pica]` tests passed
  21 assertions in two cases on A510 CPU0, A715 CPU3, and A710 CPU6. Full `[video_core]` passed
  135,067 assertions in 76 cases on A715.
- Source/test commit `772fea8d0` was pushed directly to `origin/master` with command-line Git over
  SSH. Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 2 minutes 38 seconds.
  The ARM64-only, v2-signed APK is 29,007,272 bytes with SHA-256
  `0FCC0649B4228EE70193119DB1721485D28F3D3682B21FC0EA3C1D767ACE08CB`; its signer certificate
  SHA-256 remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `772fea8d0-vanilla-thor`, minimum SDK
  29, and target SDK 37. Wi-Fi ADB installed it, an immediate force-stop left no app PID, and the
  original `stay_on_while_plugged_in=0` remained unchanged. Neither app nor game was launched.
- Bounded cleanup removed 2,498,793,651 logical host bytes and recovered 2,055,999,488 physical
  bytes, leaving 53,488,480,256 bytes free on C:. The retained active ARM64 CMake/Ninja cache is
  2,799,804,317 bytes; retained build output is only the 29,007,272-byte APK and 476-byte metadata.
  The 449,575,344-byte native test ELF, 26,204,856-byte stripped test, Gradle/JNI/R8/symbol/mapping
  staging, and the exact `/data/local/tmp` helper were removed. No PDF, benchmark, test binary,
  rendered manual page, APK, or scratch note was committed.
- This is optimization/candidate entry 134 in the overlapping Thor ledger. It accelerates only
  uncached CPU-side vertex attributes; hardware vertex loading, default attributes, and vertex-
  cache hits bypass some or all of the work. The 1.95x-3.38x exact-loop ratios cannot be added to
  the prior 133 entries or converted into whole-emulator FPS or battery watts. Fewer recurring
  lookups, indirect calls, and address operations make lower energy plausible on this path, but
  whole-game frametime, thermal slope, and battery power still require a controlled matched title/
  scene/cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B.

## Bounded PICA Vertex-Cache Output Copies (2026-08-19)

- Indexed CPU-fallback draws keep 64 cached `AttributeBuffer` objects. Each object is sixteen
  16-byte shader-output attributes, so the established hit and insertion assignments transferred
  all 256 bytes. `ShaderUnit::WriteOutput()` actually packs the `output_mask` registers into a
  prefix. The no-geometry consumer reads `rasterizer.vs_output_total` attributes, while geometry
  paths read `pipeline.vs_outmap_total_minus_1_a + 1`.
- The accepted path computes those produced and consumed counts once per indexed draw. Only an
  exact match of zero through six selects a dedicated vertex loop whose hit and insertion sites
  enter one fallthrough prefix copier. Non-indexed draws, count mismatches, and counts 7-16 execute
  the original full assignment. This preserves the old undefined/stale suffix when any consumer
  might observe it, while matched bounded consumers can observe only bytes explicitly copied from
  the cached vertex.
- The six-output ceiling came from exact device evidence rather than assuming fewer bytes always
  means less time. A temporary release ARM64 test used 64 cache entries, one million copies per
  sample, 11 alternating-order median samples, a forced memory-visible destination, and equal
  live-prefix checksums. The Thor was AC-powered, charge-limited/status 3 at 80%, and 24.0 C at the
  evidence capture, so these are wall-powered kernel timings rather than battery-discharge watts:

  | Outputs | A510 CPU 0 full -> prefix (ratio) | A715 CPU 3 full -> prefix (ratio) | A710 CPU 5 full -> prefix (ratio) |
  | ---: | ---: | ---: | ---: |
  | 1 | 23.977396 -> 10.444323 ns (2.295735x) | 3.524219 -> 2.097760 ns (1.679992x) | 4.639792 -> 2.193229 ns (2.115507x) |
  | 2 | 25.069896 -> 14.489427 ns (1.730220x) | 3.526145 -> 2.144323 ns (1.644409x) | 4.642188 -> 2.196979 ns (2.112987x) |
  | 3 | 24.530469 -> 15.545885 ns (1.577940x) | 3.599479 -> 3.556146 ns (1.012185x) | 4.649583 -> 3.569739 ns (1.302499x) |
  | 4 | 23.869271 -> 17.615364 ns (1.355026x) | 3.414427 -> 2.169479 ns (1.573847x) | 4.696667 -> 2.217083 ns (2.118399x) |
  | 5 | 24.528542 -> 19.026146 ns (1.289202x) | 3.578541 -> 3.469531 ns (1.031419x) | 4.689792 -> 3.520364 ns (1.332190x) |
  | 6 | 23.827136 -> 22.483073 ns (1.059781x) | 3.560261 -> 3.521146 ns (1.011109x) | 4.668177 -> 3.571980 ns (1.306888x) |

  CPU7/X3 was initially online but Android `core_ctl` parked it before the pinned benchmark; no
  power or core setting was changed to force it online, so no X3 timing is claimed.
- Two broader candidates were rejected. Copying every width through the fallthrough helper made a
  full sixteen-output transfer 0.927145x on A510, 0.980161x on A715, and 0.975609x on A710. Adding
  a recurring `count > 6` branch before the old full assignment was worse on A510: widths 8, 12,
  15, and 16 measured only 0.818907x, 0.819426x, 0.775222x, and 0.828437x. The accepted draw-level
  loop choice pays the width gate once and leaves the wide inner loop unchanged.
- Final ThinLTO grows `PicaCore::LoadVertices()` from 1,424 bytes (`0x590`) to 2,392 bytes (`0x958`)
  because it retains separate bounded and full loops. The bounded hit/insertion sites load and
  store only the selected Q attributes. The full path still contains eight paired Q load/store
  groups and no recurring output-count branch. That 968-byte code-footprint cost is explicit; only
  one loop executes for a draw.
- The permanent test fills source and destination with different byte patterns, verifies every
  live prefix for counts 0-6, and proves every suffix byte remains its original canary. The final
  benchmark-free `[video_core]` suite passed 135,081 assertions in 77 cases independently on A510
  CPU0, A715 CPU3, and A710 CPU5. X3 was parked during the final attempt. Native Android ARM64
  tests/library builds passed before and after removing the temporary benchmark.
- Source/test commit `12aa05cf7` was pushed directly to `origin/master` with command-line Git over
  SSH. Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 3 minutes 6 seconds.
  The ARM64-only, v2-signed APK is 29,006,976 bytes with SHA-256
  `C1BEB8DF79FE973AD2A08F5E995C9E405DE9C608416B1EC9C3589217F071FF94`; its signer certificate
  SHA-256 remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `12aa05cf7-vanilla-thor`, minimum SDK
  29, and target SDK 37. Wi-Fi ADB installed it, an immediate force-stop left no app process, and
  `stay_on_while_plugged_in` was restored to its recorded original value 0. Neither app nor game
  was launched.
- Exact bounded cleanup removed 2,472,669,362 logical host bytes and recovered 2,028,470,272
  physical bytes, leaving 53,457,612,800 bytes free on C:. The retained active ARM64 CMake/Ninja
  cache is 2,806,151,867 bytes; retained build output is only the 29,006,976-byte APK, Gradle's
  476-byte output metadata, and the 391-byte install metadata. The 449,605,752-byte native test
  ELF, stripped test, Gradle/JNI/R8/symbol/mapping staging, and exact `/data/local/tmp` helper were
  removed. No PDF, benchmark, test binary, rendered manual page, APK, or scratch note was committed.
- This is optimization/candidate entry 135 in the overlapping Thor ledger. It accelerates only
  cache transfers in indexed draws that reach CPU vertex processing and expose a matching 0-6
  output layout; hardware vertex shaders, non-indexed draws, shader execution on misses, and wider
  outputs bypass the new copy. The 1.01x-2.30x exact-kernel ratios cannot be added to the prior 134
  entries or converted into whole-emulator FPS or battery watts. Fewer recurring load/store bytes
  make lower energy plausible on this path, but whole-game frametime, thermal slope, and battery
  power still require a controlled matched title/scene/cache/renderer/driver/resolution/layout/
  performance-mode/fan/brightness/duration A/B.

## Direct PICA Vertex-Cache Output Submission (2026-08-19)

- Entry 135 still routed an exact matching shader-output prefix through a transient `vs_output`
  object on a cache hit and through that object plus a bounded/full copy on a miss. Every
  `GeometryPipeline::SubmitVertex()` consumer is synchronous: the no-GS route calls its output
  handler immediately, while point, variable-primitive, and fixed-primitive geometry backends copy
  the required prefix into their own storage before returning. `ShaderUnit::WriteOutput()` likewise
  packs every live output contiguously from attribute zero.
- Entry 136 selects a direct specialization once per indexed draw whenever the shader output-mask
  popcount equals the downstream count (`rasterizer.vs_output_total` without GS or
  `pipeline.vs_outmap_total_minus_1_a + 1` with GS). A hit submits the selected cache entry by
  reference. A miss writes shader output directly into the circular replacement entry, advances
  the established ID/count/position state, and synchronously submits that same entry. Exact matches
  from zero through sixteen are eligible. Non-indexed draws and every count mismatch retain the old
  complete 256-byte assignment so no wider consumer can observe a stale suffix.
- The temporary Android 29 ARM64 benchmark modeled entry 135 and entry 136 separately for cache
  hits and misses. It used 64 cache entries, one million iterations per sample, 11 alternating-
  order median samples, identical nonzero checksums, and output counts 1, 2, 4, 6, 7, 12, and 16.
  Miss timing included the same noinline output producer on both sides; the old side then performed
  entry 135's prefix/full cache copy, while the direct side produced into the cache entry. The
  device reported 80%, `Discharging`, and 25.0 C, so this is not battery-watt evidence:

  | Outputs | A510 hit / miss | A715 hit / miss | A710 hit / miss |
  | ---: | ---: | ---: | ---: |
  | 1 | 1.259018x / 1.111344x | 1.015095x / 1.720265x | 1.056847x / 1.145992x |
  | 2 | 1.473457x / 1.162658x | 1.018112x / 1.185771x | 1.070993x / 1.193822x |
  | 4 | 1.805628x / 1.151155x | 1.052026x / 1.251375x | 1.104641x / 1.266315x |
  | 6 | 2.271616x / 1.169522x | 1.009034x / 1.299087x | 1.051900x / 1.326456x |
  | 7 | 3.062659x / 1.258411x | 1.087324x / 1.295447x | 1.883147x / 2.256147x |
  | 12 | 2.959693x / 1.317051x | 1.148548x / 1.370569x | 1.942198x / 1.661994x |
  | 16 | 2.988929x / 1.313046x | 1.699750x / 1.444158x | 3.028781x / 1.552859x |

  A510 used CPU0 (`0xd46`), A715 CPU3 (`0xd4d`), and A710 CPU5 (`0xd47`). CPU7/X3 was listed
  online but Android `core_ctl` rejected the affinity mask before timing; no power or core-control
  setting was changed to force it, so no X3 result is claimed.
- Final ThinLTO shrank `PicaCore::LoadVertices()` from entry 135's 2,392 bytes (`0x958`) to 2,236
  bytes (`0x8bc`). The exact-match hit path computes the cache-entry address and passes it directly
  to `SubmitVertex()` with no Q-vector transfer. Its miss path makes that replacement address the
  `WriteOutput()` destination and performs no later copy. The fallback hit and insertion sites
  retain eight paired Q load/store groups, and the produced/consumed equality branch remains
  outside both recurring loops.
- The permanent test covers every produced/consumed pair from 0 through 16, rejects all non-indexed
  direct cases, byte-compares every observable exact-match prefix with the previous output, and
  proves the untouched cache suffix remains canary data. The final benchmark-free `[video_core]`
  suite passed 135,679 assertions in 77 cases independently on A510 CPU0, A715 CPU3, and A710 CPU5.
  X3 affinity was still parked. Native tests and the ThinLTO Android library rebuilt successfully
  after removing all temporary benchmark source.
- Source/test commit `b41e339e7` was pushed directly to `origin/master` with command-line Git over
  SSH. Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 2 minutes 5 seconds.
  The ARM64-only, v2-signed APK is 29,007,296 bytes with SHA-256
  `B0F3E187C12570E30CED57C0796E722F9EE1CFD79730D1AD146535F6FFD58713`; its signer certificate
  SHA-256 remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `b41e339e7-vanilla-thor`, minimum SDK
  29, and target SDK 37. Wi-Fi ADB installed it, an immediate force-stop left no app PID, and the
  original `stay_on_while_plugged_in=0` remained unchanged. Neither app nor game was launched.
- Exact bounded cleanup removed 2,472,677,041 logical host bytes and recovered 2,028,490,752
  physical bytes, leaving 53,457,633,280 bytes free on C:. Retained repo build output is only the
  29,007,296-byte APK and 476-byte metadata; the active ARM64 CMake/Ninja cache is 2,806,287,942
  bytes. The 449,610,168-byte native test ELF, both stripped test binaries, Gradle/JNI/R8/symbol/
  mapping staging, and device `/data/local/tmp` helpers were removed. No PDF, benchmark, test
  binary, rendered manual page, APK, or scratch note was committed.
- This is optimization/candidate entry 136 in the overlapping Thor ledger and supersedes entry
  135's bounded-copy implementation at HEAD. It accelerates only exact-layout cache traffic in
  indexed draws that reach CPU vertex processing; hardware vertex shaders, non-indexed draws, and
  shader execution on cache misses limit whole-game impact. The exact hit/miss ratios cannot be
  added to the prior 135 entries or converted into emulator FPS or battery watts. Eliminating up
  to two transient transfers makes lower energy plausible on this route, but whole-game frametime,
  thermal slope, and battery power still require a controlled matched title/scene/cache/renderer/
  driver/resolution/layout/performance-mode/fan/brightness/duration A/B.

## Draw-Cached Packed Shader Input Map (2026-08-19)

- The software vertex path called `ShaderUnit::LoadInput(ShaderRegs, AttributeBuffer)` for every
  cache miss or non-indexed vertex. For each active attribute, `GetRegisterForAttribute()` rebuilt
  the same 64-bit map from two 32-bit draw registers and selected a nibble with a variable shift.
  Final AArch64 showed the more important aliasing cost: it reloaded that 64-bit value from
  `ShaderRegs` inside every attribute iteration.
- Two byte-array predecode candidates were measured first. The input form improved its isolated
  recurring loop, but a separate decode pass made small full draws regress until enough vertices
  amortized setup. The output form removed `RBIT`/`CLZ` selection work on larger cores but regressed
  one-output and several A510 shapes, so no output-map change was retained.
- The accepted design keeps the register map packed. `ShaderInputMap` snapshots the two map words
  into one `u64` plus the active count once per draw. Each vertex copies that `u64` to a local GPR,
  masks its low nibble, shifts by four, and copies the corresponding 16-byte attribute. This has no
  O(attribute-count) setup pass or draw-size threshold and preserves the original ascending-order,
  duplicate-register last-write semantics.
- The final prototype benchmark used the real `ShaderRegs`, `ShaderUnit`, `AttributeBuffer`, and
  release ThinLTO build. Each cell processed 500,000 total vertices, used seven alternating-order
  median samples, and required identical nonzero checksums. It covered attribute counts 1, 2, 4,
  6, 8, 12, and 16 crossed with draw sizes 1, 2, 4, 8, 16, 32, and 64. Results include the once-per-
  draw snapshot cost:

  | Core | All 49 cells | 1 attribute / 1 vertex | 8 attributes / 8 vertices | 16 attributes / 64 vertices |
  | --- | ---: | ---: | ---: | ---: |
  | A510 CPU 0 | 1.051684x-1.899835x | 1.051684x | 1.728793x | 1.860196x |
  | A715 CPU 3 | 1.036312x-1.254600x | 1.036312x | 1.223495x | 1.254600x |
  | A710 CPU 5 | 0.994865x-1.292195x | 0.994865x | 1.154375x | 1.170091x |

  The lone 0.994865x row is a 0.5135% A710 one-attribute/one-vertex measurement-edge tie; every
  other A710 cell improved by at least 2.0805%, and the packed recurring loop removes the repeated
  config load in final production code. CPU7/X3 affinity was rejected by Android `core_ctl`, so no
  X3 result is inferred. The device reported 80%, `Discharging`, and 24.0 C after the run; these are
  timing results, not battery-watt evidence.
- Final ThinLTO grows `PicaCore::LoadVertices()` from entry 136's 2,236 bytes (`0x8bc`) to 2,272
  bytes (`0x8e0`). Both the direct-cache and fallback CPU loops load the packed map once per vertex,
  then use `AND #0xf`, `LSR #4`, a Q load, and an indexed Q store. Neither recurring inner loop
  reloads `ShaderRegs`. Immediate-mode and geometry input retain the original config overload.
- The permanent differential test covers active counts 1-16 across 32 deterministic mappings per
  count, including identity, reverse, all-to-one, random duplicate, low-word, and high-word cases.
  It starts both shader units with identical nonzero canaries and byte-compares every register, so
  ordering, duplicate overwrite, and untouched-register behavior are all covered. The final
  benchmark-free `[video_core]` suite passed 136,191 assertions in 78 cases independently on A510
  CPU0, A715 CPU3, and A710 CPU5. X3 affinity remained unavailable.
- Source/test commit `8a6668d3e` was pushed directly to `origin/master` with command-line Git over
  SSH. Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 1 minute 54 seconds.
  The ARM64-only, v2-signed APK is 29,009,108 bytes with SHA-256
  `6369A3E600A7FF6C1451A4917B7DAE2D38E5F71F2AE368BEFF6EC753FCAB75A3`; its signer certificate
  SHA-256 remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `8a6668d3e-vanilla-thor`, minimum SDK
  29, and target SDK 37. Wi-Fi ADB installed it, an immediate force-stop left no app PID, and the
  original `stay_on_while_plugged_in=0` remained unchanged. Neither app nor game was launched.
- Exact bounded cleanup removed 2,561,017,772 logical host bytes and recovered 2,116,902,912
  physical bytes, leaving 53,206,994,944 bytes free on C:. Retained repo build output is only the
  29,009,108-byte APK and 476-byte metadata; the active ARM64 CMake/Ninja cache is 2,806,594,337
  bytes. The 449,633,104-byte native test ELF, both roughly 44.1 MB stripped test binaries,
  Gradle/JNI/R8/symbol/mapping staging, and both device `/data/local/tmp` helpers were removed. No
  PDF, benchmark, test binary, rendered manual page, APK, or scratch note was committed.
- This is optimization/candidate entry 137 in the overlapping Thor ledger. It accelerates only
  CPU-fallback vertex input mapping; hardware vertex shaders and indexed cache hits can bypass it,
  while vertex loading and shader execution still dominate many misses. The exact mapping ratios
  cannot be added to the prior 136 entries or converted into whole-emulator FPS or battery watts.
  Removing repeated config loads and variable shifts makes lower energy plausible on this path,
  but whole-game frametime, thermal slope, and battery power still require a controlled matched
  title/scene/cache/renderer/driver/resolution/layout/performance-mode/fan/brightness/duration A/B.

## Config-Packed Shader Input Map and Single-Attribute Fast Path (2026-08-19)

- Entry 137 cached the packed input map once for ordinary CPU-fallback draws, but the remaining
  config overload still called `ShaderRegs::GetRegisterForAttribute()` for every attribute. That
  route is used by immediate-mode drawing, point-geometry input assembly, and the shader debug
  interpreter. Final AArch64 reloaded the same unaligned 64-bit map and applied a variable shift in
  every loop iteration.
- Entry 138 gives both overloads one internal always-inlined packed loop. The config route handles
  one active attribute with one direct 16-byte load/store. For two through sixteen attributes it
  reads the adjacent low/high map words once with a little-endian `memcpy` that Clang lowers to one
  unaligned `LDUR X`, then masks the low nibble and shifts the local `u64` by four per attribute.
  A compile-time adjacency check guards that load, and a numeric high/low-word fallback preserves
  other byte orders. The `ShaderInputMap` route uses its guaranteed 1-16 count in a do/while and
  therefore no longer needs a recurring zero-count entry branch.
- Two intermediate shapes were not accepted. Routing the config overload through the separately
  linked `ShaderInputMap` constructor produced a 0x9c-byte method with a stack frame, canary, and
  PLT call. A later direct expression removed that call but emitted two 32-bit loads plus `ORR`;
  short 500,000-iteration A715 windows also exposed unstable count-4/count-16 outliers. The final
  contiguous load is smaller, and the timing window was increased fourfold before acceptance.
- The final temporary benchmark used the real `ShaderRegs`, `ShaderUnit`, and `AttributeBuffer`.
  Each sample performed 2,000,000 config loads over 64 nonzero source buffers; eleven samples used
  alternating old/new order and median selection, and old/new checksums had to match. It covered
  active counts 1, 2, 4, 6, 8, 12, and 16. The Thor was AC-powered at 80% and 23.0 C, so these are
  timing results and not battery-discharge watt measurements:

  | Attributes | A510 CPU 0 | A715 CPU 3 | A710 CPU 5 |
  | ---: | ---: | ---: | ---: |
  | 1 | 7.598750 -> 6.063099 ns; 1.253278x | 1.979688 -> 1.509375 ns; 1.311594x | 2.047135 -> 1.568933 ns; 1.304795x |
  | 2 | 11.655442 -> 9.104114 ns; 1.280239x | 2.636042 -> 2.389661 ns; 1.103102x | 2.721250 -> 2.535156 ns; 1.073405x |
  | 4 | 19.310859 -> 12.611120 ns; 1.531257x | 3.983047 -> 3.510860 ns; 1.134493x | 4.344036 -> 3.785468 ns; 1.147556x |
  | 6 | 26.787838 -> 22.585911 ns; 1.186042x | 5.489870 -> 4.584870 ns; 1.197388x | 5.701041 -> 4.956797 ns; 1.150146x |
  | 8 | 34.602761 -> 19.749063 ns; 1.752122x | 7.135651 -> 5.774792 ns; 1.235655x | 7.261563 -> 6.239922 ns; 1.163726x |
  | 12 | 50.674089 -> 26.832604 ns; 1.888527x | 9.737943 -> 8.319453 ns; 1.170503x | 10.040156 -> 9.061068 ns; 1.108054x |
  | 16 | 71.097943 -> 38.928724 ns; 1.826362x | 12.624531 -> 10.458464 ns; 1.207111x | 12.968307 -> 11.524506 ns; 1.125281x |

  CPU7/X3 was parked by Android `core_ctl`, so no X3 timing is inferred. No governor, frequency,
  core-control, or app setting was changed. A transient CPU-wake helper was removed after the A710
  process bound, and a final device check found no helper or emulator process.
- Final ThinLTO makes the exported config overload 76 bytes (`0x4c`) and the packed-map overload 64
  bytes (`0x40`, down from 0x48). Both are leaf functions with no stack frame or PLT call. The
  config route contains one count check, one `LDUR X` for counts above one, and the compact
  `AND`/`LSR` plus Q-load/indexed-Q-store loop. `PicaCore::DrawImmediate()` and
  `GeometryPipeline_Point::SubmitVertex()` inline the same shape, proving the production callers
  receive the optimization rather than only the benchmark wrapper.
- The permanent test now uses an independent scalar reference instead of calling either optimized
  overload. For every active count 1-16 and 32 identity/reverse/all-to-one/random mappings it
  compares both the draw-cached and config-driven overloads byte-for-byte, including high-word
  attributes, duplicate last-write ordering, and untouched canary registers. The final
  benchmark-free `[video_core]` suite passed 136,703 assertions in 78 cases independently on A510
  CPU0, A715 CPU3, and A710 CPU5. X3 affinity remained unavailable.
- Source/test commit `e1379b962` was pushed directly to `origin/master` with command-line Git over
  SSH. Exact post-commit JDK 17 packaging with
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` passed in 2 minutes 53 seconds.
  The ARM64-only, v2-signed APK is 29,008,796 bytes with SHA-256
  `2D189933474FF0BEFE9DDBE13B700DA33BCEE07D570BEFA7EEE0E664D865DBF9`; its signer certificate
  SHA-256 remains `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `e1379b962-vanilla-thor`, minimum SDK
  29, and target SDK 37. Wi-Fi ADB installed it successfully, an immediate force-stop left no app
  PID, and the original `stay_on_while_plugged_in=0` remained unchanged. Neither app nor game was
  launched.
- Exact bounded cleanup removed 2,472,737,762 logical host bytes and recovered 2,028,523,520
  physical bytes, leaving 53,200,289,792 bytes free on C:. Retained repo build output is only the
  29,008,796-byte APK and 476-byte metadata; the active ARM64 CMake/Ninja cache is 2,806,751,027
  bytes. The 449,635,192-byte native test ELF, stripped benchmark/final test binaries, Gradle/JNI/
  R8/symbol/mapping staging, and device `/data/local/tmp` helper were removed. No PDF, benchmark,
  test binary, rendered manual page, APK, or scratch note was committed.
- This is optimization/candidate entry 138 in the overlapping Thor ledger. It accelerates only
  config-driven input-register mapping plus entry 137's one-attribute packed overload; ordinary
  indexed draws can reuse entry 137's snapshot, hardware vertex shaders and cache hits can bypass
  CPU mapping, and shader execution still dominates many misses. The exact 1.07x-1.89x path ratios
  cannot be added to the prior 137 entries or converted into whole-emulator FPS or battery watts.
  Fewer config loads, variable shifts, branches, and constructor overhead make lower energy
  plausible on this route, but whole-game frametime, thermal slope, and battery power still
  require a controlled matched title/scene/cache/renderer/driver/resolution/layout/performance-
  mode/fan/brightness/duration A/B.

## Packed AArch64 Boolean-Uniform Write (2026-08-19)

- `ShaderSetup::WriteUniformBoolReg()` previously looped over sixteen guest boolean registers,
  extracted each source bit separately, compared sixteen old bytes, and performed sixteen byte
  stores. Entry 139 replaces only the AArch64 path with one packed 16-byte update. It duplicates
  the 16-bit input's low and high bytes into the vector halves, masks both halves with
  `[1,2,4,8,16,32,64,128]`, normalizes the set bits to byte `1`, XORs against one old Q load,
  reduces change with `UMAXV`, and performs one Q store. The existing scalar implementation remains
  the non-AArch64 fallback.
- The local Cortex optimization guides were read directly before acceptance. A510 pages 36 and
  43-44 list byte compare/logical work at latency 3 with split `2,1` throughput, GPR `DUP` at 3/1,
  and 16-byte `UMAXV` at 4/1. A715 pages 29 and 34-35 list 16-byte `UMAXV` at latency 6/throughput
  1/2, GPR `DUP` at 3/1, and generally stronger logical throughput. A710 pages 43 and 52-53 list
  compare/logical at 2/2, 16-byte `UMAXV` at 4/1/2, and GPR `DUP` at 3/1. X3 pages 26 and 31-32
  list compare/logical at 2/4 and 4H/4S `UMAXV` at 2/2, but CPU7 was parked by Android `core_ctl`.
  These tables guided the shape; only physical Thor results accepted it.
- A separate packed-float24 attribute candidate used `TBL` plus vector exact conversion and passed
  one million randomized equality cases, but it was rejected after A510 medians fell to
  0.536978x/0.522080x for the branch form and 0.541669x/0.529736x for the fully vector form on
  normal/special inputs. No part of that experiment entered production or increments the ledger.
- The exact old-scalar/new-packed benchmark first checked all 65,536 possible register values, then
  used seven alternating-order samples over 256 changing or unchanged inputs and 32,768 repeats:

  | Thor core | Changing write | Unchanged rewrite |
  | --- | ---: | ---: |
  | A510 CPU 0 | 89.722766 -> 86.142626 ns; 1.041561x | 89.225719 -> 86.220199 ns; 1.034859x |
  | A715 CPU 3 | 7.599741 -> 4.146931 ns; 1.832618x | 7.594979 -> 4.140058 ns; 1.834510x |
  | A710 CPU 5 | 5.657480 -> 3.861586 ns; 1.465066x | 5.503595 -> 3.863660 ns; 1.424451x |

  The A510 samples were bimodal under Android scheduling/frequency behavior, so the conservative
  slow-frequency medians above are the claim; faster same-run samples are not substituted. X3
  affinity was unavailable and no X3 result is inferred.
- Final production ThinLTO emits an 84-byte (`0x54`), 21-instruction, straight-line function with
  two `DUP`s, one constant Q load, vector `AND`/normalization, one old Q load, `EOR`, one new Q
  store, and `UMAXV`; the prior compiled scalar mirror was 360 bytes (`0x168`) with repeated scalar
  byte loads/stores and bit extraction. The permanent test exhaustively checks all 65,536 inputs
  from inverse initial values, verifies exact byte `0`/`1` mapping and dirty-on-change, then rewrites
  the same value and verifies no false dirty flag. The complete `[video_core]` suite passed 398,847
  assertions in 79 cases independently on A510 CPU0, A715 CPU3, and A710 CPU5.
- Source/test commit `785056d04` was pushed directly to `origin/master` with command-line Git over
  SSH. The post-commit JDK 17 ARM64 native build and
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` packaging both passed. The
  ARM64-only, v2-signed APK is 29,009,032 bytes with SHA-256
  `087AAF72BC7E7C56B5F700084ECB747F373AE8465B4D676B975E9F104D2C2AEA`; its signer certificate
  SHA-256 is `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `785056d04-vanilla-thor`, minimum SDK
  29, and target SDK 37. Wi-Fi ADB installed it successfully; the app was immediately force-stopped,
  no app PID remained, and `stay_on_while_plugged_in` was restored and verified as `0`. Neither app
  nor game was launched.
- Exact bounded cleanup removed 2,472,794,359 logical host bytes and recovered 2,028,003,328
  physical bytes, leaving 52,972,720,128 bytes free on C:. Retained repo build output is only the
  29,009,032-byte APK and 476-byte metadata; the active ARM64 CMake/Ninja cache is 2,800,888,899
  bytes. The 449,664,136-byte native test ELF, Gradle/JNI/R8/symbol/mapping staging, and all host/
  device benchmark helpers were removed. No PDF, benchmark, test binary, rendered manual page,
  APK, or scratch note was committed.
- This is optimization/candidate entry 139 in the overlapping Thor ledger. It accelerates only PICA
  boolean-uniform register writes; call frequency depends on each game's command stream, and many
  frames spend most time elsewhere. The exact 1.03x-1.83x kernel ratios cannot be added to the
  prior 138 entries or converted into whole-emulator FPS or battery watts. One vector load/store
  pair and fewer decoded instructions make lower energy plausible on this path, but whole-game
  frametime, thermal slope, and battery power still require a controlled matched A/B.

## Grouped AArch64 Float32 Uniform Upload (2026-08-19)

- `ShaderSetup::WriteUniformFloatRegRange()` previously sent every transfer word through
  `PackedAttribute::Push()`. That repeated queue-index branching and storage per word, then compared
  each completed uniform even when the setup was already dirty. Entry 140 adds an AArch64-only,
  out-of-line `WriteUniformFloat32Groups()` helper for aligned, empty-queue, complete float32 groups.
  Each four-word group uses one Q load, `REV64` plus `EXT #8` to map transfer order to the guest
  `Vec4`, and one Q store. An already-dirty batch uses a store-only loop; a clean batch XORs each old
  vector with its replacement, ORs those differences, and performs one final `UMAXV` reduction.
  The last packed raw group is copied to `uniform_queue.buffer`, preserving observable/save-state
  queue representation.
- Partial queues, float24 writes, scalar tails, out-of-range uniform indices, and non-AArch64 hosts
  still use the original word-at-a-time route. The permanent differential test spans float24 and
  float32, start indices 0/1/94/95/96/127, every partial queue prefix, clean and already-dirty
  states, counts from zero through 63 around group boundaries, and raw bit patterns including both
  zero signs, infinities, quiet/signaling-NaN-like values, subnormals, normals, and randomized data.
  It compares every uniform, queue word/index, configuration index, and dirty flag against repeated
  scalar writes. A separate exact-rewrite test proves an identical clean float32 range remains
  clean.
- The local Cortex manuals guided the candidate. A510 pages 43-46 list `REV64`/`EXT` at latency 3
  with split `2,1` throughput and a one-register Q load at latency 3/throughput 2. A715 pages 34-36
  list the permutations at 2/2 and the Q load at 6/3; A710 pages 52-55 list 2/2 and 6/3; X3 pages
  31-33 list 2/4 and 6/3. The tables did not decide acceptance. A separate grouped float24 candidate
  was rejected because small batches on A510 were unstable or regressive even where larger or
  already-dirty batches won; production contains no grouped float24 path.
- The exact differential benchmark validated both formats first, then ran seven alternating old/new
  order samples with 65,536 repeats per sample in unchanged-clean, already-dirty, and changing-clean
  modes. Median old-over-new ranges over one, two, four, eight, sixteen, and twenty-four groups were:

  | Thor core | Exact uploader-kernel improvement |
  | --- | ---: |
  | A510 CPU 0 | 1.150440x-3.819898x |
  | A715 CPU 3 | 1.957553x-14.839140x |
  | A710 CPU 6 | 1.875851x-12.244903x |

  The one-group cases improved in every mode: A510 1.150440x-1.499645x, A715
  1.957553x-2.217076x, and A710 1.875851x-2.096214x. X3 was parked by Android `core_ctl`, so no X3
  result is inferred.
- Final ThinLTO emits a 144-byte (`0x90`) grouped helper. Its store-only loop is one post-index Q
  load, `REV64`, `EXT #8`, one post-index Q store, and a loop branch; the clean loop adds one old Q
  load plus XOR/OR accumulation and performs one final `UMAXV`. The range dispatcher remains 532
  bytes (`0x214`) with scalar float24, partial, and tail paths intact. The focused shader-setup tests
  passed 286,361 assertions in five cases on Thor's A510. The complete benchmark-free
  `[video_core]` suite passed 411,912 assertions in 81 cases independently on A510 CPU0, A715 CPU3,
  and A710 CPU6.
- Source/test commit `8f6479d0b` was pushed directly to `origin/master` with command-line Git over
  SSH. The post-commit JDK 17 `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache`
  package build passed in 2 minutes 8 seconds. The ARM64-only, v2-signed APK is 29,009,252 bytes
  with SHA-256 `A97C4F59A6B7F3792B96EE97EB30921B9B02E053A6AF0CDC863BB2F3DD3A1FCC`; its signer certificate
  SHA-256 is `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `8f6479d0b-vanilla-thor`, minimum SDK 29,
  and target SDK 37. Wi-Fi ADB installed it successfully; the app was immediately force-stopped,
  its PID remained absent, and `stay_on_while_plugged_in` was restored and verified as `0`. Neither
  app nor game was launched.
- Exact bounded cleanup removed 2,472,995,339 logical host bytes and recovered 2,028,593,152 physical
  bytes, leaving 52,510,937,088 bytes free on C:. Retained repo build output is only the 29,009,252-
  byte APK and 476-byte metadata; the active ARM64 CMake/Ninja cache is 2,801,242,638 bytes. The
  449,761,696-byte native test ELF, Gradle/JNI/R8/symbol/mapping staging, and all host/device benchmark
  helpers and rendered manual pages were removed. No PDF, benchmark, test binary, rendered page,
  APK, or scratch note was committed.
- This is optimization/candidate entry 140 in the overlapping Thor ledger. It is the total accepted
  ledger count; earlier references to 78 described only a narrower recent-work subset. These exact
  1.15x-14.84x ratios accelerate only the float32 uniform-upload kernel and cannot be added to the
  prior 139 entries or converted into whole-emulator FPS or battery watts. Fewer queue writes,
  comparisons, loads, branches, and decoded instructions make lower energy plausible on this path,
  but whole-game frametime, thermal slope, and battery power still require a controlled matched A/B.

## Already-Dirty AArch64 PICA LUT Copies (2026-08-19)

- Entry 141 targets batch uploads to PICA lighting, fog, and procedural-texture LUTs. Once a table's
  dirty bit is set, loading and comparing every old word cannot change observable dirty state.
  AArch64 batches of at least seven words now normalize the circular index once, split only at table
  wrap boundaries, and copy each contiguous span. Clean uploads and one-to-six-word dirty batches
  retain the exact comparison loop. The procedural-texture table switch also moved outside the word
  loop, so every batch selects Noise, ColorMap, AlphaMap, Color, or ColorDiff once rather than once
  per uploaded word. Non-AArch64 code is unchanged.
- The first broader candidate vectorized clean uploads and appeared extremely fast in a prototype
  whose table size was runtime-variable. That result was rejected: production's 128- and 256-entry
  tables are compile-time powers of two, so Clang uses masks and can auto-vectorize non-wrapping
  clean loops instead of paying the prototype's integer division. A power-of-two-table rerun found
  clean-span regressions in important shapes. The shipped path therefore eliminates work only when
  dirty state proves comparisons redundant. Cortex load/store tables on A510 page 32, A715 page 26,
  A710 page 39, and X3 page 23 guided the memory-traffic audit but did not decide acceptance.
- The final standalone Android 29 ARM64 benchmark first passed 20,000 randomized differential
  trials over 128/256-entry tables, dirty and clean starts, unchanged data, random values, arbitrary
  16-bit offsets, and counts through 511. It then used seven alternating old/new-order samples per
  cell. Median old-over-new ratios for the accepted already-dirty seven-to-255-word cases were:

  | Thor core | Exact already-dirty LUT-kernel improvement |
  | --- | ---: |
  | A510 CPU 0 | 1.263743x-5.664785x |
  | A715 CPU 3 | 1.061347x-10.354599x |
  | A710 CPU 4 | 1.042289x-10.966256x |

  One-to-six-word copies were rejected and keep the comparison route. The minimum accepted rows
  were wrap-heavy seven/eight-word cases; common contiguous seven-word rows improved 2.337156x on
  A510, 1.452297x on A715, and 1.460733x on A710. X3 affinity remained unavailable, so no X3 result
  is inferred. The Thor was AC-powered at 80%, 4.269 V, and 25.0 C; these are sustained performance
  measurements, not battery-discharge watts.
- Permanent AArch64 differential coverage checks both table sizes; counts 0-8 and boundaries through
  511; offsets at 0/1/125/127/128/129/65535; clean and dirty starts; explicit unchanged clean data;
  multiple wraps; and 10,000 randomized cases per size. Focused tests passed 21,008 assertions in
  three cases. The complete `[video_core]` suite passed 432,920 assertions in 82 cases independently
  on A510 CPU0, A715 CPU3, and A710 CPU4. Final ThinLTO emits count-seven and dirty-state gates,
  circular `memcpy` spans for the accepted path, and 32-word AdvSIMD compare/store loops for clean
  power-of-two spans. The procedural table selection is not inside those recurring loops.
  `PicaCore::HandleSpecialRegBatch()` is 2,736 bytes (`0xAB0`).
- Source/test commit `79e81fc35` was pushed directly to `origin/master` with command-line Git over
  SSH. The upstream audit found the fork 264 commits ahead and zero behind `upstream/master`
  (`f6a3e3aa5`), so no merge was required. The post-commit JDK 17 native build and
  `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache` package build passed. The
  ARM64-only, v2-signed APK is 29,010,652 bytes with SHA-256
  `FA66A0ACED75ED29E4CD6DE2DF3CF67776F7513F2C7EE8637F75AD3D21226E59`; its signer certificate
  SHA-256 is `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `79e81fc35-vanilla-thor`, minimum SDK 29,
  and target SDK 37. Wi-Fi ADB installed it successfully; the app was immediately force-stopped,
  its PID remained absent, and `stay_on_while_plugged_in` was restored and verified as `0`. Neither
  app nor game was launched.
- Exact bounded cleanup removed 2,500,538,406 logical host bytes and recovered 2,055,352,320 physical
  bytes, leaving 52,041,588,736 bytes free on C:. Retained repo build output is only the 29,010,652-
  byte APK and 476-byte metadata; the active ARM64 CMake/Ninja cache is 2,801,563,819 bytes. The
  449,854,232-byte native test ELF, Gradle/JNI/R8/symbol/mapping staging, rendered manual pages, and
  all host/device benchmark helpers were removed. No PDF, benchmark, test binary, rendered page,
  APK, or scratch note was committed.
- This is optimization/candidate entry 141 and the total overlapping ledger count is now 141, not
  78. The exact 1.04x-10.97x figures cover only already-dirty LUT batch uploads; they cannot be
  added to the prior 140 entries or converted into whole-emulator FPS or battery watts. Removing
  old-value reads, comparisons, per-word procedural switches, and circular index work makes lower
  energy plausible on this path, but whole-game frametime, thermal slope, and battery power still
  require a controlled matched A/B.

## Low-Lane AArch64 PICA Partial Stores (2026-08-19)

- Entry 142 tightens the existing store-only lowering for six partial destination masks. A source
  group beginning at PICA X is already in the low scalar portion of its SIMD register. `x` and `xy`
  therefore use immediate `STR S`/`STR D` instead of first calculating an address and executing an
  `ST1` element store. Noncontiguous `xz`, `xw`, `xyw`, and `xzw` use the same direct first store,
  then calculate only the remaining group's address. Their recurring destination-write bodies fall
  from four instructions to three; `x`/`xy` fall from two to one.
- Disabled lanes remain untouched because scalar `STR S` and `STR D` write exactly the low four or
  eight bytes. The empty mask, full `STR Q`, non-X-leading masks, and contiguous `xyz` post-indexed
  route are unchanged. Lanes Y/Z/W retain `ST1` because they are not the low scalar register.
  Output and temporary offsets are aligned multiples of the store width and well inside the scaled
  immediate range. The Arm Architecture Reference Manual DDI0487 M.c scalar SIMD `STR` and earlier
  `ST1` element semantics establish the encoding/correctness model; physical Thor timing decided
  acceptance.
- The standalone Android 29 ARM64 harness first compared every byte against an independent expected
  image for all six masks, eight destination offsets, distinct lane values, and untouched sentinels.
  It then used eleven alternating old/new-order samples, with eight exact write sequences per inner
  iteration. The longer confirmation medians were:

  | Thor core | `x` | `xy` | `xz` | `xw` | `xyw` | `xzw` |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | A510 CPU 0 | 5.901266x | 1.089771x | 1.351870x | 1.318610x | 0.997448x | 4.466630x |
  | A715 CPU 3 | 1.000873x | 0.995859x | 0.999763x | 0.999827x | 0.995792x | 1.001867x |
  | A710 CPU 5 | 0.998092x | 1.008555x | 1.204097x | 1.190127x | 1.207907x | 1.205705x |
  | X3 CPU 7 | 0.997424x | 1.002594x | 0.998304x | 1.000084x | 0.999953x | 1.000223x |

  Every cell stayed above the 0.995 acceptance floor. A715 and X3 results are correctly described
  as ties; the material gains concentrate on A510's element-store forms and A710's two-store forms.
  The Thor was AC-powered at 80%, 4.269 V, and 26.0 C before testing, so these are sustained
  instruction-path measurements rather than battery-discharge watt evidence.
- Permanent coverage retains all 14 partial masks and both output banks, and now specifically runs
  the six direct-store masks through output register 15 in both banks and temporary register 15.
  Enabled components receive distinct inputs while every disabled component retains a nonzero
  sentinel. The complete `[video_core][shader]` interpreter/JIT suite passed 18,506 assertions in
  52 cases independently on A510 CPU0, A715 CPU3, A710 CPU5, and X3 CPU7. The JDK 17 native ARM64
  build compiled the changed JIT and tests and linked `libcitra-android.so` plus the test runner.
  The established complete `[video_core]` gate then passed 433,094 assertions in 82 cases on A510.
- Source/test commit `cf7285cd8` was pushed directly to `origin/master` with command-line Git over
  SSH. The post-commit JDK 17 `:app:assembleVanillaRelWithDebInfoLite --no-configuration-cache`
  package build passed. The ARM64-only, v2-signed APK is 29,010,184 bytes with SHA-256
  `9DA8210CA63082D1ECC9D60AE95EF1E27C09330794F206E38F971B9C50FBB629`; its signer certificate
  SHA-256 is `0E5F42FF8E92CEDCBE3379BE71C8370B09BC10880584ACE4CF50F880EC514D4E`.
  It reports package `org.azahar_emu.azahar.debug`, version `cf7285cd8-vanilla-thor`, minimum SDK 29,
  and target SDK 37. Wi-Fi ADB installed it successfully; the app was immediately force-stopped,
  no app PID remained, and `stay_on_while_plugged_in` was restored and verified as `0`. Neither app
  nor game was launched.
- Exact bounded cleanup removed 2,500,666,313 logical host bytes and recovered 2,029,305,856
  physical bytes, leaving 51,789,287,424 bytes free on C:. Retained repo build output is only the
  29,010,184-byte APK and 476-byte metadata; the active ARM64 CMake/Ninja cache is 2,807,835,967
  bytes. The 449,916,600-byte native test ELF, Gradle/JNI/R8/symbol/mapping staging, and all host/
  device benchmark helpers were removed. No PDF, benchmark, test binary, rendered page, APK, or
  scratch note was committed.
- This is optimization/candidate entry 142 and raises the total overlapping ledger count to 142.
  The table measures only a saturated generated destination-store body; its ratios cannot be added
  to the prior 141 entries or converted into whole-game FPS or battery watts. The main remaining
  performance question is subsystem-level CPU/GPU synchronization, Vulkan barrier/render-pass
  churn, texture upload/conversion traffic, hardware-shader fallback frequency, pipeline churn,
  frame pacing, and idle wakeups. Those need counters plus matched scenes; further instruction
  reductions should not substitute for that whole-emulator accounting.

## 2026-09-17 Medarot 9 4x Fast-Forward Diagnosis, Thor MCP Server, Rejected Flush Fast Path

- Question: Medarot 9 (`0004000000174F00`) was reported as very slow. The goal was full speed at
  2x to 4x with fast-forward working, reached through code efficiency.
- Setup: AYN Thor over USB serial `c3ca0370`, Mesa Turnip R8 (Vulkan 1.4.335), performance mode
  2, fan mode 4, brightness 255 in manual mode, USB power, Eco Turbo on. Scene: the intro street
  dialog after New Game, reached by a held START and a held A on the title screen. Control build:
  production `31e455c30-vanilla-thor`, 29,177,431 bytes, SHA-256
  `8D4C2AE10C7C89EFF817867E9736AFA6AB7DB31FB5A8334D53F007A8498A1F4B`. Profiling builds used
  `-PthorFrameProfiling=true` for counters and temporary logs; no FPS claim comes from them.
- Finding 1: the title presents at 20 FPS by design. The title screen and the intro scene show
  Speed 100% at 3x with 5.4 to 6.2 ms of emulation work per 50 ms frame. The reported slowness
  is the native 20 FPS, not a speed deficit.
- Finding 2, control matrix on the production build (SurfaceFlinger mean FPS and P95 interval,
  KGSL GPU busy at 615 MHz, NativeEmulation thread user and system time as percent of one core):
  2x/100%: 19.9 FPS, 50.6 ms, 7.8%, 23.3/0.7. 2x/300%: 52.3 FPS, 33.7 ms, 23.0%, 65.3/0.7.
  3x/100%: 19.9 FPS, 50.6 ms, 13.2%, 24.0/0.7. 3x/300%: 57.9 FPS, 16.9 ms, 38.5%, 64.3/0.3.
  4x/100%: 19.8 FPS, 84.2 ms, 20.6%, 23.3/21.3. 4x/300%: 30.3 FPS, 50.5 ms, 32.1%, 36.0/30.3.
  The overlay at 4x/300% read Speed 156%, frame 10.8 ms, GSP command time 3.1 ms, remainder
  7.2 ms. At 3x/300% it read Speed 286%.
- Finding 3, frame profiler at 4x/300% per 300-swap window: `download_per_swap` 0.333 with
  125 KiB per swap, which is one 384 KiB RGB8 400x240 surface per frame; `finish` 100;
  `wait_ms_per_swap` 3.70 at 4x and 2.17 at 3x; `display_transfer` 200 at 276 Mpix (4x) and
  156 Mpix (3x); zero texture copies; zero software fallbacks; 100% accelerated draws. The wait
  is `vkWaitSemaphores` with an infinite timeout, so the kernel time is the driver's wait, not
  emulator polling.
- Finding 4, temporary logs at every flush site located the trigger: the guest CPU read path in
  `src/core/memory.cpp` (`Read<u32>`), 4 bytes at the framebuffer base, alternating between
  `0x2040C870` and `0x20452D80`, guest PC `0x004008C0`, LR `0x004008D0`, a loop that reads the
  whole buffer. It was not the texture copy fallback, not the vertex fetch flush, not surface
  validation, and not a CPU write. The read comes 2 ms after the frame's last draw; the next draw
  follows 20 ms later, which is the GPU wait.
- Experiment A, reverted before measurement: a small-CPU-write counter before the
  flush-and-remove branch in `InvalidateRegion`. The branch never fired for this title.
- Experiment B, rejected: a dirty-free-span fast path in `RasterizerCache::FlushRegion` for
  requests of 8 bytes or less. Candidate matrix in the same order as the control matrix:
  2x/100%: 19.9 FPS, 50.6 ms, 7.8%, 23.7/0.3. 2x/300%: 51.9 FPS, 33.7 ms, 22.4%, 65.3/0.3.
  3x/100%: 19.9 FPS, 50.6 ms, 13.2%, 23.7/0.3. 3x/300%: 55.3 FPS, 33.7 ms, 37.5%, 62.0/3.3.
  4x/100%: 19.6 FPS, 84.3 ms, 20.2%, 22.7/22.7. 4x/300%: 30.8 FPS, 50.6 ms, 33.0%, 33.7/31.7.
  The overlay at 4x/300% read Speed 153%, frame 10.9 ms, GSP command time 3.2 ms, remainder
  7.3 ms. Every difference is inside run-to-run noise. The code was reverted.
- Experiment C, settings, informational only: `disable_right_eye_render = true` at 4x/300% on
  the candidate build gave 31.2 FPS, GPU busy 33.8%, thread 36.7/29.3. No change. The title does
  not render a second eye.
- Kept: `LOG_DEBUG` lines at the acceleration refusal points of `AccelerateTextureCopy` and
  `AccelerateDisplayTransfer`. They cost nothing while the `HW.GPU` class stays at Info.
- Tooling added in the same work: `tools/thor-mcp/server.py` with `.mcp.json`, the `/goal`
  command, and the writing standard in `CLAUDE.md`. Two facts about the device that the tools
  encode: a held press is required because the guest samples input once per frame, and a launch
  must use the tree-form content URI or the app crashes with a `SecurityException`.
- Not measured: a battle scene, unplugged battery power, and image correctness beyond the
  captured screenshots. No power or thermal claim is made.
- Build and cleanup: the final production APK of this work is `f0ac9bad4-vanilla-thor` plus the
  debug-log change, 29,179,671 bytes, SHA-256
  `1E7C4752804F0F2ECAE20E0BA24FBA318D37CF22C19F555A93F1B2349DFDB01E`, installed on the Thor at
  the end of the session with the user's config.ini restored byte for byte. The upstream merge
  moved the active `arm64-v8a` RelWithDebInfo CMake hash from `5h1x5ud1` to `1qa67114`. The
  profiling hash `b542p143` and the obsolete `5h1x5ud1` tree were removed after the Gradle daemon
  was stopped: 6,470,640,220 bytes reclaimed. Only `1qa67114` remains under `.cxx/RelWithDebInfo`.

## 2026-09-18 E.X. Troopers First Measurements and the Pause-Menu Freeze

- Setup as on 2026-09-17: production build `f0ac9bad4-vanilla-thor` with the debug-log change,
  Turnip R8, performance mode 2, fan mode 4, brightness 255, USB power. The per-title profile
  forces 2x and a 100% limit for this title. Screen kept on with `svc power stayon usb`.
- Intro video: 30 FPS, Speed 100%, frame 8.2 ms, GPU busy 6.8%, emulation thread about 48% of
  one core. Save-slot and episode screens: 60 FPS, Speed 100%, frame 9.3 ms, GSP command time
  4.7 ms, GPU busy 99.9%, SurfaceFlinger mean 39.4 FPS with intervals of 16.9 ms and 33.7 ms.
  Later videos: 30 FPS locked at 33.7 ms, GPU 7%, thread 40% to 56%. Gameplay: not reached in
  six minutes of videos.
- Freeze: a held START during a video opened the game's pause menu (overlay Speed 96%, 37 FPS),
  then the frame stopped changing. Thread states: all NativeEmulation threads S, VulkanWorker S
  between spins, VulkanPresent S. The log held 19,923 lines of `dequeueBuffer timed out:
  Function not implemented (-38)` from the VulkanWorker thread and nothing else. Reproduced twice.
  A first freeze at the Capcom logo happened after the screen timed out during a launch and a
  MENU key wake brought the secondary-display launcher to the front; that one is a device-state
  case, the pause-menu one is not.
- No emulator change was made for this title yet. No power or thermal claim is made.
- Engine scene reached after about six minutes of intro videos and A presses: 60 FPS, Speed 100%,
  frame 10.1 ms, GSP command time 4.7 ms, remainder 5.2 ms, GPU busy 62.6% at 2x, SurfaceFlinger
  59.3 FPS with 16.9 ms P95. The name-entry screen before it: 49.5 FPS mean with GPU busy 99.9%.
  E.X. Troopers therefore runs at 60 FPS in engine scenes and 30 FPS in videos by design; no
  frame-rate patch applies. The open question for this title is why 2x rendering of a 400x240
  scene costs about 10 ms of Adreno 740 time per frame.
- Frame profiler on the E.X. Troopers save-slot screen (profiling build, 2x, 300-swap window of
  5.21 s): draw batches 241,759 of which 80,404 accelerated and 161,355 software, all software
  fallbacks tagged `gs` (a geometry-shader mode the Vulkan backend does not accelerate);
  render pass begins 221 per swap with 236 image barriers at render pass ends per swap; display
  transfers 2,896 at 1,001 Mpix; validation copies 1,448 at 166 Mpix; presents 598 direct;
  scheduler submits 2 per swap, waits 0.16 ms per swap, no downloads, no finishes. KGSL busy
  99.9% at 43.6 presented FPS. Reading: two thirds of the draws run their vertex stage on the
  CPU, and the render-target switching forces about 440 render passes per frame, which a tiled
  GPU pays for with a full tile store and load each time. Both are large projects: hardware
  geometry-shader acceleration for that mode, and render pass merging across dependent targets.

## 2026-09-18 Medarot 9 Frame Pacing Located and a 30 FPS Code Tested

- Method: a temporary kernel trace logged the guest PC and link register at every blocking wait,
  every address arbitration, and a code dump around each new link register, plus an on-demand
  memory dump driven by a request file. The main thread waits on a light event at `0x00487CA4`
  61 times per second. Its wait wrapper returns to `0x00234690`, which returns into the frame
  function at `0x0040084C` at two sites: `0x004009FC` once per frame and `0x00400A2C` twice.
- The frame function takes the vsync target in `r1`. It reads the vblank count, waits once, then
  waits until the target is reached, and stores the count read before the last wait, so the last
  wait counts as elapsed for the next frame. The caller at `0x003D8268` loads the target with
  `ldrh r1, [r4, #0x4e]` (word `E1D414BE`) from the game object, and the value is 3.
- Code: `D3000000 00000000` then `003D8268 E3A01002` replaces that load with `mov r1, #2`.
  With it enabled the title screen and the intro present at a locked 30 FPS (SurfaceFlinger
  median 33.72 ms, P95 33.72 ms) at Speed 100% with 8.2 ms of work per frame.
- Pacing check: one screenshot per second after the New Game press, mean luminance of the
  dialog-box region. At 20 FPS the bright school scene spans seconds 3 to 8 and the dialog box
  appears at second 9. At 30 FPS the bright scene spans seconds 3 to 5 and the dialog box appears
  at second 7. The game logic advances per frame, so the code runs the game about 1.5 times too
  fast. The code is bundled with that fact in its name and is not a finished 30 FPS patch. A
  finished patch needs the logic step scaled; the frame function stores an elapsed-vblank count at
  object offset `0x104` and a float at `0x108`, which are the first candidates.
- A full dump of `0x00100000` to `0x00500000` was taken for offline analysis.
- Constant check from the code dump: `20.0f` at `0x3DCCE0` is loaded at `0x003DCB9C` as a
  comparison threshold in a float smoothing routine, and `0.05f` at `0x3DD428` is loaded at
  `0x003DD074` to scale a 16-bit size. Neither is the logic step. No `1/30` or `1/60` literal
  exists in the region. The logic step is therefore not a single constant.
- Final state: the kernel trace was reverted before the production build; the cheat file is on the
  device in its disabled bundled form; config.ini was restored byte for byte; the screen-on
  setting was set back to false.
- The production APK installed at the end of this session carries the bundled cheat and no
  diagnostics: 29179911 bytes, SHA-256 `1265C19B9939E9F92D88C96E7FC1876838111AAEDCAB07042E8A0B56A50E28FA`. A sanity launch of Medarot 9 with the cheat disabled
  presented at 20 FPS and the profiling counters were absent.
- Cleanup: the profiling hash `b542p143` was removed after the Gradle daemon was stopped,
  3,225,522,870 bytes reclaimed. Only `1qa67114` remains under `.cxx/RelWithDebInfo`.
- Field patch, same day: the saved registers on the main thread's stack at the vblank wait give
  the frame function's caller `this` as `0x0801DA48`, so the vsync field is `0x0801DA96`. A cheat
  writing 16-bit 2 there, with the original load instruction intact, gave: title 29.9 FPS at a
  33.75 ms median; intro 29.9 FPS at 33.74 ms; overlay FPS 30, Speed 100%, frame 7.8 ms. The
  per-second luminance series after the New Game press was
  `0.0, 2.9, 3.3, 90.0, 90.9, 110.8, 116.1, 105.0, 112.1, 54.5, ...` against the 20 FPS baseline
  `0.0, 3.2, 3.2, 90.0, 89.9, 109.0, 116.3, 108.4, 112.1, 54.3, ...`: the same scene timing. The
  game therefore steps its logic from the same field. The instruction patch is withdrawn and the
  bundled cheats now write the field.
- A launch attempted while the screen was asleep aborted in `EmuWindow_Android` with
  `surface is nullptr` and then `Presentation not supported on this platform`. Recorded as a
  robustness bug; the tools keep the screen on during a session.
- 60 FPS field value (16-bit 1): title and intro at 59.3 FPS with a 16.87 ms median and P95;
  overlay FPS 60, Speed 100%, frame 13.6 ms with 12.4 ms remainder; GPU busy 39.5% at 3x;
  emulation thread 41% of one core. The pacing series matched the 20 FPS baseline again. The
  emulation work per frame is 13.6 ms of a 16.7 ms budget at 60 FPS, so 30 FPS is the setting
  that leaves headroom, and fast-forward does not apply at 60 FPS.
- E.X. Troopers geometry-shader draws, from a one-shot log in the command processor at the
  save-slot screen: every configuration is mode 0 (Point), topology 3 (Shader),
  `input_to_uniform` 0, fixed vertex count 1, stride 1, start index 0, with vertex-shader output
  maps of 8 or 10 attributes and three geometry programs at main offsets 0x2E, 0x3C, and 0x58.
  Draw sizes range from 1 to about 20 vertices. The command processor rejects all of them at
  `PicaCore::ProcessDraw` with the comment "register preservation", before any backend check, so
  the rasterizers' point-mode checks and `SetupGeometryShader` never see a geometry shader. The
  GLSL decompiler logs an error for `EMIT` and `SETEMIT`, and the ARM64 shader JIT compiles them,
  so the software path already runs at JIT speed. A hardware path needs: decompiler support for
  the emit instructions and the emit slot and winding flags, a geometry-shader uniform block, a
  fragment interface driven by the geometry output map instead of the vertex output map, a
  geometry stage in the Vulkan pipeline cache and disk cache, and a guard for programs that read
  registers before writing them. Expected gain: the GSP command time of about 4.7 ms per frame
  and the 520 immediate draws per frame move to the GPU. It does not remove the 440 render pass
  switches per frame, which come from the game's render-to-texture pattern and carry the image
  barriers that stall the GPU on the menu screens.
- Installed at the end of this work: production APK with the field-patch cheats and no
  diagnostics, 29179895 bytes, SHA-256 `2F68B6F0DC3DA8F7FD8BC6D1482FFDD8FFAAD219CF5E62F5EC79CF70A29C460C`. Sanity launch with the cheats disabled presented at
  20 FPS with no diagnostic lines in the log. config.ini restored byte for byte; screen-on
  setting reset to false; performance mode 2, fan mode 4, brightness 255 unchanged.

## 2026-09-18 E.X. Troopers Geometry Expansion, Pass Restart Reasons, and a Device Fault

- Geometry program dump (temporary command processor log, removed): three point-mode programs
  at main offsets 0x2E, 0x3C, and 0x58, shader topology, one vertex per invocation, outputs
  o0 to o2 mapped to position, color, and texcoord0. Program 0x2E is a sprite expander: two
  branches, four quads, no loops, no address registers, uniforms f[80] and f[95]. Programs 0x58
  and 0x3C share code. Their first invocation of a draw stores gs_in[4] to gs_in[7] in registers
  11 to 14 and parameters in registers 10 and 15, then ends; later invocations read those
  registers. That is state kept between invocations.
- Fused vertex shader: the decompiler gained identifier prefixes, a uniform block name, and
  EMIT/SETEMIT translation (`DecompileOptions`); `GenerateGeometryExpandedVertexShader` runs
  the vertex program, feeds the geometry program, and keeps corner `v % 3` of primitive
  `v / 3`; unused host vertices go to clip position (2, 2, 2, 1). Winding follows the software
  assembler: a primitive emit with the winding flag sends (slot 1, slot 0, slot 2).
- Profiler on the save-slot screen at 2x, 300-swap windows, geometry build: draw batches
  193,042; accelerated 182,994 (94.79%); software 10,048, all program 0x58; expanded geometry
  draws 93,773. Control window the same day: 33.49% accelerated. Frame pacing 40.5 FPS mean and
  GPU busy 99.9% in both. Pass restarts per swap unchanged: begins 251, color switch 139.5,
  depth toggle 27.9, area shrink 16.7, area other 27.9. Reading: the draw path is no longer the
  cost on this screen; the tile loads and stores of about 250 passes per swap are.
- Render target switch sequence (temporary log, removed) in the early phase: color surface
  0x180D4800 with depth 0x183FD400 for 14 draws, then the same color without depth for one
  draw, then with depth, then color 0x18070800 without depth for one draw, then back. All are
  256x512 RGBA8 surfaces drawn through 248x400 or 240x400 viewports.
- Device fault timeline. Kernel bursts of `CP: AHB bus error` (details 0x10008e07/0x12144) at
  09:01, 12:11, 12:39, then at 12:44 and at every Vulkan launch after it, one to three seconds
  in, for E.X. Troopers, Medarot 9, and Bravely Default, on candidate builds and on the
  production control (version a8aef3a21). While the app is stalled the errors repeat in groups
  of ten every five seconds; they stop when the app is stopped. After a warm reboot the first
  launch faulted again. OpenGL launched E.X. Troopers through its intro with zero new lines.
  Checked and unchanged: config.ini (412 bytes, equal to the backup), the per-title ini, every
  file under the user directory (nothing modified since 12:38), the extracted Turnip R8 driver
  (SHA-256 fdd378...de09, equal to the zip entry), the redirect directory (empty), the ROM
  (SHA-256 4e5512...c688 twice), temperatures 45 to 55 C, thermal_pwrlevel 0, performance mode
  2 and fan mode 4, GPU debug layers off, no tracing session, both panels on. The performance
  mode only moves `min_pwrlevel` (1 in mode 2, 4 in mode 0); `ifpc`, `force_no_nap`, and the
  other power nodes are root-only. The next test is a full power-off of the handheld, which a
  warm reboot does not replace.
- New MCP tools: `app_maintenance`, `gpu_faults`, `ui_dump`, `ui_tap`. The maintenance channel
  is a request file in the user directory read at app startup; an exported receiver was
  rejected as too wide a surface.
- Cleanup: the temporary logs in the rasterizer are removed; the pass merging switches stay off.
- Device fault, second pass (2026-09-18, 16:30 to 17:30) with the new tools. A full power-off
  did not clear it: zero fault lines at boot, a 20-line burst three seconds into the first
  Vulkan launch, then the stall. Short launches, fault lines added per launch: baseline +30,
  secondary panel off +29, disk shader cache off +40, asynchronous shaders off +20, resolution
  1x +40, hardware shaders off +40 (that one did not stall). Turnip R8 Sysmem +40 and the
  T30 build +40 (both recovered), R8 +30 (stalled). The system Qualcomm Vulkan driver logged
  zero fault lines and still stalled with the GPU idle: the stall and the fault are separate
  symptoms of one cause below the emulator's settings. The app's private data is unchanged
  since 2026-09-15 (`app_maintenance list` now covers shared_prefs, cache, and databases).
  Internal storage has 39 GB free. Clearing the immersive-mode confirmation changed nothing.
  The panels' refresh setting could not be changed from the shell. Kernel view: `hwcg=1`,
  `ifpc=1`, `force_no_nap=1`, `min_pwrlevel=1`, all root-only; the performance mode only moves
  `min_pwrlevel`. Open test: a fifteen-minute cool-down with the app stopped, then one launch.

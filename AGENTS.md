# Agent Notes

- Always work directly on the repository's existing default branch (`master` here, or `main` in repositories that use it). Do not create, switch to, or leave work on any other branch.
- Commit small, coherent, verified slices directly on `master` and push them to `origin/master` frequently. Never include unrelated user files or generated build output just to make a checkpoint.
- Use command-line Git over the repository's SSH remotes for status, fetch, commit, and push operations. Do not use GitHub workflow guides, PR automation, web publishing flows, or the GitHub CLI unless the user explicitly asks for them.
- Keep fork-specific source, patches, tests, and documentation in this repository. Do not create a separate repository or fork for a customized dependency; vendor that dependency here when a normal submodule commit would otherwise require another remote.
- `externals/soundtouch` is intentionally vendored from former submodule commit `9ef8458d8561d9471dd20e9619e3be4cfe564796` so its Thor AArch64 overlap path stays in this repository. Do not restore it to a gitlink; retain the LGPL license and omit unused prebuilt example binaries.
- `externals/cryptopp` is intentionally vendored from former submodule commit
  `8d92d788421483a43e09acf1cd4a2861cb2b8cab` so ARM feature-probe repairs stay in this repository.
  Do not restore it to a gitlink. Crypto++ `try_compile` probes include installed-style
  `<cryptopp/...>` headers and therefore must receive the vendored `include/` directory. Keep
  CRC32 and PMULL in specialized translation units with runtime `HasCRC32()` / `HasPMULL()` gates;
  never enable optional crypto ISA extensions globally. AES and SHA already use their existing
  hardware paths, so do not attribute their performance to the CRC32/PMULL probe repair.
- SoundTouch integer samples require an exact 32-bit `LONG_SAMPLETYPE`; never change it back to C++
  `long`, which is 64-bit under Android's AArch64 LP64 ABI and scalarizes the FIR. The AArch64
  stereo FIR must reuse the canonical coefficient vector for both channels while `LD2`
  deinterleaves samples, preserving the 64 taps, signed accumulation, arithmetic divide-by-16384,
  saturation, generic non-AArch64 coefficient-table path, and exact output. Final linked code should
  retain paired coefficient loads, two sample `LD2`, independent `SMLAL`/`SMLAL2` accumulators, and
  `ADDV` reductions per sixteen taps rather than duplicated coefficient `LD2` or scalar `SMADDL`.
- SoundTouch's integer WSOLA correlation state (`corr`, rolling `lnorm`, and `maxnorm`) is also
  intentionally 32-bit. Do not restore C++ `long`/`unsigned long` on Android LP64. Preserve the
  distinction between the initial paired normalizer shift and the accumulator path's per-sample
  shifted subtraction/addition, including their possible rounding-unit difference. Android
  AArch64 Clang should retain `interleave_count(1)`: final linked code must stay spill-free and use
  two `LD2`, four `SMULL`/`SMLAL`, two shifts, two vector adds, and one loop branch per eight stereo
  frames before the `ADDV` reduction. Re-run the 16/256/1024-frame differential coverage after
  changing the correlation math or compiler hints.
- Preserve the Android AArch64 SoundTouch full-search batch from commit `ac8037b39`. For stereo
  integer NEON builds, `seekBestOverlapPositionFull()` evaluates four adjacent offsets together so
  they share the compare load and contiguous input loads; its four rolling normalizer deltas must
  remain sequential and bit-exact with the scalar search. Keep the current Android/AArch64/NEON,
  non-OpenMP, unaligned-safe, two-channel gates and the ordinary loop for every other build or tail.
  Do not replace full search with `quickseek`, which trades minor audio quality for speed. Retain
  the independent 8/44.1/48 kHz full-search reference test and the five-case, 3,776-assertion ARM64
  SoundTouch gate. Final `calcCrossCorrBatch4` linked size should remain 568 bytes, not the rejected
  3,328-byte general-loop expansion. Same-session profiling reduced correlation self share from
  1.30% to 0.87% and full SoundTouch processing from 1.66% to 1.01%, while a six-versus-six process
  bracket was neutral. Treat this as a recurring-hotspot reduction, not an FPS or battery-watt win.
- Azahar's `TimeStretcher` is a pure-tempo SoundTouch client: pitch and rate remain exact unity, so
  it must enable `SETTING_BYPASS_RATE_TRANSPOSER_AT_UNITY` before any samples enter the pipeline.
  Keep the setting default-off for generic SoundTouch clients, reject explicit mid-stream changes,
  preserve it across `clear()`/`flush()`, and automatically disable it on the first non-unity
  effective rate so dynamic rate/pitch crossover behavior cannot silently change. The bypass must
  report TDStretch-only latency and tail-call `TDStretch::putSamples` without the AA FIR,
  interpolator, or RateTransposer FIFO path. Retain byte-exact 0.72/0.93/1.08 tempo coverage,
  awkward chunk boundaries, flush/clear checks, and the non-unity auto-disable assertion.
- Do not force Android Cubeb output above its 4,000-frame power-saving threshold. On the physical
  Thor, the accepted minimum-latency path created a 32,728-Hz, 1,962-frame AudioTrack at roughly
  118-131 ms reported latency with zero current underruns. A 4,096-frame candidate raised reported
  latency to 271.84 ms and accumulated 989 underruns within roughly one minute. It was fully
  reverted. Any future audio-buffer experiment must be opt-in, prove clean interactive audio, and
  beat the existing path in matched whole-device battery measurements before acceptance.
- Ask the user before making a materially different product, source-policy, or UX choice when the repository and existing requirements do not settle it. Keep moving with safe, reversible assumptions when the choice does not materially change the result.
- The active GitHub fork is `git@github.com:noeldvictor/azahar-thor-experiment.git`; keep fork-facing docs branded as Azahar Thor Experiment, not upstream Azahar.
- Public-facing docs should clearly disclose that this is a personal, AI-assisted/vibe-coded, no-support experiment with no stability guarantee.
- Android work lives under `src/android`; keep cheat-build branding and UI changes scoped there when possible.
- Android guest-memory search is strictly for legally owned offline single-player games. Keep it
  blocked while a room is joining or connected, and never scan or write until the emulation loop
  has acknowledged the pause request. Initial scans are aligned unsigned little-endian 8/16/32-bit
  exact-value searches over mapped process-image/application-heap pages, capped at one million
  candidates; refinements compare against the last paused snapshot. A test write must target a
  surviving candidate, record the original value, verify readback, allow only one pending write,
  and restore only when the current value still equals the verified test value. If the game changes
  that value first, discard the stale restore without writing. Do not generate a persistent cheat
  from a one-session raw address. Initial searches must remain safely cancelable without leaving a
  partial search. Do not add host-process scanning, online use, background scans, raw-address-only
  reusable cheats, DRM/anti-cheat bypasses, or an unverified write path.
- Keep Android guest-memory search beginner-first: expose the labeled `Find value` toolbar action,
  explain that opening Cheats pauses the running game, start the main path as a 32-bit exact search,
  and keep 8/16-bit sizing plus hexadecimal input under `Advanced`. Refinements should describe
  what happened to the visible game number, possible-match lists should not lead with raw addresses,
  and the normal loop must provide an explicit `Back to game` action. Preserve the guarded
  reversible-write and disabled-by-default saved-cheat behavior while simplifying wording.
- Performance work targets AYN Thor Base/Pro/Max: Snapdragon 8 Gen 2, Adreno 740, active cooling, LPDDR5X, and UFS 3.1 storage according to AYN's current product page. The mirrored Thor manual claims UFS 4.0, so do not use storage generation as an optimization premise without verifying the physical device. Do not tune defaults around Thor Lite / Snapdragon 865 unless the user explicitly asks.
- Label Thor CPU-affinity measurements from the device MIDRs, not assumed Linux numbering: CPUs
  0-2 are Cortex-A510 (`0xd46`), CPUs 3-4 are Cortex-A715 (`0xd4d`), CPUs 5-6 are Cortex-A710
  (`0xd47`), and CPU 7 is Cortex-X3 (`0xd4e`).
- The primary engineering goal is higher sustained Azahar performance at lower battery power on AYN Thor. Treat average FPS, frametime distribution, battery power, temperature, thermal slope, visual correctness, and stability as joint acceptance criteria; a short FPS-only improvement is not a win.
- Treat the Thor's device-wide High Performance mode as opt-in headroom, not the default power-test
  condition. On the capped 7th Dragon title, Standard mode reduced the fixed Adreno clock from 615
  to 401 MHz while preserving the exact screenshot, 30 FPS pacing (33.431 ms mean / 34.314 ms P95,
  zero intervals over 50 ms), and essentially the same process CPU ticks as High Performance.
  Prefer Standard for the under-6-W acceptance run unless a title-specific matched check misses
  speed, then test Performance before High Performance. The current comparison was AC-powered, so
  it ranks the device policy but does not prove watts. Always restore the user's original mode after
  experiments, and do not silently make privileged system-setting changes from Azahar.
- Do not extrapolate the capped 30-FPS policy result to demanding 60-FPS titles. At 2x in the Super
  Mario 3D Land attract loop, High Performance/615 MHz measured 59.256 FPS and 20.673 ms P95;
  Performance/550 MHz measured 58.397 FPS and 27.626 ms; Standard/401 MHz measured 57.935 FPS and
  27.460 ms. All had zero intervals over 50 ms and clean audio, but neither lower mode was a free
  speed-preserving swap. Keep Standard first for the <=6-W search, then test Performance and High
  only when the fixed target misses its speed gate. Changing High Performance to Standard on this
  firmware also reset fan mode 4 to 1; always read and restore both settings explicitly rather than
  assuming one vendor toggle leaves the other unchanged.
- Use `tools/measure-thor-power.ps1` for the under-6-W acceptance gate. Its default gate requires
  the production ARM64 package, the accepted config hash, Standard performance mode 0, fan mode 4,
  generic Turnip R8 by exact logged metadata, Wi-Fi ADB, a real discharging battery, and both mean
  and nearest-rank P95 power at or below 6 W.
  Pass the fixed scene's expected screenshot hash and brightness for a publishable run. The script
  must continue to reject Android's simulated/stopped battery state, every dumpsys external-power
  flag, and the Thor's USB, wireless, or UCSI charger-online sysfs flag before/during/post run.
  Never use `dumpsys battery unplug`, AC `power_now`, a debuggable/profiler APK, a mismatched scene,
  or a mean-only result to close the gate. Retain raw CSV, JSON summary, frame hashes, temperature
  range, thermal slope, charge-counter cross-check, process CPU-tick rate, and KGSL busy/clock
  statistics; `thor-power-results/` is intentionally ignored unless evidence is explicitly selected
  for publication. Keep the default anti-idle floors at least 10 process CPU ticks/second and 1%
  mean GPU busy for the fixed 7th Dragon scene. These are loose validity checks, not optimization
  metrics; calibrate and explicitly override them for a materially different title or scene. Also
  retain the start/end SurfaceFlinger gate: exactly two Azahar BLAST layers, at least one layer with
  60 intervals, at least 29 FPS mean, at most 40 ms P95, and zero intervals over 50 ms. Thor's
  secondary-display layer currently exposes no latency history, so layer presence plus the primary
  timestamp stream is the strict evidence available without a profiler APK. Require the same active
  AudioFlinger track before/after as well: 32,728 Hz, at most 2,048 frames, at most 150 ms reported
  latency, and zero underruns. A power/FPS win with audio breakup or a restarted track is a failure.
  The production package must emit the one-time JSON `Active Vulkan driver metadata` log. Do not
  substitute Mesa's runtime banner: generic and forced-Sysmem R8 expose the same banner despite
  measurably different work. The current strict default is package `1e2c106bc-vanilla-thor`, driver
  name `Mesa Turnip driver v26.0.0 - R8`, version `Vulkan 1.4.335`, and library
  `vulkan.ad07xx.so`; explicitly override all affected expectations for another accepted artifact.
  It must also require manual brightness mode and two active physical displays, record each display
  service brightness before warmup, after warmup, and after sampling, and reject panel/state/
  brightness drift. The Thor has separate `panel0-backlight` and `panel1-backlight` sysfs devices,
  but both returned `actual_brightness=0` while visibly ON; do not treat those raw nodes as luminance
  evidence. The live display service reported both panels ON at brightness 1.0 when Android's global
  setting was 255. Full-scale brightness is not a sensible hidden constant near a 6 W total-device
  ceiling: select and record a repeatable lower manual brightness for the unplugged acceptance
  matrix, without silently changing the user's setting during development.
  On this firmware `settings put system screen_brightness` changes only display 0; display 4 stays
  at its prior value, and writing `dual_screen_brightness_level` alone does not actuate it. The
  installed Dual Screen Assistant and the device's own `framework.jar` show that Android 13 hidden
  `DisplayManager.setBrightness(displayId,float)` is the real secondary-panel path. Transaction 35
  was verified as that method on this exact build, and `service call display 35 i32 4 f 0.18503937`
  set display 4 to the same normalized value produced by primary brightness 48 (about 95 nits from
  the device display curve). Re-derive the transaction after firmware changes. A fixed-brightness
  gate must reject a primary-only dimming change; both physical displays must match the normalized
  expected value and be restored after experiments.
- Treat internal resolution as a forest-level GPU-load control before accepting more sub-percent
  source tuning. In the fixed 7th Dragon title scene, changing only `resolution_factor` from 3x to
  2x reduced mean KGSL busy from 8.374% to 5.488% (34.46%) and 1x reduced it to 3.933% (53.04%),
  while both alternatives retained about 29.91 FPS, sub-34.31-ms P95 pacing, zero intervals over
  50 ms, and a clean 1,962-frame AudioTrack. This comparison was AC-powered at a fixed 615 MHz, so
  it proves less GPU work but not fewer watts. Preserve 3x as the accepted configuration and never
  silently change a user's resolution. A publishable resolution/power matrix must keep title,
  scene, build, renderer, driver, performance/fan modes, brightness, and display layout fixed; pass
  the matching config and screenshot hashes to the power tool for each row. The mostly-2D title
  screen does not establish representative 3D quality, so require a stable 3D scene before calling
  2x a general quality/performance balance or 1x acceptable.
- The follow-up Super Mario 3D Land 3D title/attract bracket supplies that representative check.
  Opening/closing 3x controls measured 20.798%/20.856% mean KGSL busy (0.28% spread); 2x measured
  13.119%, 37.01% below their mean, while 1x measured 8.651%, 58.46% below it. The 2x pacing snapshot
  was best at 59.256 FPS / 20.673 ms P95 with no interval over 50 ms; 1x did not improve pacing and
  was visibly softer. Treat 2x as the current efficiency candidate for demanding 60-FPS 3D titles
  and 1x as an aggressive fallback, never an automatic quality default. The attract loop is animated,
  so its non-repeatable screenshot hashes are not suitable for the strict watt gate; select a stable
  gameplay/menu phase before a physical-battery matrix. The user's exact 3x config must remain
  restored after experiments.
- Keep generic Turnip R8 as the accepted driver for the fixed 7th Dragon scene unless a harder,
  matched workload proves otherwise. A live 20-sample bracket at 3x and 615 MHz measured generic
  R8 at 8.022% mean KGSL busy, forced-Sysmem R8 at 9.775% (21.86% more GPU time), and the older
  PurpleVK/T26 build at 8.008% (a 0.18% noise-scale tie). All reproduced the exact accepted frame;
  process CPU activity was also similar. Reject Sysmem for this workload, and do not call PurpleVK
  faster from this tie. Preserve the user's selected driver, restore it after experiments, and
  require a representative 3D/shader-stutter case plus battery evidence before changing the generic
  R8 recommendation. Driver identity is part of every performance/power matrix, not metadata to
  omit because config and screenshot hashes happen to match.
- MrPurple T30 is a compatible manual option on the Thor, not the accepted performance default.
  The official 2026-08-17 `turnip_mrpurple_T30-toasted.adpkg.zip` archive has SHA-256
  `F65B2D3353FD4AA7190BB5426B94468E99FFEA7A58A830BC0C4651DB89353227` and reports PurpleVK
  26.2.99 / Vulkan 1.4.359 on Adreno 740. In three phase-locked 20-second Super Mario 3D Land runs
  after 45-second cold-launch warmups, T30 used 1.440694% more instructions and 2.021561% more CPU
  cycles than generic R8; the instruction ranges did not overlap, while cycles were noisy. Both
  rendered cleanly at the 60-FPS cap. Keep generic R8 active, leave T30 available for manual
  per-title experiments, and never infer that a newer driver is faster without a matched hardware
  bracket. The stock Qualcomm Vulkan driver remains an unranked fallback, not a proven winner; it
  still needs the same title/scene/caches/visual/stability bracket against R8. This AC-powered
  comparison contains no battery-watt evidence.
- Deeply audit x86- and x64-originated code before assuming the ARM64 port is efficient. Check compile-time architecture branches, scalar fallbacks, host feature detection, atomics/spin loops, cache maintenance, SIMD width and lane semantics, Dynarmic A64 codegen, shader/PICA translation, Vulkan synchronization, memory copies/conversions, and thread scheduling. Compare with current RPCS3 and sibling ARM emulator lessons, but port only techniques that match 3DS guest semantics and Azahar's host architecture.
- Prefer runtime-gated AArch64/NEON hardware acceleration and fewer memory passes, barriers, wakeups, and format conversions. Do not enable global Cortex-X3/SVE flags, assume x86 memory ordering, replace PICA floating-point operations with non-equivalent host instructions, or add background worker threads without measured Thor evidence.
- Every ARM64 optimization must have an explicit correctness argument, a native `arm64-v8a` build, and a repeatable Thor A/B plan. Do not claim lower watts or higher sustained speed until the same title, scene, caches, renderer, resolution, driver, performance mode, fan mode, brightness, and display layout have been compared on device.
- After a long micro-optimization tranche, require whole-frame evidence before accepting more
  instruction-path candidates. Use the opt-in Thor profiler only through
  `-PthorFrameProfiling=true`; ordinary builds must compile its counters and timers out. Its
  `ThorFrameProfile` windows cover draw acceleration/fallback reasons, Vulkan submissions and
  waits, render-pass reuse/churn, texture transfer volume and high-level cache-path provenance,
  and presentation copies. A profiling
  APK perturbs timing and is not valid for FPS, power, or thermal A/B claims. Do not count profiler
  instrumentation as an optimization-ledger entry. `RenderPassImageBarriers` counts only barriers
  emitted while ending render passes, not every Vulkan image barrier in the emulator. Install or
  launch on the Thor only when the user explicitly authorizes on-device testing; the 2026-08-19
  direct-presentation sprint has that authorization.
- Treat the forest/trees pivot as an acceptance gate, not just a documentation warning. Without a
  permitted profiler capture, accept new performance code only when static correctness evidence
  proves that it removes a recurring whole-frame operation, memory pass, broad synchronization
  point, driver call, wakeup, or panel/compositor request. Keep merely plausible instruction-level
  ideas in the candidate ledger until a profile ranks their subsystem. Never add isolated speedup
  ratios together or translate them into whole-game FPS or watt estimates.
- Android Eco Turbo caps host presentation/composition to 60 FPS when the active frame limit is
  above 100% or is `0`/unthrottled. With VSync enabled, keep either capped path on FIFO so the
  mobile swapchain supplies back-pressure instead of MAILBOX replacing undisplayed frames.
  Preserve non-FIFO handling when Eco Turbo is off, VSync is off, or the display is classified as
  low refresh. Present-mode selection occurs during swapchain creation, so do not claim every
  hotkey-Turbo session changed modes without proving that the swapchain was recreated.
- A skipped-presentation path may use `Scheduler::FlushIfPending()` only when it supplies no signal
  or wait semaphore and `CommandChunk::Empty()` proves that no GPU command was recorded. Any real
  emulation work must still submit; presentation signaling, explicit readback/finish waits, and
  resource-retirement safety remain unchanged. Track elided submissions with
  `SchedulerEmptyFlushesSkipped` in opt-in Thor frame-profiler builds, while ordinary builds must
  compile the counter/logging path out.
- Duplicate-frame suppression must be decided before preparing guest display render targets. An
  explicit screenshot always overrides suppression. Preserve the existing video-dump duplicate
  policy, defer right-eye skipped-state consumption until a frame is actually prepared, and keep
  the inner mailbox/window duplicate guard as a correctness backstop. Android OpenGL's no-output
  exit must still poll the secondary window, close swap timing, run `EndFrame()` (including primary
  event polling and the frame limiter), and call `rasterizer.TickFrame()` without switching to the
  presentation GL state. Vulkan's suppressed path must still call `FlushIfPending()` so recorded
  emulation commands submit. Track Vulkan's early preparation elision with
  `DuplicateFramePreparationsSkipped` in opt-in Thor profiler builds; normal builds must compile the
  counter and profiler strings out.
- `Memory::PageTable` raw entries must remain actual host page-base pointers on AArch64, identical
  to the pointers exposed to normal C++ memory callers and rebuilt from savestates. Do not set
  Dynarmic's `absolute_offset_page_table` or restore guest-base-adjusted entries based only on
  arithmetic tests or shorter generated code. That experiment caused reproducible non-fastmem JIT
  faults while booting Art Academy and 7th Dragon on the Thor. Any future page-table representation
  change requires real A32 JIT memory coverage plus multi-title on-device boot/render validation;
  a successful compile and pointer round-trip test are not sufficient.
- Preserve the redundant Dynarmic page-table reload guard from commit `10cb11ad7`. `SetPageTable`
  may return early only when a live JIT already targets the identical shared `PageTable` object;
  initial JIT construction and every actual table change must retain the established context save,
  JIT lookup/creation, and context restore. On the steady 7th Dragon title, three matched 15-second
  Simpleperf runs reduced mean task-clock 2.622%, cycles 2.656%, and retired instructions 5.350%,
  while the symbol's sampled share fell from 2.57% to 0.45%. Preserve the byte-identical 7th Dragon
  and Art Academy screenshots and multi-title no-fault device checks.
- Preserve the scheduler ownership-churn guard from commit `6b3c1b6d8`. The process and page-table
  setters take `shared_ptr` by const reference and refresh stored ownership only when the pointee
  actually changes. Do not skip the existing memory-page-table, live-JIT, CPU, or timing handoff,
  and do not remove the `owner_process.lock()` lifetime acquisition. On the steady 7th Dragon
  title, three matched whole-app runs retired 3.134% fewer instructions while task-clock and cycles
  remained within noise; `SetRunningCPU` fell from 1.07% to 0.73% of sampled cycles and
  `SetCurrentProcess` fell from 0.90% to below the 0.20% reporting floor. This is a measured
  recurring-work reduction, not a demonstrated FPS or watt reduction. Retain the exact 7th Dragon
  and Art Academy screenshot/no-fatal checks after changing this handoff.
- Preserve the process-lifetime Android ANGLE query cache from commit `d75d854d4`.
  `GraphicsUtil.openGLRendererString` is fixed until app restart, so `GetWorkingGraphicsAPI()` must
  not cross JNI again on every resolution-scale lookup. Preserve the same ANGLE-to-Vulkan override
  for every graphics setting; cache only the Java boolean, not mutable settings. A matched
  whole-app A/B was neutral within 0.13%, so this is an eliminated per-draw/per-cache boundary call,
  not an FPS or watt claim. The baseline call tree entered Java CheckJNI below
  `GetResolutionScaleFactor`; the cached build's tree was self-only after startup, with exact 7th
  Dragon and Art Academy hashes and no fatal logs.
- Preserve the Android detailed-frame-timing gate from commit `65e2f5a9f`. Normal FPS, emulation
  speed, and system frametime reporting must remain available, but the high-frequency SVC, IPC,
  GPU, and swap `steady_clock` scopes should run only while the detailed frametime overlay is
  requested. Keep per-scope active state so a live setting change cannot mismatch nested starts and
  ends, and refresh the native gate both during overlay updates and `EmulationFragment.onResume()`.
  A same-binary three-by-three 7th Dragon A/B retired 1.091% fewer instructions with the breakdown
  hidden; task-clock and cycles changed -0.214% and -0.278%. `__kernel_clock_gettime` self share
  fell from 1.11% in the prior profile to 0.29%, and `steady_clock::now` fell from 0.86% to 0.02%.
  Treat this as measured recurring-work removal, not an FPS or battery-watt claim.
- Preserve the redundant Android performance-overlay redraw guard from commit `2b02f5cf5`. The
  one-second updater must continue sampling and formatting enabled statistics, but it should call
  `TextView.setText()` only when the formatted text actually changes. Apply the configured overlay
  background when the overlay is enabled or its settings are refreshed, not on every timer tick.
  Retain live enable/disable, FPS changes, background changes, and detailed-stat gating. In an exact
  7th Dragon six-versus-six bracket this reduced mean task-clock 3.573%, CPU cycles 3.425%, and
  retired instructions 1.865% at only 0.176% higher sampled frequency; the overlay-triggered
  `ViewRootImpl.doTraversal` call tree fell from 1.40% to 0.19%. Preserve the exact frame and visible
  `FPS: 30` checks. This is recurring Android UI-work removal, not a battery-watt claim while the
  Thor remains AC-powered.
- Preserve the Vulkan Choreographer-wakeup elision from commit `b62eb36f2`. Android must post one
  frame callback on fragment resume and keep it alive while the native renderer window is still
  being constructed. Once running, the base/Vulkan window reports that it does not require another
  Java display-vsync callback; only `EmuWindow_Android_OpenGL` opts in and continues through
  `TryPresenting()`/`eglSwapBuffers`. Do not make Vulkan repost itself or let OpenGL lose its callback.
  Retain real Vulkan launch/HOME/resume and temporary-restored OpenGL presentation checks. A matched
  six-versus-six 7th Dragon bracket reduced mean whole-process CPU cycles 9.409% and retired
  instructions 5.077% at only 0.009% higher sampled frequency; the Vulkan profile no longer
  contained Azahar's `EmulationFragment.doFrame` or JNI `doFrame` chain. This is recurring CPU and
  display-scheduling work removal, not a battery-watt claim while the Thor remains AC-powered.
- Do not add an atomic fast path around `System::signal_mutex` based on the 2026-08-20 profile.
  Caller attribution found that the ordinary no-signal lock was only about 0.05% of whole-app
  sampled cycles; changing asynchronous reset/save/load/shutdown timing for that cost misses the
  forest. Reconsider only with stronger normal-build evidence and explicit concurrency coverage.
- Do not restore the rejected `dirty_regions.empty()` early return in `RasterizerCache::FlushRegion`
  without new attribution. A three-run 7th Dragon total looked about 0.5% lower, but the function's
  own sampled share rose from 1.03% to 1.12%; the dirty map was not the ranked self-cost and the
  apparent total was noise. Do not instead guard the final `dirty_regions -= flushed_intervals` on
  `flushed_intervals.empty()`: instruction-level attribution identified Boost erase machinery, but
  a zero-lost-sample candidate profile left `FlushRegion` self share effectively unchanged at 0.89%
  versus 0.91% control. Required dirty-surface downloads and interval ownership remain intact.
- Do not remove `ProcessNormalCommandBatch()`'s command-list bounds check or coalesce its recurring
  `delay_generator.AddCommands(1)` calls based only on smaller AArch64 code. The candidate reduced
  the linked helper from 236 to 208 bytes and passed all 21,008 focused PICA assertions, but an exact
  7th Dragon six-versus-six bracket regressed mean process CPU cycles by 0.559% and retired
  instructions by 0.206% (median regressions 0.467% and 0.504%). `ProcessCmdList` sampling moved
  only from 0.73% to 0.71% self, inside noise. Keep the per-command delay updates and defensive
  bounds condition unless a differently shaped implementation wins a matched whole-app bracket.
- Do not add a maintained 64-bit nonempty-priority mask to `ThreadQueueList` based on the 2026-08-20
  scheduler profile. Although it replaced historical-empty-queue scans with AArch64 `RBIT`/`CLZ`,
  its enqueue/dequeue maintenance increased mean 7th Dragon task-clock 1.051%, cycles 1.156%, and
  instructions 1.836%; `PopNextReadyThread` also rose from 0.65% to 0.73% of sampled cycles. The
  implementation, tests, and object-layout change were reverted. Reconsider only with a different
  representation and matched evidence that reduces both the target and whole-app work.
- Do not move the thread-wakeup `UnscheduleEvent` scan from `ThreadManager::SwitchContext` into
  `Thread::ResumeFromWait` based on the 2026-08-20 profile. The candidate passed all 85 assertions
  in the four ARM64 `CoreTiming` tests and reduced `UnscheduleEvent` from 0.33% to 0.08% of sampled
  cycles, but it merely moved cost: `ResumeFromWait` rose from 0.14% to 0.24% and
  `ThreadWakeupCallback` from 0.16% to 0.22%. Mean 7th Dragon task-clock, cycles, and retired
  instructions also regressed 0.510%, 0.532%, and 0.883%. The implementation was fully reverted;
  retain cancellation when a ready thread is selected unless new whole-app evidence proves a net
  reduction without changing early-wakeup or timeout semantics.
- Do not delete `ready_queue.remove()` from `ThreadManager::SwitchContext` unconditionally. The
  selected thread is normally popped by `PopNextReadyThread`, but its no-better-thread branch
  returns the current running thread without popping it. `SwitchContext` then requeues that thread
  before loading its context, so it must remove the self-selected thread again. The unconditional
  deletion passed 439,504 of 439,505 assertions in the broad ARM64 `[core]` suite (the sole failure
  was the Android harness-only missing `get_build_flavor` function) yet crashed 7th Dragon within
  one second with `Thread must be ready to become running`. Preserve the self-switch removal even
  if ordinary cross-thread switches are given a narrower fast path.
- Preserve the scheduler self-switch fast path from commit `7d31114d6`. `Reschedule()` must call
  `PopNextReadyThread()` first so core-1 CPU limiting and runnable selection still occur, but when
  its result is the exact current thread there is no context handoff: return before `SwitchContext`
  instead of saving/loading the same CPU state, requeueing/removing the same thread, cancelling its
  nonexistent wakeup timeout, and repeating process/TLS setup. Do not weaken the equality check or
  skip real thread/null transitions. Three matched 7th Dragon samples reduced mean task-clock,
  cycles, and retired instructions 1.188%, 1.143%, and 0.833% at only 0.101% higher sampled
  frequency; `SwitchContext` inclusive profile share fell from 1.75% to 1.32% and
  `UnscheduleEvent` self share from 0.33% to 0.16%. Preserve exact 7th Dragon and Art Academy
  screenshot/no-fatal gates after scheduler changes. This is recurring-work evidence, not a watt
  claim while the Thor remains AC-powered.
- Dynarmic A32 keeps guest NZCV in reserved callee-saved `W23`. `A32SetCpsrNZCV` must load its IR
  argument directly into `X23` through `ReadIntoFixedRegister()` so a flags value becomes one
  `MRS X23, NZCV`, not `MRS Xtemp, NZCV` plus `MOV W23, Wtemp`. Fixed-register reads may target
  only registers excluded from the active allocator order; preserve use accounting, flag spilling,
  callback/state synchronization, and the linked-block arithmetic-NZCV regression test.
- Dynarmic ARM64 read/write operands may inherit the read value's physical register only for a
  non-immediate value of the same host-register class with exactly one remaining IR use, exactly
  one active lock, and no prior realization of the output. `ReplaceLastUseWith()` transfers the
  location metadata to the output, and the `RAReg` lifetime must unlock that new value without
  clearing its reused location. Preserve the allocate-and-copy fallback for every other case and
  retain the A32 VTBX read/write regression on a real ARM64 host.
- Preserve the move-free ARM64 Dynarmic lowerings built on that final-use contract:
  `Pack2x32To1x64` must reuse the low operand and insert the high word with `BFI`,
  `LeastSignificantWord` must remain a zero-code low-word alias through `DefineAsExisting()`, and
  `PackedSelect` must reuse the final-use GE mask as `BSL`'s destination. Keep the real A32 `UMLAL`
  packed-word regression and the A32 `SEL` regression over all 16 GE masks; shared values must
  continue through the allocator's conservative copy fallback.
- Preserve ARM64 signed-narrow fusion only when `LeastSignificantByte`/`LeastSignificantHalf` has
  exactly one use and the immediately following IR instruction is the matching word/long signed
  extension. In that case the narrow value aliases its source and `SXTB`/`SXTH` performs both jobs.
  Keep `UXTB`/`UXTH` for zero extensions, shared/non-adjacent values, exclusive stores, ordinary
  stores outside the separately documented exact-width gate, and every unrecognized consumer; only
  the separately documented sole-consumer shift-count and ordinary-store fusions may bypass it.
  Retain the real A32 `SXTB`, `SXTH`, `SMULBB`, and dirty-high-byte `LSL` regression so an over-broad
  alias cannot silently corrupt shift semantics.
- ARM64 Dynarmic may omit the second `AND #0xff` in no-carry A32 `LogicalShiftLeft32`,
  `LogicalShiftRight32`, and `ArithmeticShiftRight32` only when the shift argument resolves through
  identities to a materialized `LeastSignificantByte`. A byte with exactly one eventual consumer
  may instead alias its raw source only when that consumer takes it as argument 1 of 32-bit LSL,
  LSR, or ROR. For no-carry LSL/LSR, preserve `TST #0xe0` plus EQ selection: AArch64 consumes only
  bits 4:0, while bits 7:5 distinguish the A32 0..31 range from 32..255. ROR may use the raw source
  directly because both architectures rotate by the low five bits; its carry path must retain the
  low-byte zero test. Do not extend this alias to ASR: its raw-count clamp regressed on A715, so ASR
  retains `UXTB` and the established canonical path. Preserve generic U8 masks and the complete
  carry lowerings. Keep real guest coverage for dirty-upper-bit amounts 0, 1, 31, 32, 33, and 255
  across no-flags and carry-producing LSL/LSR/ASR/ROR.
- A32 scalar NEON long multiply must broadcast its selected 16-bit or 32-bit source lane with
  `VectorBroadcastElement()` before `VectorMultiplySignedWiden()` or
  `VectorMultiplyUnsignedWiden()`. Do not restore the x86-shaped
  `VectorGetElement()` plus `VectorBroadcast()` pair: on ARM64 it lowers to an element-to-GPR
  `UMOV` followed by a GPR-to-SIMD `DUP`, while the direct form is one element `DUP`. Retain real
  guest `VMULL.S16`, `VMLAL.U16`, `VMLSL.S32`, and `VMULL.U32` coverage with distinct lane indices,
  signed extremes, accumulator wrapping, and complete 64-bit results.
- A32 D-register `VZIP.8`/`VZIP.16` must keep the two halves of `VectorInterleaveLower()` in SIMD:
  write the lower half with `SetVector()` and rotate the upper half down by 64 bits for its
  `SetVector()`. Do not restore `VectorGetElement(64)` plus `SetExtendedRegister()`: the ARM64
  backend turns each half into `UMOV` to a GPR followed by `FMOV` back to a D register. Preserve
  low/high D-register encoding coverage, both legal element sizes, and the existing Q-form path.
- A32/A64 `VABDL` and `VABAL` must express the widening absolute difference with
  `VectorSignedAbsoluteDifferenceWiden()` or `VectorUnsignedAbsoluteDifferenceWiden()` before any
  accumulation. The ARM64 backend must lower those IR operations directly to `SABDL`/`UABDL` on
  the selected 64-bit source half. Do not restore the A32 `VectorGetElement(64)` ->
  `ZeroExtendToQuad()` -> `VectorZeroExtend()` chain or the A64 pair of pre-extensions; they turn a
  native one-instruction operation into cross-register-bank transfers and separate widening.
  Keep the x64 polyfill and signed/unsigned 8/16/32-bit guest coverage, including signed extremes
  and widened-lane accumulator wraparound.
- A32/A64 `VADDL`/`VADDW` and `VSUBL`/`VSUBW` must preserve their widening or wide operation in
  `VectorSignedAddSubWiden()`/`VectorUnsignedAddSubWiden()` or
  `VectorSignedAddSubWide()`/`VectorUnsignedAddSubWide()`. The ARM64 backend must lower these
  directly to `SADDL`/`UADDL`/`SSUBL`/`USUBL` or `SADDW`/`UADDW`/`SSUBW`/`USUBW` on the selected
  64-bit source half. Do not restore frontend `VectorSignExtend()`/`VectorZeroExtend()` plus generic
  `VectorAdd()`/`VectorSub()` sequences: those expand native long forms from one host instruction
  to three and wide forms to two. Keep the x64 polyfill and A32 signed/unsigned long/wide tests,
  including signed extremes and modular destination-lane wraparound.
- A32/A64 vector and by-element `VMLAL`/`VMLSL` or `SMLAL`/`UMLAL`/`SMLSL`/`UMLSL` must retain
  `VectorSignedMultiplyAccumulateWiden()`/`VectorUnsignedMultiplyAccumulateWiden()` through IR.
  The ARM64 backend must consume the accumulator with `ReadWriteQ()` and emit the matching native
  long multiply-accumulate/subtract. Do not split this back into widening multiply plus generic
  add/sub: that doubles recurring host instructions and measured 5.017x slower on Cortex-A510.
  Eight-independent-chain timing was otherwise tied within 0.6% on A710/A715/X3, so describe this
  as an exact-path instruction/efficiency win rather than an emulator-wide speedup. Preserve the
  x64 polyfill, direct SIMD lane broadcast before the fused operation, signed/unsigned 8/16/32-bit
  semantics, modular accumulator wraparound, and the A32 full-vector plus scalar-lane tests.
- A32 `SHASX`/`SHSAX`/`UHASX`/`UHSAX` mixed halving operations must keep the ARM64 backend's
  `REV32` plus native `SHADD`/`SHSUB` or `UHADD`/`UHSUB` and element-to-element lane insert. Do not
  restore the widening/sign-mask/shift/narrow sequence: that expands each recurring guest operation
  from four host instructions to nine and measured 2.316x-2.506x slower on the tested Thor
  A510/A715/A710 cores. Preserve signed floor rounding, unsigned underflow, both ASX/SAX lane
  arrangements, and the permanent A32 edge-case test. This is a hot-path result, not a whole-game
  FPS or watt claim.
- A32 `SASX`/`SSAX`/`UASX`/`USAX` mixed wrapping operations must keep the ARM64 backend's `REV32`,
  narrow `ADD`/`SUB`, and element-to-element low-lane insert. When GE is live, preserve signed GE
  through `SHADD`/`SHSUB` plus `CMGE`, unsigned addition carry through `CMHI`, and unsigned
  subtraction no-borrow through `UHSUB` plus `CMGE`; when GE is dead, retain the four-instruction
  result-only path. Do not restore the extension/extract/sign-mask/narrow sequence: it uses 10
  signed or 11 unsigned instructions instead of eight and measured 1.024x-1.334x slower across
  tested Thor A510/A715/A710/X3 cores. Do not substitute the rejected seven-instruction widening
  candidate: its final `XTN` lengthened the dependency chain and regressed tested A715/A710 cores
  by 5.4%-10.9%. Preserve both ASX/SAX layouts, signed non-negative GE, unsigned carry/no-borrow,
  NZCV/Q, and the permanent multi-edge A32 test. Keep this result path-local until a matched game
  and power A/B exists.
- A32 `QASX`/`QSAX`/`UQASX`/`UQSAX` must remain packed through
  `PackedSaturatedAddSubU16/S16` or `PackedSaturatedSubAddU16/S16`. The ARM64 backend must spill
  lazy host FPSR state before using `SQADD`/`SQSUB` or `UQADD`/`UQSUB`, exchange the second source
  with `REV32`, and insert only the alternate low lane. Do not restore scalar halfword extraction,
  extension, two generic saturation clamps, and repacking: that expands the recurring path from
  four host instructions to 21 and measured 1.11x-2.14x slower on tested A510/A715/A710 cores.
  Preserve signed/unsigned saturation, ASX/SAX lane placement, unchanged guest NZCV/Q/GE flags,
  the x64 SSE4.1/SSE2 lowering, and the permanent A32 edge-case test. Treat this as a path-local
  result until a matched game/power A/B exists.
- For local Android builds, use JDK 17 and the Android SDK from `src/android`.
- The Android APK target for this repo is the AYN Thor, so keep `abiFilter` set to `arm64-v8a` only. Do not build x86_64 unless the user explicitly asks for it.
- When building an APK to send to the AYN Thor, use `.\gradlew.bat :app:assembleVanillaRelWithDebInfoLite` and install `app/build/outputs/apk/vanilla/relWithDebInfoLite/app-vanilla-relWithDebInfoLite.apk`. This is release-optimized, debug-signed, uses the `-thor` version suffix, and keeps the `.debug` package so it installs over the Thor test app without the debug/JNI-debug performance hit.
- Keep the Android wrapper on upstream Gradle 8.14.5 and `targetSdk = 37` unless a later upstream
  merge changes them. SDK 37 edge-to-edge handling depends on a black emulation root, the named
  `coordinator_layout`, and one null-safe display-cutout margin listener attached to both the
  coordinator and in-game menu. Do not restore the deleted `values-v35` opt-out theme or cast a
  possibly null `layoutParams` to a non-null margin layout. Rebuild the full Thor APK after changing
  target SDK, wrapper, window-inset, theme, or emulation-layout code.
- Use `:app:assembleVanillaDebug` only when an actual debuggable APK is needed.
- Before pushing Android changes, verify at least `:app:compileVanillaDebugKotlin`; prefer a full `:app:assembleVanillaRelWithDebInfoLite` when native code, packaging, or Thor installs are involved.
- Vulkan Anime4K is a real three-stage filter: copy the unscaled source to an independent image, generate the RG16F X gradient, generate the R16F Y/luma gradient, then refine into the scaled surface. Never bind the destination surface as one of its sampled inputs. Preserve explicit transfer, color-attachment, and fragment-read dependencies and compare a fixed frame against the OpenGL path on Adreno after changing this code.
- Texture-filter output already persists in the owning rasterizer surface's scaled GPU image until
  guest writes invalidate or re-upload that region; do not add a disk cache for these transient GPU
  surfaces without measured Thor evidence that hashing, storage I/O, synchronization, and VRAM
  duplication are a net power win. Screen-filter Anime4K is a separate final-presentation pass and
  normally runs for each presented frame. Any Android per-game cache manager must key persistent
  data by title ID, report sizes, separate Vulkan/OpenGL shader and future preprocessed-texture
  caches from texture dumps, and never label or delete the user's custom/downloaded texture pack as
  disposable cache. The current long-press **Manage Cached Data** dialog accounts for the exact
  per-title files removed by the existing Vulkan/OpenGL shader-cache actions, performs filesystem
  size queries off the UI thread, and requires explicit confirmation before either deletion. Keep
  size accounting and deletion coverage aligned when cache paths change.
- Vulkan guest-texture and final-presentation samplers deliberately keep anisotropy disabled with
  `maxAnisotropy = 1.0f`. PICA exposes nearest/linear and mip filtering but no anisotropy control,
  and the OpenGL path does not add it. Preserve exact guest filter semantics and deterministic
  nearest presentation; do not restore device-maximum anisotropy without an explicit user setting,
  visual validation, and a matched Thor performance/power A/B. The Vulkan device feature may stay
  enabled for future controlled uses.
- Large AArch64 `Common::FindMinMax()` index scans deliberately switch at 128 bytes to four
  independent minimum and four independent maximum accumulators over each 64-byte batch. Preserve
  exact unsigned `u8`/`u16` extrema, the one-vector and scalar tails, empty-input sentinels, and the
  unchanged non-AArch64 paths. Do not lower the crossover to one batch: its extra vector setup and
  tree reduction nearly consume the first 64-byte saving. Final ThinLTO should retain two Q-form
  `LDP`, eight `UMIN`/`UMAX`, and five address/control instructions per repeated band with no
  spills. Keep prefix-reference coverage across 127/128/129 bytes and equivalent halfword counts.
- The indexed PICA CPU-fallback vertex cache is a fully associative 64-entry circular-replacement
  cache. On AArch64, scan each complete sixteen-ID band with two Q loads, two halfword compares,
  `XTN`/`XTN2` or equivalent `UZP1` narrowing, one lane-select/`ORN` equivalent, and one `UMINV`,
  then retain the scalar tail and non-AArch64 path.
  Preserve first-match behavior if duplicate IDs occur, the `[0, vertex_cache_count)` valid-prefix
  invariant before the cache fills, hit behavior that does not advance replacement state, and the
  existing circular replacement order after 64 misses. Keep exhaustive count/value differential
  coverage and inspect final ThinLTO before treating the source shape as a performance result.
- AArch64 PICA output-vertex construction may use the exact-six handler only when
  `vs_output_total & 7` equals six. Select it before CPU vertex submission, rewire the geometry and
  geometry-shader handlers only when that exact-six mode changes, and keep counts 0-5 and 7 on the
  established generic constructor. The specialization must still initialize all 32 overflow-map
  slots to `f24::One()`, apply all four components of outputs 0-5 in order, copy only the 96-byte
  visible `OutputVertex`, and preserve absolute-value/saturating color clamping. Do not restore the
  rejected direct-write, per-vertex count switch, or seven-output specialization: exact Thor
  measurements regressed at least one intended shape/core. Retain randomized byte-exact coverage
  for every count and all mapping slots, inspect final ThinLTO for six unconditional mappings with
  no count ladder, and keep the measured result path-local until a matched title/power A/B exists.
- PICA vertex attribute format and component count are loader-construction invariants. Keep the
  compact `VertexLoaderUtils::AttributeLoader` predecode outside `LoadVertex()` so an uncached
  vertex does not repeat the format switch or runtime component/default loops. Preserve all 16
  BYTE/UBYTE/SHORT/FLOAT x one-to-four-component shapes, the invalid/retention path, default
  attributes, source addresses/strides, and missing `(0,0,0,1)` components. On AArch64, BYTE4 must
  sign-widen twice before `SCVTF`, UBYTE4 must zero-widen twice before `UCVTF`, SHORT4 must
  sign-widen once before `SCVTF`, and FLOAT4 may use an exact 16-byte copy. Retain byte-for-byte
  randomized coverage, final linked-code inspection, and every-accessible-core Thor timing. Do not
  convert the 1.23x-3.02x attribute-kernel result into whole-game FPS or watts: hardware vertex
  shaders and vertex-cache hits can bypass this work.
- PICA physical attribute translation is also fixed for one `VertexLoader`/draw lifetime. Resolve
  each non-default attribute's base plus loader offset once in the constructor and keep the direct
  backing pointer; do not restore per-uncached-vertex `MemorySystem::GetPhysicalPointer()` calls.
  Keep one pointer per attribute so separate loader offsets remain distinct, preserve strides and
  default attributes, and map a failed constructor lookup to the established invalid/retention
  route before any pointer arithmetic. Do not retain these pointers beyond the loader lifetime.
  Permanent coverage must use the real `MemorySystem`, combine multiple formats with a default
  attribute, and prove that guest writes after construction remain visible. Inspect final ThinLTO
  for direct pointer-plus-stride math and no physical lookup inside `LoadVertex()`. The measured
  1.95x-3.38x address/load loop result is path-local, not whole-game FPS or watts.
- A `MemorySystem` physical span may borrow the current backing only for immediate, bounded hot-path
  access that cannot outlive or replace that backing. Keep `MemoryRef` wherever retained ownership
  is required; never store the borrowed span across a draw, reset, remap, or backing change. Check
  its remaining size before use, and retain permanent base/offset/one-past-end coverage. Vulkan
  vertex setup uses this narrow route to avoid copying a `shared_ptr` for each loader. Cortex-A510
  guide page 51 identifies acquire/release atomics as multicycle issue entries that suppress
  co-issue until their last cycle, but the manual is only candidate guidance. Two alternating,
  fixed-time Super Mario 3D Land traces per APK twin must remain the acceptance evidence: aggregate
  `SetupVertexArray` cycles fell 22.94%, and its share of `AccelerateDrawBatch` fell from 21.56% to
  16.38% while parent work rose 1.42%. Keep that result path-local; it is not an FPS or watt claim.
- Do not combine Vulkan vertex and fixed/default-attribute uploads into one `StreamBuffer`
  reservation merely to remove a `Map()`/`Commit()` pair. The exact implementation preserved the
  contiguous bytes, offsets, bindings, and watch lifetime and reduced aggregate `Map`/`Commit`
  cycles by 18.64%/15.27%, yet three alternating Super Mario 3D Land traces measured a 2.41%
  aggregate `SetupVertexArray()` regression; its process-normalized share rose 3.55%. Keep the two
  reservations until a materially different implementation beats the complete recurring path, not
  just its helper-call count.
- Do not restore the rejected one-entry Vulkan texture-descriptor cache merely to reduce Turnip
  descriptor-update/bind calls. Three alternating Super Mario 3D Land pairs reduced those driver
  leaves 38.53%/15.74% and the combined direct-cost share 23.16%, but complete
  `AccelerateDrawBatch()` work moved only -0.37% raw while total sampled work rose 0.70%, and
  `SyncTextureUnits()` inclusive share rose 3.79%. Reconsider only with a simpler ownership design
  or a matched title/scene showing a material complete-path or frame-level win; exact image-view,
  sampler, surface-generation, cube-shape, and GPU-lifetime guards remain mandatory.
- Do not enable the blending subset of `VK_EXT_extended_dynamic_state3` merely because Turnip R8
  advertises it. The physical Thor confirmed all four required logic-op-enable, blend-enable,
  blend-equation, and color-write-mask features and rendered the exact Super Mario 3D Land loop
  correctly, but a profiling-off control/candidate bracket left Turnip's `tu_CmdBindPipeline` self
  share effectively unchanged at 1.25297% versus 1.25021% (0.22% relative, noise scale). Azahar's
  own `PipelineCache::BindPipeline` self share rose 9.17% relative while the animated scene's
  descriptor shares also varied upward. The candidate was fully reverted. Reconsider only when a
  ranked title proves frequent blend-only pipeline-key churn and a matched bracket shows less
  recurring driver work; extension availability is not optimization evidence.
- Do not bypass the Vulkan graphics-pipeline map lookup from a remembered pipeline/state match.
  The exact shortcut passed focused static/dynamic-state tests but crashed the physical Thor's
  Turnip worker in `tu_cmd_render<chip7>` with a null dereference. Also do not retain the safer
  consecutive disk-bookkeeping cache merely to skip `PipelineInfo::Hash()` and the known-pipeline
  set lookup. It preserved the required optimized map lookup and passed 53 assertions in six
  physical-device Vulkan cases, but only 153,847 of 300,000 live Super Mario 3D Land queries
  (51.28%) repeated the prior static state. `ShaderDiskCache::GetPipeline()` remained below 1% of
  process work, while control/candidate trace variance was larger than the entire function. Keep
  the direct hash/set route until a ranked title proves materially higher repetition and a matched
  complete-path bracket beats the comparison/cache overhead without changing pipeline lifetime.
- Keep the rasterizer cache's last framebuffer-surface selection guarded by both the active color/
  depth `SurfaceParams` and `surface_generation`. Compare resolution scale explicitly because
  `SurfaceParams::operator==` intentionally omits it. Advance the generation for every registered
  surface-set change, slot replacement, and surface scale change; if validation changes the
  generation, do not publish the selection found before validation. A cache hit may skip only
  `GetSurfaceSubRect()`/page-table selection: it must still reacquire the surface pointers and
  levels, mark render-target use, call `ValidateSurface()`, construct/lookup the backend
  framebuffer, and return the normal RAII helper so draw-region invalidation still occurs. Retain
  focused tests for generation, active/inactive attachments, parameter changes, resolution scale,
  and invalid entries. On the exact Turnip/Adreno 740 Super Mario 3D Land bracket, the aggregate
  `GetFramebufferSurfaces()` share fell from 2.16438% to 1.49514% of sampled user cycles (30.92%
  relative), while `GetSurfaceSubRect()` fell from 1.19870% to 0.47212% (60.61% relative) and
  validation stayed neutral. Keep this as path-local CPU-work evidence, not an FPS or watt claim.
- Preserve the four-entry aligned texture-surface selection cache from commit `1e2c106bc`. It may
  reuse a surface ID only when the complete `SurfaceParams`, explicit resolution scale, and
  `surface_generation` match. The explicit scale comparison is required because
  `SurfaceParams::operator==` intentionally omits it. A hit skips only the recurring
  `GetSurface()` page-table/search route: it must still call `ValidateSurface()` over the requested
  interval before returning. Cache only a successful normal aligned lookup, keep circular
  replacement, and leave the odd-size temporary-surface/mipmap route unchanged. Registration,
  unregistration, slot replacement, cache clear, and scale-up must continue advancing the shared
  generation so stale IDs cannot match. Retain focused tests for valid, invalid, changed-generation,
  changed-parameter, and scale-only cases. On the exact Turnip/Adreno 740 Super Mario 3D Land
  four-run bracket, `SyncTextureUnits()` fell from 1.48519% to 1.20368% of sampled user cycles
  (18.95% relative), `GetTextureSurface()` fell from 0.62207% to 0.47089% (24.30%), and the aligned
  `GetSurface()` subtree fell from 0.80157% to 0.59479% (25.80%). Validation remained within scene
  variation. Keep this as path-local CPU-work evidence, not an additive FPS or watt claim.
- The vertex shader's packed 64-bit attribute-to-input-register map and active attribute count are
  also invariant for a CPU-fallback draw. Keep `ShaderInputMap` constructed once in
  `PicaCore::LoadVertices()` and make each recurring `ShaderUnit::LoadInput()` mask and shift its
  local GPR copy. Do not restore per-attribute `ShaderRegs::GetRegisterForAttribute()` calls or a
  byte-array predecode: the packed form removes repeated config loads without a setup threshold.
  Preserve ascending attribute order, duplicate-register last-write behavior, attributes 8-15 in
  the high map word, and untouched-register state. Retain deterministic differential coverage for
  counts 1-16 and duplicate/random maps, final ThinLTO inspection, and A510/A715/A710 full-draw
  timing. Config-driven immediate-mode, point-geometry, and debug paths must likewise read the two
  contiguous map words once per call and shift a local `u64`; keep the portable high/low-word
  fallback for non-little-endian hosts. Preserve the one-attribute direct copy, the guaranteed
  1-16 count invariant, and the no-stack/no-PLT final AArch64 form. Do not route this overload
  through a separately linked `ShaderInputMap` constructor or restore the redundant zero-count
  branch. Retain independent scalar-reference coverage for both overloads and the exact config-
  path Thor gate. Do not add the rejected output-register byte-map loop; its one-output and A510
  cases regressed.
- AArch64 `ShaderSetup::WriteUniformBoolReg()` must keep the 16 guest boolean registers as exact
  byte values `0` or `1`. Duplicate the input register's low and high bytes into the two vector
  halves, mask them with `1,2,4,8,16,32,64,128`, normalize with compare/not, compare the resulting
  16 bytes against the old vector, reduce the XOR with `UMAXV`, and perform one full 16-byte store.
  Preserve dirty semantics: a changed byte sets `uniforms_dirty`, an identical rewrite does not,
  and an already-dirty state remains dirty. Keep the scalar non-AArch64 fallback and the exhaustive
  0-65535 permanent test. Final ThinLTO should retain the 84-byte/21-instruction straight-line
  AArch64 body. Do not replace this with the rejected packed-float24 `TBL` experiment: despite exact
  random equality, it ran at about 0.52x-0.54x on A510. Treat the 1.03x-1.83x measured gain as
  boolean-uniform-path work, not whole-game FPS or watts.
- AArch64 `ShaderSetup::WriteUniformFloatRegRange()` may batch only complete float32 groups when the
  packed queue starts empty. Reverse each four-word transfer with `REV64` plus `EXT #8`, skip old-
  value loads when `uniforms_dirty` is already set, and otherwise aggregate vector XOR results for
  one final `UMAXV`. Preserve the final packed queue buffer because save states can observe it.
  Partial queues, float24 writes, scalar tails, out-of-range writes, and non-AArch64 hosts must keep
  the scalar `WriteUniformFloatReg()` route. Retain differential tests across both formats, boundary
  indices, every partial queue prefix, dirty/clean state, special float bit patterns, and identical
  rewrites. Do not restore the rejected grouped-float24 candidate: its small-batch A510 timing was
  unstable or regressive. Treat the 1.15x-14.84x measurements as uploader-kernel ratios, not whole-
  game FPS or watts.
- AArch64 PICA lighting, fog, and procedural-texture LUT batches may skip old-value loads and use
  circular contiguous copies only when the selected table is already dirty and the remaining batch
  has at least seven words. Clean uploads must retain exact compare-and-dirty semantics and the
  compiler's existing vectorizable loop; shorter dirty batches keep that same route because copy
  setup did not clear the all-core gate. Select the procedural-texture table once outside the word
  loop. Preserve circular wrap and repeated-overwrite order, unchanged-clean false dirty state,
  already-dirty absorption, table sizes 128/256, offsets beyond one wrap, and the scalar non-AArch64
  path. Retain deterministic boundary coverage, randomized differential coverage, final ThinLTO
  inspection, and A510/A715/A710 measurements. Treat the 1.04x-10.97x accepted dirty-copy range as
  LUT-upload-path work, not whole-game FPS or watts.
- Indexed PICA CPU-fallback vertex-cache entries contain the packed prefix written by
  `ShaderUnit::WriteOutput()`. When its output-mask popcount exactly equals the rasterizer or
  geometry-pipeline consumer count, select one direct-cache loop before the draw: a hit must pass
  the cache entry straight to synchronous `GeometryPipeline::SubmitVertex()`, and a miss must write
  output straight into the circular replacement entry before submitting that same entry. This is
  valid for exact matches 0-16 only while every geometry backend and no-GS handler consumes or
  copies the reference synchronously. Keep non-indexed draws and every count mismatch on the
  original full 256-byte state-propagation path with its eight paired Q loads/stores and no
  recurring output-count branch. Do not restore entry 135's intermediate bounded copy or expose a
  stale suffix to a wider consumer. Preserve the all-count direct-gate/suffix-canary test, linked
  hit/miss inspection, and accessible-core Thor measurements. The accepted hit/miss kernel ranges
  of 1.01x-3.06x/1.11x-2.26x are path-local, not whole-game FPS or battery watts.
- The AArch64 PICA command-list fast path may consume four pairs only after vector preflight proves every header has an in-range ordinary register ID, zero extra-data length, and no special handler. Preserve ordered scalar writes for duplicate/nonconsecutive IDs, the compact partial/special fallback, exact byte masks, command-delay counts, and dirty-bit behavior.
- The AArch64 PICA `EX2` helper keeps its eight exact float words in one aligned two-Q-register
  block. Preserve their lane mapping, keep `EX2` in the `needs_one` analysis set, and retain the
  polynomial's multiplication/addition order, NaN behavior, and input clamps when changing its
  paired-load lowering. Keep range reduction as scalar `FCVTNS`, lane-to-GPR `MOV`, then scalar
  `SCVTF`. `FRINTN` plus `FCVTZS` and direct GPR-destination `FCVTNS` were correct in focused tests
  but repeatably regressed A710 by about 2.9% and 20%, respectively.
- The AArch64 PICA `LG2` positive-input helper similarly keeps its five exact coefficient words in
  one aligned two-Q-register block. Its unbiased exponent is signed: convert the 32-bit GPR directly
  with scalar `SCVTF`, never unsigned `UCVTF` or a GPR-to-vector move first. Preserve its
  `SRC2`/`VSCRATCH2` lane map, Horner order, and special-value result vectors. Classify the scalar
  input with `FCMP input,#0.0`, then `B.VS` for NaN before `B.LE` for signed zero, negative finite
  values, and negative infinity. Do not restore the old SIMD-mask/GPR sequence or add a separate
  self-compare: the three-instruction classifier was faster on every Thor core class, while the
  two-compare alternative regressed A715 and X3. Keep NaN, both signed zeros, both infinities,
  negative inputs, and powers of two across negative and positive exponents covered on ARM64.
- Keep AArch64 PICA `RCP` on exact scalar `FDIV`: a hardware estimate plus one or two Newton steps
  measured slower on every Thor core class. `RSQ` deliberately uses one scalar `FRSQRTE`, squares
  that estimate, applies `FRSQRTS` with the original input, then performs the final `FMUL`. Do not
  replace the squared-estimate operand with a precomputed `input * estimate`; that changes the
  architecture's infinity-times-zero special handling. Preserve zero, infinity, negative, and NaN
  behavior, keep one refinement only (two were slower than exact everywhere), and retain dense
  positive-normal exponent coverage on real ARM64.
- AArch64 PICA `DP3` must retain sanitized four-lane multiplication but reduce only X/Y/Z. Form the
  X+Y pair in a scratch scalar while broadcasting Z independently, then perform one scalar `FADD`
  and final lane broadcast. Preserve x64's `(X + Y) + Z` grouping, ignore W even when it is NaN,
  and do not reassociate or fuse the operations. Do not restore the W-zero insertion followed by
  two dependent pairwise reductions; the shorter dependency graph measured 16.7-26.0% faster on
  Thor core classes. Keep interpreter/JIT W-NaN and broadcast-result coverage.
- AArch64 PICA `DP4`, `DPH`, and `DPHI` must reduce their sanitized four-lane product with two
  same-source Q-form `FADDP` instructions. The first produces `[X+Y, Z+W, X+Y, Z+W]`; the second
  computes the same ordered `(X+Y)+(Z+W)` result in every lane. Do not restore the scalar second
  `FADDP` plus `DUP`: it adds one recurring host instruction and measured 1.37x-1.57x slower in
  exact independent/dependent Thor kernels. Preserve DPH/DPHI's forced source-one W value, x64's
  arithmetic grouping, sanitized zero/infinity multiplication, swizzles, destination masks, and
  full-lane output coverage. Treat the measured gain as path-local until a matched game/power A/B.
- AArch64 PICA `MOVA` consumes only X/Y. Keep its truncating conversion on D-form `.2S` `FCVTZS`,
  then choose extraction by the destination mask. X-only and Y-only must use one signed element
  transfer (`SMOV Xd, Vn.S[lane]`), which combines the SIMD-to-GPR move and sign extension. XY must
  retain one packed low-64-bit transfer followed by `SXTW`/`ASR`: two `SMOV`s measured 26.1-97.5%
  slower on the Thor's A710/A715/X3 cores. Do not widen the conversion back to Q-form or write
  disabled address/loop registers. Preserve negative truncation, partial masks, ignored exceptional
  Z/W inputs, and initial-state behavior; keep explicit X-only, Y-only, and XY interpreter/JIT
  coverage. D-form conversion measured essentially twice the Q-form throughput on every core class,
  while partial-mask `SMOV` removed one more instruction and won or tied on all four classes.
- The AArch64 PICA `CMP` helper combines X/Y only when both lanes use the same operation. Preserve
  the ordered `FCMEQ`/`FCMGT`/`FCMGE` masks, inverted-equality implementation of `NotEqual` so NaN
  remains unordered/true, sign-bit extraction for lanes zero and one, and the unchanged scalar path
  for mixed operators. Keep all six operators covered against the interpreter on real ARM64.
- AArch64 PICA conditional flow relies on `COND0`/`COND1` remaining canonical zero/one values from
  byte loads, bit extraction, or `CSET`. Keep `Compile_EvaluateCondition()` to one flag-setting
  instruction and return the condition code that means guest-true: OR uses `CMN/NE`, `TST/EQ`, or
  `CMP/GE/LE`; AND uses `TST/NE`, `CMN/EQ`, or `CMP/GT/LT`; JustX/JustY use `CMP/EQ`. IFC and CALLC
  branch on the inverse, while BREAKC and JMPC branch on the returned condition. Do not restore
  scratch-register boolean inversion/materialization or assume every result is EQ/NE. Preserve all
  sixteen truth-table combinations and permanent IFC/CALLC/JMPC/BREAKC interpreter/JIT coverage.
- The AArch64 PICA source-swizzle planner must preserve exact four-lane selector composition. Its
  26 primitive `EXT`/`REV64`/`ZIP`/`UZP`/`TRN`/`DUP`/lane-move operations cover exactly one
  identity, 26 one-operation, and 122 two-operation selectors; the remaining 107 selectors retain
  the literal `LDR` plus `TBL` fallback. Keep the compile-time all-256 mapping proof and the
  permanent `All Source Swizzles` generated-shader test. Do not claim this affects draws that
  successfully use hardware vertex shaders; it targets immediate, geometry, and software-fallback
  shader invocations.
- AArch64 PICA partial destination stores must keep disabled components untouched without loading
  and blending the old vector. Preserve paired X/Y and Z/W lane groups, the zero-write empty mask,
  and full-mask `STR Q`. For `x`/`xy`, store low `S`/`D` directly with an immediate `STR`; for
  `xz`/`xw`/`xyw`/`xzw`, use that direct first store and form only the remaining group's address.
  Keep contiguous `xyz` on its post-indexed two-store path and retain `ST1` for every nonzero source
  lane. Do not generalize scalar `STR` to lanes one through three. Preserve both output banks and
  far output/temporary offsets in the permanent destination-mask test. Exact affected-path timing
  ranged from 0.997x-5.90x on A510, 0.996x-1.002x on A715, 0.998x-1.208x on A710, and
  0.997x-1.003x on X3; near-one results are ties, not whole-game gains.
- AArch64 PICA `EX2`/`LG2` calls made while a guest `CALL` return is live in `X30` must preserve that
  guest link in reserved `X16` around the local math-helper `BL`, then restore `X30` and return
  architecturally through `X30`. Keep the helper target passed by reference: Oaknut attaches an
  unresolved branch writeback to that exact `Label` object before the helper is bound. Ordinary
  math calls outside guest subroutines stay as one direct `BL`; keep the established root and guest
  stack layout unchanged. The local math helpers must not grow an ABI/external call while `X16` is
  live unless the guest link is explicitly preserved. Do not return through `X16`/`X17` or compact
  the guest root frame: exact A510 measurements rejected both designs. Retain nested-CALL plus
  `EX2`/`LG2` coverage, all-core shader runs, and exact-path alternating-order measurements.
- The AArch64 PICA program/swizzle range updater scans eight words per first-stage NEON block and combines both comparison masks before its unchanged `UMAXV`. Preserve the all-equal `UINT32_MAX` sentinel, exact highest-changed-lane result (including low lane zero and high lane four), paired stores only after a detected change, the four-word tail, scalar remainder, dirty flags, and biggest-range accounting.
- The AArch64 ETC1/ETC1A4 block decoder maps selector and negation bit `4 * x + y` into two
  row-major eight-pixel AdvSIMD bands. Preserve horizontal `x / 2` versus flipped `y / 2`
  subblocks, table selection, signed modifiers, exact `[0,255]` saturation, column-major ETC1A4
  alpha nibble order, RGBA byte order, arbitrary signed output stride, and the unchanged scalar
  non-AArch64 path. Final ThinLTO should retain vector `USHL`, `SQXTUN`, `ZIP`, four Q stores, and
  one `TBL`/`SLI` alpha expansion for ETC1A4 rather than regressing to a 16-pixel scalar loop.
- AArch64 converted RGB5A1, RGB565, and RGBA4 texture copies deliberately process sixteen pixels
  per linear loop or two Morton rows per tile loop. Preserve exact 5/6/4/1-bit replication on
  decode, high-bit truncation on encode, bottom-up Morton row placement, padded row strides, and
  the scalar non-AArch64 path. Full-tile decode should retain `LD2`, vector shifts/masks,
  vector narrowing, `ZIP`, and ordinary paired Q stores; do not replace its output with `ST4`.
  Encode must share byte-level channel masks and packing across both eight-pixel halves. Linear
  encode uses one Q-form `LD4` per sixteen pixels, `SHLL`/`SHLL2`, and paired Q stores; do not
  split it back into two D-form `LD4` operations. Morton encode retains two D-form `LD4` loads
  because its rows are non-contiguous, but combines their components before shared preparation and
  retains `ST2` for the Morton rows. Keep the exhaustive 65,536-value round-trip and odd
  linear-length/canary coverage, and recheck final ThinLTO instead of assuming the intrinsics
  survived.
- AArch64 IA8, RG8, I8, A8, and IA4 Morton expansion must combine each two-row band with `ZIP`
  and ordinary paired Q stores; do not reintroduce D-form byte `ST4`. Native RGB8 and D24
  two-row Morton copies may retain structured `LD3` for deinterleaving, but packed output must use
  the exact two-`TBL2` row shuffle plus ordinary Q/D stores rather than D-form byte `ST3`. Preserve
  component order, bottom-up rows, padded stride, both swizzle directions, the scalar non-AArch64
  path, and the compile-time 24-byte shuffle proof. Final ThinLTO must be checked because
  Cortex-A510 documents these D-form byte stores at only `1/25` (`ST4`) and `1/17` (`ST3`).
- Converted linear RGB8 copies process sixteen pixels per AArch64 vector body. Decode must preserve
  packed BGR to RGBA order and opaque alpha while using one exact 48-byte `LD3` plus register ZIPs,
  not four-register `TBL`. Encode must preserve RGBA to packed BGR order with three overlapping
  adjacent-input `TBL2` operations whose compile-time indices stay below 32; do not widen them back
  to `TBL3`/`TBL4`. Retain exact buffer-bound alignment, the scalar tail, the non-AArch64 path, the
  37-pixel vector/tail/canary test, and final ThinLTO inspection.
- Converted D24 Morton tiles must process sixteen depths per AArch64 two-row band while preserving
  little-endian 24-bit assembly, bottom-up rows, padded strides, exact `UCVTF`/`FDIV` decode, exact
  `FMUL`/`FCVTZU` encode truncation, and the scalar non-AArch64 path. Keep D-form `LD3`, one-table
  Morton shuffles, `ZIP`/`UZP`/narrowing, and ordinary packed stores; do not introduce the
  Cortex-A510-hostile four-table `TBL`, reciprocal approximations, per-pixel scalar work, or hot-loop
  spills. Retain edge/pattern depth coverage and byte-exact canaries, and recheck final ThinLTO.
- Vulkan D24S8 staging unpack deliberately handles sixteen packed S8D24 pixels per AArch64 band.
  Load the complete 64-byte band before overwriting its in-place depth plane, preserve the trailing
  contiguous stencil plane, exact integer D24 shift, exact D32 `UCVTF`/`FDIV`, scalar tail, zero
  length, and five-bytes-per-pixel contract. Final ThinLTO should retain ordinary paired Q loads,
  four `USHR`, three `UZP1`, paired Q depth stores, and one Q stencil store; do not replace this
  with `LD4`, a table constant, approximate reciprocal math, or per-pixel scalar work. Keep the
  15/16/17 and 31/32/33 boundaries, depth edges, both modes, and canary coverage.
- Raster fill downloads deliberately materialize repeating two-, three-, and four-byte patterns
  with a phase-preserving prefix, one seed, and exponentially growing non-overlapping `memcpy`
  calls; all-equal patterns use one `memset`. Do not restore the per-pattern tiny-copy loop or its
  backup/restore writes. Preserve arbitrary start/end offsets, bytes outside the requested range,
  and the source pattern. `CanFill()` must keep its at-most-16-byte compatibility probe on the
  stack rather than allocating. Retain exhaustive phase/length and large-range canary coverage,
  and verify final ThinLTO leaves each renderer's `DownloadFillSurface()` with one `FillMemory()`
  call rather than an inlined tiny-copy loop.
- Vulkan `CommandChunk::Empty()` must derive emptiness from its linked-list head: successful first record sets `first`, and `ExecuteAll()` destroys every command before clearing it. Do not add a separate stale counter. Preserve the scheduler's queue-before-execution lock order and shared-condition-variable `notify_all` behavior; they prevent worker/waiter races.
- Routine Vulkan timeline progress polling is deliberately limited to every fourth submitted tick, matching the command-buffer pool depth. Preserve immediate `Refresh()` calls for explicit waits and exhausted resource pools, monotonic cached completion, and conservative garbage-collection behavior; stale-low progress may delay reuse/deletion but must never permit unfinished GPU resources to be reused or destroyed.
- `ResourcePool::CommitResource()` must pass the current completion snapshot explicitly into every
  search. Search both the hinted tail and wrapped prefix with cached monotonic `KnownGpuTick()`
  before calling `Refresh()`; cached progress may be stale-low, but any object it marks complete is
  already safe. Only a complete cached miss may query the driver. After `Refresh()`, both ranges
  must use the newly loaded completion value; never capture the pre-refresh value by copy in the
  search closure. Preserve first-free circular order within each snapshot and grow only after both
  refreshed ranges fail. A false miss allocates four more Vulkan command buffers or another
  64-descriptor-set batch, so retain forward, wrapped, cached-zero-refresh, refresh-count,
  allocation-count, and full `[video_core]` coverage.
- Vulkan `current_tick` and `gpu_tick` are numerical sequence/completion caches, not memory-publication primitives. Keep their loads, increment, and monotonic `AdvanceGpuTick()` compare/exchange relaxed unless new side data is explicitly published through a tick; Vulkan submission/completion and the existing queue/fence mutexes provide the required ordering. Query the timeline-semaphore driver counter once per `Refresh()` and fold it into the cache with atomic max so a CAS retry never repeats the driver call or regresses known completion.
- A Vulkan frame that skips host presentation must still submit pending emulation commands with
  `Scheduler::Flush()`, but it must not call `Finish()` or otherwise wait for GPU completion.
  Command-buffer/descriptor reuse, stream-buffer wrap, and deferred destruction already gate on
  completed timeline ticks. Keep synchronous `Finish()` at explicit CPU readbacks, render-frame
  recreation, presentation-window destruction, and renderer teardown where the host actually needs
  completed work or is about to destroy its backing resources.
- Native Vulkan presentation must use `Scheduler::FlushWithDynamicSubmission()` so final
  composition and any required swapchain transfer stay in one scheduler command buffer and one
  graphics-queue submission. The intermediate fallback's worker-side prepare callback must acquire
  and capture the exact swapchain image plus its binary semaphores, record the transfer and both
  post-transfer image transitions, and wait for `image_acquired` at `Transfer`. Android's exact-size
  direct route acquires before recording composition and instead waits at `ColorAttachmentOutput`.
  Only after `vkQueueSubmit` may the post-submit
  callback enqueue the frame while holding `queue_mutex`; release that predicate mutex before
  notifying the present thread. Do not restore a separate presentation command pool/buffer, second
  `vkQueueSubmit`, `render_ready` handoff, or `present_done` fence wait/reset. Keep the worker drain
  for LibRetro cache ticks and the synchronous presentation fallback. Deferred
  rasterizer-cache destruction is safe only when the runtime completion tick is strictly newer
  than the sentenced resource tick; equality must retain the resource because that tick can still
  be queued or in flight. Any value read by a worker callback while the producer may begin the next
  frame, such as the presentation clear color, must be captured by value.
- `vkAcquireNextImageKHR` must never wait indefinitely while holding `swapchain_mutex`: the present
  thread needs that same host-synchronization lock to return previously submitted images. Use a
  finite timeout, treat `VK_TIMEOUT`/`VK_NOT_READY` as retryable, release the lock between retries,
  and never abandon a successfully acquired/suboptimal image with a signaled binary semaphore.
  Acquire semaphores are frame-owned; publish the scheduler submission tick before queueing the
  frame, and wait for that tick before signaling the same binary semaphore again. Preserve
  out-of-date/surface-lost recreation and do not turn an ordinary timeout into an abort.
- Android Vulkan may render final composition directly into an acquired swapchain image only when
  the frame and current swapchain extents match exactly, the swapchain is valid, and one finite
  acquire succeeds immediately while holding `swapchain_mutex`. `Retry`, `Recreate`, an extent
  mismatch, or incomplete direct framebuffer state must fall back to the intermediate image; never
  add a direct-path acquire retry loop before composition. Wait for the acquired image at
  `ColorAttachmentOutput`. Keep the direct render pass `Undefined` on entry and `PresentSrcKHR` on
  exit with an explicit external-to-color-output dependency, and preserve the frame-owned acquire-
  semaphore submission-tick reuse rule. Direct image views and framebuffers are Android-only
  swapchain-lifecycle resources: destroy them before recreation, rebuild them only for a valid new
  image set, and clear stale swapchain images/count on destruction. Keep the fallback framebuffer,
  render pass, copy/blit route, and restore-only recovery intact. Count successful direct renders
  only through the opt-in `PresentDirectRenders` Thor profiler event.
- Preserve the upstream Android surface-lifetime guards integrated in merge `411e559ba` alongside
  the Thor presentation superpath. `surface_mutex` must cover native-window replacement,
  renderer/window construction, shutdown, and both primary/secondary destruction callbacks;
  `System::Init()` must hold that recursive lock and wait for a live primary surface. Repeated
  notifications for the same native window must not create another Vulkan surface, and an
  unconsumed `next_surface` must be destroyed before replacement or `PresentWindow` teardown.
  This fork has no presentation `command_pool`, `render_ready`, or `present_done`; do not restore
  those removed objects while porting upstream cleanup. Keep the NDK motion factory/device lifetime
  mutexes and sensor-queue mutex so pause/resume cannot race polling or destruction.
- Preserve the entry-152 device acceptance result. On the animated 7th Dragon III title at the
  exact 1920x1080 layout, the old profiler build reported `presented=300`, `copies=300`, and
  `511.920` presentation MPix per steady 5.014-second window. The direct build reported
  `direct=300`, `copies=0`, and the same presented pixels, while old, profiled-direct, and normal-
  direct screenshots had the identical SHA-256
  `E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C`. A profiler route-count
  win and a pixel-identical frame prove removal and correctness, not FPS or battery watts.
- Treat Android `PresentFrames` as a combined all-window counter, not per-panel FPS. A post-entry-160
  Thor audit reproduced two steady 5.014-second 7th Dragon windows with `swaps=300`,
  `presented=300`, `direct=300`, and `duplicate_prepare_skipped=150`. Live Android state showed two
  active physical displays (IDs 0 and 4) at 60 Hz and two Azahar `SurfaceView`/BLAST pairs. The
  Vulkan path presents each of the 150 new 30-FPS guest frames to both the main and secondary
  windows before clearing `game_frames_updated`; the combined 300 presents are therefore required
  dual-panel output, not an unskipped duplicate. Do not suppress one window or divide the renderer
  cadence based on the combined count. Re-evaluate only with per-window counters, both-panel pixel
  checks, and an exact live display/surface inventory.
- Preserve the follow-up cache-path attribution from profiler commit `ca82a5fc3`. In the same
  steady 7th Dragon window, all 150 texture copies / 129.600 MPix came from accelerated guest PICA
  texture-copy commands, all 300 blits / 233.280 MPix came from accelerated guest PICA display-
  transfer commands, and surface-validation copies/blits were zero. These are guest-visible
  framebuffer operations, not another host presentation layer. Do not remove or alias them without
  exact PICA memory/coherency reasoning, multi-title pixel/state tests, and a new Thor capture.
- Preserve the entry-161 dirty-region semantic no-op guard. A nonzero-owner invalidation may skip
  `DirtyRegionMap::set()` only when Boost ICL proves that the complete invalidated interval is
  already mapped to that exact owner. Wrong-owner, extending, partially covered, and mixed-owner
  intervals must update normally, and owner-zero invalidation must retain the established erase.
  Keep the map helper independently testable and retain exact-range, contained-subrange,
  wrong-owner, extension, and mixed-owner coverage. Profiler-only invalidation/update/elision
  counters must compile out of normal builds. On the Thor's Super Mario 3D Land attract loop,
  steady five-second windows elided 96.64%-98.58% of candidate same-owner rewrites; a scene-matched
  cycle profile reduced `InvalidateRegion` inclusive share from 2.08% to 1.35% and removed the
  control's large allocation/free branches. This proves recurring CPU work removal, not watts.
- Vulkan presentation frames use the swapchain's exact format. When the intermediate frame and
  acquired swapchain image also have identical extents, retain the direct `vkCmdCopyImage` route:
  it preserves every pixel bit-for-bit and avoids asking the transfer path to perform a filtered
  blit with a 1:1 mapping. Keep `vkCmdBlitImage` for actual extent scaling when the destination
  supports blitting, and keep the established overlapping copy fallback when it does not. Do not
  infer equal extents from Android alone; select the route from the acquired swapchain extent.
- Vulkan presentation's combined submission must make final color-attachment writes visible to
  the transfer read, transition the acquired image from `Undefined` to `TransferDstOptimal`, and
  wait for `image_acquired` only at `Transfer`. After copy/blit, restore the intermediate image from
  `TransferSrcOptimal` to `General` with `TransferRead` to `ColorAttachmentWrite`, and transition
  the acquired image to `PresentSrcKHR` with no destination access. The next render pass starts in
  `General`; same-queue order plus that post-copy barrier owns intermediate-image reuse. Preserve
  the restore-only path when acquisition/recreation fails, serialize queue submit/present with
  `submit_mutex`, and retain queue-idle synchronization for swapchain/surface recreation. Never
  restore the former `AllCommands` scopes, `render_ready` binary handoff, or host fence reuse wait.
- Vulkan `Surface::BlitScale()` must use the surface's `PipelineStageFlags()` and `AccessFlags()` as
  the producer/consumer scopes around the transfer stage; the Base and Scaled images share the same
  surface-use flags. Do not restore `AllCommands` or generic memory read/write scopes around these
  resolution-scale blits. Preserve the D24S8 unsupported-hardware fallback, format-selected filter,
  image layouts, mip/layer ranges, and the upload, download, and `ScaleUp()` call semantics. The
  opt-in `TextureScaleBlits` and `TextureScaleBlitPixels` profile fields must remain compiled out of
  ordinary builds with the rest of `THOR_FRAME_PROFILING`.
- HLE audio intermediate mixes deliberately use `PlanarQuadFrame32` from `Source::MixInto()` through
  aux exchange and final downmix. Preserve channel-major live storage, contiguous whole-buffer aux
  copies on little-endian hosts, the endian-converting fallback, and the historical sample-major
  `QuadFrame32` save-state archive representation. Do not reintroduce `LD4`/`ST4` transposes without
  final ThinLTO inspection across the X3/A715/A710/A510 manuals; Cortex-A510 documents Q-form
  32-bit `ST4` throughput as `1/50`.
- AArch64 HLE source gain mixing deliberately handles eight stereo samples per band with ordinary
  paired Q loads plus `UZP`, widens signed 16-bit samples, converts to float, multiplies by the
  exact gain, truncates with `FCVTZS`, and adds to the four planar `s32` buses. Keep the ramp-active
  choice outside the sample loop and preserve `float(sample) * (1 / 159)` plus fused
  `start + (end - start) * progress`, the post-frame ramp state, and the scalar non-AArch64 path.
  Do not replace the source loads with structured `LD2`/`LD4`. Keep steady, ramped, disabled,
  signed-16 edge, existing-destination, and canary coverage; final ThinLTO should retain the
  eight-sample NEON loop without a per-sample ramp branch or per-iteration vector spill/reload
  traffic. After fusing all three buses, one entry/exit `d8`/`d9` callee-save pair is the measured
  trade for two removed calls.
- `Source::MixInto()` deliberately handles all three intermediate buses in one frame-level call.
  Preserve one caller invocation per source, the single disabled-source state transition, and
  silent-bus elision only when every ending gain is exact signed zero and either no ramp is active
  or every starting gain is exact signed zero. Any nonzero gain or NaN must take the arithmetic
  path, and nonzero-to-zero/zero-to-nonzero ramps must still mix. On AArch64, final ThinLTO should
  retain one Q `FCMEQ`/`UMINV` predicate per checked gain vector rather than four scalar compares.
  Keep three-bus, signed-zero, zero-to-zero ramp, nonzero-ramp, disabled-state, and canary coverage.
- An active AArch64 HLE source bus may use the front-stereo specialization only when both ending
  rear gains are exact signed zero and, during a ramp, both starting rear gains are also exact
  signed zero. Preserve the integer `AND`/`TST #0x7fffffff7fffffff` predicate: any nonzero bit
  pattern after removing the two sign bits, including a subnormal, infinity, or NaN, must use the
  full four-channel path. The accumulating front path must not load or write rear destinations;
  the first-definition front path must clear both rear planes once so the complete bus is defined.
  Accumulating full steady/ramped loops must remain 52/74 instructions per eight samples, with
  front loops at 32/46. Direct full loops should remain 38/60, and direct front loops 26/40 with
  no destination load or vector add. Keep each nested `std::array` pointer within its own array
  object instead of relying on cross-subarray pointer arithmetic. Recheck final ThinLTO and the
  front/rear destination canaries after edits.
- `GenerateCurrentFrame()` deliberately leaves the complete three-bus set pending instead of
  clearing all 7,680 bytes up front. Until one source is audible, use `MixIntoFirst()` to direct-
  write every bus that source routes; then clear its adjacent silent-bus runs and return every later
  source to the original `MixInto()` accumulation path. Do not carry per-bus initialization checks
  through later sources: their recurring control work can exceed the one-time direct-write saving.
  The all-silent case must remain one contiguous 7,680-byte clear. Preserve exact signed-zero/NaN
  predicates and advance each gain ramp exactly once. Keep first steady/ramped full/front,
  multi-bus, all-silent, disabled-state, existing-destination, and canary coverage. Final ThinLTO
  must keep the 1,244-byte spill-free accumulator and direct loops without destination loads/adds.
- The final HLE mixer skips a 160-sample downmix only when that bus's frame-wide mixer volume
  compares equal to exact signed zero; every nonzero or NaN volume retains the arithmetic path.
  The first audible bus, including an auxiliary bus after leading signed-zero buses, must define
  `current_frame` directly from its already-clamped contribution. Later audible buses retain the
  original per-bus clamp followed by saturating accumulation, and an all-silent frame must clear
  the complete output even after an audible prior frame. Preserve aux exchange semantics, saved
  intermediate buffers, Surround's Stereo behavior, and exact multiply/FMA/conversion order.
  Keep `MixCurrentFrame()` `CITRA_NO_INLINE`: final ThinLTO must keep the full mixer out of
  `Mixers::Tick()`, which is currently 136 bytes after native aux-return routing. The common
  first-bus AArch64 Stereo/Mono loops
  should remain 36/35 instructions per eight samples with Q-form `ST2` and no output `LD2` or
  `SQADD`; later accumulated paths deliberately retain their output `LD2` and two `SQADD` at 38/36
  instructions. Keep multiple-bus saturation, signed-zero, first-audible-aux, and silent-after-
  audible Mono/Stereo regression coverage.
- Final HLE mixing must consume main and disabled auxiliary buses directly from the current
  `Tick()` input. On native little-endian targets, enabled ARM11 auxiliary returns must also mix
  through four independent channel pointers into the shared return buffer; only the generic
  non-native-endian fallback stages and converts them in `state.intermediate_mix_buffer`.
  `AuxSend()` still writes each enabled source bus to shared memory. Retain all historical
  mixer-state archive slots for save compatibility: their native-endian contents are transiently
  irrelevant because the next tick bypasses them. Final AArch64 ThinLTO should leave `Tick()` at
  136 bytes, `AuxReturn()` as a 4-byte return, `AuxSend()` at 108 bytes, and only zero, one, or two
  2,560-byte `memcpy` calls for enabled sends. The four source pointers must load before, not
  inside, each NEON sample loop. Keep all-disabled, both-enabled, and mixed enabled/disabled
  routing and untouched-disabled-shared-output coverage.
- AArch64 HLE source filters deliberately vectorize the independent left/right channels, never
  adjacent time samples: the simple and biquad recurrences must remain sequential. Keep filter
  coefficients and histories register-resident across each 160-sample frame, preserve the exact
  reset passthrough coefficients (`1 << 15` and `1 << 14`) plus their final history, and retain the
  scalar non-AArch64 path. Final ThinLTO should continue to show `SMULL`/`SMLAL`, arithmetic shift,
  and `SQXTN`; do not assume source intrinsics are useful without checking the linked library.
- HLE GC-ADPCM decoding deliberately loads one packed byte for each two recurrent samples and
  sign-extends both four-bit values without a lookup table. Preserve high-nibble-before-low-nibble
  feedback order, scale/coefficient selection, signed fixed-point arithmetic, saturation, duplicated
  stereo output, partial frames, the historical padded second sample for odd lengths, and final
  `yn1`/`yn2`. Final AArch64 ThinLTO should retain one byte load plus direct signed bitfield
  extraction per pair and no `SIGNED_NIBBLES` symbol or indexed nibble-table load. Keep the
  independent table-reference test across all nibble values, scales, histories, clipping, and
  frame boundaries.
- HLE PCM8/PCM16 decoding deliberately fills its `StereoBuffer16` deque through one counted,
  sequential output iterator. Preserve PCM8's exact unsigned-byte-to-high-byte mapping, native
  little-endian PCM16 loads, mono duplication, stereo ordering, zero length, and the scalar data
  representation. Do not restore per-sample `deque::operator[]`: final AArch64 ThinLTO should
  advance the destination pointer directly and check only the 4 KiB deque-block boundary, without
  reconstructing the destination from the deque start/map on every sample. Keep the 1023/1024/1025
  and multi-block regression cases.
- HLE partial embedded PCM16 updates must call the separate suffix decoder with the latched physical
  address and `current_sample_number`; do not restore full-buffer decode followed by deque prefix
  erase/move. The suffix decoder must re-read every retained frame from guest memory, not merely
  append newly extended data, so updates to unconsumed samples remain visible. Keep ordinary
  `DecodePCM16()` unchanged, preserve mono/stereo byte layout, zero/equal/end positions, and reset
  `current_sample_number` to zero before a declared-length shrink exactly as the established path
  did. PCM8 and ADPCM partial updates remain separately unimplemented and must not be enabled by
  analogy without title evidence and exact state/feedback coverage.
- AArch64 HLE linear interpolation deliberately evaluates the independent stereo lanes with one
  AdvSIMD `SQDMULH`. Preserve the DSP's signed-16 saturated delta, the unsigned 24-bit phase, the
  exact Q24-to-Q31 `phase << 7` mapping, truncation rather than rounding, and the scalar
  non-AArch64 path. Do not replace it with `SQRDMULH`, float interpolation, or time-lane
  vectorization. Recheck final ThinLTO whenever this math or its deque traversal changes.
- Exact `1.0f` HLE Linear resampling may route through the None copy loop only while `fposition` is
  Q24-aligned. Preserve both gates: a fractional starting phase still requires interpolation, and
  every non-unity rate must retain the original phase progression. The routed path must leave
  output fill, deque consumption, history, and final `fposition` byte-for-byte identical to Linear;
  final AArch64 ThinLTO should tail-branch to None before `SQDMULH` rather than duplicating a second
  copy loop inside Linear.
- HLE resampler traversal treats history as a virtual prefix: `V(0) = xn2`, `V(1) = xn1`, and
  `V(j) = input[j - 2]` for `j >= 2`. Keep the input index monotonic, cache the adjacent sample
  window, consume exactly that many real deque samples, and preserve `xn2`, `xn1`, `fposition`, and
  partial-output behavior across calls. Do not reinsert history into the deque or accept a helper
  call in the valid per-output ARM64 loop; final ThinLTO should reuse the cached window when the
  index is unchanged and issue one sequential sample load when it advances by one.
- `Source::GenerateFrame()` must not pre-clear a nonempty source's complete 640-byte output frame:
  every resampler mode overwrites the produced prefix. Preserve a full clear before the empty-entry
  dequeue/disable early returns, clear only `[frame_position, end)` after an active underrun, and do
  that tail clear before sample accounting and filtering. Keep the exact silence, enable/buffer
  state, resampler history, filter history, and saved-frame behavior. Final AArch64 ThinLTO should
  have no 640-byte `memset` on the steady full-frame path while retaining the empty and partial
  clears.
- AArch64 Y2R conversion deliberately processes eight pixels per AdvSIMD band for all five input
  formats. Preserve planar 4:2:2/4:2:0 horizontal chroma duplication, interleaved YUYV ordering,
  signed 32-bit widening products, both arithmetic-shift stages, offsets, saturation, numeric
  `0xRRGGBB00` output, 8x8 tile placement, and the scalar non-AArch64 path. Final ThinLTO must retain
  `SMULL`/`SMLAL`/`SMLSL`, `SQXTUN`/`UQXTN`, and register ZIP packing in each format path; do not
  replace the packed output with D-form byte `ST4`. Keep all-format, width/height, coefficient-edge,
  and untouched-row canary coverage. The test-only conversion entry point must remain hidden so it
  is garbage-collected from production shared libraries.
- AArch64 Y2R output packing deliberately processes sixteen intermediate `0xRRGGBB00` words per
  band for RGBA8, RGB8, RGB5A1, and RGB565. Preserve exact little-endian output byte order, alpha
  replacement, high-bit truncation, CDMA transfer-unit/gap progression, the scalar tail, and the
  unchanged non-AArch64 path. RGB8 must keep its three adjacent-input `TBL2` maps in the outlined
  helper so their constants load once per CDMA unit; final ThinLTO should retain a 12-instruction
  repeated loop with paired/ordinary Q stores. RGBA8 must OR alpha into the known-zero low byte of
  each valid intermediate word and use ordinary Q stores; RGB5A1/RGB565 must retain one Q-form
  `LD4`, byte masks, `SHLL`/`SHLL2`, and paired Q stores per sixteen pixels. Do not reintroduce
  `ST3`, `ST4`, per-pixel packing, or vectorized scalar-tail alias checks. Keep 15/16/17 and
  31/32/37 boundaries, channel/alpha edges, and output canaries; the test-only packing entry point
  must stay hidden and absent from the production shared library.
- `Rotation::None` plus linear Y2R output must write completed tile rows directly into the final
  strip. `linear_lut` is the identity, so do not restore the redundant tile-to-`tmp_tile` scatter
  followed by a second output copy. Preserve the unchanged rotated and Block8x8 paths, partial
  heights, arbitrary valid line strides, tile order, and untouched padding. Final AArch64 ThinLTO
  should keep the outlined 68-byte writer with one post-indexed Q-form `LDP`, one post-indexed
  Q-form `STP`, decrement, and branch per eight-pixel band. Retain zero/multiple-tile,
  1/2/7/8-row, padded-stride, and guard-canary coverage; its test hook must remain absent from the
  production shared library.
- Zero-gap 8-bit Y2R input deliberately borrows the contiguous guest CDMA stream until that strip's
  conversion has consumed it. Preserve exact `address += amount` and `image_size -= amount` state,
  zero-length behavior, independent direct/compact decisions for each plane, gapped-transfer
  compaction, and every 16-bit format's low-byte extraction. Do not restore an unconditional input
  staging copy. Final AArch64 ThinLTO should keep the outlined helper at 136 bytes with a
  seven-instruction direct route (`LDRH`, `CBZ`, `LDP`, `ADD`, `SUB`, `STP`, `RET`) and no copied
  data. Retain zero-gap untouched-staging tests plus gapped byte-reference and guard-canary coverage;
  the test wrapper must remain absent from the production library.
- Zero-gap `Rotation::None` plus linear Y2R output deliberately gathers each completed tile row,
  packs its final format, and writes guest memory in one pass. Preserve RGBA8 alpha replacement,
  RGB8 byte order and odd-tile tail, RGB5A1/RGB565 truncation, exact address/image-size progression,
  and the fact that every input strip is consumed before output begins. Rotated, Block8x8, and
  gapped output must retain the established staging routes. Final AArch64 ThinLTO should keep the
  RGBA8/RGB8/RGB5A1/RGB565 helpers at 188/304/208/192 bytes and their repeated bodies at 10
  instructions per 8 pixels, 11 per 16, 15 per 8, and 13 per 8 respectively. RGB8 must retain three
  adjacent-input `TBL2` operations for paired tiles plus its exact one-tile Q/D tail; 16-bit formats
  may retain one D-form byte `LD4` because horizontally adjacent tile rows are non-contiguous, but
  all formats must use ordinary guest stores and no `ST3`/`ST4`. Keep zero/odd/even tile counts,
  0/1/2/7/8-row, alpha-edge, transfer-unit, state, and guard-canary coverage; the hidden test wrapper
  must remain absent from the production library.
- The complete direct Y2R route must not allocate the dead CDMA strip buffer. Bypass it only when
  output is zero-gap `Rotation::None` plus linear and every active input is either zero-gap
  YUV422/YUV420 8-bit planar or zero-gap interleaved YUYV. Ignore inactive-plane gaps, but retain
  staging for any active gap, both 16-bit formats, rotation, Block8x8, or output gap. A null staging
  pointer may reach `PrepareInputData8()` only on its zero-gap borrowed-pointer branch. Keep the
  fallback allocation uninitialized: do not use value-initializing `make_unique<T[]>()` or add a
  `memset`. Final AArch64 ThinLTO should keep `PerformConversion()` at `0x2c18`, branch around one
  `new[]`, leave the tile allocation intact, and use `CBZ` to skip only the matching strip-buffer
  `delete[]`; staging partition addresses remain outside the strip loop. Retain all-format active-
  gap/output-condition predicate tests plus null-staging transfer/state coverage, and keep the
  hidden test hook absent from the production library.
- Android Eco Turbo defaults on. Above 100% speed and at `0`/unthrottled it uses a wall-clock token
  budget to cap host presentation/composition at 60 FPS without changing guest timing or the
  selected speed. Do not replace this with a divisor derived from the requested speed: a scene
  that cannot reach that speed would be undersampled. Preserve screenshot and video-dump
  preparation, reset the budget at normal speed, and keep the UI clear that disabling Eco Turbo is
  smoother but uses more GPU work on the 120 Hz panel. For ordinary power tests, verify the launch
  log says `Renderer_FrameLimit: 100`; a zero value means truly uncapped guest rendering and can
  saturate Adreno even when host presentation is capped.
- Android emulation must request the closest current-resolution display rate within 1 Hz of 60 and
  call `Surface.setFrameRate(60, FRAME_RATE_COMPATIBILITY_DEFAULT)` on each valid game surface on
  API 30 or newer. Frontend activities keep the highest current-resolution refresh preference.
  Use refresh-only window attributes: do not reintroduce exact `60f` equality or a nonzero
  `preferredDisplayModeId`, which also expresses resolution. Do not use video-only
  `FRAME_RATE_COMPATIBILITY_FIXED_SOURCE` or force non-seamless switches for gameplay. Treat these
  calls as requests that Android may override, and do not claim panel or compositor watt savings
  before a matched on-device A/B.
- OpenGL and Vulkan presentation deliberately resolve the top-screen right eye only when an active
  main, secondary, screenshot, or frame-dump layout can sample it. Mono-left and bottom-only
  layouts must skip the per-frame right surface lookup/upload; stereo modes and explicit mono-right
  must retain it. If `RightEyeDisabler` actually blocked the just-finished eye, consume that fact
  once only when preparing a render target, and alias the current left presentation image plus
  coordinates into the right descriptor slot. A throttled/non-presented VBlank must leave the fact
  pending. Do not infer a skipped eye from the setting alone: per-title detection can disable the hack.
  Keep the fallback right texture allocated/configured for later mode changes, include additional-
  top layouts in the predicate, and retain focused layout coverage plus final AArch64 branch/codegen
  inspection.
- ARM and Thumb-2 `SMLALD`/`SMLALDX`/`SMLSLD`/`SMLSLDX` deliberately use the generic signed
  multiply-add/subtract-long IR operations. Keep ARM64 on four signed-halfword extracts followed by
  two `SMADDL`/`SMSUBL` operations, including exchange and accumulator aliasing. Do not replace this
  with an AdvSIMD `SMULL`/horizontal-reduction route: Thor measurements showed the GPR path was
  faster on every accessible cluster. Retain ARM and Thumb signed-edge, 64-bit wrap, unchanged-flag,
  and source/destination alias tests.
- ARM and Thumb-2 plain `SMLAL` plus `SMLALBB`/`SMLALBT`/`SMLALTB`/`SMLALTT` deliberately use
  the generic signed multiply-add-long IR operation. Keep plain ARM64 on one `SMADDL`; keep only the
  two required signed-halfword extracts before `SMADDL` for the halfword forms. Preserve modulo-
  64-bit accumulation, ARM `S`-bit N/Z updates, unchanged C/V/Q/GE state, Thumb behavior, and every
  source/destination accumulator alias. Retain the permanent plain/halfword signed-edge, wrap,
  flag, and alias coverage.
- ARM and Thumb-2 `UMULL`/`UMLAL` deliberately use `UnsignedMultiplyLong` in generic Dynarmic IR.
  Keep ARM64 on native `UMULL Xd, Wn, Wm`, followed by the required packed-accumulator `ADD` for
  `UMLAL`. Do not globally fuse `UMLAL` to `UMADDL`: Thor measurements improved A510 but regressed
  A715, A710, and X3. Leave `UMAAL` on its existing generic multiply/add lowering; native `UMULL`
  and reassociated/fused candidates regressed X3 or other big cores. Preserve ARM `S`-bit N/Z,
  unchanged C/V/Q/GE, modulo-64-bit arithmetic, Thumb behavior, unsigned extremes, and every
  source/destination alias in permanent tests.
- ARM and Thumb-2 `SMULL` deliberately use `SignedMultiplyLong` in generic Dynarmic IR. Keep ARM64
  on one native `SMULL Xd, Wn, Wm`; do not restore two `SXTW` operations followed by X-form `MUL`.
  The native path measured 1.600x-3.500x faster across the Thor's X3, A715, A710, and A510 core
  classes. Preserve exact signed 32x32-to-64 arithmetic, ARM `S`-bit N/Z updates, unchanged
  C/V/Q/GE, Thumb behavior, signed extremes/zero, and every source/destination alias in permanent
  tests.
- ARM and Thumb-2 `SMMUL{R}`/`SMMLA{R}`/`SMMLS{R}` must retain the signed long operations in
  generic Dynarmic IR. Keep `SMMUL` on `SignedMultiplyLong`; form the accumulator as a zero-extended
  word shifted left 32 bits and use `SignedMultiplyAddLong` or `SignedMultiplySubtractLong` for
  `SMMLA`/`SMMLS`. ARM64 must emit `SMULL`, or `LSL` plus `SMADDL`/`SMSUBL`. Do not restore the two
  `SXTW` operations, X-form `MUL`, generic add/subtract, or zero-plus-`BFI` accumulator pack: exact
  Thor sequences measured 1.586x-2.000x for `SMMUL` and 1.573x-2.130x for the fused forms across
  measured A510/A715/A710 cores. Preserve modulo-64-bit add/subtract, the unrounded high word,
  rounding from intermediate bit 31 with 32-bit wrap, unchanged NZCV/Q/GE, Thumb behavior, signed
  extremes, and source/destination aliases in permanent tests. Keep the claim path-local until a
  matched game/power A/B exists.
- ARM and Thumb-2 `SMULWB`/`SMULWT` deliberately keep the signed halfword as `U32` and use
  `SignedMultiplyLong(U32, U32)` before the 16-bit logical shift. ARM64 must emit
  `SXTH + SMULL + LSR` for the bottom form or `ASR + SMULL + LSR` for the top form; do not restore
  separate word-to-long extensions around X-form `MUL`. Exact Thor sequences measured
  1.458x-2.237x across A510/A715/A710/X3. Do not apply the same lowering to `SMLAWB`/`SMLAWT`
  without new all-core evidence: the full sticky-Q candidate improved A510 but repeated medians
  regressed A715 slightly and X3 by up to 1.01%, so those accumulate forms intentionally retain
  their established lowering. Preserve signed 32x16 multiplication, exact bits 16-47, unchanged
  NZCV/Q/GE for `SMULW`, top/bottom selection, Thumb behavior, signed extremes, and source/
  destination aliases in permanent tests. Keep the speed claim path-local until a matched game/
  power A/B exists.
- ARM64 Dynarmic may collapse `SignExtendByteToWord` or `SignExtendHalfToWord` followed by
  `SignExtendWordToLong` only when the narrow extension has a non-immediate source, exactly one
  use, and the long extension is its immediately following argument-zero consumer. The word
  extension must alias its input and the long extension must emit one direct `SXTB Xd,Wn` or
  `SXTH Xd,Wn`. Keep shared, immediate, non-adjacent, mismatched, ordinary word-only, and unrelated
  chains on the established `SXTB`/`SXTH` plus `SXTW` lowering; keep the producer and consumer
  predicates symmetrical. This removes one instruction from the current ARM/Thumb-2
  `SMLAWB`/`SMLAWT` halfword path but does not authorize the separately rejected fused multiply/
  accumulate rewrite. Preserve bottom/top forms, destination aliases with each source role,
  signed overflow, sticky CPSR.Q, NZCV/GE, unrelated GPRs, and FPSCR in permanent tests. Exact
  independent/dependent byte/halfword sequences measured 1.82x-4.34x across Thor A510/A715/A710/
  X3; keep the claim path-local until a matched title and battery-power A/B exists.
- ARM64 Dynarmic may fuse `VectorSignExtend8/16/32` or `VectorZeroExtend8/16/32` with the
  immediately following matching `VectorLogicalShiftLeft16/32/64` only when the extension has one
  use and the shift consumes it as argument zero. An immediate smaller than the original narrow
  element width emits native `SSHLL`/`USHLL`; an immediate exactly equal to that width is accepted
  only for zero extension and emits native `SHLL`. The extension then aliases its narrow source.
  Shared, non-adjacent, mismatched-width, non-immediate, larger-than-width, and signed maximum-width
  forms must retain `SXTL`/`UXTL` plus `SHL`; the alias and fused-emitter predicates must remain
  symmetrical so a fallback never sees an unextended operand. Preserve signed/unsigned 8/16/32-bit
  A32 `VSHLL` coverage, architectural maximum-width `VSHLL.I8/I16/I32`, high registers, source/
  destination overlap, untouched SIMD state, and unchanged CPSR flags. Treat the measured result
  as path-local until a matched game and power A/B exists.
- ARM64 Dynarmic may fuse a sole-use, immediately adjacent `VectorLogicalShiftRight16/32/64`
  followed by matching `VectorNarrow` or `VectorUnsignedSaturatedNarrow`, or
  `VectorArithmeticShiftRight16/32/64` followed by matching
  `VectorSignedSaturatedNarrowToSigned`/`ToUnsigned`. The narrow consumer must be argument zero,
  the immediate must be 1 through half the source width, and the producer/consumer predicates must
  remain symmetrical. Emit `SHRN`, `UQSHRN`, `SQSHRN`, or `SQSHRUN` respectively; saturating forms
  must load the host FPSR so guest FPSCR.QC remains sticky. Shared, non-adjacent, mismatched,
  non-immediate, zero, or out-of-range forms retain the generic shift plus narrow path. Preserve
  all 16/32/64-bit source widths, high registers, partial/full overlap, unrelated SIMD state, CPSR,
  FPSCR state, and QC behavior in permanent tests.
- A32/A64 vector rounding shift-right narrowing must use the first-class
  `VectorRoundingNarrow`, `VectorSignedSaturatedRoundingNarrowToSigned`/`ToUnsigned`, or
  `VectorUnsignedSaturatedRoundingNarrow` IR operation. ARM64 must emit one `RSHRN`, `SQRSHRN`,
  `SQRSHRUN`, or `UQRSHRN`; saturating forms must load host FPSR so guest FPSCR.QC remains sticky.
  Do not restore the frontend's shift/broadcast/AND/equal/subtract/narrow DAG on ARM64: the exact
  fused paths measured 13.13x-14.81x on Thor A510, 2.81x-3.54x on A715, 3.23x-3.59x on A710,
  and 3.51x-3.96x on X3. x64 and RISC-V must polyfill the first-class operation back into that
  overflow-safe DAG. Preserve all four rounding instruction families, 16/32/64-bit sources,
  legal shifts, high registers, source/destination overlap, exact negative rounding, saturation,
  unrelated SIMD state, CPSR/FPSCR state, and QC behavior in permanent tests. Keep the claim
  path-local until a matched title and power A/B exists.
- A32/A64 vector `VRSHR`/`SRSHR`/`URSHR` and `VRSRA`/`SRSRA`/`URSRA` must retain the first-class
  signed/unsigned rounding shift-right or rounding shift-right-accumulate IR operations. ARM64 must
  emit one `SRSHR`/`URSHR` or `SRSRA`/`URSRA`; x64 and RISC-V must polyfill back to the established
  overflow-safe shift/broadcast/AND/equal/subtract sequence plus the optional modular add. Preserve
  8/16/32/64-bit lanes, legal immediate shifts including the element width, D/Q forms, high
  registers, source/destination overlap, exact negative rounding, modular accumulator wrap,
  unrelated SIMD state, and unchanged CPSR/FPSCR. Do not fuse plain non-rounding `VSRA` into
  `SSRA`/`USRA`: although it improved A510 and was neutral on A715/A710, the exact sequence regressed
  Thor X3 by 5.7%-22.3%. The accepted `VRSHR` path measured 9.88x-10.61x on A510, 2.50x-2.51x on
  A715, 2.71x-2.72x on A710, and 3.52x-4.77x on X3; `VRSRA` measured 5.08x-5.65x, 2.99x-3.01x,
  3.39x-3.50x, and 2.54x-3.37x respectively. Keep these claims path-local until a matched title and
  battery-power A/B exists.
- A32/A64 vector `VSLI`/`SLI` and `VSRI`/`SRI` must retain first-class shift-insert IR. ARM64 must
  emit one native `SLI` or `SRI`; x64 and RISC-V must use the exact polyfill that preserves the
  destination bits outside the insertion field. Keep each 8/16/32/64-bit lane's legal immediate
  range, D/Q forms, low/high registers, source/destination overlap, unrelated SIMD state, and
  unchanged CPSR/FPSCR under permanent tests. Do not restore ARM64's five-instruction
  shift/immediate/broadcast/bit-clear/OR expansion: the exact native path measured 6.94x-8.32x on
  Thor A510, 1.99x-2.01x on A715, 2.17x-2.21x on A710, and 2.42x-2.43x on X3. Keep these claims
  path-local until a matched title and battery-power A/B exists.
- ARM64 Dynarmic `PackedAbsDiffSumU8` must retain the two-instruction `UABDL H8` plus low-`H4`
  `UADDLV` lowering used by A32 ARMv6 `USAD8`/`USADA8`. The low four widened halfwords are exactly
  the four guest byte lanes; never reduce all eight lanes or restore the old `MOVI`/`UABD`/`AND`/
  `UADDLV` mask path. Preserve ARM and Thumb encodings, maximum difference sum 1020, modular
  `USADA8` accumulator wrap, destination/source and accumulator aliases, unrelated registers, and
  unchanged NZCV/Q/GE flags in permanent tests. The exact four-to-two instruction path measured
  1.759435x on Thor A510, 2.515585x on A715, 2.505252x on A710, and 2.806593x on X3. Keep these
  claims path-local until a matched title and battery-power A/B exists.
- A32 ARMv6 `PKHBT`/`PKHTB` must retain the first-class `PackHalfwordBottom`/`PackHalfwordTop` IR.
  ARM64 must reuse the top source with `BFXIL` for bottom shift 0, reuse the bottom source with
  `BFI` for bottom shift 16, otherwise use at most `LSL` plus `BFXIL`; top shifts 1-16 use one
  `BFXIL`, while shifts 17-32 use `ASR` plus `BFXIL` with ASR #32 represented by #31. x64 and
  RISC-V must polyfill these operations back to the exact shift/two-mask/OR DAG. Preserve ARM and
  Thumb encodings, bottom shifts 0-31, top shifts 1-32, destination/source/all-source aliases,
  unrelated registers, and unchanged NZCV/Q/GE flags in permanent tests. The seven exact forms
  measured 1.196462x-3.014697x on Thor A510, 1.750912x-2.460514x on A715,
  1.344066x-2.163796x on A710, and 1.489624x-2.232581x on X3. Keep these claims path-local until a
  matched title and battery-power A/B exists.
- A32 ARM/Thumb-2 `SXTAB`/`SXTAH`/`UXTAB`/`UXTAH` with rotation zero must retain the first-class
  `SignedExtendAndAdd32`/`UnsignedExtendAndAdd32` IR. ARM64 must emit one extended-register `ADD`
  using `SXTB`, `SXTH`, `UXTB`, or `UXTH`; x64 and RISC-V must polyfill back to the established
  narrow/extend plus modular-add DAG. Preserve destination/addend/value aliases, high registers,
  unrelated registers, and unchanged NZCV/Q/GE flags in permanent ARM and Thumb tests. Do not send
  nonzero rotations through this path: the required `ROR` plus extended `ADD` was neutral on A510
  and one representative form repeated a 0.50% regression. Exact rotation-zero paths measured
  1.329924x-1.340523x on A510, 1.165637x-1.204479x on A715, and 1.126133x-1.135539x on A710.
  The X3 was parked by `core_ctl` during this run, so its optimization-guide evidence is not a
  physical Thor benchmark. Keep all speed claims path-local until a matched title and power A/B.
- A32 ARM/Thumb-2 `SSAT16`/`USAT16` must retain the first-class `PackedSignedSaturation16`/
  `PackedUnsignedSaturation16` IR and one combined overflow pseudo-result. ARM64 must share the
  two lanes' bounds, clamp with scalar `CMP`/`CSEL`, pack with `BFI`, compare the packed result to
  the input once, and call `A32OrQFlag` once. Do not substitute AdvSIMD `SQSHL`/`SQSHLU`: their
  host `FPSR.QC` side effect is not the guest ARM11 `CPSR.Q` result and can corrupt guest VFP
  `FPSCR.QC`. Signed saturation to 16 bits may alias the input with overflow false; unsigned
  saturation to zero bits must return zero and compare against the input. x64 and RISC-V must
  polyfill the first-class operation back into the exact two-lane scalar DAG. Preserve every
  signed 1-16 and unsigned 0-15 immediate, ARM and Thumb encodings, source/destination aliases,
  untouched registers/NZCV/GE, sticky initial Q, and unchanged FPSCR in permanent tests. The exact
  path measured 1.09x-1.31x on Thor A510, about 2.00x on A715, 1.93x-2.02x on A710, and
  1.50x-2.03x on X3. Keep these claims path-local until a matched title and battery-power A/B.
- A32 ARM/Thumb-2 `SXTB16` must retain the first-class `PackedSignExtendByteToHalf` IR. ARM64 must
  use `SBFX` to save the selected upper byte in a scratch register before `SXTB` writes the result,
  then use `BFI` to insert the sign-extended upper halfword. Do not reverse the first two
  operations: the final-use `ReadWriteW` allocation may alias the guest source and destination.
  x64 and RISC-V must polyfill the operation back to the established two-mask, constant, multiply,
  and OR DAG. Preserve ARM and Thumb encodings, rotations 0/8/16/24, source/destination aliases,
  untouched GPRs, unchanged NZCV/Q/GE, and unchanged FPSCR in permanent tests. The exact path
  measured 1.00x-1.33x on Thor A510, 1.54x-2.04x on A715, 1.24x-1.33x on A710, and 1.24x-1.62x on
  X3; the A510 rotation-eight independent form was effectively tied at 1.000965x. Keep these claims
  path-local until a matched title and battery-power A/B exists.
- A32 ARM/Thumb-2 `SXTAB16` may use `PackedSignExtendByteToHalf` before `PackedAddU16` only for
  rotations 8/16/24. Keep rotation zero on the established mask/mask/constant/multiply/OR DAG: the
  shorter scalar composition regressed the doubled Thor X3 independent run by 2.0%. Do not replace
  the accepted path with the tested `FMOV`/`UZP1`/`FMOV`/`SADDW`/`FMOV` fusion; despite wins on
  A510/A715/A710 and dependent X3 chains, it regressed X3 independent rotation zero by 10.6% and
  rotation eight by 2.2%. x64 and RISC-V must continue to polyfill the first-class operation back
  to the portable DAG. Preserve ARM and Thumb encodings, rotations 0/8/16/24, distinct operands,
  every two-way alias, all-way aliasing, modular halfword wrap, untouched GPRs, unchanged NZCV/Q/GE,
  and unchanged FPSCR. The accepted ROR8 path measured 1.050745x-1.065097x on A510,
  1.159406x-1.159727x on A715, 1.089628x-1.120201x on A710, and 1.067154x-1.076240x on X3. Keep
  these claims path-local until a matched title and battery-power A/B exists.
- A32 ARM/Thumb-2 `RBIT` must retain the first-class `ReverseBits32` IR operation. ARM64 must emit
  one native `RBIT`; x64 and RISC-V must polyfill it back to the exact mask/shift/OR network. Keep
  permanent ARM and Thumb coverage for distinct operands and source/destination aliases while
  proving untouched GPRs, unchanged NZCV/Q/GE, and unchanged FPSCR. The old 17-instruction ARM64
  path fell to one instruction and measured 11.306165x-17.584485x for independent chains and
  7.024895x-9.644375x for a sequential dependency chain across all four Thor CPU classes. Keep
  these claims path-local until a matched title and battery-power A/B exists.
- A32 ARM/Thumb-16/Thumb-2 `REV16` must retain the first-class `ByteReverseHalfwords32` IR
  operation. ARM64 must emit one native `REV16`; x64 and RISC-V must polyfill it back to the exact
  shift/mask/OR network. Do not restore Thumb-16's separate upper/lower-half extraction, reversal,
  extension, shift, and OR graph. Preserve all three guest encodings, distinct operands, source/
  destination aliases, untouched GPRs, unchanged NZCV/Q/GE, and unchanged FPSCR in permanent
  tests. The measured five-instruction ARM/Thumb-2 body fell to one and measured
  3.605787x-4.435562x for independent chains and 3.096017x-5.122643x for a sequential dependency
  chain across all four Thor CPU classes. Keep these claims path-local until a matched title and
  battery-power A/B exists.
- A32 ARM/Thumb-16/Thumb-2 `REVSH` must retain the first-class `ByteReverseSignedHalf32` IR
  operation. ARM64 must emit `REV; ASR #16`; x64 and RISC-V must polyfill it back to
  `LeastSignificantHalf`, `ByteReverseHalf`, and `SignExtendHalfToWord`. Do not restore the old
  ARM64 `UXTH; REV16; SXTH` sequence or substitute `REV16; SXTH`: the latter was materially slower
  on the A510 dependency chain. Preserve all three guest encodings, distinct operands, source/
  destination aliases, dirty upper-half inputs, untouched GPRs, unchanged NZCV/Q/GE, and unchanged
  FPSCR in permanent tests. The three-instruction body fell to two and measured
  1.550576x-2.621212x for independent chains and 1.499462x-2.631136x for a sequential dependency
  chain across all four Thor CPU classes. Keep these claims path-local until a matched title and
  battery-power A/B exists.
- A32 ARM/Thumb-2 `UBFX`/`SBFX` must retain the first-class `UnsignedBitFieldExtract32`/
  `SignedBitFieldExtract32` IR operations. ARM64 must emit one native `UBFX` or `SBFX` for every
  non-full-width legal field and alias the source without code for `lsb=0,width=32`; x64 and
  RISC-V must polyfill back to the exact `LSR; AND` or `LSL; ASR` graph. Preserve both guest
  encodings, boundary fields, signedness, distinct and source/destination-alias operands, untouched
  GPRs, unchanged NZCV/Q/GE, and unchanged FPSCR in permanent tests. Exact Thor measurements were
  1.5054x-2.0579x for unsigned throughput and 2.0176x-2.1327x for signed throughput. Dependency
  chains were about 2.00x on A715/A710/X3 but only 1.02x-1.03x on A510, consistent with the A510
  manual's latency table. Keep these claims path-local until a matched title and battery-power A/B
  exists.
- A32 ARM/Thumb-2 `BFI` must retain first-class `BitFieldInsert32` and
  `BitFieldInsertSelf32` IR. ARM64 must lower the distinct form with one read/write destination,
  one source read, and one native `BFI`; the self form must use one read/write operand so register
  allocation cannot insert a hidden `MOV`. Preserve the zero-code full-width replacement and
  `lsb=0` self identity. x64 and RISC-V must polyfill both operations back to the exact
  destination-mask/source-shift/source-mask/OR graph. Keep ARM and Thumb encodings, boundary
  fields, distinct and destination/source-alias operands, unrelated registers, NZCV/Q/GE, and
  FPSCR under permanent tests. Do not route `BFC` through this operation: its established ARM64
  logical-immediate clear already costs one instruction. Repeated exact Thor measurements found
  2.0028x-3.8745x throughput gains, neutral distinct dependency chains on A510/A710/X3, a 2.0011x
  distinct dependency gain on A715, and 1.5004x-3.1851x self dependency gains. Keep all claims
  path-local until a matched title and battery-power A/B exists.
- A32 ARM/Thumb-2 `MOVT` must retain the first-class `MoveTopHalf32` IR operation for nonzero
  immediates. ARM64 must use one read/write operand and emit one native `MOVK Wd,#imm,LSL#16`;
  x64 and RISC-V must polyfill it back to the exact low-half `AND` plus shifted-immediate `OR` DAG.
  Keep immediate zero on the established one-`AND #0xffff` identity-reduced path: the otherwise
  equivalent MOVK candidate repeatedly regressed independent A510 measurements by 7.1%-9.0%.
  Preserve ARM and Thumb-2 encodings, low/high destination registers, zero/boundary/dirty values,
  every unrelated GPR, NZCV/Q/GE, and FPSCR in permanent tests. Exact accepted-path measurements
  were 2.6231x/2.0075x independent/dependent on A510, 2.8808x/2.0001x on A715,
  2.9015x/1.9990x on A710, and 2.7145x/1.9993x on X3 for a representative nonzero immediate.
  Keep these claims path-local until a matched title and battery-power A/B exists.
- ARM64 Dynarmic ordinary A32 byte/halfword stores may alias
  `LeastSignificantByte`/`LeastSignificantHalf` to the raw word only when that value has exactly
  one use and its consumer is matching `A32WriteMemory8`/`A32WriteMemory16`. Native `STRB`/`STRH`
  must then perform the final truncation with no preceding `UXTB`/`UXTH`. Do not extend this to
  shared or non-store U8/U16 values, exclusive writes, mismatched widths, or the endian-reversal
  path. Preserve exact low-width callback arguments and fastmem writes for dirty-upper-bit inputs,
  ARM/Thumb encodings, data/base aliases, unrelated GPRs, NZCV/Q/GE, and FPSCR in permanent tests.
  The exact store-saturated Thor benchmark was throughput-neutral, so describe this only as one
  removed host instruction and lower code-cache/front-end/integer-issue work until a matched title
  and battery-power A/B exists.
- ARM64 Dynarmic ordinary A32 signed byte/halfword loads may fold a sole immediately following
  `SignExtendByteToWord`/`SignExtendHalfToWord` consumer into native `LDRSB`/`LDRSH`. Keep the load
  and extension predicates symmetrical: shared, non-adjacent, mismatched, ordered/acquire,
  exclusive, endian-reversed, A64, and unrelated producers must retain their old lowering. Direct
  fastmem/page-table hits use the signed load; callback and fastmem/page-table fallback paths must
  still sign-extend the returned narrow value before the extension aliases it. Do not hold a
  `GetArgumentInfo()` result while falling through to the legacy extension emitter. Preserve ARM
  and Thumb encodings, destination/base aliases, callback and fastmem reads, boundary values,
  unrelated GPRs, NZCV/Q/GE, and FPSCR in permanent tests. The exact load/accumulate loop reduced
  median affected-path time by 18.9%/53.8% for byte/halfword on A510 and 1.7%/1.7% on A715, while
  A710 was neutral within 0.1%; CPU 5 and X3 affinity were parked during this run. Treat the win as
  path-local instruction/code-cache/front-end work until a matched title and battery-power A/B.
- ARM64 Dynarmic may alias a sole, immediately adjacent, non-immediate A32
  `LogicalShiftLeft32` into a following flag-free/carry-free `Add32` only for immediate shifts
  1 through 4. Emit one shifted-register `ADD Wd,Wbase,Windex,LSL #shift`. Keep flags/carry,
  shared, non-adjacent, immediate-source, variable, zero, and shifts 5 through 31 on the
  established lowering. Preserve ARM and Thumb-2 encodings, destination/base/index aliases,
  full-width wrap, unrelated GPRs, NZCV/Q/GE, and FPSCR in permanent tests. Do not widen the gate
  from instruction-count intuition: exact Thor base-dependent shifts 16/31 regressed to about
  0.50x on A715/A710/X3, while the accepted 1..4 range was independently rechecked on A510.
- ARM64 Dynarmic may apply that same symmetrical sole-use/immediately-adjacent/non-immediate gate
  to A32 `LogicalShiftRight32` and `ArithmeticShiftRight32` feeding flag-free/carry-free `Add32`
  for immediates 1 through 31. Emit one `ADD Wd,Wbase,Windex,LSR/ASR #shift`. Keep carry or flag
  pseudos, shared/non-adjacent producers, immediate sources, variable/zero/32 shifts, and unrelated
  consumers on the established ADD lowering; shifted subtraction is governed by the separate rule
  below, and the ADD LSL gate must not widen beyond 1..4. Preserve ARM and Thumb-2 encodings,
  destination/base/index aliases, signed ASR behavior, modular 32-bit wrap, unrelated GPRs,
  NZCV/Q/GE, and FPSCR. Actual-JIT trace words and
  representative shifts 1/2/3/4/8/16/31 must remain the performance gate: affected-path medians
  improved on A510/A715/A710, while X3 independent work was neutral and dependency chains won.
  Keep all claims path-local until a matched title and battery-power A/B exists.
- ARM64 Dynarmic may alias a sole, immediately adjacent, non-immediate A32 `LogicalShiftLeft32`
  into flag-setting `Add32` or normal-carry-in `Sub32` only for immediate shifts 1 through 4 and
  only when the arithmetic instruction's sole pseudo-operation is `GetNZCVFromOp`. Emit one
  `ADDS`/`SUBS Wd,Wbase,Windex,LSL #shift`; this covers ARM/Thumb-2 ADDS/SUBS and the same IR used
  by CMN/CMP. The shift must have one use and no carry pseudo-result. Keep shared/non-adjacent,
  immediate-source, variable, zero, carry/overflow/other pseudo users, every flag-setting LSR/ASR,
  and flag-setting LSL 5 through 31 on the established split lowering. Preserve destination/base/
  index aliases, comparison no-write behavior, full NZCV including carry and overflow boundaries,
  Q/GE, unrelated GPRs, and FPSCR in permanent ARM and Thumb tests. Do not generalize from static
  instruction count: base-dependent right/wide-shift forms measured about 0.51x-0.53x on Thor's
  A715/A710/X3, while the accepted small-LSL range stayed above the 0.995 floor on all four core
  classes. Keep gains path-local until a matched title and battery-power A/B exists.
- ARM64 Dynarmic may alias a sole, immediately adjacent, non-immediate A32 LSL/LSR/ASR producer
  into an ordinary flag-free/carry-free `Sub32` only when its immediate is 1 through 31 and the
  subtraction carry-in is the normal true value. Emit one
  `SUB Wd,Wbase,Windex,LSL/LSR/ASR #shift`. The shift producer and subtraction consumer must use
  the same eligibility helper so fallback cannot observe an unshifted alias. Keep shared,
  non-adjacent, immediate-source, variable, zero/32, flag/carry, reverse-subtract/borrow, and
  unrelated forms on the established lowering. Preserve ARM and Thumb-2 encodings, every
  destination/base/index alias, signed ASR behavior, modular 32-bit wrap, unrelated GPRs,
  NZCV/Q/GE, and FPSCR. Actual-JIT words for all three shift families and representative immediate
  boundaries plus per-core Thor measurements must remain the gate. Do not apply the SUB 1..31
  result to ADD's independently measured LSL 1..4 limit, and keep the gains path-local until a
  matched title and battery-power A/B exists.
- ARM64 Dynarmic may alias a sole, immediately adjacent, non-immediate A32 LSL/LSR/ASR/ROR
  producer into operand 1 of a flag-free/carry-free `And32`, `Eor32`, or `Or32` for immediate
  shifts 1 through 31. Emit one shifted-register `AND`/`EOR`/`ORR`. The producer and consumer must
  use the same eligibility helper so a producer can alias its raw input only when its consumer
  will encode the shift. Keep flag or carry pseudos, shared/non-adjacent producers, immediate
  sources, variable shifts, zero/32/RRX forms, shifts in another operand, and unrelated consumers
  on the established lowering. Preserve ARM and Thumb-2 encodings, every destination/source alias,
  full-width logical results, unrelated GPRs, NZCV/Q/GE, and FPSCR. Require actual-JIT words for
  all three logical families and all four shift kinds plus representative boundaries and all-core
  Thor measurements. This independent logical result does not widen ADD's measured LSL 1..4 gate;
  keep all gains path-local until a matched title and battery-power A/B exists.
- ARM64 Dynarmic may alias a sole, immediately adjacent, non-immediate A32 LSL/LSR/ASR/ROR
  producer into a no-flags/no-carry `Not32` for immediate shifts 1 through 31. Emit one native
  shifted-register `MVN`. The producer and consumer must share one eligibility helper so the
  producer aliases its raw source only when `Not32` will encode the shift. Keep flag/carry pseudos,
  shared or non-adjacent producers, immediate sources, variable shifts, zero/32/RRX forms, and
  unrelated consumers on the established lowering. Preserve ARM and Thumb-2 encodings, distinct
  and source/destination-alias operands, full-width results, unrelated GPRs, NZCV/Q/GE, and FPSCR.
  Require actual-JIT words for all four shift kinds, representative boundaries, and correctness
  runs on every Thor core class. Keep gains path-local until a matched title and battery-power A/B.
- Do not globally fold a shifted operand into ARM64 `BIC` for A32 `AndNot32`. Although the unary
  shifted-input and independent shapes can improve, repeated A510 base-dependent confirmations
  measured approximately 0.9857x and 0.9926x for representative ASR/LSL forms. Retain the split
  shift plus `BIC` lowering unless a future dependency-aware predicate proves the exact safe shape
  and wins on every intended Thor core class. Instruction count and the manuals' logical timing
  rows are candidate guidance, not sufficient acceptance evidence.
- Do not globally replace A32/A64 same-width `SABD/UABD` plus `ADD` for `VABA` with native
  `SABA/UABA`. Although independent and big-core dependency patterns can win, exact accumulator-
  chain measurements regressed to 0.6595x-0.6890x on A510 for signed/unsigned 8/16/32-bit forms.
  Keep the split lowering unless a future gate proves its dependency shape and wins on every
  intended Thor core class. The manuals' slower A510 `SABA/UABA` timing is a warning, not a
  substitute for the retained all-core benchmark evidence.
- Do not globally fuse A32 `MLA`/`MLS` into ARM64 `MADD`/`MSUB`. Exact four-chain measurements
  showed attractive independent A510 results but regressed the dependent A510 path and both
  measured patterns on A715; independent A710 and X3 patterns also regressed badly. Retain the
  split `MUL` plus `ADD`/`SUB` lowering unless a future title-gated, dependency-aware proof wins
  on every intended Thor core class.
- Do not replace A32 `SMUSD`/`SMUSDX` lane extraction and multiply/subtract lowering with the
  shorter-looking scalar `SMULL`/`SMSUBL` sequence globally. After 64-byte loop alignment, the
  exact A510 `SMUSD` kernel regressed from 2.599973 to 2.702699 ns/op (0.961991x), while `SMUSDX`
  was only a 0.999915x tie. Big-core wins do not override the efficiency-core regression; retain
  the established lowering unless a narrower dependency-aware gate wins on every intended core.
- Keep generated Android storage bounded. Check free C: space and the sizes of
  `src/android/app/.cxx` and `src/android/app/build` before and after large native builds. Retain
  only the active `arm64-v8a` release configuration cache and APKs still needed for testing; after
  verification, remove stale Debug, x86/x86_64, obsolete CMake configuration-hash, and Gradle
  intermediate trees using exact validated paths inside this repository. An opt-in profiling hash
  must be removed in the same work tranche after its binary proof is captured; stop the Gradle
  daemon first if it holds an intermediate open. Before handoff, verify that only the active release
  hash remains under `.cxx/RelWithDebInfo`, retain `build/outputs/apk` instead of packaging/mapping/
  symbol staging, and report the logical bytes removed. Do not leave tens of gigabytes of
  reproducible build output behind or run a broad cleanup that could touch source, manuals, saves,
  or unrelated user files.
- Do not pass Gradle `--configuration-cache` for Android packaging. `app/build.gradle.kts` runs
  command-line Git during configuration, and Gradle 8.13 rejects that while storing the cache even
  after native and APK tasks succeed. Use `--no-configuration-cache`; the ordinary Gradle build
  cache and active native CMake/Ninja cache remain useful.
- Drive Android native builds through Gradle with the pinned JDK, SDK, NDK, and vcpkg environment;
  do not invoke an unrelated system Ninja directly against `app/.cxx`. A `.ninja_deps` access or
  sharing failure can mean another Gradle/CMake build still owns the active configuration: wait for
  that owner instead of deleting the cache or starting a parallel build. Toggling
  `thorFrameProfiling` reconfigures the native target and can legitimately rebuild all 2,203 ARM64
  actions even when the configuration hash remains `5h1x5ud1`.
- The Thor may enumerate through both USB (`c3ca0370`) and wireless ADB. The user currently prefers
  wireless ADB at `192.168.1.33:5555`; use that transport for installs and tests unless they ask to
  switch back, and always pass `-s` so the same physical device is not addressed twice. Record AC,
  USB, or battery power state with performance evidence: wall-powered measurements are useful for
  sustained thermals but are not battery-discharge watt measurements. Strip a large native test
  executable into a temporary file before pushing it to `/data/local/tmp`, and remove both
  temporary copies immediately after the run.
- Do not commit generated Gradle, CMake, or APK output.
- Thor GPU driver UX should stay simple: keep the guided driver picker with visible per-driver download buttons, recommendation notes, recent Turnip rollback choices, manual ZIP fallback, and system-driver fallback working. The guide fetches K11MCH1 AdrenoToolsDrivers release assets at runtime and must validate `meta.json` before installing.
- Android per-title settings are data-driven, not hardcoded. `Config::ApplyGameSettings()` overlays
  `<user dir>/GameSettings/<16-hex title id>.ini` onto the freshly reloaded global configuration at
  game boot and on `reloadSettings()`, for that session only, and never writes `config.ini`. The
  overlay is sparse: `ReadValues()` runs with `sparse_overlay` set, every `ReadSetting` returns
  early unless the file defines the key, the raw `Get*` reads are guarded the same way, and
  Controls, Camera, log filter, LLE module selection, Debugging, and Miscellaneous are skipped so a
  per-title file can never clear input mappings or defaults. Bundled defaults ship under
  `src/android/app/src/main/assets/game_profiles/` and are copied into `GameSettings/` only when no
  file of that name exists, so in-app edits always win. The long-press **Game Settings** screen
  reuses the ordinary settings UI scoped to that title, hides the global-only sections and the
  reset action, and saves only keys that differ from the global value or were already overridden.
  Do not reintroduce a title-ID `if` in `native.cpp`; add or edit the manifest instead.
- E.X. Troopers (`0004000000053700`) keeps its Thor compatibility profile as
  `game_profiles/0004000000053700.ini`: 2x resolution, JIT/HW shader/shader cache basics, custom
  texture loading off, normal frame limit, and `[Compatibility] skip_texture_copy_fallback`. Keep
  its recommended cheat preset at 30 FPS unless on-device testing proves 60 FPS is stable.
- Conception II (`0004000000112C00`) ships `game_profiles/0004000000112C00.ini` as the Thor crisp
  presentation profile: `resolution_factor = 5`, `texture_filter = 0`, `screen_filter = 2`
  (Snapdragon GSR), linear presentation. The reasoning is geometric, not aesthetic: the 1920x1080
  primary panel shows the 400x240 top screen at 1800x1080 and the 1240x1080 secondary panel shows
  the 320x240 bottom screen at 1240x930, so 3x is stretched 1.5x/1.29x with bilinear filtering
  while 5x (2000x1200 / 1600x1200) is downscaled 0.9x/0.78x. Prefer supersampled downscale over
  any upscaling filter when the goal is crispness without invented detail. This is a per-title
  choice; the global 3x acceptance configuration is unchanged.
- Snapdragon Game Super Resolution 1 is vendored verbatim from Qualcomm's BSD-3-Clause
  `sgsr1_shader_mobile.frag` as `host_shaders/vulkan_present_sgsr.frag` and exposed as
  `ScreenFilter::SGSR`, present pipeline index 4. Keep the 12-tap weighting, edge vote, adaptive
  sharpening, clamps, and RGBA operation mode exactly as published; only the `ViewportInfo` and
  sampler plumbing differ. Its upscale-only guard compares covered area, because `o_resolution` is
  stored height-first and the guest screen may be rotated into the layout. SGSR 2 needs motion
  vectors and depth that PICA content cannot supply, and Adreno Frame Motion Engine is not an
  app-callable API; do not promise either. OpenGL keeps plain presentation for this filter.
- Keep first-party Markdown current when behavior changes: `README.md`, `AGENTS.md`, `AI-POLICY.md`, `.github/PULL_REQUEST_TEMPLATE.md`, `docs/*.md`, `tools/README.md`, and Android asset READMEs. Leave vendored dependency Markdown and license files alone unless a dependency itself changes.
- Track Thor performance findings in `docs/thor-optimization-notes.md`. Thor dual-display mode intentionally pins the primary panel to the 3DS top screen and the secondary panel to the 3DS bottom screen, and the app must not recreate the old hidden virtual secondary-display render path.
- Keep Snapdragon/Adreno/ARM research used for this fork under `docs/research/` and a provenance index under `docs/hardware/`. Prefer concise project-specific summaries. Do not commit vendor, device, or console manual PDFs; keep local research copies outside the Git repository and record their public source, revision, hash, and project relevance in the provenance index.
- Preserve the shadow-source storage-view guard added alongside merge `3b27718bf`. Upstream's
  rewritten `RasterizerVulkan::SyncUtilityTextures()` binds the unit named by
  `lighting.config0.shadow_selector` as an R32Uint storage image, but Vulkan storage usage and the
  mutable RGBA8 allocation are only applied when `TextureInfo::is_shadow_source` was true at
  `Surface` construction, which requires that unit's type to be `Shadow2D` or `ShadowCube`.
  Conception II (`0004000000112C00`) enables shadow reading on a unit that is not typed that way,
  so `StorageView()` aborted the emulation thread on the
  `traits.native == eR8G8B8A8Unorm` assertion. Keep `Surface::SupportsStorageView()` mirroring the
  allocator's exact `native == eR8G8B8A8Unorm && storage_support` condition, and keep binding the
  null surface when it fails. Do not attempt to repair this by assigning
  `SurfaceFlagBits::ShadowSource` after construction: format, usage, and mutability are decided in
  the constructor, so a late flag cannot make an allocation storage capable. Allocating the
  selected unit as a shadow source instead would change how that texture is sampled elsewhere and
  requires visual validation on a shadow-reading title first.
- Treat cleanup as part of finishing a task rather than a separate request. Before handing work
  back, remove the stale CMake configuration hashes, Gradle intermediates, and scratch artifacts
  that the work created, both in this repository and on the device, and report the logical bytes
  reclaimed. Delete only exact validated paths; never sweep broadly enough to reach source, saves,
  manuals, research copies, or unrelated user files, and leave evidence from earlier sessions alone
  unless its removal is explicitly requested.
- This repository has one authoritative engineering ledger. Keep accepted rules and rejected
  experiments in `AGENTS.md`, dated measurements in `docs/thor-optimization-notes.md`, and a public
  description in `README.md`. `README.md` and `CLAUDE.md` point at `AGENTS.md` instead of restating
  engineering detail, so a behavior change is recorded in exactly one place.
- Medarot 9 (`0004000000174F00`, Kuwagata, English patch) copies its rendered 400x240 RGB8 top
  framebuffer with the CPU every frame. The copy loop runs at guest PC `0x004008C0`. The first
  4-byte read of each frame hits the dirty framebuffer surface. `RasterizerCache::FlushRegion`
  treats a request of 8 bytes or less as a CPU read and flushes the whole dirty region. That is
  one 384 KiB download and one GPU finish per frame. The finish waits for the frame's draws, which
  the scheduler submits only at that point, so the CPU and the GPU never overlap in this title.
  At 3x the GPU frame is about 6.7 ms and fast-forward reaches the 60 FPS Eco Turbo presentation
  cap at Speed 286%. At 4x the GPU frame is about 11 ms and fast-forward stops near Speed 150%,
  with about 30% of one core spent inside the driver's semaphore wait. The title presents at
  20 FPS by design and runs at Speed 100% at 2x and 3x. Do not read the 4x limit as a renderer
  regression. Disabling right-eye rendering changed nothing; the title does not render a second
  eye. Evidence: the 2026-09-17 Medarot 9 entry in `docs/thor-optimization-notes.md`.
- Rejected 2026-09-17: a dirty-free-span fast path in `RasterizerCache::FlushRegion` that let a
  small read inside the last flushed span skip the dirty-region lookup. A matched six-config
  production A/B on the Thor showed no change in frame pacing, GPU busy, or emulation-thread user
  and system time. The code was reverted. Reconsider only with a profile that attributes
  measurable time to the flush lookup itself. The next candidate for that title is to unregister
  a surface after a small CPU read has flushed it in full, which mirrors the CPU-write path and
  lets the remaining reads of the frame take the fast memory path.
- Diagnostics kept from that work: `LOG_DEBUG(HW_GPU, ...)` lines at every point where
  `AccelerateTextureCopy` and `AccelerateDisplayTransfer` refuse acceleration. Enable them with
  `log_filter = *:Info HW.GPU:Debug` under `[Miscellaneous]` in `config.ini`. A per-title
  `Compatibility.skip_texture_copy_fallback` key has no effect, because `src/video_core/gpu.cpp`
  sets that value from the hack list with a `false` default. Add a hack-list entry instead.
- Device automation: the `thor` MCP server in `tools/thor-mcp/server.py` is the supported way to
  drive the Thor. Send button presses as held presses; the guest samples input once per frame and
  a plain `input keyevent` tap is missed. Launch a title through the tree-form content URI that
  the app holds a grant for; the plain document URI crashes `EmulationFragment.onCreate` with a
  `SecurityException`. Install A/B builds with `adb install -r -d`; the working tree's version
  code is newer than any kept control APK.
- E.X. Troopers (`0004000000053700`, English patch v1.0.2) on 2026-09-18: videos present at 30 FPS
  at Speed 100% with the GPU near idle. The save-slot and episode screens present at 60 FPS at
  Speed 100%, but KGSL GPU busy is 99.9% at 2x and SurfaceFlinger shows a mix of 16.9 ms and
  33.7 ms intervals, so presentation drops frames there. The GSP command time on those screens is
  about 4.7 ms per frame. Treat that screen as the efficiency target for this title and rank it
  from the frame profiler counters before changing code. Gameplay was not reached within the
  first six minutes of intro videos.
- Bug, open: opening the E.X. Troopers pause menu during a video freezes emulation. The overlay
  stops updating, every NativeEmulation thread sleeps, and the VulkanWorker thread logs
  `dequeueBuffer timed out: Function not implemented (-38)` in a loop. Reproduced twice in one
  session. The log buffer filled with that line, so the trigger context was lost; reproduce with
  `logcat -G 64M` and a filter that drops the timeout line before reading the fork's own
  presentation messages. Until it is fixed, do not press START during an E.X. Troopers video.
- Device automation facts added 2026-09-18: a screen timeout pauses the app and a wake sequence
  with the MENU key while the screen is off brings the secondary-display launcher over the app;
  the present loop then spins on the same `dequeueBuffer` timeout. Keep the screen on with
  `svc power stayon usb` during a session and set it back to `false` at the end. Send A presses to
  advance videos and dialogs; START pauses.
- E.X. Troopers efficiency ranking from the profiler (2026-09-18): 67% of draws take the software
  vertex path because of a geometry-shader mode that `RasterizerVulkan::AccelerateDrawBatch`
  refuses, and the renderer begins about 221 render passes per swap with 236 image barriers.
  Those two facts, not shader cost, saturate the Adreno 740 at 2x on the menu screens. Any work
  on this title starts with one of them and must be measured with the counters above.
- Medarot 9 30 FPS code (2026-09-18): `src/android/app/src/main/assets/cheats/0004000000174F00.txt`
  holds `30 FPS - game speed 1.5x - experimental` and a 60 FPS variant, both disabled by default.
  They replace `ldrh r1, [r4, #0x4e]` at `0x003D8268` (word `E1D414BE`), the load of the vsync
  target 3 passed to the frame function at `0x0040084C`, with `mov r1, #2` or `mov r1, #1`. The
  frame rate follows, but the game logic is frame-stepped and runs proportionally faster. Do not
  present these as a finished patch. Finding and scaling the frame-counted timers is the open
  step. The method that located the pacing loop, a kernel trace of blocking waits and address
  arbitration with an on-demand memory dump, is recorded in the notes and is temporary code, not
  a feature.
- Correction, 2026-09-18, later the same day: the Medarot 9 codes now patch the game object's
  vsync field at `0x0801DA96` (16-bit write `1801DA96 00000002` for 30 FPS, `00000001` for
  60 FPS) instead of the load instruction. The game reads that field for its logic step as well,
  so the intro pacing at 30 FPS matches 20 FPS second for second, with Speed 100%. The codes are
  named `30 FPS - Thor Experiment` and `60 FPS - Thor Experiment` and ship disabled. The object
  address is a heap address that was identical in every launch of this session; a patch to the
  field is the correct form, and the earlier instruction patch is withdrawn. A launch while the
  Thor's screen is off aborts with `surface is nullptr`; keep the screen on before launching.
- E.X. Troopers geometry shaders (2026-09-18): point mode, shader topology, inputs from vertex
  outputs, three small programs. `PicaCore::ProcessDraw` rejects every geometry shader before
  the backend is asked, so the backend point-mode checks are dead code. A hardware path is a
  multi-day feature (decompiler emit support, geometry uniforms, output-map-driven fragment
  interface, pipeline and disk cache stage, register-preservation guard). It would move the
  CPU vertex and geometry work to the GPU and remove the 520 immediate draws per frame. It would
  not remove the render pass switches and barriers that saturate the GPU on the menu screens.
  Rank the barrier cost first if the goal is 60 FPS at 3x on this title.
- Point-mode geometry programs run inside the host vertex shader (2026-09-18).
  `RasterizerVulkan::AccelerateGeometryDrawBatch` accepts a geometry draw only when the mode is
  Point, the topology is Shader, one vertex feeds one invocation, and
  `Pica::Shader::AnalyzeGeometryProgram` proves that no reachable instruction reads a temporary,
  output, condition, or address register that the same invocation did not write. That check is
  a must-write dataflow over the program's control flow graph. It is the correctness argument:
  the PICA geometry unit keeps its registers between invocations and the host shader cannot.
  E.X. Troopers program 0x2E passes and program 0x58 fails at instruction 133 (a header vertex
  stores matrix rows in registers 10 to 15 for later vertices); keep 0x58 in software. The fused
  shader draws three host vertices per reachable EMIT, one instance per PICA vertex, gathers
  indexed vertices on the CPU, repeats the fixed attribute block once per instance instead of a
  zero stride, and keeps the three emit slots in named variables instead of a dynamically
  indexed array. Its uniforms use buffer binding 6 (dynamic offset index 3). Its pipelines are
  not written to the pipeline disk cache (`PipelineCache::IsVertexShaderTransient`). Measured on
  the save-slot screen at 2x: accelerated draws 33% to 95%, 93,773 of 103,821 geometry draws
  expanded, output identical by inspection. The GPU stayed at 99.9% busy and the frame rate did
  not move: the pass restarts, not the draw path, hold that screen.
- Render pass restart reasons (2026-09-18), from the profiler counters
  `renderpass_restart color_switch/depth_toggle/same_images/area_shrink/area_grow/area_other`:
  on the E.X. Troopers save-slot screen 221 to 252 begins per swap split into 121 to 140 color
  target switches, 28 depth attachment toggles, 39 to 45 render area changes, and about 34
  passes that follow a blit, copy, or submit. Two candidates exist in `vk_rasterizer.cpp` behind
  `kRetainDepthAttachment` (keep the open pass's depth attachment for a draw that neither tests
  nor writes depth) and `kFullRenderArea` (begin a pass with the framebuffer rectangle when it
  is at most twice the draw rectangle). Both are off and unmeasured: the device fault below
  stopped every Vulkan launch before their A/B. Do not enable one without the matrix.
- Thor GPU fault (open, 2026-09-18): the kernel logs `kgsl-3d0: CP: AHB bus error` bursts and
  the app then spins on `dequeueBuffer timed out` (the buffer queue returns INVALID_OPERATION
  while the driver holds every swapchain image). From 12:44 every Vulkan launch of every title
  faulted one to three seconds in, on the unmodified production build too, after a warm reboot
  too. Ruled out with evidence in the notes: the fork's code, the emulator's transferable and
  pipeline caches, the app's private driver files (the extracted Turnip R8 hashes equal its zip;
  the redirect directory is empty), config.ini, the per-title ini, the user directory, the ROM
  (hashes identical twice), thermal state, GPU power levels, display modes, Android GPU debug
  layers, and a running tracer. OpenGL launches of the same title log no fault. Use the
  `gpu_faults` tool before and after any Vulkan measurement; a run with a new burst is void.
- Device capabilities live in the MCP server (2026-09-18). When a test needs something the
  server cannot do, add a tool there; do not add an exported Android component. Maintenance
  inside the app's private storage goes through `app_maintenance`: the app reads
  `thor_maintenance.txt` from the user directory at startup, runs one of list, verify,
  clear_redirect, reinstall_driver, or system_driver on its own driver directories, and writes
  `log/thor_maintenance.json`.
- Thor GPU fault, additional facts (2026-09-18 evening): the system Qualcomm Vulkan driver logs
  no fault but the app still stalls with the GPU idle, so the fault lines are Turnip's
  reaction, not the cause; a full power-off does not clear the condition; no emulator setting
  and no Turnip build avoids it; software vertex shaders avoid the stall but not the fault.
  Until the cause is found, take Vulkan measurements only after `gpu_faults` shows no new
  burst during the run, and prefer scenes reached without a fresh launch.
- Thor GPU fault, decoded (2026-09-18 evening): the failing access is a write to
  `RB_CCU_CNTL` (0x8e07), issued by Turnip for every command buffer, refused by the render
  backend on this unit since 12:44 on every Turnip build and every setting, never on the
  system Qualcomm driver, never on OpenGL. That is outside this fork's code. Report it to the
  Turnip package maintainer with the kernel line and the register. Until it clears, measure
  Vulkan work in scenes reached without a fresh launch or on the OpenGL backend, and keep
  `gpu_faults` around every run.
- Driver split (2026-09-18 evening). The expanded geometry draw runs on Turnip and hangs the
  Qualcomm proprietary driver on the Adreno 740 without a kernel fault; `AccelerateGeometryDrawBatch`
  keeps those draws in software when `Instance::GetDriverID()` is the Qualcomm driver. The
  system driver is the working path on the test unit while Turnip faults: the control build at
  the E.X. Troopers save-slot screen gave 56.6 to 59.3 FPS mean with 16.86 ms P95 on it, against
  40 FPS with 33.7 ms P95 on Turnip R8 this morning, both at 99.9% GPU busy at 2x. That is one
  scene of one title; it is not a general ranking of the drivers. Any driver-specific behavior
  goes behind a driver id check with a dated note here, never behind a build flag.
- Render pass merging, accepted (2026-09-18 evening). `RasterizerVulkan::Draw` begins a pass
  with the framebuffer's full rectangle when that rectangle is at most twice the draw
  rectangle's area (`kFullRenderArea`), and keeps the open pass's depth attachment for a draw
  that neither tests nor writes depth when the same color and depth buffers are still bound
  (`kRetainDepthAttachment`). The dynamic scissor still limits every draw to its rectangle,
  and a pass that started without depth stays without it. Matched on the Thor with the system
  Qualcomm Vulkan driver at the E.X. Troopers save-slot screen, 2x, same session, two runs
  each: base 57.9 and 58.9 FPS at 99.9% GPU busy; both changes on 59.3 and 59.3 FPS at 91.8%
  and 91.3%. The full rectangle alone gave 95.6%; the depth retention alone gave no change.
  The pictures match the reference. Keep both on. The color target switches, 121 to 140 per
  swap, remain and are the next target; they need the game's render-to-texture pattern
  understood, not a pass-level trick.
- GPU keepalive, rejected (2026-09-18 evening): an empty submission every 40 ms to hold the
  GPU out of inter-frame power collapse did not stop the Turnip fault; it produced more fault
  lines and the same stall. Do not restore it.
- Stalled acquire recovery (2026-09-18 evening, untested on a fault): after 4000 acquire
  retries the present window marks the swapchain for recreation instead of spinning forever.
  On a GPU that never completes its work the recreate path waits on the queue; measure it on
  the next fault before you rely on it.
- Flush heuristic on the Qualcomm driver (2026-09-18 evening): removing the per-20-draw
  flush for that driver changed nothing at the E.X. Troopers menu (91.7% and 93.0% against
  91.1% GPU busy at 59.3 FPS). Rejected for now; measure it in the ice-field scene before you
  try it again.
- Next target for E.X. Troopers 3D scenes on the system driver (2026-09-18 evening): the
  ice-field scene holds 51.9 FPS at 99.9% GPU at 2x with about 98 pass begins per swap, of
  which 64 are color target switches, plus 5 texture blits per swap at 1.8 Mpix. Those come
  from the game's render-to-texture and display-transfer pattern. The pass-level merges are
  done; the next step needs that pattern understood from a per-draw log of targets and
  samplers, not another pass-level heuristic.
- Thor GPU fault, cause and fix (2026-09-18 night). The `CP: AHB bus error` lines are Turnip
  writes to render-backend registers from the BV pipe. `tu6_init_static_regs` runs under
  `CP_SET_THREAD_BOTH`; upstream confines RB_DBG_ECO_CNTL, RB_RBP_CNTL, and RB_UNKNOWN_8E09 to
  BR with a THREAD_MODE conditional but not RB_CCU_CNTL (0x8e07) and not the RB entries of the
  a740 raw magic table (0x8e79). The Thor kernel arms the CP AHB timeout detector, so every
  such write logs. Evidence: a binary patch that turned the 0x8e07 packet into CP_NOP moved
  the fault to 0x8e79. Fix: `tools/turnip/patches/0001-*.patch` on Mesa 26.2.2 wraps both in
  the same conditional. The driver is built by `tools/turnip/build.sh` in WSL, packaged by
  `tools/turnip/package.py`, and shipped in the APK under `assets/gpu_drivers/`.
  `BundledGpuDriver` copies it into `gpu_drivers/` and selects it once per bundled version at
  the first start; a later manual driver choice is kept. Result: E.X. Troopers ran 45 minutes
  on Turnip with zero new fault lines. The fault is closed; `gpu_faults` stays as a guard.
- The E.X. Troopers freeze is not the GPU fault. It is a present-path wait cycle: after
  `swapchain.MarkForRecreation()` or an out-of-date result on an unchanged window,
  `RecreateSwapchain` on Android waits for a surface that never comes, the worker's acquires
  then hit the compositor's dequeued-buffer limit (`dequeueBuffer timed out: Function not
  implemented (-38)`, about 4000 lines in one second), and every thread ends idle. See the
  next entry for the fix.
- Present-path freeze, fixed (2026-09-18 night). `PresentWindow::RecreateSwapchain` on Android
  waited for a replacement surface in every case. A rebuild that the acquire-retry bound or an
  out-of-date result requests on a window that is still alive now reuses the current surface;
  only a destroyed window waits for the next `surfaceChanged()`. Every rebuild bumps a
  generation counter; a frame whose image was acquired from an older generation is dropped in
  `FinishPresent` instead of presented into the new swapchain. The retry bound is 1000 (about
  one second). Verified on the bundled Turnip: nine minutes of E.X. Troopers with three stall
  events, each followed by a rebuild and a running game, where the previous build froze at the
  first one. The stalls themselves come from outside the app: the system re-adds both display
  viewports every three seconds during each one, so the compositor holds every image for ten
  to thirteen seconds. Their trigger is open.
- Acquire ordering, accepted (2026-09-19). The stall behind the E.X. Troopers rebuilds was an
  ordering inversion inside the emulator: a direct-present acquire that returned not-ready
  fell back to the copy path, later frames then acquired directly on the emulation thread,
  and the two direct images filled the compositor's dequeued-buffer limit while the worker
  waited for the copy frame's image. `TryPrepareDirectPresent` now counts every copy-path
  frame in `pending_copy_acquires`; while the count is non-zero every frame takes the copy
  path, so acquires stay in submission order, and the worker releases the count when its
  acquire completes. The emulation thread never waits for an image. A rejected variant made
  the emulation thread retry the direct acquire for up to half a second: it cost one vsync per
  window per frame and held the snow field at 30 FPS with the GPU at 68% (same field FPS
  afterwards, so that scene is 30 FPS by the game's own loop; the mech fight is 60). Result:
  two nine-minute runs with zero stalls and zero rebuilds, and the present-thread timeout lines
  fell from thousands to under 200 per run.
- Stream ring sizes, accepted (2026-09-19). A scene issues about 2000 draws per frame, and each
  draw can map the vertex ring, the uniform ring, and the lookup-table ring. A ring that holds
  about one frame of data wraps every frame, and the next map then waits on the frame still in
  flight: the emulation thread and the GPU ran in lockstep instead of overlapping. Measured on
  the E.X. Troopers snow field with new counters (`blocking_waits_per_swap`,
  `blocking_wait_ms_per_swap`, `stream_wraps_per_swap`): exactly one blocking wait per swap
  costing 21.5 ms, with the lookup-table ring wrapping every 32 to 37 ms. `TEXTURE_BUFFER_SIZE`
  is now 32 MiB (still clamped by the device's texel buffer limit) and `UNIFORM_BUFFER_SIZE` is
  64 MiB. After the change the blocking wait is 0.23 to 0.42 ms per swap and that ring wraps
  every 370 to 406 ms. The frame went from 34.5 ms to 21.5 ms, guest command processing from
  28.6 ms to 6.3 ms, and the scene from 29 to 46 FPS. The GPU then sits at 99.9% and 680 MHz, so
  the scene is GPU bound from here and the render path is the next target. Cost: about 116 MiB
  more host-visible memory.
- `Scheduler::Wait` answers an already-passed tick from the cached GPU tick and returns before
  the timer and the driver call. The rings ask about nearly every allocation they recycle, about
  7700 times per swap, so this path must stay cheap. Keep the blocking counters: they separate a
  real stall from a cheap query and they found this bug.
- Runtime command channel (2026-09-19): the app polls `thor_command.txt` once per second while a
  game runs and answers in `log/thor_command.json`, echoing the request id the host sends. It
  carries save states, load states, and the performance numbers. Do not delete the result file
  from the host; the app overwrites it in place, because removing it underneath the storage
  provider leaves a stale entry. This is the same user-directory channel as ThorMaintenance and
  it adds no exported component.
- Render pass structure, measured (2026-09-19). A per-frame trace of the pass sequence
  (`ThorPasses`, profiling builds only, one frame per second) shows the passes are the game's
  own. One E.X. Troopers frame is about 90 passes over 10 colour targets: one main pass of 271
  draws into a 512x1024 target, and around 13 tiny passes of one draw each that ping-pong
  between small targets, 128x40 down to 32x64, repeated about six times per frame. That is a
  bloom or blur chain in the guest, so the 62 colour target switches per frame cannot be
  removed from the emulator side. Only their cost can be reduced.
- Full render area, re-tested and kept on (2026-09-19). Turning `kFullRenderArea` off in the
  E.X. Troopers engine scene made the frame 11.07 ms at 92.8% GPU and 680 MHz, against 8.97 ms
  at 74.1% and 615 MHz with it on, although it lowered the pass count from 90 to 77. Fewer
  passes are not automatically cheaper: expanding a pass to the framebuffer rectangle wins more
  from reuse than it loses in tile traffic. Do not turn it off again without this measurement.
- Resolution scaling of the same scene (2026-09-19), used to split fixed cost from pixel cost:
  1x 8.40 ms frame at 36.3% GPU, 2x 8.97 ms at 74.1%, 3x 10.29 ms at 99.1% and 680 MHz. That
  fits about 2.0 ms of fixed cost per frame plus about 1.0 ms per unit of 1x pixels, so at 2x
  roughly a third of the GPU time is per-pass overhead and two thirds scales with area. Both
  attachments always store, so every pass writes its render area back to memory; relaxing the
  depth store is not safe here because the same depth surface is reused across passes.
- Open, and the next step if this is taken further: per-pass GPU timestamps. The counters and
  the trace name the structure but not which passes spend the time. Do not change the render
  path again without that; two hypotheses were already disproved by measurement today.
- Turnip render mode, accepted (2026-09-19). Turnip chooses tiled rendering or rendering
  straight to memory for each render pass. A 3DS frame is about ninety passes over small
  targets, and the chooser picks tiled rendering, paying a binning and tile cost those targets
  never earn back. `GpuDriverHelper` now sets `TU_DEBUG=sysmem` when the active driver reports
  vendor Mesa, and a `TU_DEBUG` line in `thor_driver_env.txt` still overrides it. Measured on
  the E.X. Troopers engine scene at 2x from one save state, same build, same session: default
  75.6% speed and 44.9 FPS with 11.6 ms waiting in swap, forced direct path 96.1% and 57.3 FPS
  with 6.0 ms. Forcing tiled rendering (`gmem`) gave 75.2% and `noconcurrentresolves` 75.4%, so
  the chooser is picking tiled rendering by itself. The setting reaches Mesa drivers only; the
  system Qualcomm driver ignores it.
- Resolution scaling with that setting, same scene and state: 2x 96.2%, 3x 47.4%, 4x 26.4%. GPU
  time is linear in pixels, about 17 ms per frame at 2x, so 3x and 4x need the per-pixel cost
  cut, not another pass-level trick. Two compression theories were checked against the Mesa
  source and rejected: this GPU sets `supports_uav_ubwc`, so the speculative storage usage on
  RGBA8 surfaces does not disable compression, and the format list we pass is compression
  compatible. Disabling low-resolution Z changed nothing, so it is already not helping.
- A save state records the emulator build. After a rebuild the app asks before loading one, and
  a run that does not answer measures the title screen instead of the scene. `emu_command
  load_state` now answers that dialog itself. Always confirm the scene with a screenshot before
  trusting a number.

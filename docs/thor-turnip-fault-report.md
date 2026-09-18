# Turnip GPU fault report: refused write to RB_CCU_CNTL on the AYN Thor

Draft for the Turnip package maintainer (K11MCH1 AdrenoToolsDrivers) and for a Mesa issue.
Evidence dates: 2026-09-18. Device: AYN Thor, Snapdragon 8 Gen 2, Adreno 740 v2, Android 13,
kernel 5.15.123-android13-8, KGSL. App: Azahar Thor fork (org.azahar_emu.azahar.debug).

## Symptom

- Every Vulkan launch with a Turnip driver logs, one to three seconds in:
  `kgsl kgsl-3d0: CP: AHB bus error, CP_RL_ERROR_DETAILS_0:0x10008e07 CP_RL_ERROR_DETAILS_1:0x12144`
  The kernel rate-limits the callback (`gen7_err_callback: 202 callbacks suppressed`); the errors
  are continuous while the process lives and repeat in groups every five seconds, which are the
  fault-tolerance replays. The first detail word names register 0x8e07, `RB_CCU_CNTL`.
- The app then holds every swapchain image and spins on `dequeueBuffer timed out` (the buffer
  queue returns INVALID_OPERATION). Some titles recover; one title never does.

## Drivers tried, all fault

- Mesa Turnip driver v26.0.0 R8 (KIMCHI), R8 Sysmem variant, Turnip Adreno Driver T30
  (Mr_Purple_666), Mesa Turnip v26.3.0-20260918-r6 (Mesa main 590bf21, Vulkan 1.4.363).
- `TU_DEBUG` flags nobin, noubwc, nolrz, sysmem, syncdraw, gmem, and their combination: all
  fault. `FD_RD_DUMP=enable` delays the fault but does not remove it.

## What does not fault

- The system Qualcomm Vulkan driver (Adreno Vulkan Driver 512.676.53): zero fault lines, same
  app, same titles, same scenes.
- OpenGL ES through the system driver: zero fault lines.

## Ruled out on the device

Emulator caches, the extracted driver (SHA-256 equals the zip entry), the driver file redirect
directory (empty), app private data (unchanged for three days), config, the user directory, the
ROM (hashed twice), thermal state (36 to 55 C, no throttling), every GPU clock floor (401, 550,
615 MHz), inter-frame power collapse and clock gating (both on, root-only), display modes and
rotation, Android GPU debug layers, a running tracer, a warm reboot, a full power-off.

## What changed when it began

Nothing on the device that could be found. The same driver build ran the same title for hours
before 12:44 on the same day with only sporadic faults during heavy scenes.

## Kernel state

`ifpc=1 hwcg=1 force_no_nap=1 min_pwrlevel=1 max_pwrlevel=0 ft_policy=0xC2 ifpc_count=0x14BF9`
(the power collapse count grows continuously). Firmware: a740_sqe.fw, gmu_gen70200.bin,
a740_zap.mbn loaded through the sysfs fallback.

## Ask

Is the `RB_CCU_CNTL` write in the command buffer preamble expected to be refused when the render
backend is powered down between frames on this kernel, and is there a driver-side wait or an
alternative programming path for it?

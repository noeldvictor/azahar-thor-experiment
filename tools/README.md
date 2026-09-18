# Tools

This directory contains several scripts which are intended to both document and ease the convenience of certain development processes.

Most of the scripts in this directory assume that your current working directory is the Azahar root directory and the script is being called via `./tools/xxx.sh`.

## Thor Fork Note

This README is mostly inherited from upstream Azahar. It is not the release flow for Azahar Thor Experiment.

For this fork, Android/Thor builds use:

```powershell
cd src/android
.\gradlew.bat :app:assembleVanillaRelWithDebInfoLite
```

Do not treat the Google Play, Flathub, Internet Archive, or RetroArch checklist below as active work for this personal Thor fork unless the user explicitly asks.

## Thor battery-power gate

`measure-thor-power.ps1` samples the AYN Thor's battery power and temperature over Wi-Fi ADB. It is
read-only apart from optional temporary screenshots, which it removes from the device. Run its
built-in deterministic checks before using it:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\measure-thor-power.ps1 -SelfTest
```

For the accepted 7th Dragon scene, physically unplug the Thor, select device-wide Standard mode,
leave fan mode 4 and a fixed recorded brightness, launch the production Lite APK into the exact
scene, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\measure-thor-power.ps1 `
  -ExpectedBrightness <recorded-value> `
  -ExpectedScreenshotSha256 E831B2637B609C064C21C0E7531D74DC30ADC5EB3F344466C43D6BF750A3F13C
```

Defaults provide a 60-second warmup followed by 180 seconds of one-second samples. The gate passes
only when both mean and nearest-rank P95 battery power are at most 6 W. It records raw current,
voltage, direct and averaged power nodes, charge counter, battery temperature/capacity, charger
flags, Azahar process CPU ticks, KGSL GPU busy/clock data, package and device metadata, config hash,
and before/after screenshot hashes under `thor-power-results/`. The JSON summary includes a
charge-counter-derived average as an independent coarse cross-check; `power_now` remains the gate's
per-sample source when that node is plausible.

The script deliberately fails instead of producing a watt claim if the battery is simulated, any
external-power flag is present, a charger appears during sampling, the app exits, hardware is not an
AYN Thor, ADB is not using a host:port endpoint, the package is debuggable/non-ARM64, or the expected
version, config, performance mode, fan mode, brightness, or frame hash differs. It requires manual
brightness mode and exactly two active displays, records the display service's brightness for each
physical panel, and rejects a panel disappearing, turning off, or changing brightness during warmup
or sampling. When `-ExpectedBrightness` is supplied, both panels must match Android 13's normalized
value for that integer setting; dimming only the primary panel is rejected. Override an expected
value explicitly when validating a newer accepted build; do not weaken the charger or simulated-
battery checks. It also rejects an idle or frozen fixed scene unless the run averages at least 10
Azahar process CPU ticks per second and 1% KGSL GPU busy. Those deliberately loose defaults validate
that the accepted 7th Dragon workload stayed active; explicitly recalibrate them for a materially
different title or scene instead of treating them as speed targets. At the end of warmup and again
after sampling, the script also requires both Azahar BLAST layers to exist and at least one to expose
60 presentation intervals with at least 29 FPS mean, no more than 40 ms P95, and no interval over
50 ms. On this Thor firmware only the primary-display layer exposes SurfaceFlinger latency history;
the second physical-panel layer is still required to be present. These defaults are specific to the
fixed 30 FPS 7th Dragon scene and should be explicitly overridden for another target frame rate.
The same warmup/end checks require one unchanged active AudioFlinger track at 32,728 Hz, no more
than 2,048 frames or 150 ms reported latency, and zero underruns. This prevents a lower-wakeup audio
configuration from passing power and frame-pacing checks while introducing crackle or input-to-audio
delay.

The strict default also requires production version `6987ffd79-vanilla-thor` and the app's one-time
JSON driver record to identify `Mesa Turnip driver v26.0.0 - R8`, `Vulkan 1.4.335`, and
`vulkan.ad07xx.so`. Generic and forced-Sysmem R8 have the same Mesa runtime banner, so that banner is
not accepted as package identity. The tool records parsed driver metadata in `summary.json` and
fails before sampling if the structured record is absent or mismatched. Override
`-ExpectedVulkanDriverName`, `-ExpectedVulkanDriverVersion`, and
`-ExpectedVulkanDriverLibraryName` only as part of an explicit matched driver experiment.

The built-in config and screenshot expectations intentionally describe the accepted 3x scene. For
an explicit 3x/2x/1x resolution matrix, override `-ExpectedConfigSha256` and
`-ExpectedScreenshotSha256` together for each row, and keep every other scene, build, renderer,
driver, device-mode, fan, brightness, and display-layout variable fixed. Never compare a row against
another resolution's hash or treat an AC-powered KGSL-busy reduction as a battery-watt result.

The Thor exposes separate `panel0-backlight` and `panel1-backlight` sysfs devices, but this firmware
reported `actual_brightness=0` for both while both panels were visibly ON. Do not use those raw nodes
as luminance evidence. `dumpsys display` is the authoritative automation source observed here: at
Android brightness 255 it reported both physical displays ON with brightness 1.0. Full-scale panel
brightness is a major uncontrolled variable near a 6 W device budget, so use a repeatable lower
manual brightness for the eventual unplugged matrix and pass its integer setting explicitly.
Changing `settings put system screen_brightness` affects only display 0 on this firmware. The
installed Dual Screen Assistant instead calls Android 13's hidden
`DisplayManager.setBrightness(displayId, float)` for display 4; changing the persisted
`dual_screen_brightness_level` key alone did not actuate that panel. For this exact firmware,
transaction 35 was verified from `/system/framework/framework.jar` as `setBrightness(int,float)`:

```powershell
adb -s 192.168.1.33:5555 shell settings put system screen_brightness 48
adb -s 192.168.1.33:5555 shell service call display 35 i32 4 f 0.18503937
```

Those values set both panels to the same normalized brightness; the device display configuration
maps approximately 0.185 to 95 nits. Record the original values first and restore them after an
experiment. Treat the Binder transaction number as firmware-specific and verify it from the
device framework after any Thor OS update. The strict tool allows 0.01 normalized tolerance for
firmware rounding and color/brightness synchronization but still rejects a panel left at a
materially different level.

## Upstream Release Checklist

The upstream release checklist was removed from this fork-facing README because it is obsolete for Azahar Thor Experiment. This fork does not publish Google Play, Flathub, Internet Archive, compatibility-list, translation, or RetroArch releases.

### Note:

For reasons unknown, some part of the translation update process can inexplicably produce files with different content depending on the environment in which it is running, even when using the same version of the tool and the same distro.

For consistency, when updating the translations to be committed to the repository, always perform the update within the the Docker environment we use for Azahar's CI (`opensauce04/azahar-build-environment:latest`).

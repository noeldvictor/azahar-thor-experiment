# Android Per-Title Settings

Each `<16-hex title id>.ini` here is a **bundled default per-title profile**. On first run the app
copies every file in this directory into `<user dir>/GameSettings/` unless a file with that name
already exists there, so user edits always win and are never overwritten by an update.

At game launch, native code reloads `config.ini` and then overlays `GameSettings/<title id>.ini`
for that session only. Overlays are **sparse**: only keys present in the file replace the global
value; every other setting keeps whatever `config.ini` says. Nothing is written back to
`config.ini`.

Users edit these in-app: long-press a game, open **Game Settings**. That screen shows the General,
System, Graphics, Layout, Audio, Storage, and Utility sections; saving writes only the values that
differ from the global configuration (plus any override that already existed). Controls, Camera,
Network, Debugging, and Miscellaneous are intentionally global-only and are ignored if placed in
a per-title file.

Recognised sections and keys are the same names used by `config.ini`, plus:

- `[Compatibility] skip_texture_copy_fallback` - per-title core hack toggle.

Target hardware is AYN Thor Base/Pro/Max: Snapdragon 8 Gen 2 and Adreno 740. Do not use Thor Lite /
Snapdragon 865 behavior as the default profile target unless it is explicitly documented.

This directory is the per-title settings database. When a measurement finds the setting a title
wants, put it in that title's file here with the scene and the numbers that justify it, so every
user gets it without having to discover it. A user can still change any of it in **Game
Settings**, and their file is never overwritten by an update.

Keep `docs/thor-optimization-notes.md` and `AGENTS.md` current when a bundled profile changes.

- `0004000000053700.ini` - E.X. Troopers: 2x resolution, custom textures off, normal frame limit,
  and the texture-copy fallback skip enabled for smoother Thor play. Re-measured on 2026-09-19 in
  the snow field, the heaviest scene found, after the fragment depth write was removed: 2x holds
  full speed and reaches 114% in fast forward, 3x reaches 61.5% and 4x 37.8%, both still limited
  by the GPU. Keep this profile at 2x until the render path gets cheaper.
- `0004000000112C00.ini` - Conception II: 5x resolution so both Thor panels downscale instead of
  stretching, Anime4K texture filter off, Snapdragon GSR screen filter.

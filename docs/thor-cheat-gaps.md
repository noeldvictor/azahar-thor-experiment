# Thor Cheat Gap Inventory

Re-scanned on 2026-09-20 from Thor device `c3ca0370`. This replaces the 2026-05-09 snapshot,
whose title ids came from a header parse that produced one impossible `0004000000000000` entry.

Title ids here are read out of the ROMs exactly: a `.zcci` is a 0x60 byte `Z3DS` header followed
by zstd frames of 256 KiB, so decompressing the first frame gives the NCSD header and the media
id at offset 0x108. Validated against the one ROM whose filename carries its id.

ROM source: `/storage/2664-21DE/Roms/n3ds/zcci`
Cheat source: `/storage/emulated/0/Azaharuser/cheats`

| | count |
| --- | --- |
| ROMs with a title id read from the header | 104 |
| Cheat files on the device | 637 |
| Library titles **with** a cheat file | 78 |
| Library titles **without** | 26 |
| Present but effectively empty | 1 |
| Bundled in the APK | 8 |

Most of the device's 637 cheat files are for titles not in this library, so the useful number is
the 26 below rather than the raw total.

## Library titles with no cheat file

| Title ID | Game |
| --- | --- |
| `0004000000095800` | Art Academy - Lessons for Everyone |
| `00040000000E7600` | Attack of the Friday Monsters A Tokyo Tale (USA) (eShop) |
| `0004000000096600` | Castlevania - Lords of Shadow - Mirror of Fate (USA) (En,Fr,Es) |
| `000400000004D200` | Cave Story 3D |
| `0004000000132500` | Code Name - S.T.E.A.M. |
| `00040000000BBF00` | Crimson Shroud |
| `00040000001C1E00` | Detective Pikachu |
| `0004000000056200` | Doctor Lautrec and the Forgotten Knights |
| `00040000000C2D00` | HarmoKnight (U) [00040000000C2D00] [USA] |
| `0004000000074000` | Heroes of Ruin |
| `00040000000F9900` | Hometown Story |
| `00040000001CBC00` | Jake Hunter Detective Story - Ghost of the Dusk |
| `00040000001C4E00` | Mario Party - The Top 100 |
| `0004000000188C00` | Mario Sports Superstars (USA) |
| `0004000000182800` | Petit Novel series - Harvest December |
| `00040000000D0900` | Pokemon Art Academy |
| `00040000000F3000` | Professor Layton and the Azran Legacy |
| `00040000000A8500` | Professor Layton and the Miracle Mask |
| `0004000000160C00` | Project X Zone 2 (W) [0004000000160C00] [WORLD] |
| `0004000000036400` | Rayman 3D (U) [0004000000036400] [USA] |
| `000400000018CC00` | Return to Popolocrois - A Story of Seasons Fairytale [USA] |
| `00040000000B3500` | Sonic & All-Stars Racing Transformed (USA) (En,Fr,Es) |
| `00040000000D9900` | The Starship Damrey [eShop] |
| `0004000000115100` | Weapon Shop de Omasse (USA) (eShop) |
| `0004000000188500` | Yu-Gi-Oh English Patched [0004000000188500] [UNK] |
| `0004000000096700` | Zero Escape - Virtues Last Reward |

## Present but effectively empty

| Title ID | Game | Lines |
| --- | --- | --- |
| `00040000000BA800` | Pokemon Mystery Dungeon - Gates to Infinity | 2 |

## How to re-scan

The `bench` tool in the `thor` MCP server maps a title id to its ROM with the same header read,
so the mapping does not need rebuilding by hand. Compare that map against
`/storage/emulated/0/Azaharuser/cheats` for a current list.

Performance work belongs in `docs/thor-optimization-notes.md`.

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

## What the public databases have, checked 2026-09-20

The JourneyOver CTRPF-AR-CHEAT-CODES repository is the usual GitHub mirror of the GBAtemp CTRPF
database. Its full file tree was fetched and matched against this library by title id.

| | result |
| --- | --- |
| Title ids in the mirror | 628 |
| Overlap with this library | 69 of 104 |
| Of the 26 missing, found by title id | **0** |
| Of the 26 missing, same game under another region | 5 |

**The mirror is not the whole GBAtemp database, so a miss there is not a miss on GBAtemp.**
E.X. Troopers is the proof: its codes exist in the GBAtemp thread and were added to this fork from
there by hand, and the mirror does not carry that title at all. Any future sweep has to treat this
repository as a lower bound.

The five that exist for another region are listed below. Regional builds have different addresses,
exactly as the E.X. Troopers English patch differs from the Japanese release, so these are leads
for rebasing rather than codes that can be copied across.

| Our title | Our id | In the database as | Its id |
| --- | --- | --- | --- |
| Cave Story 3D | `000400000004D200` | Cave Story 3D (USA) | `000400000004A100` |
| Doctor Lautrec and the Forgotten Knights | `0004000000056200` | same (USA) | `0004000000036800` |
| Mario Sports Superstars | `0004000000188C00` | same (EUR) | `0004000000188D00` |
| Professor Layton and the Azran Legacy | `00040000000F3000` | same (GER, also FRA) | `00040000000F3100` |
| Return to PoPoLoCrois | `000400000018CC00` | same (EUR) | `0004000000188F00` |

Two name matches were rejected as different games: Castlevania Lords of Shadow Mirror of Fate
against a Castlevania NES inject, and Project X Zone 2 against the first Project X Zone.

GBAtemp itself returns HTTP 403 to automated fetching, as does GameFAQs, so the thread has to be
read by hand. The repository's `ActionReplay.7z` was checked in case it held a fuller set; it
contains only the plugin binary.

## Which of the 26 GBAtemp appears to have, searched 2026-09-20

GBAtemp returns 403 to automated fetching, but its threads are indexed, so each gap was searched
by title id with results restricted to gbatemp.net. **Everything in this table comes from search
engine summaries of those pages, not from reading the pages.** Two such summaries were already
wrong today, one about who authored a set of codes and one claiming this game runs at 30 FPS when
the device measures 60, so treat every row as a lead to verify rather than a fact. Nothing here
has been added to the bundled cheats.

| Title | Id | Reported on GBAtemp | Form |
| --- | --- | --- | --- |
| Castlevania: Lords of Shadow - Mirror of Fate | `0004000000096600` | Infinite HP and MP for Alucard, enemies do no damage, 3D depth | AR text, hex quoted |
| Code Name: S.T.E.A.M. | `0004000000132500` | 3D depth and parallax barrier | AR text, hex quoted |
| Detective Pikachu | `00040000001C1E00` | Walk through wall on B, walk speed, Tim and Pikachu size | AR text, multipliers quoted |
| Project X Zone 2 | `0004000000160C00` | Max gold, EXP and CP | AR text |
| Zero Escape: Virtue's Last Reward | `0004000000096700` | Stereoscopic 3D depth | AR text |
| Heroes of Ruin | `0004000000074000` | Infinite health, max level, gold, skill points | **Plugin**, needs conversion |
| Mario Party: The Top 100 | `00040000001C4E00` | Infinite lives and health, unlock all minigames | **Plugin**, needs conversion |
| Hometown Story | `00040000000F9900` | Money at save offset 0x0014 | **Save hex edit only**, no plugin found |

Searched and nothing surfaced: Professor Layton and the Miracle Mask, Sonic & All-Stars Racing
Transformed, HarmoKnight, Yu-Gi-Oh Saikyou Card Battle. One incidental finding worth keeping:
HarmoKnight is reported to crash when a cheat plugin is enabled.

Not yet searched, fourteen of them, mostly eShop titles and art applications where a cheat is
unlikely to exist: Art Academy, Attack of the Friday Monsters, Cave Story 3D, Crimson Shroud,
Doctor Lautrec, Jake Hunter, Mario Sports Superstars, Petit Novel series, Pokemon Art Academy,
Professor Layton and the Azran Legacy, Rayman 3D, Return to PoPoLoCrois, The Starship Damrey,
Weapon Shop de Omasse.

A plugin is a `.plg` binary rather than Azahar text cheats, so those two rows cannot be bundled
without converting them first.


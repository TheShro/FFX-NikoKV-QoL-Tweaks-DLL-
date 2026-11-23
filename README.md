# FFX Tweaks Collection: Module Edition

Final Fantasy X HD Remaster (Steam, PC)

The tweaks and fixes from **NikoKV's "FFX Tweaks Collection"** Cheat Engine table, rebuilt as a
drop-in module for ffgriever's FF10 Module Loader. No Cheat Engine, nothing to launch: install
once and play. It also fixes two problems with the table:
- A save loaded before the table was active could lose item stacks above 99.
- Closing Cheat Engine with a tweak active could break the game.

**Original work by NikoKV:** https://www.nexusmods.com/finalfantasyxx2hdremaster/mods/186

## What it does

| Tweak | Effect |
|---|---|
| Item stock limit | Item stacks go up to 255 instead of 99. |
| Lightning Dodger | Hits no longer reset the streak; leaving Thunder Plains does not matter. |
| Shorter League | The Blitzball league jumps from round 5 to round 10. |
| Chocobo Catcher | Custom starting score for every race (3 presets, see Settings). |
| Blitzball levels | "Beside Aurochs" players get +10 levels in Exhibition matches. |
| Blitzball auto-learn | Mep and Nimrook know Anti-Drain, Zalitz knows Anti-Venom. This is a fix: it is written into the save data while active. |

## Requirements

- FFX HD Remaster on Steam, current build (the 2026.09.30 update or later).
- ffgriever's FF10 Module Loader. It comes with the "FFX/X-2 HD External File Loader" on Nexus
  Mods; use version 1.2.0 or later. UnX works alongside it.
- Tested with UnX and External File Loader 1.2.0.

## Install

1. Build `ffx_tweaks.dll` (see Building), or take it from a release. Then copy it and
   `modules/ffx_tweaks.ini` into `<game folder>\modules\`. That is the folder next to FFX.exe that
   holds the loader's own `ff10-file-loader.dll`.
2. Start the game. Back up your save before trying any mod.

To uninstall, delete `ffx_tweaks.dll` from the modules folder.

## Settings

The settings are in `modules\ffx_tweaks.ini`. Restart the game after changing them. Set any tweak
to 0 to turn it off.

`ChocoboCatcherPreset` sets the starting score:
- 1 = 3/0/0/0 (the default)
- 2 = 4/0/0/0
- 3 = 30/0/0/30

The order is Tidus balls / second balls / Tidus birds / second birds, as in the original table.

## If something goes wrong

The module writes `ffx_tweaks.log` next to FFX.exe each time the game starts. Each tweak should
say `OK ... -> PATCHED`. If a tweak can't be found (for example after a game update), it says
`NOT FOUND` and is skipped; it never half-patches. Turn off the tweak you suspect in the ini and
report the log.

## Notes

- Do not use the old Cheat Engine table at the same time: only one thing can patch each spot.
  Updated Cheat Engine tables for the current build are distributed by NikoKV.
- The item stock limit is also available on its own, in `standalone-item-stock-limit/`.
- Hooks are found by scanning the game's code, so small game updates should not break it.

## Building

- `source/build_msvc.bat` needs Visual Studio's C++ tools. The DLL must be 32-bit: FFX.exe is a
  32-bit process.
- `source/build_mingw.bat` needs MinGW-w64 targeting i686.
- `source/tools/run_tests.py` runs the assembled patch code against simulated game memory.
- GitHub Actions builds both DLLs on every push (`.github/workflows/build.yml`).

## Credits

- **NikoKV:** the original tweaks, fixes and concept. All main credit goes to the original author.
- **Shro (with Claude, Anthropic):** hook research, re-porting and the module.
- **The community Item_fix.CT:** the base for the item-stock hook offsets.
- **ffgriever:** the Module Loader and External File Loader.

## Permission and licence

This module is derived from NikoKV's work. It is published with **NikoKV's express permission**,
on the condition that he is credited and linked back to:
https://www.nexusmods.com/finalfantasyxx2hdremaster/mods/186

No open-source licence is granted. All rights to the original tweaks stay with NikoKV. Reuse or
redistribution needs his permission. This module will be taken down at the original author's
request. Final Fantasy X is owned by Square Enix; no game files are included.

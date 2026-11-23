FFX ITEM STOCK LIMIT  (99 -> 255)  -  standalone module
=======================================================

What this does
  Item stacks go up to 255 instead of 99. Nothing else is changed.
  A drop-in module for ffgriever's FF10 Module Loader (works next to UnX and the
  External File Loader). No Cheat Engine needed.

Install
  1. Copy ffx_item_stock.dll into  <game folder>\modules\
     (the folder next to FFX.exe where the loader's own modules live)
  2. Start the game. To uninstall, delete the DLL.

Check it worked
  ffx_item_stock.log appears next to FFX.exe. A good run says:
      hook found at FFX.exe+XXXXXX
      PATCHED - item stacks now go up to 255
  In game, get a stack past 99 (buy or pick up items).

Notes
  - Do not combine it with the Cheat Engine "items stock limit" tweak or with the full
    ffx_tweaks module: only one thing can patch that spot. If it is already patched the
    log says NOT FOUND and nothing is changed (harmless).
  - The hook is found by scanning the exe, so it keeps working across small game
    updates. If an update changes the code itself, the log says NOT FOUND and the module
    changes nothing - the game is never left half-patched.
  - Back up your save before trying any mod.

Build it yourself (32-bit DLL - FFX.exe is a 32-bit process)
  Run build_msvc.bat (finds Visual Studio itself; must end with "14C machine (x86)"),
  or build_mingw.bat, or use the GitHub Actions workflow in .github\workflows.
  tests\check_hooks.py decodes the generated patch bytes (needs only gcc).

Credits
  Original tweak and concept: NikoKV, "FFX Tweaks Collection" - all main credit to the
  original author. Hook research and port: Shro (with Claude, Anthropic).
  Item-stock hook offsets: based on the community Item_fix.CT.
  Module loader and External File Loader: ffgriever.

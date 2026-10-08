# TitanQuestFarmRadar

An optional x86 ASI add-on for TitanQuestCore on Titan Quest Anniversary Edition
2.10. This component lives in the Titan Quest Toolkit repository under `farm-radar/`.

The compact panel stays in the bottom-left corner while enabled and a character
is loaded. It shows only the localized names of nearby living creatures that
share first place for at least one Core item's drop chance. Radius defaults to
30 world units; F8 hides/shows it for the current game session. Identical names
appear once, nearest first. A blank panel means no current matches. Scroll the
panel with the mouse wheel if there are more than eight unique names.

## Install

1. Close the game.
2. Install `scripts/TitanQuestCore.asi` from `../core/dist` with the matching build that
   exports add-on API v1.
3. Copy `dist/TitanQuestFarmRadar.asi` into the same `scripts` directory.
4. Keep the existing Core database, loot model, catalogues, collections and
   localization files. Nothing needs deleting or moving. Use the existing ASI loader.

Core alone now draws no radar. Removing only `TitanQuestFarmRadar.asi` with the
game closed removes the radar and leaves Core working. Neither DLL should be
hot-loaded/unloaded into a running game. The older integrated radar binary must
be replaced to avoid an old panel still appearing alongside this add-on.

## Settings

The add-on creates its own folder beside its ASI:
`scripts/TitanQuestFarmRadar/`. Its log and INI live there, separately from Core.

```ini
[radar]
enabled=1
hotkey=119
radius=30
```

`hotkey` is a Windows virtual-key code (119 = F8, 0 disables the shortcut).
`radius` is clamped to 1–200 world units. Settings reload once per second.
The shortcut preserves Core search/viewer keyboard priority. It hides the
panel for this process without changing `enabled` on disk.

## Dependency and design

The add-on waits for `TitanQuestCore.asi`, obtains its versioned C function table
and subscribes to the Core frame/input callbacks. It does not duplicate game
hooks, subclass the game window, load a second database/model, read offline
reference odds, or access Core's C++ heap. The canonical API header is shared at `../core/include/tqt_core.h` and `../core/include/tqm_addon.h`.
Build this component from the Toolkit checkout.

Core supplies its current player/party/difficulty source snapshot and native
font rendering services. This plugin owns live object inspection, world-distance
checks, liveness checks, name deduplication, nearest-first sorting, panel layout,
F8 and configuration. It scans every 250 ms while enabled, includes loaded
off-screen creatures, accounts for region offsets and excludes other worlds.
Unloaded creatures cannot be detected. Engine-owned list buffers are freed
through the engine's CRT. Actor pointers are not retained for later dereferencing.
Core's same-name source variant grouping and exact co-best tolerance are retained.

If Core is missing or its API is incompatible, this add-on stays inactive and
logs the dependency error. Inspect `TitanQuestFarmRadar.log` for registration,
source snapshot revisions, periodic scan diagnostics and any scan fault.
The log supports reading while the game is running. Callback faults are also reported
in the Core log with the add-on registration token.

## Build and verification

Run `build.bat` with MSVC desktop C++ Build Tools and a Windows SDK installed.
The script discovers x64-hosted x86 tools and compiles C++17 /MT /W4 /WX /Oy-;
the ASI retains PDB/MAP symbols. No third-party library or dependency download is
needed. Successful binaries are published to `dist`; previous ASIs use a non-ASI
backup extension so the loader cannot accidentally load two builds.

`tools/test.bat` tests radius boundaries, world separation, deduplication/sorting,
panel bounds, ellipsis and scrolling. With an available Python 3 runtime, run
`tools/verify_farm_radar.py` for a read-only installed-game ABI audit. Core's
separate host/source tests check API contracts and the live ranking calculation.

Both binaries are compile/offline verified. In-game verification is still
pending: confirm both ASIs load, F8 press/hold, focus/search priority, death,
teleport/menu transitions, ordinary monsters/bosses, multiple resolutions and
FPS in a crowded area. This is not a guarantee that the modeled drop will occur.

The extracted panel/rules/helpers retain the MIT license from TitanQuestCore.

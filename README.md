# Titan Quest Toolkit

Three x86 ASI modules for Titan Quest Anniversary Edition 2.10 in one repository.

| Folder | Module | Responsibility |
| --- | --- | --- |
| `core/` | `TitanQuestCore.asi` | Shared archive/catalogue generation, database, live drop model, frame/input API and native drawing services |
| `museum/` | `TitanQuestMuseum.asi` | Collection, saved progress, storage, Museum UI and game hooks specific to these features |
| `farm-radar/` | `TitanQuestFarmRadar.asi` | Nearby living-creature detection, radar panel, F8 and its own configuration |

Both plugins consume Core. FarmRadar does not require Museum. Museum does not
require FarmRadar. Core is required by either plugin and generates its database
when necessary even when Museum is absent. Plugins resolve `TQT_GetCoreApi` at
runtime, wait for the provider, and never share CRT/STL allocations across ASIs.

## Build and install

Run `build.bat` for all three modules. The binaries and matching PDBs are published
under each component's `dist/`. `core/dist/TitanQuestCore.lib` is only a build/test
artifact; do not install it. Core owns the model/solver; Museum links only its
catalogue-file reader from the auxiliary library.

Close the game and copy `core/dist/TitanQuestCore.asi` into the ASI loader's
`scripts/` directory. Copy either or both matching plugin ASIs into the same
folder. Replace the old Museum and radar binaries if installed; old radar builds
that depend on Museum must not be mixed with this release. For radar only, install
Core and FarmRadar and remove/disable the Museum ASI with the game closed.

Keep the existing `scripts/TitanQuestMuseum/` and `scripts/TitanQuestFarmRadar/`
folders and all collection/save files. Core creates its own `scripts/TitanQuestCore/`
folder, log, INI and generated shared data. It does not delete the old Museum data.
The first Core launch may take time to generate data and gray icons.

`TitanQuestCore.ini` contains `[core] text_language=EN` (or RU etc.). On its first
launch Core adopts the existing Museum `[advanced] text_language` if available,
otherwise EN. Change the Core setting with the game closed and restart; the data
stamp then regenerates the common localized catalogue. Museum's UI translations
and collection settings remain in its own folder.

Radar defaults to radius 30 and F8. Its configuration/log location is unchanged.
The old sibling FarmRadar source directory is preserved as a migration copy;
current development takes place in this repository's `farm-radar/` folder.

## Verification

Run `core/tools/test.bat` for source-service contracts, `museum/tools/test.bat`
for Museum/host/source regressions and `farm-radar/tools/test.bat` for radar rules
and layout. `test.bat` runs them sequentially. See `VERIFICATION.md` for results and
known catalogue-fixture/memory-test failures. New standalone Core hooks still need
in-game verification with Core+Radar, Core+Museum and all three modules.

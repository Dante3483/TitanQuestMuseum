# Titan Quest Toolkit

Four x86 ASI modules for Titan Quest Anniversary Edition 2.10 in one repository.

| Folder | Module | Responsibility |
| --- | --- | --- |
| `core/` | `TitanQuestCore.asi` | Shared archive/catalogue generation, database, live drop model, frame/input API and native drawing services |
| `museum/` | `TitanQuestMuseum.asi` | Collection, saved progress, storage, Museum UI and game hooks specific to these features |
| `mob-radar/` | `TitanQuestMobRadar.asi` | Nearby living-creature detection, radar panel, F8 and its own configuration |
| `museum-radar/` | `TitanQuestMuseumRadar.asi` | Optional Museum collection filter for MobRadar |

Museum and MobRadar consume Core. MobRadar does not require Museum. Museum does not
require MobRadar. MuseumRadar depends on both Museum and MobRadar. Core is required by either plugin and generates its database
when necessary even when Museum is absent. Museum and MobRadar resolve `TQT_GetCoreApi` at
runtime, wait for the provider, and never share CRT/STL allocations across ASIs.

## Build and install

Run `build.bat` for all four modules. The binaries and matching PDBs are published
under each component's `dist/`. After all builds succeed, the four ASI binaries
are also copied into the repository's root `dist/` for installation.
Museum's language resources are included in `dist/localization/`; copy that folder
beside the ASI files when installing.
`core/dist/TitanQuestCore.lib` is only a build/test
artifact; do not install it. Core owns the model/solver; Museum links only its
catalogue-file reader from the auxiliary library.

Close the game and copy `core/dist/TitanQuestCore.asi` into the ASI loader's
`scripts/` directory. Copy either or both matching plugin ASIs into the same
folder. Replace the old Museum and radar binaries if installed; old radar builds
that depend on Museum must not be mixed with this release. For radar only, install
Core and MobRadar and remove/disable the Museum ASI with the game closed.

Keep the existing `scripts/TitanQuestMuseum/` and `scripts/TitanQuestFarmRadar/`
folders and all collection/save files. Core creates its own `scripts/TitanQuestCore/`
folder, log, INI and generated shared data. It does not delete the old Museum data.
The first Core launch may take time to generate data and gray icons.

`TitanQuestCore.ini` contains `[core] text_language=EN` (or RU etc.). On its first
launch Core adopts the existing Museum `[advanced] text_language` if available,
otherwise EN. Change the Core setting with the game closed and restart; the data
stamp then regenerates the common localized catalogue. Museum's UI translations
and collection settings remain in its own folder.

Radar defaults to radius 30 and F8. MobRadar uses `scripts/TitanQuestMobRadar/` and
copies the previous FarmRadar INI on first launch. Remove `TitanQuestFarmRadar.asi`
from `scripts/` before installing MobRadar to avoid duplicate panels.
The old sibling FarmRadar source directory is preserved as a migration copy;
current development takes place in this repository's `mob-radar/` folder.

## Verification

Run `core/tools/test.bat` for source-service contracts, `museum/tools/test.bat`
for Museum/host/source regressions and `mob-radar/tools/test.bat` for radar rules
and layout. `test.bat` runs them sequentially. See `VERIFICATION.md` for results and
known catalogue-fixture/memory-test failures. New standalone Core hooks still need
in-game verification with Core+Radar, Core+Museum and all four modules.

Install MuseumRadar alongside Museum and MobRadar to hide mobs whose every best
drop is already collected. MuseumRadar validates Museum's JSON journal and reads
its contents only when changed; it never writes collection data. MobRadar alone
shows all best sources. `museum-radar/tools/test.bat` checks the reader and cache.
This uses Core API v2 and MobRadar API v2; update matching Toolkit builds together.

## Logging

Normal logs keep startup/dependency status, errors, failures and important state
changes. MobRadar does not log regular scans, creature names or positions; scans
lasting at least 8 ms produce at most one warning per minute. Core repeated frame
faults are limited to one warning per 30 seconds. MuseumRadar reports collection
availability changes rather than every collection refresh.

Museum's normal `log_level=info` retains collection deposit/take/recovery records
for diagnosing lost items. UI geometry, scrolling, rebuilding, opening windows
and other technical details require explicitly selecting `debug` or `trace`.
Keep `log_flush_each_line=0` for normal play.

## Real Museum loot

MuseumRadar also inspects existing inventory and equipped items of nearby living
monsters, regardless of best-source rank or whether drops are already collected.
It uses MobRadar's borrowed object snapshot once per second; it does not roll loot,
mutate inventory or enumerate the world's objects a second time. Highlighted
monster/item pairs appear first in the existing panel, using the same radius and F8.
An item collected in Museum shows its catalogue name; other Museum items show
`???`. Missing/invalid collection data conceals all item names. Several different
Museum item records on one monster produce several entries.

Update MobRadar and MuseumRadar together for this feature. Core and Museum binaries
need no update for it. MobRadar continues to serve API v1 for previous consumers.
Loot created only when a monster dies cannot be detected before it exists; this
feature reports current inventory/equipment and does not guarantee a future drop.
In-game verification of the new inventory feature is pending.

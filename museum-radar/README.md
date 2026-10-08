# MuseumRadar

Optional x86 ASI add-on connecting **MobRadar** to **Titan Quest Museum**.
Install matching Core, Museum and MobRadar builds, then copy
`dist/TitanQuestMuseumRadar.asi` into the game's `scripts/` directory with the game
closed. It waits for both consumers and uses MobRadar API v2; it does not access
Core directly or create a second database. No separate panel or shortcut is added.

MuseumRadar reads Museum's `tq-uniq-items.jsonl` without modifying it. File metadata
is checked once per second on a background thread; contents are read and parsed
only after a change. Atomic replacement is detected even with identical file size
and modification time. An unchanged collection does not trigger recalculation.
The Museum output-directory override and standard fallback locations are supported.

Collected items exclude their best sources from the radar. A mob remains visible
if it is a co-best source for another missing item. Lower-ranked sources are never
promoted. Pending deposits count as collected; pending withdrawals do not. Items
only held by the character are not collected. Missing, incomplete, incompatible or
invalid journals restore the full radar. Museum probability/source text is unchanged.
The current source model covers the main game database, not custom database sessions.

The add-on creates only its own diagnostic folder/log at
`scripts/TitanQuestMuseumRadar/`. Museum progress stays in its existing folder and
shared generated data stays in Core. Without this ASI, MobRadar shows all best
sources. Remove it only with the game closed; hot unloading is unsupported.

Run `build.bat` to build with MSVC x86. Run `tools/test.bat` for journal validation
and cache tests, optionally passing a real journal path. In-game verification of
the new optional connection remains pending.

## Real inventory highlighting

Nearby living monsters carrying any catalogue item are highlighted independently
of the best-source filter. The panel shows the monster name and a second item
line, using `???` until that item has a row in Museum's collected journal. Multiple
item records produce multiple lines, and already collected loot remains visible.
Inventory and all ten equipped slots are read without changing game state.

The inspector runs once per second on MobRadar's existing snapshot; borrowed
actors/objects never survive the callback. It resolves only requested IDs, keeps
up to 256 nearby monsters/loot rows, and caches panel captions between updates.
Binaries with unsupported inventory/name ABI refuse this feature while retaining
the collection filter. Startup/failure logs contain no item names or per-mob traces.
Install matching MobRadar and MuseumRadar; Core API remains v2. The catalogue-file
reader is statically linked from Core's auxiliary library, as in Museum.

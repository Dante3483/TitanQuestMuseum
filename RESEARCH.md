# v4.1 integration research

The reference stays unchanged. This project uses its proven game bridges, not its
control-panel architecture. No v5 implementation is an input.

| Concern | Reference evidence | Museum boundary |
|---|---|---|
| Loading | dllmain: ASI is an ordinary x86 DLL; worker waits for Engine/Game modules | game/bootstrap |
| Hooks | hooks + bindings + runtime: named exports, counted signatures, decoded operands; gate before MinHook enable | game |
| Render | PresentSurface fallback; Transfer page pre/post for plates and icons underneath rollover | game/native_surface |
| Caravan | SetCaravanMode/Goodbye hooks, mode 1, record geometry checked against page draw/hover | game/caravan |
| Opening | queued viewRequest, Update swaps only the Transfer accessor to the private prototype sack | game/view_adapter |
| Records | ARZ/ARC readers, DBR classes/tags and texture sizes; generated catalogue and group lists | backend/catalogue + game/archive generation |
| Categories | 6 equipment, 8 weapons, artifact; actual generator order is authoritative, not stale README examples | backend/category service |
| Discovery | collected journal record state; uncollected prototypes receive black icon textures, hover id suppressed | game/item rendering + ownership adapter |
| Ownership | collected is journal rows, have-one separately reads inventory/equipment/stash/transfer/relic vault | backend/journal + game/ownership adapter |
| MI | Rare equipment accepted by explicit MI record-name convention; retained beside Epic/Legendary | existing catalogue algorithm |
| Search | incremental rollover property index; excludes requirements/directions and optional lore; highlights, does not filter | game/text capture + backend matching |
| Owned | filters collected records only when ownership known, retains catalogue order and slot footprints | backend collection adapter |
| Pagination | fixed 16x15 sack, uniform category slots; wheel moves window one slot row, PageUp/Down a window | retain adapter; remove visual page controls |
| Interactions | take/deposit gates, exact replica journal, pending-save reconciliation, prototype escape prevention | game + backend journal |
| Text | RenderText A/W exports, LoadFont; fontmetrics header is for Grim Dawn, unsuitable for TQ | new centralized renderer; inspect installed Engine exports |
| Textures | native canvas rect/texture exports, sampled caravan palette, gray source mounted before world | game |

Highest risks: x86 thiscall/fastcall ABI and SEH; VS2012 string/vector/map layout
and engine allocator; hover hook EBP prologue; page/widget offsets decoded from
instruction operands; private sack lifetime and restoration on world teardown;
temporary icon/rect writes restored even on unwind; journal moves settled only
after character save. Keep these proven bridges and their capability checks.

Build reference: tools/find_vcvars.bat finds vcvars32; cl C++17 /MT /O2 /Oy-
/W4 /WX; MinHook C hde32; DLL /MACHINE:X86 /SAFESEH; dumpbin checks hover EBP.
New build links to a staging path and publishes only on success.

Planned modules: src/core (configuration, paths, logging), src/backend (pure
catalogue, journal, matching, slot packing, MuseumState/CollectionService),
src/game (runtime, hooks, archive generation, view/item/ownership/search adapters,
native surface), src/ui (Rect, row layout, Button, TextRenderer, StatisticsView,
MuseumPanel), src/features (MuseumController). UI only sees MuseumState and a
renderer/action contract. Backend never takes screen rectangles.

Preserve the format-1 journal identity and filenames for explicit copying of old
collections. Use a separate TitanQuestMuseum data directory; do not automatically
mutate the reference collection. The old plugin must be disabled during testing
because both plugins intercept the same game functions.

Runtime verification remains a separate user test. Compilation proves neither
caravan placement nor font alignment nor deposit/take safety in a live game.

# Titan Quest Museum

Standalone x86 native ASI project. No dependency on a loaded Unique Collection Tab
plugin, no v5 sources, no ImGui or secondary graphics hooks. The v4.1 source beside
this directory remains the reference.

## Data flow

Game hooks/adapters -> collection runtime + journal -> CollectionSnapshot ->
CollectionService/MuseumState -> MuseumController -> MuseumPanel -> native canvas.

| Directory | Responsibility |
|---|---|
| `src/core` | Configuration, module-relative paths, buffered logging, version |
| `src/backend` | Journal format and recovery rules, collection records, Owned filtering, property matching, category metadata, slot-window packing and immutable UI state |
| `src/game` | Export resolution, instruction decoding, MinHook, engine object lifetime, inventory/ownership reads, rollover capture, save observation, caravan measurement, native canvas adapter |
| `src/ui` | Rect, equal-width rows, theme, Button, Search field, TextRenderer, StatisticsView, MuseumPanel |
| `src/features` | MuseumController translates user actions and refreshes state |

`collection_runtime` retains the proven collection algorithms, separated from
game memory. Its ownership callbacks are installed by `collection_bridge`.
`slot packing` uses **inventory cells**, never screen rectangles. There are no
game headers in the collection runtime. Engine strings, allocator boundaries,
vftables and raw object offsets stay in `game`. UI has no engine includes.

The current native item grid is still the game's Transfer inventory rendering of
display prototypes. MuseumState also exposes their logical record/discovery/search
state for future frontends; UI does not enumerate native item structures.

## Native integration retained intentionally

Reverse-engineered runtime/binding, view, item, ownership, rollover, reconciliation
and slot-surface bridges were extracted from v4.1. Their guarded reads, shutdown
ordering, prototype escape gates and exception-safe temporary writes are preserved.
Retained internal helper names identify proven code; the new public state,
controller and UI are independent of those names.

Addresses appearing in bridge comments are provenance from v4.1 disassembly,
**not unconditional function pointers**. Exports resolve by decorated name;
signatures must occur exactly once; object operands and caller frames are checked.
The binding gate still runs before hooks. The hover detour still needs x86 EBP,
which the build verifies from its object disassembly.

`native_surface` owns frame evidence and item rendering, not control geometry.
It passes the accepted whole-caravan rectangle to MuseumPanel. The old pad layout,
page labels, arrow buttons, text estimates and control drawing are absent.
Existing window-procedure and wheel policy translate input to controller actions.
Walking behavior is unchanged in this phase.

## UI invariants

Panel starts at the accepted caravan's bottom. Theme values are in record pixels
and scale together. Four equal-height rows have equal padding/gaps. Row cells are
computed from shared rounded edges. One category gets the whole category row.
Search width is assigned from Transfer width; Owned has a theme width; Statistics
gets all remaining space. Button objects own the exact rendering/hit rectangles.
Both Transfer and Collection always remain separate controls.

General font size depends only on theme and UI scale. A label can fit locally in
its own button. Statistics preserves category counts, search status and `All`,
then reduces its own font only if native measured text exceeds its rectangle.
All is the final suffix. Right padding and anchor never depend on content. An
unusable area/failed metric draws no overflowing caption. Text clipping/placement
at very low resolutions still requires runtime testing: a panel below a caravan
that nearly fills the screen may extend beyond the bottom; this phase does not
move the game's caravan or change its UI scale.

## Rules and data parity

Search remains a property **highlight**, not an item filter. Owned means collected
journal records, distinct from inventory have-one marks. Unknown records stay black
and cannot reveal their normal tooltip through hover. MI convention, exclusion,
rarity/name/level sorting, footprints, fixed-grid windows and wheel row steps stay
as implemented in the C++ v4.1 source. The actual generated order is Equipment
(6), Weapons (8), Other (Artifact); this source's weapon order starts with Sword.

Journals retain their format-1 identifier, filenames, replicas, pending save
settlement and reconciliation semantics. Museum owns a separate `TitanQuestMuseum`
data directory and `TITANQUESTMUSEUM_OUT` override. At startup the worker copies
missing named journals from sibling `uniquetab` directories via temporary files
and a rename without replacement. Existing Museum journals always win; reference
journals are never changed. It does not import obsolete panel configuration.

## Verification

See BUILDING.md and RUNTIME_TESTS.md. Intermediate integration, state/backend,
UI primitives, connected UI and final builds succeeded with x86 MSVC /W4 /WX.
The native text alignment helper is verified both offline and at runtime from
the exported wrapper. No in-game verification has been performed.

The reference Python oracle is stale (1588, older category/sort/MI behavior).
The catalogue oracle used here comes from compiling **unmodified v4.1 C++**
generator sources read-only into Museum's build directory: 2688 records and
15 groups on the installed game. New generated binary/group/record/exclusion
files match it byte-for-byte. Tests keep old domain assertions; only obsolete
pad UI assertions and the stale fixed prototype count are replaced.

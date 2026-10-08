# Museum instance copy

Implemented on top of the existing format-1 journal and replica reconstruction.
No journal format, pending settlement, deposit, take or drop-source calculation
was changed. No first-sample table was introduced.

## Identity and selection

The journal stores seven byte-exact strings (base, prefix, suffix, relic,
relicBonus, relic2, relicBonus2), seed, var1, var2, b8 and stack. Rolls are
reconstructed by the game from these replica inputs; there is no separate roll
array in the existing replica. Object ids are intentionally newly allocated.

Read-only disassembly of the installed AE 2.10 Game.dll confirmed that
Item::GetItemReplicaInfo copies its internal replica through the assignment
routine, including the strings and scalar fields above. Its object-id field is
not an identity field to reuse for a new object. The existing UtReplica builder
already restores every saved replica field, with that id initially zero.

Each displayed prototype has its own object pointer, id and journal seq. The
existing page selects the newest available row of each record. Both hint and
action require that exact live prototype and the same current seq; a stale page
is refused rather than silently copying a different row. Taking the original
continues through the existing take path and permits the next stored copy to be
displayed. Similar items in player inventory cannot pass the pointer/id check.

## Input, creation and rollback

The existing window-procedure surface claims a middle click only inside an
eligible Museum slot. It queues id/seq plus page and world generations. The
game Update revalidates them before creating an independent loose object with
the existing protoCreateLoose / utReplicaFromIdentity path. The new object's
GetItemReplicaInfo must match all seven strings, seed, var1, var2, b8 and stack
before any inventory mutation. Unknown stack reads and non-single copies fail
closed. Journal take/deposit commits and prototype handout are never called.

The player's inventory room is checked before creation and checked again for
the new object. Ordinary Player::GiveItemToCharacter and the automatic
PlayerInventoryCtrl::AddItem route can stack items or drop a failed handout on
the ground. Copy instead uses the exported AddItemToSack on a checked sack:
this calls the same Sack::AddItem(item,true) and item-skill registration, without
stack merging or a ground fallback. Character inventory ownership and physics
type are set with the same engine operations used by GiveItemToCharacter.
Successful insertion is checked by the new id in the sack.
After confirmed insertion, Item::PlayDropSound plays the game's normal item
placement sound once. This optional audio step is separate from the placement
transaction; an audio fault cannot roll back or duplicate a successfully placed item.

Placement failure reverses controller item-skill registration, removes only the
new id from sacks and character inventory, then destroys the loose object.
Engine faults are guarded and an unsuccessful cleanup is logged as an error;
an arbitrary failure inside the engine's own cleanup cannot be proven safe by
compilation. The Museum prototype and its journal row are never removed.

The native tooltip latch holds the Museum object identity and rechecks it at
conversion. The bottom copy hint uses ItemBanner (RGB 153/153/153), with EN/RU
resources. The copy hint is independent of the collection-marker INI option.
Drop sources are displayed only by the unknown Museum silhouette rollover;
ordinary item tooltips never append source rows, regardless of collection state.

## Verification status

- x86 Release compilation with /W4 /WX, hook-frame verification, linking and
  publication succeeded. MSVC Hostx64 targeting x86 avoided a Hostx86 linker
  MSPDB140 version error; the project's build script was not changed.
- Previous ASI/PDB pairs were saved under dist/known-good before builds.
- No automated test suites were added or run. The developer did not run the game.
- On 2026-10-05 the user reported that initial manual play checks looked normal.
  Individual acceptance cases and replica fields were not separately confirmed.

Detailed in-game acceptance still needs confirmation: an affixed/relic/upgraded instance copied and
compared; original extraction after copying; a similar inventory item without
the hint or handler; an unknown silhouette without the hint; a completely full
inventory; imported multiple rows with different seeds; and character save/reload
with unchanged Museum ownership. Replica equality is enforced at runtime, but
visual stats, engine side effects and persistence need this game verification.

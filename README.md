# Titan Quest Museum

Current release: **1.0.2** (local Git tag `v1.0.2`).

Standalone native x86 mod for Titan Quest Anniversary Edition, with Discovery,
Monster Infrequents, Owned filtering, property Search, wheel paging and a new
four-row caravan extension panel.

Copy `dist/TitanQuestMuseum.asi` into the ASI loader's `scripts` plugin directory.
Disable the old Unique Collection Tab plugin first. On startup Museum preserves
the reference journal files by copying missing collections to its own directory.

Read [BUILDING.md](BUILDING.md), [ARCHITECTURE.md](ARCHITECTURE.md) and
[RUNTIME_TESTS.md](RUNTIME_TESTS.md). The current build is compile verified;
in-game verification is pending.

C opens a centered, read-only collection viewer while a character is loaded and
the caravan is closed. Esc, C or X closes it. Use the left category list (wheel
to scroll), the wheel over the grid to scroll rows, and the Museum search
or Owned filter. Hover an item to see its details. Unknown
items show silhouettes and the existing contextual drop sources; stored items
show the game tooltip of the newest stored journal instance, with its rolled
properties and full affixed name. Wheel over the right panel scrolls long text.
A temporary item is reconstructed only during game Update, checked against the
saved identity, captured through the existing tooltip path and destroyed. The viewer
writes no journal rows, and cannot deposit, withdraw or duplicate items.
Its navigation and search are independent of the caravan. Mouse and key input
are claimed through the existing native input hooks while it is open; this must
still be verified in game. The game continues running behind the viewer.

The standalone viewer key is [view] viewer_hotkey=67 (Windows VK_C). Set it to 0 to disable the shortcut. INI version 18 adds this key through the existing settings migration; other settings are retained.

Viewer search keeps items in place and highlights matches in blue, including categories and set cards. It shares the Museum property index and also searches the current best drop source name/details/displayed chance. The property index fills in the background; source calculations are unchanged.

The viewer remembers category/set/favorites mode, Owned filter, query, category-list offset and item row between openings in the current game process, separately per journal collection set. Reopening clamps offsets to the current list. Item name/property matches require a stored journal instance; set names and contextual sources remain searchable for unknown items. The Shift comparison experiment was removed.

Viewer hover borders use one blue colour. Unknown item question marks use the
game's native item name colour, including Rare equipment and artifacts. Colours
are cached, with at most one missing record resolved per game Update.

See [release notes](research/RELEASE_1.0.2.md) for scope and verification limits.

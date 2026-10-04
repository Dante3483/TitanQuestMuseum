# Titan Quest Museum

Standalone native x86 mod for Titan Quest Anniversary Edition, with Discovery,
Monster Infrequents, Owned filtering, property Search, wheel paging and a new
four-row caravan extension panel.

Copy `dist/TitanQuestMuseum.asi` into the ASI loader's `scripts` plugin directory.
Disable the old Unique Collection Tab plugin first. On startup Museum preserves
the reference journal files by copying missing collections to its own directory.

Read [BUILDING.md](BUILDING.md), [ARCHITECTURE.md](ARCHITECTURE.md) and
[RUNTIME_TESTS.md](RUNTIME_TESTS.md). The current build is compile verified;
in-game verification is pending.

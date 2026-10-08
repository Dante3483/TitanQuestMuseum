# Shared plugin API moved to Core

The runtime provider is now `TitanQuestCore.asi`, with export `TQT_GetCoreApi`.
See [Core contract](../../core/include/tqt_core.h) and
[callback contract](../../core/include/tqm_addon.h). Museum and FarmRadar are
consumers, and Museum no longer exports or dispatches the shared API.

[Toolkit architecture](../../ARCHITECTURE.md) describes data lifetimes and input priority.

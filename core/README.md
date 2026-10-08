# TitanQuestCore

Standalone x86 ASI provider for Titan Quest Toolkit plugins. Owns common generated
data, the live loot-source worker/cache, frame and input callbacks, font/rendering
services and the API registry. It starts without Museum and generates its own
shared database under `scripts/TitanQuestCore/`.

`include/tqt_core.h` is the versioned provider API. `include/tqm_addon.h` describes
the embedded frame/draw/target callback table; the historical TQM prefix is retained
for ABI compatibility, but its provider is Core. `TQT_GetCoreApi` returns null for
unsupported versions. Data copies use caller-owned buffers and snapshot revisions;
no STL object or shared allocator crosses modules. Callback modules are pinned for
process lifetime; hot unload is unsupported.

`build.bat` produces `dist/TitanQuestCore.asi` and matching symbols. The auxiliary
static library is used by file readers and offline tests and is not installed.
`tools/test.bat [path-to-loot-model.bin]` checks source-service behavior without Museum.

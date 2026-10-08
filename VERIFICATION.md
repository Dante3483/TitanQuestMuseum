# Standalone Core migration verification

- Three ASI builds use MSVC x86 /MT with retained symbols and frame pointers.
- Core source service: 13 checks, zero failures with the installed real loot model,
  including version/size validation, buffer ownership, stale revisions, context
  changes, invalidation and shared-directory access. The harness does not load Museum.
- Core callback host: 28 checks, zero failures (including fault isolation and input).
- Museum tooltip regressions: passed, zero failures after replacing its source worker.
- Earlier UI/model, radar and farm-source tests passed after the source-layout migration;
  targeted radar tests are rerun for the provider change.
- The previous broad suite still has catalogue reference mismatches (3108 current
  records versus 2688 in the reference fixture) and a 200 MB peak-memory budget
  failure. These fixtures/limits were not changed to hide the failures.
- No game installation was changed during this migration.
- Standalone Core+Radar and Core+Museum still require in-game verification; offline
  contracts/import checks are not a substitute for testing new hooks in game.

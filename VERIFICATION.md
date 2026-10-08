# Standalone Core migration verification

- Four ASI builds use MSVC x86 /MT with retained symbols and frame pointers.
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

## Radar completed-drop filter

- Source-service collection API: 18 checks, zero failures.
- Ranking/filter model: 13 synthetic checks, zero failures, including all collected,
  partial collection, exact ties and preservation of full Museum source text.
- MuseumRadar JSON reader/cache: 12 checks, zero failures with the supplied real 92-record journal,
  including malformed/truncated files, unsupported format and count mismatches.
- Existing radar radius/layout rules: 22 checks, zero failures.
- Radar performance update: 31 radius/layout/cache checks, zero failures; MobRadar
  rebuilt and copied into root dist. The installed log showed approximately 24,000
  objects per scan. Class filtering now precedes name processing, detailed
  arachnid logging is removed, and unchanged labels reuse width measurements.
  Actual scan/frame-time improvement still requires in-game measurement.
- Latest installed-game logs: Core and both radar consumers connected successfully;
  Museum and MuseumRadar loaded 92 collected records without logged warnings or
  faults. Installed MobRadar differs from the latest dist binary and still emits
  old arachnid diagnostics, so this session does not validate the performance build.
- Core API is now version 2. Matching modules must be installed together.
- JSON reads run off the rendering thread only when metadata changes, including
  atomic file replacement, and never write to the journal.
- MobRadar and MuseumRadar build separately; MobRadar exports its versioned API.
- In-game verification of this new filter is still pending.

## Concise logging update

Routine scan/source/collection-refresh logs are removed. Museum UI details now
require debug mode; errors and collection safety records remain visible at info.
Slow scan warnings are limited to once per minute and repeated Core frame faults
to once per 30 seconds. No tests were run for this logging-only change at the
user's request; all four modules were rebuilt and the root dist refreshed.

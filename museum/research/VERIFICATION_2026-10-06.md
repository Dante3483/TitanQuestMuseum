# Museum verification — 2026-10-06

The user authorized offline tests for this run. The previous source-search fix
was committed locally as `e1c2526` before starting. No push or game deployment.

## Added coverage

- Shared production rules now exercise co-best source matching independently of
  ordering, lower-probability exclusion, floating-point noise, invalid values,
  empty lists and retention of more than ten tied maxima.
- Viewer search rules exercise withholding unknown item names/properties without
  even calling the private matcher, while allowing public set/source matches.
- Navigation snapshots retain Unicode queries, filters and offsets independently
  per journal collection set. The shared clamp handles shortened/empty lists.
  These checks do not simulate the live window's close/open input hooks.
- A real isolated journal holds two instances with the same base record but
  different seeds, all seven replica strings, var1, var2 and b8. Fifty read-only
  newest-row/replica round trips leave file bytes, row counts, pending states and
  write count unchanged. Stale withdrawal is refused, newest withdrawal exposes
  the unchanged older instance, and the older instance survives journal reload.
- INI 17 migration adds viewer C, keeps existing caravan/search settings and
  preserves a custom viewer key. Expected current version/counts no longer assume
  version 17 or 38 settings.

The new rules are called by production code rather than copied into test-only
implementations. The catalogue and tooltip test scripts were missing recently
added loot-source dependencies; these were added, and tooltip engine dependencies
are explicitly stubbed for the offline harness.

## Results

| Suite | Result |
| --- | --- |
| Search, including new source/viewer checks | Passed |
| Journal | 166 checks passed |
| Journal fixture replica round trips | 1588 / 1588 passed |
| Prototype assembly/layout | 40 checks passed |
| Storage decisions/journal integration | 54 checks passed |
| Tooltip latch/vector logic | Passed |
| View transition safety | Passed |
| Configuration/migration | Passed |
| Museum layout/model | 805 checks passed |
| Native bindings | 74 checks passed |
| Journal import | Passed |
| Catalogue generator | Failed old reference comparisons and 200 MB peak budget |

The generator produced 3108 items and a 442000-byte catalogue, versus the older
381576-byte checked-in oracle. The current generated catalogue, group list and
excluded list match a separately compiled `e1c2526` generator byte for byte.
That baseline also exceeds the memory budget: approximately 262.0 MB vs 262.5 MB
for the current run. These failures predate today's test refactoring. The oracle
and budget were not changed to conceal the failures. Updating the reference with
an independent content audit and reducing generator peak memory remain open.

One initially failing new test incorrectly assumed journal seq persisted across
reloads; journal.cpp explicitly defines it as a session identity. The corrected
test verifies saved fields and a valid new seq. Five INI persistence checks hit
Windows sandbox Access denied; rerunning the same isolated temporary-directory
suite outside that restriction passed. No game settings/save files were used.

The final x86 /W4 /WX ASI build and mandatory hook-frame check passed. Previous
ASI/PDB were saved under dist/known-good before rebuilding. Logs are under
build/test-*-current.log and build/catalogue-baseline-peak.log.

## What offline checks cannot establish

These checks do not execute native inventory insertion or copy rollback inside
Titan Quest, prove in-game rolled attributes, test bookmark persistence through
actual clicks, measure live window performance, or verify rendering and input
capture at every resolution. Full-inventory copy failure, real affixed/upgraded
copies, live reopen navigation and equal-source searches still need game checks.

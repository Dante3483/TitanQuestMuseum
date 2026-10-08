# Titan Quest Museum 1.0.2

Local release, 2026-10-06. No push or deployment to the game.

Changes since 1.0.1:

- Standalone collection window on C, with categories, sets, row scrolling,
  search, Owned filtering, favorites and help (EN/RU).
- Stored instance properties through the game's tooltip path. Viewer navigation
  and search position retained between openings within the game process.
- Unknown item names/properties excluded from search; public set names and drop
  sources remain searchable. All sources tied for the actual highest chance match.
- Centered icons, restrained scaling for small items, consistent hover borders
  and native item name colours for question marks.
- Offline coverage for search rules, multiple stored instances and preservation
  of journal identity, plus repaired test build dependencies and INI expectations.

The ASI build and mandatory hook frame check pass. Previous ASI/PDB retained in
dist/known-good before rebuilding. Earlier user checks exercised the UI; the final
native colour change has not yet been checked in game. Offline tests do not prove
native inventory rollback or the game's reproduction of every rolled attribute.

Known offline generator check failures: output differs from the old catalogue
oracle, and peak memory is about 262 MB against a 200 MB test budget. Both also
occurred on the committed baseline before the test refactor. No drop probability
formula or journal format was changed. See [verification report](VERIFICATION_2026-10-06.md).

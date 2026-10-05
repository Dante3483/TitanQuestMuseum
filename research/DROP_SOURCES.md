# Drop sources

Sources are generated from the installed ARZ database and localized Text ARC, alongside the catalogue. `loot-sources.txt` is an offline solo level-10 reference using `TQMSOURCES 2` followed by tab-separated record, source kind (0 loot, 1 equipment, 2 chest, 3 first-kill chest), difficulty, source level, probability and localized name. The runtime uses `loot-model.bin` (`TQMLOOT2`), which retains the full reachable source graph. It calculates up to ten ranked entries per item for the live context; the tooltip displays three. The reference text file is never loaded as live odds. Repeated variants with the same localized name, kind and difficulty keep their highest estimate rather than adding probabilities from mutually exclusive variants.

## Model

- Follow weighted master and item tables recursively. Include weights leading outside the museum catalogue in the denominator, including empty outcomes.
- Dynamic tables use level equations, eligible item levels and bell-slope weights. Keep all entries at the nearest lower eligible level when the normal range is empty. Integer weight rounding follows the installed game implementation.
- NPC equipment uses equip chance multiplied by weighted slot selection. Misc slots are reported as loot; wearable and weapon slots as equipment. Combine independent slots as `1 - product(1 - p)`.
- Container table arrays select one entry using the level-equation index, clamped to the last entry (confirmed in installed Game.dll `PickLootRecord`/`CalculateFixedItemLevel`), rather than combining every entry.
- Follow boss treasure proxies, difficulty-specific accessory pools and fixed item containers. Quest chests without a monster backlink are also linked through named base-game boss spawn proxies and their pool members. Only repeat boss chest proxies are included for farming; initial proxies use their explicit repeat counterpart or are skipped. Fixed loot generators choose one group by normalized Chance weights. Spawn equation bounds round with +0.5; a rounded uniform real draw gives half-sized endpoint intervals and full-sized interior intervals. Repeated attempts use the probability of at least one success, averaged over this count distribution.
- Evaluate container and dynamic-table equations with distinct callback contexts. Player average/min/max/count/difficulty come from GameEngine::GetPlayerInfo. Dynamic-table min/maxPlayerLevel and numberOfPlayers are constant 1, matching its callback. Container proxyLevel remains unsupported, rather than substituted. Skip unsupported expressions; never substitute invented values.

## Limits

These are estimates, not verified exact encounter probabilities. Spawn weighting, map availability, quest conditions, multiplayer scaling and runtime chest loot modifiers and float/RNG quantization are not fully modelled. Ranking is among the named NPC/equipment/boss-chest sources resolved by this model; it does not include every environmental chest, merchant or crafting route. It deliberately does not fabricate sources for records without a supported path. Source level is the database level, not a guarantee about a spawned enemy's level.

The three-line tooltip includes difficulty, source type, approximate percentage and level; its rows retain the approximate marker and its footer identifies database source levels. Loading/unavailable/no-current-difficulty-source states are localized. Sources are appended to discovered collection tooltips and also shown below `???` in the anonymous rollover for unknown entries. The anonymous path reads only generated source text: it never calls item name, description or stat builders. Catalogue counts and journal serialization are unchanged.

## Validation

The x86 ASI builds with the project's `/W4 /WX` settings. Offline generation against the installed database was exercised in EN and RU; generated output contains no more than three sorted sources per record, and the catalogue retains 3108 entries. This is build/data validation; in-game tooltip width, wrapping and engine rendering still need gameplay verification. No test suites were added or run.

Reference used to identify database traversal fields: [TQDB loot parser](https://github.com/fonsleenaars/tqdb/blob/master/tqdb/parsers/loot.py). Probability composition here is a separate implementation; engine behaviour was also inspected in the installed Game.dll exports/disassembly.

## Remaining context limits

The previous implementation used one player at the database source level. The live-context implementation below replaces that assumption. Repeated monster variants keep the highest conditional result and its level; this is not an averaged spawn probability. These context assumptions must not be presented as exact live-player odds. The correction fixes the group/count model; gameplay validation and actual player-context caching remain separate work.

## Level-context audit — 2026-10-05

Installed Game.dll SHA-256: `754907eacf552656945ff9eaf1763630e138506517e91b698ab28a0c3186aa86`. Addresses below are preferred-image VAs (image base 0x10000000), not runtime addresses. This audit reads the installed binary and database; it is not gameplay validation.

### Confirmed engine paths

- `GameEngine::GetPlayerInfo` (0x101A4E40) returns the engine override when enabled, otherwise delegates to `PlayerManagerClient::GetPlayerInfo` (0x1021D2D0). The latter accumulates player Character levels (+0xC5C), minimum, maximum and count, then rounds the arithmetic mean with +0.5. These are party-context values, not the monster's level.
- `FixedItemController::LoadDropLoot` (0x10182120) copies this PlayerInfo to controller +0x30 before evaluating the container equation. `GetDesignerVariable` (0x10182760) exposes averagePlayerLevel (+0x30), minPlayerLevel (+0x34), maxPlayerLevel (+0x38), numberOfPlayers (+0x3C), gameDifficulty (+0x40), and proxyLevel from the parent object. `parentLevel` is not exposed by this controller callback.
- `CalculateFixedItemLevel` (0x10181360) evaluates levelEquationFile using that controller; the rounded nonnegative result chooses/clamps an element of tables in `PickLootRecord` (0x10181280). Thus container selection really can depend on player level.
- `SelectLoot` (0x10181530), at 0x10181666, separately reads PlayerInfo and stores its average in LootLoader +0x30; the supplied loot parent-level argument goes to LootLoader +0x2C. These are distinct inputs. This level argument must be traced per container rather than replaced by a boss DB charLevel.
- `Character::CreateItemFromLootTable` (0x100D70E0) uses the Character's actual level (+0xC5C) for the loader parent level and its stored context at +0x104C..0x105C. This is a generation-time context, so changing player level later does not reroll already equipped items. Acquisition/fresh-spawn timing of that stored context is not established by this function alone.
- `LootLoader::GetLootName` (0x101CE610), at 0x101CE6A1..0x101CE6AB, passes the two distinct loader levels to the table virtual SetLevel. `LootItemTable_DynWeight::SetLevel` (0x101CD570) stores them at +0x6C/+0x70. Its designer-variable callback (0x101CD760, secondary EquationInterface base) exposes parentLevel and averagePlayerLevel from those values. For this callback minPlayerLevel, maxPlayerLevel and numberOfPlayers return constant 1; they must not be substituted from the container callback.
- `ProcessTableData` (0x101CCBC0) evaluates min/max/target once per table object (+0x78 flag) and clamps against the table's available item-level limits before selection. Context/caching timing matters to exact live odds.

### Installed database evidence

Archive scan decoded 74013 winning record paths (including internal/developer records for audit only; this does not expand the catalogue). Across that scan, 566 targetLevelEquation fields reference averagePlayerLevel. Counts describe records, not independent sources or reachable museum items. Example production equation:

`records/item/loottables/weapons/unique/axe_n02.dbr`: `((10 + parentLevel) / 2) * (1+(averagePlayerLevel / 100))`.

For Nessus' normal repeat chest:

- `records/item/containers/boss/repeatbosschest02_nessus_normal.dbr`
- equation file `records/item/containers/a02_containerlevelequation.dbr`
- equation `((averagePlayerLevel*1)/2.05)-1`
- tables `G_Default_03-05.dbr`, `G_Default_05-07.dbr`, chosen by the rounded index then clamped. For solo level 1 the first entry is selected; solo level 10 selects the second. This disproves a universal static chance for this chest.

Spawn-count equations commonly reference numberOfPlayers, e.g. `(3+(1.6*numberOfPlayers))*0.9`; multiplayer can affect at-least-one probability even when per-draw weights stay unchanged. Fixed-weight/direct item selections without level equations do not acquire a level dependency merely because other tables have it.

### Corrections to the earlier conclusion

The existing helper evaluate(s, level) initializes both parent and player to the same database source level. This is a modelling assumption, not engine behaviour. It also lacks maxPlayerLevel/minPlayerLevel/proxyLevel/gameDifficulty support. Current displayed numbers must remain estimates; the previous ranking check only established sorting and numeric bounds, not fidelity to live engine odds.

The earlier weighted-group/at-least-one corrections remain relevant, but the rounded real RNG endpoint distribution is conditional on the actual RNG implementation; SelectLootNumber calls through the engine RNG virtual method and identifying RandomUniformLocked alone does not prove that dispatch. Do not describe that detail as fully settled.

### Implementation requirements resulting from this audit

1. Separate equation contexts for container selection and dynamic item tables, reproducing each callback's variables.
2. Use actual generation-context player level/party size and source parent level; resolve variable source levels and container parent-level inputs. Never silently substitute the same scalar for all of them.
3. Precompute/cache supported contexts outside tooltip rendering; rerank sources for the selected context, retaining candidates beyond the current top-three file. A file truncated to three reference-level results cannot reconstruct a different-level top three.
4. Reproduce available-item clamping and generation-time table caching; finish RNG dispatch, chest modifiers and source-variant semantics before claiming exact encounter probabilities.

Evidence dumps and database inventory are retained in ignored build/audit-*.dis and build/audit-equations.json. The narrative above preserves confirmed findings and explicitly separates remaining uncertainties; it does not claim those uncertainties are already implemented or resolved.


## Live-context implementation

- `loot-model.bin` stores the reachable nodes, resolved container equations, item levels and localized source names. It retains all supported candidates, not just the reference top three. Production generation stages this sixth output and bumps the fingerprint to GDUT-STAMP 11. No journal/save format changes.
- The game-thread Present tick samples PlayerInfo and verifies an actual main player through Character::GetCharLevel, at most twice per second. Custom databases and invalid/missing player context fail closed. No game handles or callbacks reach the calculation thread.
- One background worker reads the immutable model once, calculates the selected difficulty, and caches four complete context results keyed by average/min/max player level, player count and difficulty. It publishes only a result matching the currently requested context. The tooltip does not traverse loot tables or archives. Old percentages disappear on context change.
- Preparation of the appended source lines uses mod-owned text buffers, avoiding accumulation of engine-allocated source strings across cache refreshes. The two original collection-status lines retain their existing lifetime.
- FixedItemController::LoadDropLoot at 0x101822C4..0x10182358 reads the selected FixedItemLoot's goldGeneratorLevel, converts it to an integer and passes it to SelectLoot. The solver now uses this table value as the chest's dynamic-table parent level, rather than the boss's charLevel. The container-index equation independently uses player context.
- RNG dispatch is now confirmed: GameEngine constructor at 0x101A3119 installs vtable 0x1039083C at engine +0x934. The slot at +4 points to thunk 0x100C66A6, importing Engine.dll RandomUniformLocked::IGenerate. Thus the earlier endpoint-count model applies to this installed engine (finite RNG precision remains approximated).
- Dynamic equation min/max/target levels are clamped to available item-level bounds, matching ProcessTableData. The target is not incorrectly clamped to the separately computed eligibility interval.

### Bounded real-data checks

The ASI builds successfully. Round-trip generation/loading/calculation of the persisted model was exercised with installed EN data at solo levels 1, 10, 30 and 85, and two players at level 30. Calculations took 406–479 ms on this host, on the background worker path, excluding initial model loading. Model size was 33,029,672 bytes. All inspected result files have finite probabilities in (0,1], descending rankings and at most ten entries per item. Catalogue remains 3108 items / 19 groups. These are data/build checks, not an in-game verification or proof of exact encounter odds; no test suites were run.

For the Small Torch normal repeat Nessus chest, the new model returns about 0.213648% at solo level 10 and 0.426922% at solo level 30. These are conditional model outputs, not a claim that live chest odds have been measured. The best three can change with context.

### Still approximate

Monster source levels are still database charLevel values. Map/proxy scaling and generation-time object/table caching are not fully reconstructed. Same-name variants still retain the best conditional variant and its displayed reference level, not a spawn-weighted average. Chest modifiers and all quest/source reachability conditions are not fully modelled. Consequently every displayed percentage retains the approximation marker; this patch fixes live player context and chest parent-level propagation, but does not label the entire model exact.


## Megalesios / Frostbite missing-source correction

The supplied tqdb.ru.1.5.1.json contains normal chest tagUWeapon100 (RU: Ледоруб) at 0.119 in creature tagMonsterName120 (Megalesios). Installed records confirm the source exists. Two independent importer omissions excluded it: special boss Class values were rejected despite inherited monster loot fields; repeat chest resolution accepted only the repeatbosschestproxy prefix and omitted bosschestproxy07_megalesios_repeat.dbr. Both recognition paths now support the suffix form, and creature/monster records with charLevel, description and monster drop/chest fields participate regardless of their specialized class. Fingerprint GDUT-STAMP 12 regenerates existing models automatically.

Focused installed-data calculation now ranks Megalesios repeat chest second for records/item/equipmentweapon/axe/u_n_frostbite.dbr at solo level 10 / Normal, about 0.084711%; solo level 30 gives about 0.050834%. The supplied JSON value 0.119% is NOT reproduced or declared equivalent: its chest selection/context and probability semantics still need reconciliation. This fixes missing candidates, not proof of exact probabilities. ASI build and model regeneration succeeded; collection remains 3108 / 19 groups. No test suites or in-game verification were performed.


## Full installed-record filter audit and cleanup

Inspected all 74,013 installed ARZ records, their serialized classes, drop fields and record references. Raw audit extracts are retained in ignored build/audit-source-records.json and build/audit-filter-summary.json. This is an importer/data audit, not an in-game encounter census.

Changes:
- Recognize inherited monster loot fields under both creature/monster and expansion creatures/monster directories, covering Hades and Cerberus classes previously excluded.
- Centralize internal/pet/NPC path exclusions, also covering zzdev, zz_dev, sandbox, old and E3 demonstration directories. This is a path filter, not a complete proof of runtime reachability.
- Centralize repeat-chest resolution for prefix and suffix naming in every expansion. Keep the explicitly linked original proxy when no distinct repeat record exists; do not silently discard single-proxy encounters.
- Replace the equal-charLevel difficulty heuristic with masks derived from proxy/pool/member connections; explicit Epic/Legendary pool overrides take precedence, with base pools used when no override is present. Hydra resolves to Legendary only. Sources spawned outside these proxies retain an unresolved/default difficulty mask; map and quest availability remains a separate limitation.
- Trim trailing whitespace from description tags, restoring four Colossal Scorpion source variants.
- Reuse one TSV formatter for offline diagnostics and live calculations, and one localized row formatter for both known and anonymous tooltips. Remove unused source heading/context/first-kill localization entries.

Final EN/RU generation retains 3108 catalogue items / 19 groups. EN graph retains 4082 referenced creature records; audit identifies 878 internal/pet/NPC records excluded and 175 unresolved creature descriptions before reachability pruning (includes internal/direct-text/missing-tag records, not 175 proven live encounters). Reference context has sources for 2830 items, previously 2816; this is item coverage, not encounter completeness. Three FixedItemContainerTartarus records remain explicitly unsupported, logged during generation, rather than treated as ordinary chests. Quest/map-spawned containers with no creature backlink or confirmed association are not all assigned boss identities. Therefore this audit does NOT certify every chest/encounter or exact probability.

ASI builds successfully; installed-data generation and persisted-model calculations complete, with finite bounded probabilities in the inspected contexts. Previous ASI: dist/known-good/TitanQuestMuseum-before-source-filter-audit.asi. Fingerprint GDUT-STAMP 13 forces regeneration after restart without deleting the collection. No test suites or in-game verification performed. Frostbite/Megalesios remains approximately 0.084711% at solo level 10 Normal; the supplied JSON's 0.119% is still unreconciled.

# Standalone viewer, first version

Optimization committed before starting: a5d6a15.

The immutable Catalogue already loaded by collection_runtime for set groups is
now retained and exposed via liveItemInfo. Existing group records and journalRows
are the only item/ownership inputs. The viewer owns its category, page, query and
selection; it never calls liveSelectGroup, viewRequest, item creation or journal
commit functions. It uses the existing tooltipSourceText context/calculations.

Rendering is on the native canvas through the existing Present hook. Icons load
at most one per drawn frame, cached until world resource reset, with original
alpha tinted black for unknown silhouettes. No extra graphics hook or sack.
The first version uses catalogue metadata for collected items, not a replica
stat tooltip. It supports name search, not the caravan's property search.

F8 toggles while a main player and journal set exist, outside the caravan.
Both key and native mouse gates must be installed. The shared key hook is now
installed even with caravan search disabled. WndProc navigates and closes;
ButtonEvent GetText supplies Unicode search text. Key releases retain original
bookkeeping. Native mouse and analog events are claimed while open; a short
close grace interval prevents the closing click/key from reaching gameplay.
World generation/resource reset, opening the caravan and focus loss close it.
The viewer does not pause combat or cancel an action already in progress.

Validation: x86 /W4 /WX build and mandatory hook-frame check passed. No test
suites run. In-game verification pending:

1. Outside caravan: F8, centered window, F8/Esc/X closing, focus loss, world exit.
2. Categories including sets, wheel on category list vs grid, arrows and PgUp/PgDn.
3. RU/EN name typing and Backspace; Owned filter agrees with actual journal storage.
4. Unknown silhouettes show current difficulty drop sources; stored ones show name/count.
5. Left/right/middle clicks never move, attack, drop, withdraw or duplicate underneath.
6. Close viewer and verify ordinary inventory controls, caravan deposit/take/copy
   and its previous category/search still work. Check different resolutions/UI scales.

Previous ASI/PDB pairs saved in dist/known-good before each build.

Follow-up: button captions are centered. The left list contains ordinary
categories only; its separate localized Sets button opens set cards in the
center, with journal-owned member counts. Clicking a card opens that set's
items. Clicking Sets returns to the cards. The arrows remain at the user's
request. Name search in the set list matches set names; Owned keeps sets with
at least one stored member. No caravan navigation or journal write is involved.

Layout follow-up: removed page arrows and their hit targets; wheel/PgUp/PgDn remain. The sidebar now fits 19 rows down to y=637. Search and Owned outer edges align with the item grid (x=202..738). Item outlines are hover-only; clicking may retain right-hand details but no persistent outline.

Current spacing update: aligned title/close to 12 px outer margins; item and set cards inset from the body panel; 15 visible category buttons separated by 4 px, wheel scroll with a thumb indicator. Footer shows journal-collected catalogue types / total and percentage (set groups excluded). Details are hover-only. Source panel displays up to 10 compact 40 px rows within the existing 500 px body height; long text remains ellipsized. Built successfully; in-game readability and alignment pending.

Row navigation update: wheel offset is one row (6 items / 2 set cards), clamped at total rows minus visible rows (4 / 6). PgUp/PgDn move one visible window. Draw, hover and set-card clicks share the same row offset; the footer shows the visible row range. Window height reduced from 652 to 630 with 12 px footer text, scope count above overall count. Build verified; runtime navigation and appearance pending.

Favorites update: item stars toggle catalogue-record bookmarks including unknown silhouettes. The localized Favorites view lists bookmarked base catalogue records (deduplicated), supports Owned, shared property/source search highlights and row scrolling. Each entry retains its original group/entry for property-index matching. Bookmarks persist separately as <journal-set>-favorites.txt in the mod data directory, via a flushed temporary file and atomic replacement; journal rows and save format are untouched. A read error, malformed or oversized file disables bookmark writes to preserve it. UI changes commit only after successful file replacement. Star artwork is drawn as a native polygon scanline fill, independent of font glyph coverage. Search category buttons use Museum blue fill/white captions with a gold selected border. Footer row indicator aligns with scope statistics. Manual verification pending: star add/remove on silhouettes and collected items, Favorites query/Owned/row navigation, reopening and restarting, independent save sets, final-favorite removal, ordinary caravan operations.

Star rendering revision: replaced subpixel rectangle runs with generated high-resolution white-alpha native TEX v2/DDSR sprites, using the same 64x96 header layout as shipped icons. Sprites are generated once per process in viewer-ui and that native directory source is registered before the first world, following the existing silhouette source pattern. Loaded textures are forgotten on world resource reset. Native textured-rectangle drawing reduces each star to one draw and preserves smooth alpha. Star occupies 14 px with 3 px top/right slot padding. Runtime asset generation/load and visual quality still need in-game verification. Search and two 91 px buttons end at x=738, the grid panel edge. Footer has no painted background or divider; text centers derive from body-bottom/window-bottom geometry.

Final state for this commit: shortcut C (INI viewer_hotkey=67), row scrolling, no page arrows or sidebar scrollbar, hover-only details, compact scope/overall counts without percentages, and a click-to-open help popup. Search covers the shared property index and the best contextual source. Favorites use native textured stars. Header is Titan Quest Museum (RU: Музей Titan Quest); the footer shows validated source-context difficulty. The no-sources message starts at the first source-row position. Earlier notes above describe intermediate iterations. Latest x86 build and mandatory hook-frame check passed; no test suites were run. User screenshots supplied visual feedback; complete in-game interaction verification remains separate.

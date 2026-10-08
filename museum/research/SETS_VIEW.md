Sets card catalogue

Set membership comes from catalogue.bin and is limited to existing Museum records. The catalogue shows two columns and eight rows of cards, with one-row scrolling. Each card opens discovery-aware native item prototypes; the full-width Back control restores the previous catalogue offset.

Cards show discovered parts, a contrast progress bar, and a green completed state with a localized label. Search by set name or discovered part highlights matching cards with a blue background and bright outline. Unknown parts retain anonymous tooltips and discovery-gated item highlights.

Catalogue statistics count completed sets versus available sets. Individual set views count parts. Alternate set groups are excluded from All and aggregate item-search counts. Journal/save formats are unchanged.

Category captions follow the requested selection, clearing stale set names. Empty focused search no longer alternates between caret and placeholder.

Verification: final x86 build and mandatory hook frame check passed. In-game screenshots confirmed the card catalogue and part progress before the final contrast, padding, completion-state and set-counter changes; those final visuals remain pending in-game confirmation. Tests were not rerun after switching from grouped sets to cards at the user's request; earlier grouped-view assertions are outdated.

Previous ASI snapshots remain in dist/known-good. Build publication preserves dist/TitanQuestMuseum.previous.bin and the previous PDB. No deployment or push is performed by the build.

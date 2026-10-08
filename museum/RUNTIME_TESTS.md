# Focused in-game test

Status: compile verified and offline checked; **not in-game verified**.

1. Quit Titan Quest. Disable/remove the old `uniquetab.asi` from the loader's
   plugin location. Install only `dist/TitanQuestMuseum.asi` in `scripts` beside
   TQ.exe, using the existing x86 ASI loader. Do not copy staging/backups/fixtures.
2. Start at the menu, then load a test character and open caravan -> Transfer.
   Check the normal Transfer is intact and the four-row panel is below the
   **whole** caravan, nearly its width, with equal side margins. Check all four
   rows remain visible at your resolution/UI scale.
3. Click Collection -> Equipment -> Weapons -> Other. Check category buttons fill
   the row, Artifact takes the whole row, Search width equals Transfer, both
   mode buttons stay visible, and Search/Owned/mode/section fonts stay constant.
4. Switch Torso -> Legs and Staff -> Spear -> Amulet. Check `All` is present, its
   final digit stays at the same right X position, text stays clear of Owned, and
   captions are centered. Type a known property; check highlights and category
   marks, then clear it. Toggle Owned; check the same collected records/counts.
5. Wheel over the collection/grid/panel in both directions, including at ends.
   Check existing window paging remains responsive, with no page text or arrows.
   Check MI records remain present, unknown icons stay black and reveal no normal
   tooltip on hover, while collected items show normal tooltips.
6. With a test item, verify deposit/take and then allow a character save. Return to
   Transfer, close/reopen caravan, and reload the character. Check the item and
   journal counts persist and normal Transfer remains usable.

The plugin copies missing compatible journals from sibling `uniquetab` folders
into `TitanQuestMuseum` on first startup; it never replaces an existing Museum
journal or modifies the old journal. Check `TitanQuestMuseum/TitanQuestMuseum.log`
for imports, `bindings ... all confirmed`, and native text alignment `verified`.
If data previously used a different custom output location, copy its named journal
files into Museum's data folder while the game is closed.

Send panel screenshots for steps 2-4 and describe any failed step. Compilation
and offline checks do not substitute for these runtime results.

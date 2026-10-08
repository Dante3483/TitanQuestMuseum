# First in-game feedback fixes

The reported Bow gap is the unused tail of a 15-cell-high host: three 4-cell
slot rows previously covered only 12 cells. Live placement now maps slot-row
edges onto all 15 host rows using integer division. Item footprints and the
number of visible records remain unchanged. Slot backgrounds, hover/owned
overlays and leftover covers receive the same remapped geometry. An incomplete
last window or an empty Owned filter can still contain genuinely empty slots.

The panel's outer margin is now zero, matching both caravan frame edges.
Empty Search uses native left alignment and the same small inset as query text.

Window-message consumption alone did not block DirectInput. Disassembly of the
installed Engine.dll shows Engine::ProcessUserInput **inlines** the mouse
dispatcher; detouring exported Display::HandleMouseEvent would not intercept it.
Museum therefore registers a native DisplayWidget with Engine::AddWidget and
keeps it first in the dispatch vector, preserving the other widgets' relative order.
Its six-entry ABI is Update, Render, Key, Mouse, Analog, AlwaysReceiveInput.
Only its Mouse method consumes input, only while the visible Museum panel claims
the cursor/press. Render/Update do nothing and Key/Analog return false.
The AddWidget-derived display offset and exported base-vtable slots are checked
before installation. No game RVA is hardcoded. The widget stays alive until exit.
Left/right presses begun on the panel are captured through release, including
release outside the panel. Focus loss/cancellation clears the held state.

Run `tools/verify_native_input.py` against the installed engine for read-only ABI
evidence. Runtime still requires checking stationary character on all panel
buttons, held clicks and release outside, normal movement outside the panel,
tab/close/reopen, tooltips, Owned, search and wheel across short/tall categories.

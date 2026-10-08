# Titan Quest Toolkit architecture

Core owns shared generation, live context, source snapshots and callback/drawing
services. Museum and MobRadar are independent consumers of its C API. Museum
retains its inventory/storage hooks; Core installs only common Present/input hooks.
Museum waits for Core initialization before installing its hooks so chained
trampolines preserve the existing calls. Museum reports keyboard focus to Core,
which gives its search/viewer priority over radar shortcuts.

Data lifetimes are internal to Core. The source worker computes outside frame
callbacks, validates the requested context before publishing, and increments the
revision when context changes. Consumers query counts then copy into their own
buffers; stale or short-buffer copies write no payload. Host callbacks borrow
player/render pointers only during the callback. Registry entries pin plugin
modules and isolate callback faults. No hot unload is supported.

For Museum-specific storage/UI details see [Museum architecture](museum/ARCHITECTURE.md).

MuseumRadar is an optional consumer of MobRadar API v2. It waits for loaded Museum
and MobRadar modules, watches Museum JSON metadata off the rendering thread and
publishes validated collected-record snapshots only when they change. MobRadar
forwards these through Core API v2. Core excludes collected items only when
building farming targets; full Museum probability/source text remains intact.
Missing or invalid journals restore unfiltered targets. No file data is modified.

MobRadar API v2 adds a pinned inventory-inspector callback and shared-data directory
access. One callback per second receives borrowed nearby Monster actors and the
existing object-list snapshot. MuseumRadar reads inventory/equipment IDs, resolves
requested objects against that snapshot and returns caller-owned text rows. Item
names are disclosed only for validated collected records; unavailable journal
state yields question marks. Results never store engine pointers and coexist with
filtered best-source rows. Core API remains v2 and MobRadar still exposes legacy v1.

# Titan Quest Toolkit architecture

Core owns shared generation, live context, source snapshots and callback/drawing
services. Museum and FarmRadar are independent consumers of its C API. Museum
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

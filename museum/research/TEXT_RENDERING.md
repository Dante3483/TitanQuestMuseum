# Titan Quest native text proof

Installed Engine.dll SHA256:
`0aedbb1805b4a5616f74e34d4f609f392e2c2dd4561c64c118f4772ab4f694f6`.
Read-only export dump and dumpbin disassembly were inspected; no game was launched.

Public RenderText(Rect,char/wide) takes a by-value four-float rectangle. The
point overloads construct a zero-size rectangle and call these wrappers. Wide
rectangle wrapper is RVA 0x15E7A0, ASCII 0x15E8B0 in the inspected file; the
production code resolves the decorated wide name, never those RVAs.

The wrapper obtains native glyph dimensions before alignment. Its alignment
helper (RVA 0x15E360 in this file) decrements X alignment once and branches to
`rect.x + rect.w - text.width`: enum 1 is right. A second decrement takes
`rect.x + rect.w/2 - text.width/2`: enum 2 is center. Other values keep rect.x.
The same arithmetic repeats for Y and native text height. It uses engine font
metrics, not string-length estimates. Production and verify_native_text.py
derive this helper from wrapper CALL operands and verify both branch shapes.

RenderText returns the font glyph advance as float. The public point overload is
used off canvas, with alpha zero, to measure the complete string; measurements
are cached by font pointer/text/size. Empty/invalid font metrics cannot produce
overflowing control text. Cache and font pointers are forgotten at world teardown.
Button and statistics text uses native rectangle alignment in both axes. No
arbitrary per-caption X/Y offsets or Grim Dawn font data are used.

Compile/offline proof verifies ABI shape and enum arithmetic. Live font rendering,
measurement and baseline behavior still require the runtime screenshots.

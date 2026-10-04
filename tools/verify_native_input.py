"""Read-only proof for Museum's native DisplayWidget input receiver."""
import struct
from pathlib import Path
import tqpath

d = (Path(tqpath.game_dir()) / 'Engine.dll').read_bytes()
pe = struct.unpack_from('<I', d, 60)[0]
opt = pe + 24
base = struct.unpack_from('<I', d, opt + 28)[0]
table = opt + struct.unpack_from('<H', d, pe + 20)[0]
sections = [struct.unpack_from('<IIII', d, table + 40*i + 8)
            for i in range(struct.unpack_from('<H', d, pe + 6)[0])]
def offset(rva):
    return next(raw+rva-va for size, va, rs, raw in sections if va <= rva < va+max(size, rs))
e = offset(struct.unpack_from('<I', d, opt + 96)[0])
count = struct.unpack_from('<I', d, e + 24)[0]
functions, names, ordinals = struct.unpack_from('<III', d, e + 28)
exports = {}
for i in range(count):
    n = offset(struct.unpack_from('<I', d, offset(names) + 4*i)[0])
    name = d[n:d.index(b'\0', n)].decode('ascii')
    ordinal = struct.unpack_from('<H', d, offset(ordinals) + 2*i)[0]
    exports[name] = struct.unpack_from('<I', d, offset(functions) + 4*ordinal)[0]
def code(name, size):
    start = offset(exports[name])
    return d[start:start+size]
add = code('?AddWidget@Engine@GAME@@QAEXPAVDisplayWidget@2@@Z', 27)
assert add[13:15] == b'\x81\xc1' and add[19] == 0xe8 and add[24:27] == b'\xc2\x04\x00'
display = struct.unpack_from('<I', add, 15)[0]
assert 0x100 <= display < 0x1000
v = struct.unpack('<6I', code('??_7DisplayWidget@GAME@@6B@', 24))
expected = {
    0: '?Update@DisplayWidget@GAME@@UAEXXZ',
    2: '?HandleKeyEvent@DisplayWidget@GAME@@UAE_NABVButtonEvent@InputDevice@2@@Z',
    3: '?HandleMouseEvent@DisplayWidget@GAME@@UAE_NABUMouseEvent@InputDevice@2@@Z',
    4: '?HandleAnalogInputEvent@DisplayWidget@GAME@@UAE_NABUAnalogInputEvent@InputDevice@2@@Z',
    5: '?ShouldAlwaysReceiveInput@DisplayWidget@GAME@@UAE_NXZ',
}
for slot, name in expected.items():
    assert v[slot] - base == exports[name], (slot, name)
process = code('?ProcessUserInput@Engine@GAME@@QAEXXZ', 0x208)
assert struct.pack('<I', display) in process
assert b'\x8b\x40\x0c\xff\xd0' in process, 'native mouse dispatch must use vtable +0x0c'
assert b'\xff\x50\x04' in code('?Render@Display@GAME@@QBEXAAVGraphicsCanvas@2@@Z', 70)
print(f'PASS: native DisplayWidget ABI, Render slot 1, Mouse slot 3; engine display offset {display:#x}')
print('PASS: ProcessUserInput inlines mouse dispatch; exported HandleMouseEvent hook would be insufficient')

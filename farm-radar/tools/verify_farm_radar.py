"""Read-only ABI audit for the nearby farming panel, using the installed AE binaries."""
import hashlib
import struct
import re
from pathlib import Path
import tqpath

class PE:
    def __init__(self, path):
        self.data = path.read_bytes()
        pe = self.u32(0x3c)
        assert self.u16(pe + 4) == 0x14c, 'x86 image required'
        optional = pe + 24
        self.base = self.u32(optional + 28)
        table = optional + self.u16(pe + 20)
        self.sections = [struct.unpack_from('<8sIIII', self.data, table + i * 40)
                         for i in range(self.u16(pe + 6))]
        exports = self.offset(self.u32(optional + 96))
        count = self.u32(exports + 24)
        functions, names, ordinals = struct.unpack_from('<III', self.data, exports + 28)
        self.exports = {}
        for i in range(count):
            start = self.offset(self.u32(self.offset(names) + i * 4))
            name = self.data[start:self.data.index(0, start)].decode()
            ordinal = self.u16(self.offset(ordinals) + i * 2)
            self.exports[name] = self.u32(self.offset(functions) + ordinal * 4)
    def u16(self, at): return struct.unpack_from('<H', self.data, at)[0]
    def u32(self, at): return struct.unpack_from('<I', self.data, at)[0]
    def offset(self, rva):
        return next(s[4] + rva - s[2] for s in self.sections if s[2] <= rva < s[2] + s[3])
    def body(self, name, count=210):
        at = self.offset(self.exports[name])
        return self.data[at:at + count]

game = Path(tqpath.game_dir())
engine = PE(game / 'Engine.dll')
actors = PE(game / 'Game.dll')
plugin = (Path(__file__).resolve().parents[1] / 'src/plugin.cpp').read_text(encoding='utf-8')
for name in re.findall(r'bind\([^,]+,e,"([^"]+)"\)', plugin):
    assert name in engine.exports, f'missing add-on engine export: {name}'
print('PASS: add-on ObjectManager/vector/ObjectName binding names exist')
coords = engine.body('?GetCoords@Entity@GAME@@QBE?AVWorldCoords@2@XZ')
assert coords[150:165] == bytes.fromhex('8b442444 8db1a0000000 b90d000000')
assert coords[165:177] == bytes.fromhex('8bf8 f3a5 5f5e 83c438 c20400')
position = engine.body('?GetWorldPosition@WorldVec3@GAME@@QBE?AVVec3@2@XZ', 90)
assert position[:15] == bytes.fromhex('83ec0c 8b01 f30f7e402c 660f6e5834')
assert engine.body('?GetWorldIndex@Region@GAME@@QBEHXZ', 4) == bytes.fromhex('8b4128c3')
assert engine.body('??0WorldCoords@GAME@@QAE@PAVRegion@1@ABVCoords@1@@Z', 21) == bytes.fromhex(
    '8b542408 8b442404 8901 f30f7e4224 660fd64104 8b422c')[:21]
alive = actors.exports['?IsAlive@Character@GAME@@UBE_NXZ'] + actors.base
for class_name in ('Monster', 'Ormenos'):
    vt = actors.offset(actors.exports[f'??_7{class_name}@GAME@@6B@'])
    assert actors.u32(vt + 86 * 4) == alive, f'{class_name} liveness slot differs'
    print(f'PASS: {class_name} inherits Character::IsAlive at slot 86')
print('PASS: WorldCoords = 52 bytes, hidden output argument, WorldVec3 prefix = Region* + Vec3')
print('PASS: world position includes integer region offsets; separate world index available')
for label, pe in [('Engine.dll', engine), ('Game.dll', actors)]:
    print(label, 'SHA256', hashlib.sha256(pe.data).hexdigest())

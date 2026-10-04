"""Validate final PE and reference preservation; write evidence inside Museum only."""
from pathlib import Path
import hashlib, json, struct
p=Path(__file__).resolve().parents[1]
reference=p.parent
count=0
for line in (p/'research/REFERENCE_INVENTORY.txt').read_text(encoding='utf8').splitlines():
    path, size, digest=line.split(' | ')
    data=(reference/path).read_bytes()
    assert len(data)==int(size) and hashlib.sha256(data).hexdigest()==digest, f'Reference changed: {path}'
    count+=1
binary=p/'dist/TitanQuestMuseum.asi'
data=binary.read_bytes()
pe=struct.unpack_from('<I',data,0x3c)[0]
assert data[:2]==b'MZ' and data[pe:pe+4]==b'PE\0\0'
assert struct.unpack_from('<H',data,pe+4)[0]==0x14c, 'not x86'
assert struct.unpack_from('<H',data,pe+22)[0]&0x2000, 'not a DLL'
assert struct.unpack_from('<H',data,pe+24)[0]==0x10b, 'not PE32'
report={'binary':str(binary.relative_to(p)), 'bytes':len(data),
        'sha256':hashlib.sha256(data).hexdigest(), 'pe':'x86 PE32 DLL',
        'reference_files_unchanged':count,'compile_verified':True,'in_game_verified':False}
(p/'build/VERIFICATION.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(report,indent=2))

"""Read-only proof of the exported rectangle text wrapper and alignment values."""
import hashlib, struct
from pathlib import Path
import tqpath

data=(Path(tqpath.game_dir())/'Engine.dll').read_bytes()
pe=struct.unpack_from('<I',data,0x3c)[0]
sections=struct.unpack_from('<H',data,pe+6)[0]
optional_size=struct.unpack_from('<H',data,pe+20)[0]
optional=pe+24
imagebase=struct.unpack_from('<I',data,optional+28)[0]
table=optional+optional_size
def offset(rva):
    for i in range(sections):
        size,va,rawsize,raw=struct.unpack_from('<IIII',data,table+40*i+8)
        if va <= rva < va+max(size,rawsize): return raw+rva-va
    raise ValueError('RVA outside image')
export_rva=struct.unpack_from('<I',data,optional+96)[0]
e=offset(export_rva)
count=struct.unpack_from('<I',data,e+24)[0]
functions,names,ordinals=struct.unpack_from('<III',data,e+28)
name=b'?RenderText@GraphicsCanvas@GAME@@QAEMVRect@2@ABVColor@2@PBGPBVGraphicsFont@2@HW4XAlignment@12@W4YAlignment@12@_NHW4RenderFontStyle@2@66H6@Z'
wrapper=None
for i in range(count):
    n=offset(struct.unpack_from('<I',data,offset(names)+i*4)[0])
    if data[n:data.index(b'\0',n)]==name:
        ordinal=struct.unpack_from('<H',data,offset(ordinals)+i*2)[0]
        wrapper=struct.unpack_from('<I',data,offset(functions)+ordinal*4)[0]
        break
assert wrapper is not None, 'rectangle RenderText export missing'
found=False
for i in range(180):
    at=offset(wrapper)+i
    if data[at]!=0xe8: continue
    rva=wrapper+i+5+struct.unpack_from('<i',data,at+1)[0]
    try: code=data[offset(rva):offset(rva)+0xa0]
    except ValueError: continue
    if code[:10]!=bytes.fromhex('8b442420 f30f10442408'): continue
    assert [code[j] for j in (0x17,0x18,0x1a,0x1b,0x63,0x64,0x66,0x67)]==[0x48,0x74,0x48,0x75]*2
    found=True
    print(f'PASS: rectangle wrapper RVA {wrapper:#x}, alignment helper RVA {rva:#x}; X/Y: 0 left/top, 1 right/bottom, 2 center')
assert found, 'alignment helper shape not verified'
print('Engine.dll SHA256',hashlib.sha256(data).hexdigest())

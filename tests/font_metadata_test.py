"""The packaged font must render a readable weight in stb_truetype (no variable axes)."""
from pathlib import Path
import struct
font=(Path(__file__).resolve().parents[1]/'resources/font/NotoSansHebrew.ttf').read_bytes()
tables={}
for i in range(struct.unpack_from('>H',font,4)[0]):
    tag,checksum,offset,length=struct.unpack_from('>4sIII',font,12+16*i)
    tables[tag]=offset
assert b'fvar' not in tables,'stb_truetype cannot select the weight axis'
weight=struct.unpack_from('>H',font,tables[b'OS/2']+4)[0]
assert weight==500,weight
print('Packaged font: static medium (500), compatible with Switch rasterizer')

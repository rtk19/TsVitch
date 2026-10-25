"""Validate Switch release metadata and packaged runtime resources."""
import argparse
from pathlib import Path
import struct

parser=argparse.ArgumentParser()
parser.add_argument('file',type=Path)
parser.add_argument('--version',default='0.5.1')
parser.add_argument('--author',default='Refael Bar')
args=parser.parse_args()
data=args.file.read_bytes()
assert data[0x10:0x14]==b'NRO0'
size=struct.unpack_from('<I',data,0x18)[0]
assert data[size:size+4]==b'ASET'
icon_offset,icon_size,nacp_offset,nacp_size,romfs_offset,romfs_size=struct.unpack_from('<6Q',data,size+8)
for offset,length in [(icon_offset,icon_size),(nacp_offset,nacp_size),(romfs_offset,romfs_size)]:
    assert offset>0 and length>0 and size+offset+length<=len(data)
version=data[size+nacp_offset+0x3060:size+nacp_offset+0x3070].split(b'\0')[0].decode()
author=data[size+nacp_offset+0x200:size+nacp_offset+0x300].split(b'\0')[0].decode()
assert version==args.version,(version,args.version)
assert author==args.author,(author,args.author)
root=Path(__file__).resolve().parents[1]
for resource in ['font/NotoSansHebrew.ttf','i18n/he/tsvitch.json','i18n/he/iptv.json','xml/views/video_card_live.xml']:
    assert (root/'resources'/resource).read_bytes() in data,resource
print(f'Validated NRO {version}, developer {author}, Hebrew resources and channel cards ({len(data):,} bytes)')

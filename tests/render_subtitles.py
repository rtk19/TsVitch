# Manual libass rendering check. Requires Python Pillow and the host libass.
# Run: python3 tests/render_subtitles.py [output-directory]
# Inspect both PNGs for Hebrew reading order, punctuation and mixed Latin/digits.
# Uses no system font provider; matches the Switch font-loading configuration.
import ctypes as c
from pathlib import Path
import sys
import os
repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('/tmp/nx-subtitle-preview')
output.mkdir(parents=True, exist_ok=True)
from PIL import Image
ass = c.CDLL('libass.so.9')
rounded = c.CDLL(os.environ['TSVITCH_SUBTITLE_RENDERER_LIB']) if os.environ.get('TSVITCH_SUBTITLE_RENDERER_LIB') else None
def setup(name, restype, args):
    wrapped = rounded and name in ('ass_render_frame', 'ass_renderer_done', 'ass_set_frame_size')
    fn = getattr(rounded if wrapped else ass, '__wrap_' + name if wrapped else name)
    fn.restype = restype; fn.argtypes = args; return fn
ptr = c.c_void_p
library = setup('ass_library_init', ptr, [])()
setup('ass_set_fonts_dir', None, [ptr,c.c_char_p])(library, str(repo / 'resources/subtitle-fonts').encode())
renderer = setup('ass_renderer_init', ptr, [ptr])(library)
setup('ass_set_frame_size',None,[ptr,c.c_int,c.c_int])(renderer,1280,720)
# No fontconfig/system font provider: match Switch's font discovery.
setup('ass_set_fonts',None,[ptr,c.c_char_p,c.c_char_p,c.c_int,c.c_char_p,c.c_int])(renderer,None,b'Liberation Sans',0,None,1)
setup('ass_set_shaper',None,[ptr,c.c_int])(renderer,1)
script='''[Script Info]
ScriptType: v4.00+
PlayResX: 1280
PlayResY: 720
[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Liberation Sans,34,&H00FFFFFF,&H00FFFFFF,&H00000000,&H66000000,0,0,0,0,100,100,0,0,4,0,6,2,30,30,10,-1
[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:00.00,0:00:05.00,Default,,0,0,0,,איפה אנחנו מתחילים, אני, אתה?
Dialogue: 0,0:00:05.00,0:00:10.00,Default,,0,0,0,,עברית ו-English: מספר 123, שָׁלוֹם!
Dialogue: 0,0:00:10.00,0:00:15.00,Default,,0,0,0,,שורה ראשונה\\NSecond line 123
'''.encode()
track=setup('ass_read_memory',ptr,[ptr,c.c_char_p,c.c_size_t,c.c_char_p])(library,script,len(script),b'UTF-8')
assert track
class ASSImage(c.Structure): pass
ASSImage._fields_=[('w',c.c_int),('h',c.c_int),('stride',c.c_int),('bitmap',c.POINTER(c.c_ubyte)),('color',c.c_uint32),('dst_x',c.c_int),('dst_y',c.c_int),('next',c.POINTER(ASSImage)),('type',c.c_int)]
render=setup('ass_render_frame',c.POINTER(ASSImage),[ptr,ptr,c.c_longlong,c.POINTER(c.c_int)])
for ms in (1000,6000,11000):
    changed=c.c_int(); cur=render(renderer,track,ms,c.byref(changed)); count=0; has_background=False
    out=Image.new('RGBA',(1280,720),(48,64,82,255))
    while cur:
        im=cur.contents
        if im.color == 0x00000066 and im.w > 20 and im.h > 10:
            has_background=True
            if rounded:
                assert im.bitmap[0] == 0, 'Top corner must be transparent'
                assert im.bitmap[im.w-1] == 0, 'Opposite corner must be transparent'
                assert im.bitmap[(im.h-1)*im.stride] == 0, 'Bottom corner must be transparent'
                assert im.bitmap[(im.h//2)*im.stride+im.w//2] == 255, 'Box centre must remain solid'
                assert any(0 < im.bitmap[y*im.stride+x] < 255
                           for y in range(min(12, im.h)) for x in range(min(12, im.w))), 'Edge must be antialiased'
        if im.w and im.h:
            bitmap=b''.join(c.string_at(c.addressof(im.bitmap.contents)+row*im.stride, im.w) for row in range(im.h))
            alpha=Image.frombytes('L',(im.w,im.h),bitmap).point(lambda x: x*(255-(im.color&255))//255)
            ink=Image.new('RGBA',(im.w,im.h),((im.color>>24)&255,(im.color>>16)&255,(im.color>>8)&255,255)); ink.putalpha(alpha)
            out.alpha_composite(ink,(im.dst_x,im.dst_y)); count+=1
        cur=im.next
    assert count > 0
    # The 60%-opaque black background must be emitted by libass itself.
    assert has_background, 'Missing translucent background'
    out.convert('RGB').save(str(output / f'hebrew-{ms}.png'))
    print(f'Bundled fonts with no system provider at {ms}ms: {count} rendered glyph layers, PASS')
    if rounded:
        # Repeated unchanged frames must not accumulate clipping or change text.
        def snapshot(images):
            result=[]
            while images:
                im=images.contents
                bitmap=b''.join(c.string_at(c.addressof(im.bitmap.contents)+row*im.stride, im.w)
                                for row in range(im.h)) if im.w and im.h else b''
                result.append((im.w,im.h,im.dst_x,im.dst_y,im.color,bitmap))
                images=im.next
            return result
        before=snapshot(render(renderer,track,ms,c.byref(changed)))
        for repeat in range(20):
            assert snapshot(render(renderer,track,ms,c.byref(changed))) == before
        # Compare with libass' original output. Text and box geometry/opacity
        # must stay byte-identical; only the four background corners change.
        original=ass.ass_render_frame
        original.restype=c.POINTER(ASSImage)
        original.argtypes=[ptr,ptr,c.c_longlong,c.POINTER(c.c_int)]
        plain=snapshot(original(renderer,track,ms,c.byref(changed)))
        assert len(plain)==len(before)
        for a,b in zip(before,plain):
            assert a[:5]==b[:5]
            if a[4] != 0x66: assert a[5]==b[5], 'Text pixels must not change'

if rounded:
    # Docked resolution and seek past the last cue must invalidate old images.
    setup('ass_set_frame_size',None,[ptr,c.c_int,c.c_int])(renderer,1920,1080)
    cur=render(renderer,track,11000,c.byref(changed))
    assert cur and cur.contents.bitmap[0]==0
    assert not render(renderer,track,16000,c.byref(changed))
    # Background off: outline/shadow glyphs must remain unchanged.
    off=script.replace(b',4,0,6,2,',b',1,1,0.5,2,').replace(b'&H66000000',b'&H80000000')
    off_track=setup('ass_read_memory',ptr,[ptr,c.c_char_p,c.c_size_t,c.c_char_p])(library,off,len(off),b'UTF-8')
    wrapped_pixels=snapshot(render(renderer,off_track,1000,c.byref(changed)))
    assert wrapped_pixels==snapshot(original(renderer,off_track,1000,c.byref(changed)))
    setup('ass_free_track',None,[ptr])(off_track)
    print('Rounded background: unchanged frames, multiline, docked size and background-off PASS')
setup('ass_free_track',None,[ptr])(track)
setup('ass_renderer_done',None,[ptr])(renderer)
setup('ass_library_done',None,[ptr])(library)

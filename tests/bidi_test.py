"""Exercise the same FriBidi reorder used by the generated NanoVG renderer."""
import ctypes
import ctypes.util

library = ctypes.CDLL(ctypes.util.find_library('fribidi'))
library.fribidi_log2vis.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_int,
    ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p,
    ctypes.c_void_p, ctypes.c_void_p]
library.fribidi_log2vis.restype = ctypes.c_ubyte

def reorder(text):
    logical = (ctypes.c_uint32 * len(text))(*map(ord, text))
    visual = (ctypes.c_uint32 * len(text))()
    direction = ctypes.c_uint32(0x40)  # FRIBIDI_PAR_ON
    assert library.fribidi_log2vis(logical, len(text), ctypes.byref(direction), visual, None, None, None)
    return ''.join(map(chr, visual))

assert reorder('שלום') == 'םולש'
assert '123' in reorder('ערוץ 123')
assert 'English' in reorder('שלום English 123')
assert reorder('English 123') == 'English 123'
print('Hebrew and mixed-direction rendering checks passed')

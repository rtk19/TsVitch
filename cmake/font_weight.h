#ifndef TSVITCH_FONT_WEIGHT_H
#define TSVITCH_FONT_WEIGHT_H
// A subtle fractional horizontal embolden, applied once when caching a glyph.
// Glyph advances and line wrapping are untouched. Icon/emoji glyphs stay crisp.
static inline void tsvitchEmbolden(unsigned char *bitmap, int width, int height, int stride,
                                   unsigned int codepoint) {
    int x, y;
    if ((codepoint >= 0xe000 && codepoint <= 0xf8ff) || (codepoint >= 0x1f000 && codepoint <= 0x1faff))
        return;
    for (y = 0; y < height; ++y)
        for (x = width - 2; x > 0; --x) {
            unsigned char *pixel = bitmap + y * stride + x;
            if (pixel[-1] > *pixel)
                *pixel = (unsigned char)(*pixel + (pixel[-1] - *pixel) * 35 / 100);
        }
}
#endif

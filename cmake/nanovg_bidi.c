#include <fribidi.h>
// Reorder at the rendering boundary, after NanoVG has wrapped logical text into lines.
// Logical strings remain intact for search, clipboard, URLs and persistence.
static char* tsvitchVisualText(const char* string, const char* end) {
    if (!string) return NULL;
    size_t length = end ? (size_t)(end - string) : strlen(string);
    int rtl = 0;
    for (size_t i = 0; i < length; ++i) if ((unsigned char)string[i] >= 0xd6 && (unsigned char)string[i] <= 0xdb) { rtl = 1; break; }
    if (!rtl || !length) return NULL;
    FriBidiChar* logical = malloc((length + 1) * sizeof(FriBidiChar));
    FriBidiChar* visual = malloc((length + 1) * sizeof(FriBidiChar));
    char* output = malloc(length * 4 + 1);
    if (!logical || !visual || !output) { free(logical); free(visual); free(output); return NULL; }
    FriBidiStrIndex count = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, string, (FriBidiStrIndex)length, logical);
    FriBidiParType direction = FRIBIDI_PAR_ON;
    if (!fribidi_log2vis(logical, count, &direction, visual, NULL, NULL, NULL)) {
        free(logical); free(visual); free(output); return NULL;
    }
    const FriBidiStrIndex bytes = fribidi_unicode_to_charset(FRIBIDI_CHAR_SET_UTF8, visual, count, output);
    output[bytes] = 0;
    free(logical); free(visual);
    return output;
}
float nvgText(NVGcontext* ctx, float x, float y, const char* string, const char* end) {
    char* visual = tsvitchVisualText(string, end);
    float result = tsvitchTextLtr(ctx, x, y, visual ? visual : string, visual ? NULL : end);
    free(visual);
    return result;
}
float nvgTextWithCursor(NVGcontext* ctx, float x, float y, const char* string, const char* end, int cursor) {
    // Preserve logical cursor offsets during text editing.
    char* visual = cursor < 0 ? tsvitchVisualText(string, end) : NULL;
    float result = tsvitchTextWithCursorLtr(ctx, x, y, visual ? visual : string, visual ? NULL : end, cursor);
    free(visual);
    return result;
}

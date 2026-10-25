#include <fribidi.h>
// Reorder at the rendering boundary, after NanoVG has wrapped logical text into lines.
// Logical strings remain intact for search, clipboard, URLs and persistence.
static char* tsvitchVisualText(const char* string, const char* end, int* cursor) {
    if (!string) return NULL;
    size_t length = end ? (size_t)(end - string) : strlen(string);
    int rtl = 0;
    for (size_t i = 0; i < length; ++i) if ((unsigned char)string[i] >= 0xd6 && (unsigned char)string[i] <= 0xdb) { rtl = 1; break; }
    if (!rtl || !length) return NULL;
    FriBidiChar* logical = malloc((length + 1) * sizeof(FriBidiChar));
    FriBidiChar* visual = malloc((length + 1) * sizeof(FriBidiChar));
    char* output = malloc(length * 4 + 1);
    FriBidiStrIndex* mapping = cursor ? malloc((length + 1) * sizeof(FriBidiStrIndex)) : NULL;
    FriBidiLevel* levels = cursor ? malloc((length + 1) * sizeof(FriBidiLevel)) : NULL;
    if (cursor && (!mapping || !levels)) {free(mapping);free(levels);free(logical);free(visual);free(output);return NULL;}
    if (!logical || !visual || !output) { free(mapping);free(levels);free(logical); free(visual); free(output); return NULL; }
    FriBidiStrIndex count = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, string, (FriBidiStrIndex)length, logical);
    FriBidiParType direction = FRIBIDI_PAR_ON;
    if (!fribidi_log2vis(logical, count, &direction, visual, mapping, NULL, levels)) {
        free(mapping);free(levels);free(logical); free(visual); free(output); return NULL;
    }
    if (cursor && count > 0) {
        int logicalCursor = *cursor < 0 ? 0 : *cursor > count ? count : *cursor;
        if (logicalCursor == 0) *cursor = mapping[0] + ((levels[0] & 1) ? 1 : 0);
        else *cursor = mapping[logicalCursor-1] + ((levels[logicalCursor-1] & 1) ? 0 : 1);
    }
    free(mapping);free(levels);
    const FriBidiStrIndex bytes = fribidi_unicode_to_charset(FRIBIDI_CHAR_SET_UTF8, visual, count, output);
    output[bytes] = 0;
    free(logical); free(visual);
    return output;
}
float nvgText(NVGcontext* ctx, float x, float y, const char* string, const char* end) {
    char* visual = tsvitchVisualText(string, end, NULL);
    float result = tsvitchTextLtr(ctx, x, y, visual ? visual : string, visual ? NULL : end);
    free(visual);
    return result;
}
float nvgTextWithCursor(NVGcontext* ctx, float x, float y, const char* string, const char* end, int cursor) {
    // Keep logical editing offsets while positioning the caret in visual bidi order.
    char* visual = tsvitchVisualText(string, end, cursor >= 0 ? &cursor : NULL);
    float result = tsvitchTextWithCursorLtr(ctx, x, y, visual ? visual : string, visual ? NULL : end, cursor);
    free(visual);
    return result;
}

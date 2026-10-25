#ifdef __SWITCH__
// Ported from rtk19/nx-torrent-player, Apache-2.0; same libass box mask and sizing.
#include <ass/ass.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

// libass BorderStyle 4 has padding but no corner radius. Wrap its final image
// list so mpv still owns shaping, bidi, positioning and subtitle composition.
// Never edit libass' cached bitmap: it may be shared with other images/frames.
namespace
{
struct Frame
{
    struct Mask
    {
        int w = 0, h = 0, frameHeight = 0;
        std::vector<unsigned char> pixels;
    };
    int height = 720;
    std::vector<ASS_Image> images;
    std::vector<Mask> masks;
};
std::mutex framesMutex;
std::map<ASS_Renderer*, Frame> frames;

bool backgroundBox(const ASS_Image& im)
{
    // This is the optional background's exact RGBA color. In libass 0.17.x
    // BorderStyle 4 does not initialize image.type, so identify solid boxes by
    // their color and coverage, never by that field or by text geometry.
    if (im.color != 0x00000066 || im.w < 4 || im.h < 4 || !im.bitmap)
        return false;
    for (int y = 0; y < im.h; ++y)
        for (int x = 0; x < im.w; ++x)
            if (im.bitmap[y * im.stride + x] != 255) return false;
    return true;
}

void roundMask(const ASS_Image& im, int frameHeight,
               std::vector<unsigned char>& mask)
{
    mask.assign(static_cast<size_t>(im.w) * im.h, 255);
    const float r = std::min({im.w * 0.5f, im.h * 0.18f,
                              frameHeight * (12.0f / 720.0f)});
    const int edge = static_cast<int>(std::ceil(r));
    // Only the four corners need work; the centre stays fully covered. Pixel
    // centre distance gives a one-pixel antialiased edge at 720p and 1080p.
    for (int y = 0; y < edge; ++y)
        for (int x = 0; x < edge; ++x)
        {
            float dx = r - (x + 0.5f), dy = r - (y + 0.5f);
            float coverage = std::clamp(r + 0.5f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
            auto a = static_cast<unsigned char>(std::lround(coverage * 255));
            mask[y * im.w + x] = mask[y * im.w + im.w - 1 - x] = a;
            mask[(im.h - 1 - y) * im.w + x] =
                mask[(im.h - 1 - y) * im.w + im.w - 1 - x] = a;
        }
}
}

extern "C" {
ASS_Image* __real_ass_render_frame(ASS_Renderer*, ASS_Track*, long long, int*);
void __real_ass_renderer_done(ASS_Renderer*);
void __real_ass_set_frame_size(ASS_Renderer*, int, int);

ASS_Image* __wrap_ass_render_frame(ASS_Renderer* renderer, ASS_Track* track,
                                  long long now, int* changed)
{
    int change = 0;
    ASS_Image* original = __real_ass_render_frame(renderer, track, now, &change);
    if (changed) *changed = change;
    std::lock_guard<std::mutex> lock(framesMutex);
    auto& frame = frames[renderer];
    bool hasBox = false;
    size_t count = 0;
    for (auto* im = original; im; im = im->next)
    {
        ++count;
        hasBox = hasBox || backgroundBox(*im);
    }
    if (!hasBox)
    {
        frame.images.clear();
        frame.masks.clear();
        return original;
    }

    frame.images.resize(count);
    frame.masks.resize(count);
    size_t i = 0;
    for (auto* im = original; im; im = im->next, ++i)
    {
        auto& copy = frame.images[i];
        std::memcpy(&copy, im, sizeof(copy));
        copy.next = i + 1 < count ? &frame.images[i + 1] : nullptr;
        if (backgroundBox(*im))
        {
            auto& mask = frame.masks[i];
            if (mask.w != im->w || mask.h != im->h || mask.frameHeight != frame.height)
            {
                roundMask(*im, frame.height, mask.pixels);
                mask.w = im->w;
                mask.h = im->h;
                mask.frameHeight = frame.height;
            }
            copy.bitmap = mask.pixels.data();
            copy.stride = copy.w;
            copy.type = ASS_Image::IMAGE_TYPE_SHADOW;
        }
    }
    // Refresh all glyph pointers even on unchanged frames: libass only promises
    // that its image list is valid until the next call to ass_render_frame.
    return frame.images.data();
}

void __wrap_ass_set_frame_size(ASS_Renderer* renderer, int w, int h)
{
    __real_ass_set_frame_size(renderer, w, h);
    std::lock_guard<std::mutex> lock(framesMutex);
    frames[renderer].height = h;
}

void __wrap_ass_renderer_done(ASS_Renderer* renderer)
{
    {
        std::lock_guard<std::mutex> lock(framesMutex);
        frames.erase(renderer);
    }
    __real_ass_renderer_done(renderer);
}
}

#endif

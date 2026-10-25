#pragma once
#include <array>
#include <utility>

// mpv 0.39 text styles, shared by startup and the live background switch.
namespace subtitle_style
{
using Setting = std::pair<const char*, const char*>;
inline std::array<Setting, 4> background(bool enabled)
{
    // BorderStyle 4 puts one translucent box around all lines. In this mode
    // shadow-offset controls padding, rather than casting a text shadow. The
    // final box mask gets antialiased corners in rounded_subtitles.cpp.
    return {{
        {"sub-border-style", enabled ? "background-box" : "outline-and-shadow"},
        {"sub-back-color", enabled ? "#99000000" : "#80000000"},
        {"sub-border-size", enabled ? "0" : "1"},
        {"sub-shadow-offset", enabled ? "6" : "0.5"},
    }};
}
}

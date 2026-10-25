#pragma once
#include <array>
#include <charconv>
#include <string>

namespace tsvitch {
// Only published stable x.y.z releases are eligible for the stable update channel.
inline bool isNewerStableRelease(std::string tag, const std::array<int, 3>& current) {
    if (!tag.empty() && tag.front() == 'v') tag.erase(0, 1);
    std::array<int, 3> version{};
    size_t start = 0;
    for (size_t i = 0; i < version.size(); ++i) {
        const auto end = tag.find('.', start);
        if ((i < 2 && end == std::string::npos) || (i == 2 && end != std::string::npos)) return false;
        const auto stop = end == std::string::npos ? tag.size() : end;
        if (stop == start || (stop - start > 1 && tag[start] == '0')) return false;
        for (size_t j = start; j < stop; ++j) if (tag[j] < '0' || tag[j] > '9') return false;
        const auto parsed = std::from_chars(tag.data() + start, tag.data() + stop, version[i]);
        if (parsed.ec != std::errc{} || parsed.ptr != tag.data() + stop) return false;
        start = stop + 1;
    }
    return version > current;
}
}

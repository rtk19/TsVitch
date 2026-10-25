#pragma once
#include "utils/programme.hpp"
#include "utils/search_text.hpp"
#include "utils/xmltv_time.hpp"
#include <ctime>
#include <set>
#include <nlohmann/json.hpp>
#include <algorithm>
namespace tsvitch {
inline std::string epgField(const nlohmann::json &data, const char *key) {
    if (!data.is_object() || !data.contains(key))
        return {};
    const auto &value = data[key];
    return value.is_string()           ? value.get<std::string>()
           : value.is_number_integer() ? value.dump()
                                       : std::string{};
}
// Providers may return either base64 or plain UTF-8. Never turn plain text into binary garbage.
inline std::string epgText(const std::string &text) {
    static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (text.empty() || text.size() % 4)
        return text;
    uint32_t buffer = 0;
    int bits = 0;
    std::string decoded;
    bool padding = false;
    size_t pads = 0;
    for (unsigned char c : text) {
        if (c == '=') {
            padding = true;
            if (++pads > 2)
                return text;
            continue;
        }
        auto index = alphabet.find(c);
        if (padding || index == std::string::npos)
            return text;
        buffer = (buffer << 6) | uint32_t(index);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded += char((buffer >> bits) & 255);
        }
    }
    if ((buffer & ((1u << bits) - 1)) != 0 || decoded.empty())
        return text;
    for (const auto &unit : utf8Characters(decoded)) {
        const auto c = utf8Codepoint(unit);
        if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c == 127 || utf8Encode(c) != unit ||
            c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return text;
    }
    return decoded;
}
inline int64_t epgTimestamp(const nlohmann::json &data, const char *key) {
    try {
        auto value = epgField(data, key);
        size_t end = 0;
        auto result = std::stoll(value, &end);
        return end == value.size() && result > 0 ? (result > 100000000000LL ? result / 1000 : result) : 0;
    } catch (...) {
        return 0;
    }
}
inline int currentProgrammeIndex(const std::vector<Programme> &programmes, int64_t now) {
    int result = -1;
    for (size_t i = 0; i < programmes.size(); ++i)
        if (programmes[i].nowPlaying && (result < 0 || programmes[i].start > programmes[result].start))
            result = i;
    if (result >= 0)
        return result;
    for (size_t i = 0; i < programmes.size(); ++i)
        if (programmes[i].start <= now && now < programmes[i].end &&
            (result < 0 || programmes[i].start > programmes[result].start))
            result = i;
    return result;
}
inline std::string programmeTime(const Programme &p) {
    if (p.start <= 0 || p.end <= p.start)
        return p.time;
    const std::time_t start = p.start, end = p.end;
    std::tm first{}, last{};
    localtime_r(&start, &first);
    localtime_r(&end, &last);
    char a[32], b[32];
    std::strftime(a, sizeof(a), "%d/%m  %H:%M", &first);
    std::strftime(b, sizeof(b), "%H:%M", &last);
    return std::string(a) + " – " + b;
}
inline int64_t epgDateTimestamp(std::string text) {
    if (text.size() < 19)
        return 0;
    std::string digits;
    for (size_t i = 0; i < 19; ++i)
        if (std::isdigit(static_cast<unsigned char>(text[i])))
            digits += text[i];
    return digits.size() == 14 ? xmltvTimestamp(digits) : 0;
}
inline std::vector<Programme> parseShortEpg(const nlohmann::json &data) {
    std::vector<Programme> result;
    if (!data.is_object() || !data.contains("epg_listings") || !data["epg_listings"].is_array())
        return result;
    std::set<std::string> seen;
    for (const auto &item : data["epg_listings"]) {
        Programme p;
        p.title = epgText(epgField(item, "title"));
        p.description = epgText(epgField(item, "description"));
        p.start = epgTimestamp(item, "start_timestamp");
        p.end = epgTimestamp(item, "stop_timestamp");
        if (!p.end)
            p.end = epgTimestamp(item, "end_timestamp");
        if (!p.start)
            p.start = epgDateTimestamp(epgField(item, "start"));
        if (!p.end)
            p.end = epgDateTimestamp(epgField(item, "end"));
        for (const auto *key : {"now_playing", "is_now"})
            if (item.contains(key)) {
                const auto &v = item[key];
                p.nowPlaying = p.nowPlaying ||
                               (v.is_boolean() ? v.get<bool>()
                                               : epgField(item, key) == "1" || epgField(item, key) == "true");
            }
        p.time = programmeTime(p);
        const auto key = std::to_string(p.start) + ":" + std::to_string(p.end) + ":" + p.title;
        if (!seen.insert(key).second)
            continue;
        if (!p.title.empty())
            result.push_back(std::move(p));
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const auto &a, const auto &b) { return a.start < b.start; });
    return result;
}
} // namespace tsvitch

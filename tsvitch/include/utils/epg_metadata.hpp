#pragma once
#include "utils/programme.hpp"
#include "utils/search_text.hpp"
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
        return end == value.size() && result > 0 ? result : 0;
    } catch (...) {
        return 0;
    }
}
inline std::vector<Programme> parseShortEpg(const nlohmann::json &data) {
    std::vector<Programme> result;
    if (!data.is_object() || !data.contains("epg_listings") || !data["epg_listings"].is_array())
        return result;
    for (const auto &item : data["epg_listings"]) {
        Programme p;
        p.title = epgText(epgField(item, "title"));
        p.description = epgText(epgField(item, "description"));
        p.start = epgTimestamp(item, "start_timestamp");
        p.end = epgTimestamp(item, "stop_timestamp");
        if (!p.end)
            p.end = epgTimestamp(item, "end_timestamp");
        p.time = epgField(item, "start") + " - " + epgField(item, "end");
        if (!p.title.empty())
            result.push_back(std::move(p));
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const auto &a, const auto &b) { return a.start < b.start; });
    return result;
}
} // namespace tsvitch

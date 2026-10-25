#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace tsvitch {
inline std::vector<std::string> utf8Characters(const std::string &text) {
    std::vector<std::string> result;
    for (size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        size_t size = c < 0x80                 ? 1
                      : c >= 0xc2 && c <= 0xdf ? 2
                      : c >= 0xe0 && c <= 0xef ? 3
                      : c >= 0xf0 && c <= 0xf4 ? 4
                                               : 1;
        if (i + size > text.size())
            size = 1;
        uint32_t value = c & (size == 1 ? 0xff : size == 2 ? 0x1f : size == 3 ? 0x0f : 0x07);
        for (size_t j = 1; j < size; ++j) {
            if ((static_cast<unsigned char>(text[i + j]) & 0xc0) != 0x80) {
                size = 1;
                break;
            }
            value = (value << 6) | (static_cast<unsigned char>(text[i + j]) & 63);
        }
        if ((size == 2 && value < 0x80) || (size == 3 && value < 0x800) || (size == 4 && value < 0x10000) ||
            value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            size = 1;
        result.push_back(text.substr(i, size));
        i += size;
    }
    return result;
}
inline uint32_t utf8Codepoint(const std::string &c) {
    if (c.empty())
        return 0;
    uint32_t value = static_cast<unsigned char>(c[0]) & (c.size() == 1   ? 0xff
                                                         : c.size() == 2 ? 0x1f
                                                         : c.size() == 3 ? 0x0f
                                                                         : 0x07);
    for (size_t i = 1; i < c.size(); ++i)
        value = (value << 6) | (static_cast<unsigned char>(c[i]) & 63);
    return value;
}
inline std::string utf8Encode(uint32_t c) {
    std::string s;
    if (c < 0x80)
        s += char(c);
    else if (c < 0x800) {
        s += char(0xc0 | (c >> 6));
        s += char(0x80 | (c & 63));
    } else if (c < 0x10000) {
        s += char(0xe0 | (c >> 12));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    } else {
        s += char(0xf0 | (c >> 18));
        s += char(0x80 | ((c >> 12) & 63));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    }
    return s;
}
inline bool textMark(uint32_t c) {
    return (c >= 0x591 && c <= 0x5bd) || c == 0x5bf || (c >= 0x5c1 && c <= 0x5c2) ||
           (c >= 0x5c4 && c <= 0x5c5) || c == 0x5c7 || (c >= 0x300 && c <= 0x36f) ||
           (c >= 0x64b && c <= 0x65f);
}
inline std::string normalizeSearchText(const std::string &text) {
    std::string out;
    for (const auto &unit : utf8Characters(text)) {
        auto c = utf8Codepoint(unit);
        if (textMark(c) || c == 0x200e || c == 0x200f)
            continue;
        if (c >= 'A' && c <= 'Z')
            c += 32;
        if ((c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xde) || (c >= 0x410 && c <= 0x42f))
            c += 32;
        if (c >= 0x391 && c <= 0x3ab && c != 0x3a2)
            c += 32;
        if (c == 0x5da)
            c = 0x5db;
        else if (c == 0x5dd)
            c = 0x5de;
        else if (c == 0x5df)
            c = 0x5e0;
        else if (c == 0x5e3)
            c = 0x5e4;
        else if (c == 0x5e5)
            c = 0x5e6;
        out += utf8Encode(c);
    }
    return out;
}
inline bool searchMatches(const std::string &text, const std::string &query) {
    return normalizeSearchText(text).find(normalizeSearchText(query)) != std::string::npos;
}
class SearchText {
    std::vector<std::string> units;
    size_t cursor = 0, maximum;

  public:
    explicit SearchText(std::string value = {}, size_t maximum = 128) : maximum(maximum) { set(value); }
    void set(const std::string &value) {
        units = utf8Characters(value);
        if (units.size() > maximum)
            units.resize(maximum);
        cursor = units.size();
    }
    void insert(const std::string &value) {
        auto incoming = utf8Characters(value);
        if (incoming.size() > maximum - units.size())
            incoming.resize(maximum - units.size());
        units.insert(units.begin() + cursor, incoming.begin(), incoming.end());
        cursor += incoming.size();
    }
    void backspace() {
        if (!cursor)
            return;
        do {
            const auto mark = textMark(utf8Codepoint(units[cursor - 1]));
            units.erase(units.begin() + --cursor);
            if (!mark)
                break;
        } while (cursor);
    }
    void move(int step) {
        if (step < 0 && cursor)
            --cursor;
        else if (step > 0 && cursor < units.size())
            ++cursor;
    }
    void clear() {
        units.clear();
        cursor = 0;
    }
    size_t position() const { return cursor; }
    std::string value() const {
        std::string out;
        for (const auto &c : units)
            out += c;
        return out;
    }
};
} // namespace tsvitch

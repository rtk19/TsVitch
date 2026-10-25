#pragma once
#include <string>
#include <cstdint>

namespace tsvitch {
inline int64_t xmltvTimestamp(const std::string& value) {
    if (value.size() != 14 && value.size() != 20) return 0;
    if (value.substr(0,14).find_first_not_of("0123456789") != std::string::npos) return 0;
    int year = std::stoi(value.substr(0,4));
    const int month = std::stoi(value.substr(4,2)), day = std::stoi(value.substr(6,2));
    const int hour = std::stoi(value.substr(8,2)), minute = std::stoi(value.substr(10,2)), second = std::stoi(value.substr(12,2));
    if (year < 1 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59) return 0;
    static constexpr int monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (day < 1 || day > monthDays[month - 1] + (month == 2 && leap ? 1 : 0)) return 0;
    // Gregorian civil date to days since 1970-01-01; no platform timegm or local timezone dependence.
    year -= month <= 2;
    const int era = year / 400;
    const int yearOfEra = year - era * 400;
    const int dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    const int64_t days = static_cast<int64_t>(era) * 146097 + dayOfEra - 719468;
    const int64_t utc = days * 86400 + hour * 3600 + minute * 60 + second;
    int offset = 0;
    if (value.size() == 20) {
        if (value[14] != ' ' || (value[15] != '+' && value[15] != '-') ||
            value.substr(16).find_first_not_of("0123456789") != std::string::npos) return 0;
        const int hours = std::stoi(value.substr(16,2)), minutes = std::stoi(value.substr(18,2));
        if (hours > 23 || minutes > 59) return 0;
        offset = (hours * 60 + minutes) * 60;
        if (value[15] == '-') offset = -offset;
    }
    return static_cast<int64_t>(utc) - offset;
}
}

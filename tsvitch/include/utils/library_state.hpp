#pragma once
#include "utils/config_helper.hpp"
#include "utils/library_preferences.hpp"
namespace tsvitch {
inline std::string librarySourceKey() {
    auto &c = ProgramConfig::instance();
    if (c.getIntOption(SettingItem::IPTV_MODE) == 0)
        return "m3u:" + c.getM3U8Url();
    auto server = c.getXtreamServerUrl();
    while (!server.empty() && server.back() == '/')
        server.pop_back();
    return "xtream:" + server + "#" + c.getXtreamUsername();
}
inline std::string libraryCacheKey() {
    auto &c = ProgramConfig::instance();
    return librarySourceKey() +
           (c.getIntOption(SettingItem::IPTV_MODE) == 1 ? "#" + c.getXtreamPassword() : "");
}
inline LibraryPreferences &libraryPreferences() {
    static LibraryPreferences store(ProgramConfig::instance().getConfigDir() + "/library_preferences.json");
    return store;
}
} // namespace tsvitch

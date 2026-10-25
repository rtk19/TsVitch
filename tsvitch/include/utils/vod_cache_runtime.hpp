#pragma once
#include "utils/config_helper.hpp"
#include "utils/vod_cache.hpp"
namespace tsvitch {
inline VodCache &vodCacheStore() {
    static auto *cache = new VodCache(ProgramConfig::instance().getConfigDir() + "/cache/vod");
    return *cache;
}
} // namespace tsvitch

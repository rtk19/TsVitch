#pragma once
#include "utils/watch_store.hpp"
#include "utils/config_helper.hpp"
namespace tsvitch {
inline WatchStore& watchStore() {
    static WatchStore store(ProgramConfig::instance().getConfigDir()+"/watch_history.json",ProgramConfig::instance().getConfigDir()+"/playback_positions.json");
    return store;
}
}

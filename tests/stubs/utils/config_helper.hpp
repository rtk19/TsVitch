#pragma once
#include <string>
namespace brls {
struct Logger {
    template<class... T> static void info(T&&...) {}
    template<class... T> static void debug(T&&...) {}
    template<class... T> static void error(T&&...) {}
};
}
struct ProgramConfig {
    static ProgramConfig& instance() { static ProgramConfig config; return config; }
    std::string getConfigDir() { return "."; }
};

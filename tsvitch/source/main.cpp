#include <borealis.hpp>
#include <filesystem>

#ifdef __SWITCH__
#include <switch.h>
#include <sys/socket.h>
#endif

#include "tsvitch.h"
#include "utils/app_updater.hpp"
#include "config/server_config.h"

#include "utils/config_helper.hpp"
#include "utils/activity_helper.hpp"
#include "view/mpv_core.hpp"

#include "core/HistoryManager.hpp"
#include "core/FavoriteManager.hpp"
#include "core/DownloadProgressManager.hpp"
#ifndef __SWITCH__
#include "view/auto_tab_frame.hpp"
#include "view/vod_grid.hpp"
#include "../../tests/ui_smoke_steps.hpp"
#endif

#ifdef IOS
#include <SDL2/SDL_main.h>
#endif

int main(int argc, char* argv[]) {
#ifndef __SWITCH__
    bool uiSmokeTest=false,uiSmokeCacheOnly=false;
#endif
    if (argc > 0 && argv[0]) tsvitch::setApplicationPath(argv[0]);
    for (int i = 1; i < argc; i++) {
#ifndef __SWITCH__
        if (std::strcmp(argv[i], "--ui-smoke-test") == 0) uiSmokeTest=true;
        if (std::strcmp(argv[i], "--ui-smoke-cache") == 0) {uiSmokeTest=true;uiSmokeCacheOnly=true;}
#endif
        if (std::strcmp(argv[i], "-d") == 0) {
            brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        } else if (std::strcmp(argv[i], "-v") == 0) {
            brls::Logger::setLogLevel(brls::LogLevel::LOG_VERBOSE);
        } else if (std::strcmp(argv[i], "-dv") == 0) {
            brls::Application::enableDebuggingView(true);
        } else if (std::strcmp(argv[i], "-t") == 0) {
            MPVCore::TERMINAL = true;
        } else if (std::strcmp(argv[i], "-o") == 0) {
            const char* path = (i + 1 < argc) ? argv[++i] : APP_NAME ".log";
            brls::Logger::setLogOutput(std::fopen(path, "w+"));
        }
    }

#if __SWITCH__
    if (brls::Logger::getLogLevel() >= brls::LogLevel::LOG_DEBUG) {
        socketInitializeDefault();
        nxlinkStdio();
    }
#endif

    ProgramConfig::instance().init();

#ifdef __SWITCH__
    bool canUseLed = false;
    if (R_SUCCEEDED(hidsysInitialize())) {
        canUseLed = true;
    }
#endif

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init application");
        return EXIT_FAILURE;
    }

    brls::Application::getPlatform()->exitToHomeMode(true);
    brls::Application::createWindow(APP_NAME);
    brls::Logger::info("createWindow done");

    // Initialize global download progress manager
    tsvitch::DownloadProgressManager::getInstance()->initialize();
    brls::Logger::info("DownloadProgressManager initialized");

    Register::initCustomView();
    Register::initCustomTheme();
    Register::initCustomStyle();

    brls::Application::getPlatform()->disableScreenDimming(false);

    bool isAppMode = brls::Application::getPlatform()->isApplicationMode();
    brls::Logger::info("Application mode check: isApplicationMode = {}", isAppMode);

    if (isAppMode) {
        brls::Logger::info("Opening MainActivity (main interface)");
        Intent::openMain();
    } else {
        brls::Logger::info("Opening HintActivity (hint interface)");
        Intent::openHint();
    }

    if (std::string(SERVER_URL_VALUE).size() > 0) {
    //check if user_id is set, if not register a new user
    if (ProgramConfig::instance().getDeviceID().empty()) {
        brls::Logger::info("No user ID found, registering a new user...");

        CLIENT::register_user(
            [](const std::string& user_id, int status) {
                if (status == 200) {
                    brls::Logger::info("Registered new user ID: {}", user_id);
                } else {
                    brls::Logger::error("Failed to register user ID: {}", status);
                }
            },
            [](const std::string& error, int status) {
                brls::Logger::error("Error registering user ID: {} (status: {})", error, status);
            });
    } else {
        brls::Logger::info("User ID already exists: {}", ProgramConfig::instance().getDeviceID());

        CLIENT::check_user_id(
            [](const std::string& exists, int status) {
                if (status == 200) {
                    brls::Logger::info("User ID exists: {}", exists);
                } else {
                    brls::Logger::error("Failed to check user ID: {}", status);
                }
            },
            [](const std::string& error, int status) {
                brls::Logger::error("Error checking user ID: {} (status: {})", error, status);
            });
    }

    }

    GA("open_app", {{"version", APPVersion::instance().getVersionStr()},
                    {"language", brls::Application::getLocale()},
                    {"window", fmt::format("{}x{}", brls::Application::windowWidth, brls::Application::windowHeight)}})

#ifndef __SWITCH__
    if (!uiSmokeTest)
#endif
        APPVersion::instance().checkUpdate();

#ifndef __SWITCH__
    auto smokeNext=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    int smokeStep=0;
#endif
    while (brls::Application::mainLoop()) {
#ifndef __SWITCH__
        // Exercise the production screen lifecycle under Xvfb, without a Switch or subscription.
        if(uiSmokeTest && std::chrono::steady_clock::now()>=smokeNext) {
            if(uiSmokeStep(smokeStep,uiSmokeCacheOnly)){++smokeStep;smokeNext=std::chrono::steady_clock::now()+std::chrono::milliseconds(600);}
        }
#endif
    }

    brls::Logger::info("mainLoop done");

    // Cleanup download progress manager
    tsvitch::DownloadProgressManager::getInstance()->cleanup();

    tsvitch::shutdownUpdater();
    ProgramConfig::instance().exit(argv);

    HistoryManager::get()->save();
    FavoriteManager::get()->save();

#ifdef __SWITCH__
    if (canUseLed) hidsysExit();
    if (brls::Logger::getLogLevel() >= brls::LogLevel::LOG_DEBUG) {
        socketExit();
        nxlinkStdio();
    }
#endif

    tsvitch::applyPendingUpdate();
    return EXIT_SUCCESS;
}

#ifdef __WINRT__
#include <borealis/core/main.hpp>
#endif

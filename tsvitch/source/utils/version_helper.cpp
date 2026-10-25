#include "utils/app_updater.hpp"
#include <cstdlib>
#include "utils/release_version.hpp"
#include <fmt/format.h>
#include <cpr/cpr.h>
#include <pystring.h>
#include <borealis/core/i18n.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/platforms/desktop/steam_deck.hpp>

#include "utils/config_helper.hpp"
#include "utils/dialog_helper.hpp"
#include "api/tsvitch/util/http.hpp"
#include "fragment/latest_update.hpp"

using namespace brls::literals;

#define STR_IMPL(x) #x
#define STR(x) STR_IMPL(x)

APPVersion::APPVersion() {
    git_commit = std::string(STR(BUILD_TAG_SHORT));
    git_tag    = std::string(STR(BUILD_TAG_VERSION));
    major      = atoi(STR(BUILD_VERSION_MAJOR));
    minor      = atoi(STR(BUILD_VERSION_MINOR));
    revision   = atoi(STR(BUILD_VERSION_REVISION));
}

std::string APPVersion::getVersionStr() { return fmt::format("{}.{}.{}", major, minor, revision); }

std::string APPVersion::getPlatform() {
#ifdef IOS
    return "iOS";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(PS4)
    return "PS4";
#elif defined(__linux__)
    if (brls::isSteamDeck()) return "SteamDeck";
    return "Linux";
#elif defined(__WINRT__)
    return "UWP";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__SWITCH__)
#ifdef BOREALIS_USE_DEKO3D
    return "NX-deko3d";
#else
    return "NX";
#endif
#elif defined(__PSV__)
    return "PSVita";
#else
    return "Unknown";
#endif
}

std::string APPVersion::getPackageName() { return std::string{STR(BUILD_PACKAGE_NAME)}; }

bool APPVersion::needUpdate(std::string latestVersion) {
    return tsvitch::isNewerStableRelease(latestVersion, {major, minor, revision});
}

void APPVersion::checkUpdate(int delay, bool showUpToDateDialog) {
    if (delay <= 0) tsvitch::checkForUpdates(showUpToDateDialog);
    else brls::Threading::delay(delay, [showUpToDateDialog] { tsvitch::checkForUpdates(showUpToDateDialog); });
}

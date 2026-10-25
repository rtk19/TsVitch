#include <borealis/core/activity.hpp>
#include <borealis/core/i18n.hpp>
#include "utils/iptv_browser.hpp"
#include "utils/playlist.hpp"
#include "utils/programme.hpp"
#include "utils/epg_metadata.hpp"
#include "tsvitch.h"
#include "core/HistoryManager.hpp"
#include <borealis/core/ime.hpp>
#include "utils/xmltv.hpp"
#include "utils/app_updater.hpp"
#include "utils/config_helper.hpp"
#include "utils/url_encode.hpp"
#include "utils/activity_helper.hpp"
#include "view/grid_dropdown.hpp"
#include "tsvitch/util/http.hpp"
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <algorithm>
#include <ctime>
#include <functional>

namespace tsvitch {
namespace {
using Json = nlohmann::json;
using Done = std::function<void(const Json &)>;
std::string field(const Json &j, const std::string &key) {
    if (!j.is_object() || !j.contains(key) || j[key].is_null())
        return "";
    return j[key].is_string() ? j[key].get<std::string>() : j[key].is_number_integer() ? j[key].dump() : "";
}
std::string baseUrl() {
    auto url = ProgramConfig::instance().getXtreamServerUrl();
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    return url;
}
void request(const std::string &action, const std::string &argument, Done done,
             std::function<void()> failed = {}) {
    auto &config = ProgramConfig::instance();
    if (config.getIntOption(SettingItem::IPTV_MODE) != 1 || baseUrl().empty()) {
        brls::Application::notify(brls::getStr("iptv/s15"));
        return;
    }
    const auto url =
        baseUrl() + "/player_api.php?username=" + encodeUrlComponent(config.getXtreamUsername()) +
        "&password=" + encodeUrlComponent(config.getXtreamPassword()) + "&action=" + action + argument;
    brls::Application::notify(brls::getStr("iptv/s16"));
    cpr::GetCallback(
        [done, failed](cpr::Response response) {
            auto data = Json::parse(response.text, nullptr, false);
            const bool success = !response.error && response.status_code == 200 && !data.is_discarded();
            brls::sync([done, failed, data = std::move(data), success]() {
                if (!success) {
                    if (failed)
                        failed();
                    else
                        brls::Application::notify(brls::getStr("iptv/s17"));
                    return;
                }
                done(data);
            });
        },
        cpr::Url{url}, cpr::Timeout{45000}, HTTP::HEADERS, HTTP::PROXIES, HTTP::VERIFY);
}
void message(const std::string &text) {
    auto *dialog = new brls::Dialog(text.empty() ? brls::getStr("iptv/s18") : text);
    dialog->addButton(brls::getStr("iptv/s10"), [] {});
    dialog->open();
}
std::string details(const Json &info) {
    std::string result;
    for (const auto &key : {"name", "plot", "description", "genre", "releasedate", "releaseDate", "rating",
                            "director", "cast", "duration"}) {
        const auto value = field(info, key);
        if (!value.empty())
            result += std::string(key) + ": " + value + "\n\n";
    }
    return result;
}

} // namespace
void showProgrammeInfo(const LiveM3u8 &channel) {
    if (channel.contentType == "catchup") {
        auto live = channel;
        live.url = channel.liveUrl;
        live.title = channel.liveTitle;
        live.contentType = "live";
        showProgrammeInfo(live);
        return;
    }
    if (ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE) == 0 &&
        (channel.contentType == "movie" || channel.contentType == "series")) {
        message(channel.title + "\n\n" + channel.description);
        return;
    }
    if (channel.url.find("/series/") != std::string::npos) {
        message(channel.title + "\n\n" + channel.description);
        return;
    }
    if (channel.url.find("/movie/") != std::string::npos) {
        request("get_vod_info", "&vod_id=" + encodeUrlComponent(channel.id), [channel](const Json &data) {
            message(channel.title + "\n\n" + (data.contains("info") ? details(data["info"]) : ""));
        });
        return;
    }
    if (ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE) != 1 ||
        !ProgramConfig::instance().getSettingItem(SettingItem::XMLTV_URL, std::string{}).empty()) {
        showXmltvGuide(channel);
        return;
    }
    request("get_simple_data_table", "&stream_id=" + encodeUrlComponent(channel.id),
            [channel](const Json &data) {
                if (!data.contains("epg_listings") || !data["epg_listings"].is_array() ||
                    data["epg_listings"].empty()) {
                    presentProgrammeGuide(channel, {});
                    return;
                }
                auto programmes = parseShortEpg(data);
                presentProgrammeGuide(channel, programmes);
            });
}
} // namespace tsvitch

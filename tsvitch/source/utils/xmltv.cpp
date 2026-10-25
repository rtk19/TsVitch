#include <borealis/core/i18n.hpp>
#include "utils/xmltv.hpp"
#include "utils/programme.hpp"
#include "utils/epg_metadata.hpp"
#include "utils/url_encode.hpp"
#include "utils/xmltv_time.hpp"
#include "utils/config_helper.hpp"
#include "tsvitch/util/http.hpp"
#include "view/grid_dropdown.hpp"
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include <tinyxml2.h>
#include <zlib.h>
#include <ctime>
#include <cstring>
#include <borealis/core/ime.hpp>
#include <mutex>
#include <optional>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace tsvitch {
namespace {
bool loading = false;
std::optional<LiveM3u8> pendingDisplay;
std::time_t lastAttempt = 0;
std::mutex mutex;
std::map<std::string, std::vector<Programme>> guide;
std::map<std::string, std::string> channelNames;
std::string cachedUrl;
std::time_t cachedAt = 0;
std::string text(tinyxml2::XMLElement *node, const char *key) {
    auto *child = node->FirstChildElement(key);
    return child && child->GetText() ? child->GetText() : "";
}
std::string inflateXml(const std::string &source) {
    if (source.size() < 2 || static_cast<unsigned char>(source[0]) != 0x1f ||
        static_cast<unsigned char>(source[1]) != 0x8b)
        return source;
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(source.data()));
    stream.avail_in = source.size();
    if (inflateInit2(&stream, 15 + 32) != Z_OK)
        throw std::runtime_error("Cannot decompress XMLTV");
    std::string result;
    char buffer[32768];
    int status;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        status = inflate(&stream, Z_NO_FLUSH);
        result.append(buffer, sizeof(buffer) - stream.avail_out);
        if (result.size() > 96 * 1024 * 1024) {
            inflateEnd(&stream);
            throw std::runtime_error("XMLTV exceeds memory limit");
        }
    } while (status == Z_OK);
    inflateEnd(&stream);
    if (status != Z_STREAM_END)
        throw std::runtime_error("Invalid compressed XMLTV");
    return result;
}
std::string sourceUrl(const LiveM3u8 &channel) {
    auto url = ProgramConfig::instance().getSettingItem(SettingItem::XMLTV_URL, std::string{});
    return url.empty() ? channel.epgSource : url;
}
std::vector<Programme> programmesFor(const LiveM3u8 &channel) {
    std::lock_guard<std::mutex> lock(mutex);
    if (cachedUrl != sourceUrl(channel))
        return {};
    auto id = channel.epgId.empty() ? channel.id : channel.epgId;
    if (!guide.count(id) && channelNames.count(channel.title))
        id = channelNames.at(channel.title);
    auto found = guide.find(id);
    return found == guide.end() ? std::vector<Programme>{} : found->second;
}
std::string clockSource;
int64_t clockOffset = 0;
std::time_t clockAttempt = 0;
bool clockLoading = false;
std::string clockKey() {
    auto &c = ProgramConfig::instance();
    return c.getXtreamServerUrl() + "#" + c.getXtreamUsername();
}
void primeClock() {
    auto &c = ProgramConfig::instance();
    const auto key = clockKey();
    const auto now = std::time(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (clockLoading || (clockSource == key && now - clockAttempt < 1800))
            return;
        clockLoading = true;
        clockSource = key;
        clockAttempt = now;
        clockOffset = 0;
    }
    auto base = c.getXtreamServerUrl();
    while (!base.empty() && base.back() == '/')
        base.pop_back();
    cpr::GetCallback(
        [key, now](cpr::Response response) {
            int64_t offset = 0;
            try {
                if (!response.error && response.status_code == 200 && response.text.size() < 1024 * 1024) {
                    auto j = nlohmann::json::parse(response.text, nullptr, false);
                    if (j.contains("server_info")) {
                        auto stamp = epgTimestamp(j["server_info"], "timestamp_now");
                        if (stamp > 0)
                            offset = stamp - (now + std::time(nullptr)) / 2;
                    }
                }
            } catch (...) {
            }
            std::lock_guard<std::mutex> lock(mutex);
            clockLoading = false;
            if (clockSource == key)
                clockOffset = offset;
        },
        cpr::Url{base + "/player_api.php?username=" + encodeUrlComponent(c.getXtreamUsername()) +
                 "&password=" + encodeUrlComponent(c.getXtreamPassword())},
        cpr::Timeout{12000}, HTTP::HEADERS, HTTP::VERIFY, HTTP::PROXIES);
}
struct ShortGuide {
    std::vector<Programme> programmes;
    std::time_t attempted = 0, updated = 0, touched = 0;
    bool loading = false;
};
std::map<std::string, ShortGuide> shortGuides;
size_t activeShortGuides = 0;
bool useShortGuide() {
    return ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE) == 1 &&
           ProgramConfig::instance().getSettingItem(SettingItem::XMLTV_URL, std::string{}).empty();
}
std::string shortGuideKey(const LiveM3u8 &channel) {
    auto &config = ProgramConfig::instance();
    return config.getXtreamServerUrl() + "\n" + config.getXtreamUsername() + "\n" +
           config.getXtreamPassword() + "\n" + channel.id;
}
void primeShortGuide(const LiveM3u8 &channel) {
    if (channel.id.empty())
        return;
    const auto key = shortGuideKey(channel);
    const auto now = std::time(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto found = shortGuides.find(key);
        if (found != shortGuides.end()) {
            auto &entry = found->second;
            entry.touched = now;
            if (entry.loading || now - entry.updated < 120 || now - entry.attempted < 30)
                return;
        }
        if (activeShortGuides >= 4)
            return;
        if (shortGuides.size() >= 256 && found == shortGuides.end()) {
            auto oldest = shortGuides.end();
            for (auto it = shortGuides.begin(); it != shortGuides.end(); ++it)
                if (!it->second.loading &&
                    (oldest == shortGuides.end() || it->second.touched < oldest->second.touched))
                    oldest = it;
            if (oldest == shortGuides.end())
                return;
            shortGuides.erase(oldest);
        }
        auto &entry = shortGuides[key];
        entry.loading = true;
        entry.attempted = entry.touched = now;
        ++activeShortGuides;
    }
    auto &config = ProgramConfig::instance();
    auto base = config.getXtreamServerUrl();
    while (!base.empty() && base.back() == '/')
        base.pop_back();
    const auto url = base + "/player_api.php?username=" + encodeUrlComponent(config.getXtreamUsername()) +
                     "&password=" + encodeUrlComponent(config.getXtreamPassword()) +
                     "&action=get_short_epg&stream_id=" + encodeUrlComponent(channel.id) + "&limit=2";
    cpr::GetCallback(
        [key](cpr::Response response) {
            std::vector<Programme> programmes;
            bool success = false;
            try {
                if (!response.error && response.status_code == 200 && response.text.size() <= 1024 * 1024) {
                    auto data = nlohmann::json::parse(response.text, nullptr, false);
                    if (!data.is_discarded()) {
                        programmes = parseShortEpg(data);
                        success = true;
                    }
                }
            } catch (...) {
            }
            std::lock_guard<std::mutex> lock(mutex);
            --activeShortGuides;
            auto &entry = shortGuides[key];
            entry.loading = false;
            if (success) {
                entry.programmes = std::move(programmes);
                entry.updated = std::time(nullptr);
            }
        },
        cpr::Url{url}, cpr::Timeout{12000}, HTTP::HEADERS, HTTP::PROXIES, HTTP::VERIFY,
        cpr::ProgressCallback([](cpr::cpr_pf_arg_t total, cpr::cpr_pf_arg_t now, cpr::cpr_pf_arg_t,
                                 cpr::cpr_pf_arg_t,
                                 intptr_t) { return total <= 1024 * 1024 && now <= 1024 * 1024; }));
}

void display(const LiveM3u8 &channel) { presentProgrammeGuide(channel, programmesFor(channel)); }

} // namespace
void configureEpgSource() {
    brls::Application::getImeManager()->openForText(
        [](std::string url) {
            if (!url.empty() && url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) {
                brls::Application::notify(brls::getStr("iptv/s27"));
                return;
            }
            ProgramConfig::instance().setSettingItem(SettingItem::XMLTV_URL, url);
            std::lock_guard<std::mutex> lock(mutex);
            cachedUrl.clear();
            guide.clear();
            channelNames.clear();
            lastAttempt = 0;
            pendingDisplay.reset();
        },
        brls::getStr("iptv/s28"), brls::getStr("iptv/s29"), 2048,
        ProgramConfig::instance().getSettingItem(SettingItem::XMLTV_URL, std::string{}));
}
void loadGuide(const LiveM3u8 &channel, bool show) {
    const auto url = sourceUrl(channel);
    if (url.empty()) {
        if (show)
            presentProgrammeGuide(channel, {});
        return;
    }
    bool cached;
    {
        std::lock_guard<std::mutex> lock(mutex);
        cached = cachedUrl == url && std::time(nullptr) - cachedAt < 1800;
    }
    if (cached) {
        if (show)
            display(channel);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (loading) {
            if (show)
                pendingDisplay = channel;
            return;
        }
        if (!show && std::time(nullptr) - lastAttempt < 120)
            return;
        loading = true;
        lastAttempt = std::time(nullptr);
    }
    if (show)
        brls::Application::notify(brls::getStr("iptv/s31"));
    cpr::GetCallback(
        [url, channel, show](cpr::Response response) {
            try {
                if (response.error || response.status_code != 200 || response.text.size() > 96 * 1024 * 1024)
                    throw std::runtime_error("Cannot download XMLTV guide");
                const auto xml = inflateXml(response.text);
                tinyxml2::XMLDocument document;
                if (document.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS ||
                    !document.FirstChildElement("tv"))
                    throw std::runtime_error("Invalid XMLTV guide");
                auto *root = document.FirstChildElement("tv");
                std::map<std::string, std::vector<Programme>> parsed;
                std::map<std::string, std::string> names;
                for (auto *item = root->FirstChildElement("channel"); item;
                     item = item->NextSiblingElement("channel")) {
                    const char *id = item->Attribute("id");
                    if (id)
                        for (auto *name = item->FirstChildElement("display-name"); name;
                             name = name->NextSiblingElement("display-name"))
                            if (name->GetText())
                                names[name->GetText()] = id;
                }
                const auto now = std::time(nullptr);
                for (auto *item = root->FirstChildElement("programme"); item;
                     item = item->NextSiblingElement("programme")) {
                    const char *id = item->Attribute("channel");
                    if (!id)
                        continue;
                    Programme programme;
                    programme.start =
                        xmltvTimestamp(item->Attribute("start") ? item->Attribute("start") : "");
                    programme.end = xmltvTimestamp(item->Attribute("stop") ? item->Attribute("stop") : "");
                    if (!programme.start || programme.end <= programme.start ||
                        programme.end < now - 31 * 86400 || programme.start > now + 14 * 86400)
                        continue;
                    programme.title = text(item, "title");
                    programme.description = text(item, "desc");
                    for (const auto *key : {"sub-title", "category", "date"}) {
                        const auto value = text(item, key);
                        if (!value.empty())
                            programme.description += "\n" + value;
                    }
                    const std::time_t start = programme.start, end = programme.end;
                    std::tm begin{}, finish{};
                    localtime_r(&start, &begin);
                    localtime_r(&end, &finish);
                    char time[80];
                    std::strftime(time, sizeof(time), "%d/%m %H:%M", &begin);
                    programme.time = time;
                    std::strftime(time, sizeof(time), "%H:%M", &finish);
                    programme.time += std::string(" - ") + time;
                    parsed[id].push_back(std::move(programme));
                }
                for (auto &pair : parsed)
                    std::sort(pair.second.begin(), pair.second.end(),
                              [](const Programme &a, const Programme &b) { return a.start < b.start; });
                bool open = show;
                auto selected = channel;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    guide = std::move(parsed);
                    channelNames = std::move(names);
                    cachedUrl = url;
                    cachedAt = now;
                    loading = false;
                    if (pendingDisplay) {
                        selected = *pendingDisplay;
                        pendingDisplay.reset();
                        open = true;
                    }
                }
                if (open)
                    brls::sync([selected, url] {
                        if (sourceUrl(selected) == url)
                            display(selected);
                        else
                            loadGuide(selected, true);
                    });
            } catch (const std::exception &error) {
                auto failedChannel = channel;
                bool notify = show;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    loading = false;
                    notify = notify || pendingDisplay.has_value();
                    if (pendingDisplay)
                        failedChannel = *pendingDisplay;
                    pendingDisplay.reset();
                }
                if (notify) {
                    const std::string message = error.what();
                    brls::sync([message, failedChannel] {
                        brls::Application::notify(message);
                        presentProgrammeGuide(failedChannel, {});
                    });
                }
            }
        },
        cpr::Url{url}, cpr::Timeout{60000}, HTTP::PROXIES, HTTP::VERIFY);
}
void showXmltvGuide(const LiveM3u8 &channel) { loadGuide(channel, true); }
void primeProgrammeGuide(const LiveM3u8 &channel) {
    if (useShortGuide()) {
        primeClock();
        primeShortGuide(channel);
    } else
        loadGuide(channel, false);
}
int64_t programmeNow() {
    const auto now = std::time(nullptr);
    if (ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE) != 1)
        return now;
    std::lock_guard<std::mutex> lock(mutex);
    return now + (clockSource == clockKey() ? clockOffset : 0);
}
Programme currentProgrammeData(const LiveM3u8 &channel) {
    const auto now = programmeNow();
    if (useShortGuide()) {
        std::lock_guard<std::mutex> lock(mutex);
        auto found = shortGuides.find(shortGuideKey(channel));
        if (found != shortGuides.end()) {
            auto programmes = found->second.programmes;
            if (std::time(nullptr) - found->second.updated > 120)
                for (auto &p : programmes)
                    p.nowPlaying = false;
            const int index = currentProgrammeIndex(programmes, now);
            if (index >= 0)
                return programmes[index];
        }
        return {};
    }
    for (const auto &p : programmesFor(channel))
        if (p.start <= now && now < p.end)
            return p;
    return {};
}
std::string currentProgramme(const LiveM3u8 &channel) { return currentProgrammeData(channel).title; }
} // namespace tsvitch

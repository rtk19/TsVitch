#include <borealis/core/activity.hpp>
#include <borealis/core/i18n.hpp>
#include "utils/iptv_browser.hpp"
#include "utils/playlist.hpp"
#include "utils/programme.hpp"
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
using Done = std::function<void(const Json&)>;
std::string field(const Json& j, const std::string& key) {
    if (!j.is_object() || !j.contains(key) || j[key].is_null()) return "";
    return j[key].is_string() ? j[key].get<std::string>() : j[key].is_number_integer() ? j[key].dump() : "";
}
std::string baseUrl() {
    auto url = ProgramConfig::instance().getXtreamServerUrl();
    while (!url.empty() && url.back() == '/') url.pop_back();
    return url;
}
void request(const std::string& action, const std::string& argument, Done done, std::function<void()> failed = {}) {
    auto& config = ProgramConfig::instance();
    if (config.getIntOption(SettingItem::IPTV_MODE) != 1 || baseUrl().empty()) {
        brls::Application::notify(brls::getStr("iptv/s15"));
        return;
    }
    const auto url = baseUrl() + "/player_api.php?username=" + encodeUrlComponent(config.getXtreamUsername()) +
        "&password=" + encodeUrlComponent(config.getXtreamPassword()) + "&action=" + action + argument;
    brls::Application::notify(brls::getStr("iptv/s16"));
    cpr::GetCallback([done, failed](cpr::Response response) {
        auto data = Json::parse(response.text, nullptr, false);
        const bool success = !response.error && response.status_code == 200 && !data.is_discarded();
        brls::sync([done, failed, data = std::move(data), success]() {
            if (!success) { if (failed) failed(); else brls::Application::notify(brls::getStr("iptv/s17")); return; }
            done(data);
        });
    }, cpr::Url{url}, cpr::Timeout{45000}, HTTP::HEADERS, HTTP::PROXIES, HTTP::VERIFY);
}
std::string streamUrl(const std::string& kind, const std::string& id, std::string extension) {
    if (extension.empty()) extension = "mp4";
    if (extension.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos) extension = "mp4";
    auto& config = ProgramConfig::instance();
    return baseUrl() + "/" + kind + "/" + encodeUrlComponent(config.getXtreamUsername()) + "/" +
        encodeUrlComponent(config.getXtreamPassword()) + "/" + encodeUrlComponent(id) + "." + extension;
}
void message(const std::string& text) {
    auto* dialog = new brls::Dialog(text.empty() ? brls::getStr("iptv/s18") : text);
    dialog->addButton(brls::getStr("iptv/s10"), [] {});
    dialog->open();
}
std::string details(const Json& info) {
    std::string result;
    for (const auto& key : {"name", "plot", "description", "genre", "releasedate", "releaseDate", "rating", "director", "cast", "duration"}) {
        const auto value = field(info, key);
        if (!value.empty()) result += std::string(key) + ": " + value + "\n\n";
    }
    return result;
}
void play(const LiveM3u8& media, const std::string& summary) {
    auto* dialog = new brls::Dialog(media.title + "\n\n" + summary);
    dialog->addButton(brls::getStr("iptv/s8"), [media] { HistoryManager::get()->add(media); Intent::openLive({media}, 0, [] {}); });
    dialog->addButton(brls::getStr("iptv/s9"), [] {});
    dialog->open();
}
void episodes(const Json& series, const std::string& season) {
    const auto& seasons = series["episodes"];
    if (!seasons.contains(season) || !seasons[season].is_array()) { message(brls::getStr("iptv/s19")); return; }
    std::vector<LiveM3u8> items;
    std::vector<std::string> labels, summaries;
    for (const auto& entry : seasons[season]) {
        const auto id = field(entry, "id");
        if (id.empty()) continue;
        LiveM3u8 item;
        item.id = id; item.title = field(entry, "title");
        if (item.title.empty()) item.title = "Episode " + field(entry, "episode_num");
        item.groupTitle = brls::getStr("iptv/s6") + " " + season;
        item.url = streamUrl("series", id, field(entry, "container_extension"));
        item.description = entry.contains("info") ? details(entry["info"]) : "";
        items.push_back(item); labels.push_back(item.title);
        summaries.push_back(item.description);
    }
    if (items.empty()) { message(brls::getStr("iptv/s19")); return; }
    BaseDropdown::text(brls::getStr("iptv/s7"), labels, [items, summaries](int index) {
        if (index >= 0 && static_cast<size_t>(index) < items.size()) play(items[index], summaries[index]);
    }, -1);
}
void seriesInfo(const std::string& id) {
    request("get_series_info", "&series_id=" + encodeUrlComponent(id), [](const Json& data) {
        if (!data.contains("episodes") || !data["episodes"].is_object()) { message(brls::getStr("iptv/s20")); return; }
        std::vector<std::string> keys, labels;
        for (auto it = data["episodes"].begin(); it != data["episodes"].end(); ++it) keys.push_back(it.key());
        std::sort(keys.begin(), keys.end(), [](const std::string& a, const std::string& b) {
            return a.size() == b.size() ? a < b : a.size() < b.size();
        });
        for (const auto& key : keys) labels.push_back(brls::getStr("iptv/s6") + " " + key);
        if (labels.empty()) { message(brls::getStr("iptv/s20")); return; }
        BaseDropdown::text(brls::getStr("iptv/s6"), labels, [keys, data](int index) {
            if (index >= 0 && static_cast<size_t>(index) < keys.size()) episodes(data, keys[index]);
        }, -1);
    });
}
void displayCatalogue(bool series, const Json& data) {
        if (!data.is_array() || data.empty()) { message(brls::getStr("iptv/s21")); return; }
        std::vector<std::string> labels = {brls::getStr("hints/search")};
        std::vector<Json> items;
        for (const auto& entry : data) {
            if (!entry.is_object()) continue;
            labels.push_back(field(entry, "name")); items.push_back(entry);
        }
        BaseDropdown::text(series ? brls::getStr("iptv/s1") : brls::getStr("iptv/s0"), labels, [series, items, data](int index) {
            if (index == 0) {
                brls::Application::getImeManager()->openForText([series, data](std::string query) {
                    auto lower = [](std::string value) {
                        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
                        return value;
                    };
                    query = lower(query);
                    Json filtered = Json::array();
                    for (const auto& entry : data) if (lower(field(entry, "name")).find(query) != std::string::npos) filtered.push_back(entry);
                    displayCatalogue(series, filtered);
                }, brls::getStr("hints/search"), "", 128, "");
                return;
            }
            --index;
            if (index < 0 || static_cast<size_t>(index) >= items.size()) return;
            const auto item = items[index];
            if (series) { seriesInfo(field(item, "series_id")); return; }
            LiveM3u8 media;
            media.id = field(item, "stream_id"); media.title = field(item, "name"); media.logo = field(item, "stream_icon");
            media.url = streamUrl("movie", media.id, field(item, "container_extension"));
            request("get_vod_info", "&vod_id=" + encodeUrlComponent(media.id), [media](const Json& data) {
                play(media, data.contains("info") ? details(data["info"]) : "");
            }, [media] { play(media, ""); });
        }, -1);
}
void catalogue(bool series, const std::string& category) {
    request(series ? "get_series" : "get_vod_streams", "&category_id=" + encodeUrlComponent(category), [series](const Json& data) {
        displayCatalogue(series, data);
    });
}
void choosePlaylistItems(LiveM3u8ListResult items,const std::string& title) {
    if(items.empty()){message(brls::getStr("iptv/s21"));return;}
    std::vector<std::string> names={brls::getStr("hints/search")};
    for(const auto& item:items)names.push_back(item.title);
    BaseDropdown::text(title,names,[items,title](int index) {
        if(index==0) {
            brls::Application::getImeManager()->openForText([items,title](std::string query) {
                LiveM3u8ListResult filtered;query=asciiLower(query);
                for(const auto& item:items)if(asciiLower(item.title).find(query)!=std::string::npos)filtered.push_back(item);
                choosePlaylistItems(filtered,title);
            },brls::getStr("hints/search"),"",128,"");return;
        }
        --index;if(index>=0&&static_cast<size_t>(index)<items.size())play(items[index],items[index].description);
    },-1);
}
void browsePlaylist(LiveM3u8ListResult items,bool series,int level) {
    if(items.empty()){message(brls::getStr("iptv/s21"));return;}
    if(!series&&level>0){choosePlaylistItems(items,brls::getStr("iptv/s0"));return;}
    if(series&&level>2){
        std::stable_sort(items.begin(),items.end(),[](const LiveM3u8& a,const LiveM3u8& b){
            try{return std::stoi(a.episode)<std::stoi(b.episode);}catch(...){return a.title<b.title;}
        });choosePlaylistItems(items,brls::getStr("iptv/s7"));return;
    }
    std::map<std::string,LiveM3u8ListResult> groups;
    for(const auto& item:items) {
        auto key=level==0?item.groupTitle:level==1?(item.seriesTitle.empty()?item.title:item.seriesTitle):item.season;
        if(key.empty())key=brls::getStr("iptv/all_items");groups[key].push_back(item);
    }
    const bool search=series&&level==1;
    std::vector<std::string> names;if(search)names.push_back(brls::getStr("hints/search"));
    std::vector<LiveM3u8ListResult> values;
    for(const auto& group:groups){names.push_back(group.first);values.push_back(group.second);}
    BaseDropdown::text(level==0?brls::getStr("iptv/s5"):level==1?brls::getStr("iptv/s1"):brls::getStr("iptv/s6"),names,[values,series,level,items,search](int index){
        if(search){
            if(index==0){brls::Application::getImeManager()->openForText([items,series,level](std::string query){
                LiveM3u8ListResult filtered;query=asciiLower(query);
                for(const auto& item:items)if(asciiLower(item.title+" "+item.seriesTitle).find(query)!=std::string::npos)filtered.push_back(item);
                browsePlaylist(filtered,series,level);
            },brls::getStr("hints/search"),"",128,"");return;}
            --index;
        }
        if(index>=0&&static_cast<size_t>(index)<values.size())browsePlaylist(values[index],series,level+1);
    },-1);
}
void m3uCatalogue(bool series) {
    brls::Application::notify(brls::getStr("iptv/s16"));
    TsVitchClient::get_file_m3u8([series](LiveM3u8ListResult items){
        items.erase(std::remove_if(items.begin(),items.end(),[series](const LiveM3u8& item){return item.contentType!=(series?"series":"movie");}),items.end());
        browsePlaylist(items,series,0);
    },[](const std::string&,int){brls::Application::notify(brls::getStr("iptv/s17"));});
}
void categories(bool series) {
    if(ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE)==0){m3uCatalogue(series);return;}
    request(series ? "get_series_categories" : "get_vod_categories", "", [series](const Json& data) {
        if (!data.is_array() || data.empty()) { message(brls::getStr("iptv/s22")); return; }
        std::vector<std::string> names, ids;
        for (const auto& entry : data) {
            const auto id = field(entry, "category_id");
            if (id.empty()) continue;
            ids.push_back(id); names.push_back(field(entry, "category_name"));
        }
        BaseDropdown::text(brls::getStr("iptv/s5"), names, [series, ids](int index) {
            if (index >= 0 && static_cast<size_t>(index) < ids.size()) catalogue(series, ids[index]);
        }, -1);
    });
}
std::string decode64(const std::string& text) {
    static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned int buffer = 0; int bits = 0; std::string result;
    for (const unsigned char c : text) {
        if (c == '=') break;
        const auto index = alphabet.find(c);
        if (index == std::string::npos) return text;
        buffer = (buffer << 6) | static_cast<unsigned int>(index); bits += 6;
        if (bits >= 8) { bits -= 8; result += static_cast<char>((buffer >> bits) & 255); }
    }
    return result;
}
}
brls::View* createIptvLibrary() {
    auto* box = new brls::Box(brls::Axis::COLUMN);
    auto add = [box](const std::string& title, std::function<void()> action) {
        auto* cell = new brls::DetailCell();
        cell->setText(title);
        cell->registerClickAction([action](brls::View*) { action(); return true; });
        box->addView(cell);
    };
    add(brls::getStr("iptv/s0"), [] { categories(false); });
    add(brls::getStr("iptv/s1"), [] { categories(true); });
    add(brls::getStr("iptv/s3"), [] { configureEpgSource(); });
    add(brls::getStr("iptv/s2"), [] { installLatestUpdate(); });
    return box;
}
void openIptvBrowser() {
    BaseDropdown::text(brls::getStr("iptv/s4"), {brls::getStr("iptv/s0"), brls::getStr("iptv/s1"), brls::getStr("iptv/s2"), brls::getStr("iptv/s3")}, [](int index) {
        if (index == 3) { configureEpgSource(); return; }
        if (index == 2) { installLatestUpdate(); return; }
        if (index == 0 || index == 1) categories(index == 1);
    }, -1);
}
void showProgrammeInfo(const LiveM3u8& channel) {
    if(channel.contentType=="catchup") {
        auto live=channel;live.url=channel.liveUrl;live.title=channel.liveTitle;live.contentType="live";
        showProgrammeInfo(live);return;
    }
    if(ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE)==0 && (channel.contentType=="movie"||channel.contentType=="series")) {
        message(channel.title+"\n\n"+channel.description);return;
    }
    if (channel.url.find("/series/") != std::string::npos) {
        message(channel.title + "\n\n" + channel.description); return;
    }
    if (channel.url.find("/movie/") != std::string::npos) {
        request("get_vod_info", "&vod_id=" + encodeUrlComponent(channel.id), [channel](const Json& data) {
            message(channel.title + "\n\n" + (data.contains("info") ? details(data["info"]) : ""));
        });
        return;
    }
    if (ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE) != 1 ||
        !ProgramConfig::instance().getSettingItem(SettingItem::XMLTV_URL, std::string{}).empty()) {
        showXmltvGuide(channel); return;
    }
    request("get_simple_data_table", "&stream_id=" + encodeUrlComponent(channel.id), [channel](const Json& data) {
        if (!data.contains("epg_listings") || !data["epg_listings"].is_array() || data["epg_listings"].empty()) {
            presentProgrammeGuide(channel,{}); return;
        }
        std::vector<Programme> programmes;
        for(const auto& item:data["epg_listings"]) {
            Programme p;p.title=decode64(field(item,"title"));p.description=decode64(field(item,"description"));
            p.time=field(item,"start")+" - "+field(item,"end");
            try {p.start=std::stoll(field(item,"start_timestamp"));
                auto stop=field(item,"stop_timestamp");if(stop.empty())stop=field(item,"end_timestamp");p.end=std::stoll(stop);
            }catch(...){}
            programmes.push_back(p);
        }
        std::sort(programmes.begin(),programmes.end(),[](const Programme& a,const Programme& b){return a.start<b.start;});
        presentProgrammeGuide(channel,programmes);
    });
}
}

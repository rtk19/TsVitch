#include <borealis/core/activity.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include "utils/iptv_browser.hpp"
#include "utils/xmltv.hpp"
#include "utils/activity_helper.hpp"
#include "utils/playlist.hpp"
#include "utils/content_metadata.hpp"
#include "utils/search_input.hpp"
#include "utils/search_text.hpp"
#include "utils/vod_helpers.hpp"
#include "utils/vod_cache_runtime.hpp"
#include "utils/watch_progress.hpp"
#include "utils/library_state.hpp"
#include "utils/url_encode.hpp"
#include "view/vod_grid.hpp"
#include "tsvitch/util/http.hpp"
#include <unordered_map>
#include <unordered_set>

namespace tsvitch {
namespace {
using Json = nlohmann::json;
using Items = LiveM3u8ListResult;
using SharedItems = std::shared_ptr<const Items>;
using Indices = std::vector<size_t>;
using SharedIndices = std::shared_ptr<const Indices>;
using Tiles = std::vector<VodTile>;
using Done = std::function<void(SharedItems)>;
using Fail = std::function<void()>;
using Fetch = std::function<void(std::function<void(Items)>, Fail)>;
struct Source {
    int mode, timeout;
    std::string scope, cache, server, user, password, playlist;
    Source() {
        auto &c = ProgramConfig::instance();
        mode = c.getIntOption(SettingItem::IPTV_MODE);
        timeout = std::max(30000, c.getIntOption(SettingItem::M3U8_TIMEOUT));
        scope = librarySourceKey();
        cache = libraryCacheKey();
        server = c.getXtreamServerUrl();
        while (!server.empty() && server.back() == '/')
            server.pop_back();
        user = c.getXtreamUsername();
        password = c.getXtreamPassword();
        playlist = c.getM3U8Url();
    }
};
std::string tr(const std::string &name) { return brls::getStr("iptv/" + name); }
std::string text(const Json &j, const std::string &key) {
    if (!j.is_object() || !j.contains(key) || j[key].is_null())
        return "";
    return j[key].is_string() ? j[key].get<std::string>() : j[key].is_number() ? j[key].dump() : "";
}
void request(const Source &source, const std::string &action, const std::string &args,
             std::function<void(const Json &)> done, Fail fail) {
    const auto url = source.server + "/player_api.php?username=" + encodeUrlComponent(source.user) +
                     "&password=" + encodeUrlComponent(source.password) + "&action=" + action + args;
    cpr::GetCallback(
        [done, fail](cpr::Response r) {
            try {
                auto data = Json::parse(r.text, nullptr, false);
                if (r.error || r.status_code != 200 || data.is_discarded()) {
                    fail();
                    return;
                }
                done(data);
            } catch (...) {
                fail();
            }
        },
        cpr::Url{url}, cpr::Timeout{45000}, HTTP::HEADERS, HTTP::PROXIES, HTTP::VERIFY);
}
std::string stream(const Source &s, const std::string &kind, const std::string &id, std::string extension) {
    if (extension.empty() ||
        extension.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") !=
            std::string::npos)
        extension = "mp4";
    return s.server + "/" + kind + "/" + encodeUrlComponent(s.user) + "/" + encodeUrlComponent(s.password) +
           "/" + encodeUrlComponent(id) + "." + extension;
}
std::string description(const Json &j) {
    std::string result;
    for (const auto *key : {"plot", "description", "genre", "releasedate", "rating", "director", "cast"}) {
        auto value = text(j, key);
        if (!value.empty())
            result += value + "\n\n";
    }
    return result;
}
VodCache &cache() { return vodCacheStore(); }
struct Subscriber {
    Done done;
    Fail fail;
    bool delivered = false;
    bool acceptStale = true;
};
using Flight = std::vector<Subscriber>;
std::unordered_map<std::string, std::shared_ptr<Flight>> flights; // UI thread only.
void deliver(const std::shared_ptr<Flight> &listeners, SharedItems items, bool stale = false) {
    for (auto &s : *listeners) {
        if (stale && !s.acceptStale)
            continue;
        s.delivered = true;
        s.done(items);
    }
}
void fetchCached(const std::string &key, Fetch fetch, Done done, Fail fail, bool force = false,
                 bool acceptStale = true) {
    auto cached = cache().find(key);
    if (cached && cached->fresh(vodNow()) && !force) {
        done(cached->items);
        return;
    }
    auto existing = flights.find(key);
    if (existing != flights.end()) {
        existing->second->push_back({done, fail, false, acceptStale});
        if (cached && acceptStale) {
            existing->second->back().delivered = true;
            done(cached->items);
        }
        return;
    }
    auto listeners = std::make_shared<Flight>();
    listeners->push_back({done, fail, false, acceptStale});
    flights[key] = listeners;
    auto network = [key, fetch, listeners] {
        fetch(
            [key, listeners](Items items) {
                auto value = std::make_shared<VodSnapshot>();
                value->items = std::make_shared<const Items>(std::move(items));
                value->saved = vodNow();
                cache().remember(key, value);
                brls::sync([key, listeners, value] {
                    flights.erase(key);
                    deliver(listeners, value->items);
                });
                brls::async([key, value] { cache().persist(key, *value); });
            },
            [key, listeners] {
                brls::sync([key, listeners] {
                    flights.erase(key);
                    for (auto &s : *listeners)
                        if (!s.delivered)
                            s.fail();
                        else
                            brls::Application::notify(tr("cache_offline"));
                });
            });
    };
    if (cached) {
        deliver(listeners, cached->items, true);
        network();
        return;
    }
    brls::async([key, force, listeners, network] {
        auto value = cache().read(key);
        brls::sync([key, force, listeners, network, value] {
            if (value) {
                cache().remember(key, value);
                deliver(listeners, value->items, force || !value->fresh(vodNow()));
                if (value->fresh(vodNow()) && !force) {
                    flights.erase(key);
                    return;
                }
            }
            network();
        });
    });
}
void fetchPlaylist(const Source &source, Done done, Fail fail, bool force) {
    fetchCached(
        source.cache + "/playlist",
        [source](std::function<void(Items)> done, Fail fail) {
            cpr::GetCallback(
                [source, done, fail](cpr::Response r) {
                    try {
                        if (r.error || r.status_code != 200) {
                            fail();
                            return;
                        }
                        done(parsePlaylist(r.text, source.playlist));
                    } catch (...) {
                        fail();
                    }
                },
                cpr::Url{source.playlist}, cpr::Timeout{source.timeout}, HTTP::HEADERS, HTTP::PROXIES,
                HTTP::VERIFY);
        },
        done, fail, force, false);
}
void rawCatalogue(const Source &source, bool series, std::function<void(Items)> done, Fail fail, bool force) {
    if (source.mode == 0) {
        fetchPlaylist(
            source,
            [series, done](SharedItems all) {
                brls::async([series, done, all] {
                    Items filtered;
                    for (const auto &item : *all)
                        if (item.contentType == (series ? "series" : "movie"))
                            filtered.push_back(item);
                    done(std::move(filtered));
                });
            },
            fail, force);
        return;
    }
    struct Join {
        std::mutex mutex;
        Items items;
        std::map<std::string, std::string> names;
        bool streams = false, categories = false, failed = false, finished = false;
    };
    auto join = std::make_shared<Join>();
    auto finish = [join, done] {
        Items result;
        {
            std::lock_guard<std::mutex> guard(join->mutex);
            if (join->failed || join->finished || !join->streams || !join->categories)
                return;
            join->finished = true;
            result = std::move(join->items);
            for (auto &item : result) {
                auto it = join->names.find(item.groupTitle);
                if (it != join->names.end())
                    item.groupTitle = it->second;
            }
        }
        done(std::move(result));
    };
    request(
        source, series ? "get_series" : "get_vod_streams", "",
        [source, series, join, finish, fail](const Json &data) {
            if (!data.is_array()) {
                {
                    std::lock_guard<std::mutex> guard(join->mutex);
                    join->failed = true;
                }
                fail();
                return;
            }
            Items items;
            items.reserve(data.size());
            for (const auto &entry : data) {
                LiveM3u8 item;
                item.id = text(entry, series ? "series_id" : "stream_id");
                if (item.id.empty())
                    continue;
                item.title = text(entry, "name");
                item.providerId = source.server + "#" + source.user;
                item.contentType = series ? "series" : "movie";
                item.groupTitle = text(entry, "category_id");
                item.logo = contentArtwork(entry, source.server + "/");
                item.addedAt = numberField(entry, "added");
                if (!item.addedAt)
                    item.addedAt = numberField(entry, "last_modified");
                item.description = description(entry);
                if (series) {
                    item.seriesId = item.id;
                    item.seriesTitle = item.title;
                } else
                    item.url = stream(source, "movie", item.id, text(entry, "container_extension"));
                items.push_back(std::move(item));
            }
            {
                std::lock_guard<std::mutex> guard(join->mutex);
                join->items = std::move(items);
                join->streams = true;
            }
            finish();
        },
        [join, fail] {
            bool notify;
            {
                std::lock_guard<std::mutex> guard(join->mutex);
                notify = !join->failed;
                join->failed = true;
            }
            if (notify)
                fail();
        });
    auto categoriesReady = [join, finish](const Json &data) {
        {
            std::lock_guard<std::mutex> guard(join->mutex);
            if (data.is_array())
                for (const auto &c : data)
                    join->names[text(c, "category_id")] = text(c, "category_name");
            join->categories = true;
        }
        finish();
    };
    request(source, series ? "get_series_categories" : "get_vod_categories", "", categoriesReady,
            [categoriesReady] { categoriesReady(Json::array()); });
}
struct EpisodeCollection {
    SharedItems raw;
    std::vector<EpisodeGroup> groups;
    std::unordered_map<std::string, size_t> byMedia;
};
struct Library {
    Source source;
    SharedItems items;
    std::shared_ptr<const EpisodeCollection> episodes;
    std::unordered_map<std::string, size_t> byKey;
    std::map<std::string, Indices> categories;
    Indices all;
    Library(const Source &source, SharedItems items, bool series) : source(source), items(std::move(items)) {
        byKey.reserve(this->items->size());
        all.reserve(this->items->size());
        for (size_t i = 0; i < this->items->size(); ++i) {
            const auto &item = (*this->items)[i];
            const auto key = series ? seriesKey(item) : mediaKey(item);
            if (!byKey.emplace(key, i).second)
                continue;
            all.push_back(i);
            categories[item.groupTitle.empty() ? tr("all_items") : item.groupTitle].push_back(i);
        }
    }
};
using Lib = std::shared_ptr<const Library>;
VodTile actionTile(const std::string &title, const std::string &key, std::function<void()> select) {
    VodTile tile;
    tile.title = title;
    tile.key = key;
    tile.select = std::move(select);
    return tile;
}
void toggleFavorite(const Source &source, const LiveM3u8 &item, bool series) {
    if (!libraryPreferences().toggleFavorite(source.scope, item, series))
        brls::Application::notify(tr("preferences_error"));
}
void favoriteAction(VodTile &tile, const Source &source, LiveM3u8 item, bool series) {
    tile.favorite = [source, item, series] { toggleFavorite(source, item, series); };
    tile.isFavorite = [scope = source.scope, item, series] {
        return libraryPreferences().isFavorite(scope, item, series);
    };
}
void play(SharedItems queue, size_t index) {
    if (index >= queue->size() || (*queue)[index].url.empty())
        return;
    if ((*queue)[index].contentType == "movie")
        Intent::openLive({(*queue)[index]}, 0, [] {});
    else
        Intent::openLive(*queue, index, [] {});
}
void options(const Source &source, SharedItems queue, size_t index) {
    const auto item = (*queue)[index];
    const bool series = item.contentType == "series";
    auto *d = new brls::Dialog(item.title);
    d->addButton(
        tr(libraryPreferences().isFavorite(source.scope, item, series) ? "remove_favorite" : "add_favorite"),
        [source, item, series] { toggleFavorite(source, item, series); });
    d->addButton(tr("start_over"), [queue, index] {
        watchStore().clear((*queue)[index]);
        play(queue, index);
    });
    d->addButton(brls::getStr("hints/back"), [] {});
    d->open();
}
void info(const Source &source, SharedItems queue, size_t index) {
    const auto &media = (*queue)[index];
    auto *d =
        new brls::Dialog(media.title + "\n\n" + (media.description.empty() ? tr("s18") : media.description));
    d->addButton(tr("continue"), [queue, index] { play(queue, index); });
    d->addButton(tr("options"), [source, queue, index] { options(source, queue, index); });
    d->addButton(brls::getStr("hints/back"), [] {});
    d->open();
}
void playEpisode(Lib library, size_t group, size_t variant = SIZE_MAX) {
    auto queue = std::make_shared<const Items>(episodeQueue(
        *library->episodes->raw, library->episodes->groups,
        [](const LiveM3u8 &item) { return watchStore().progress(item); }, group, variant));
    play(queue, group);
}
void showEpisodeSources(Lib library, size_t group) {
    const auto &set = *library->episodes;
    auto *page = openVodGrid(tr("episode_sources"));
    Tiles tiles;
    for (auto index : set.groups[group].variants) {
        const auto &media = (*set.raw)[index];
        VodTile tile;
        tile.title = media.title;
        tile.subtitle = tr("source") + " " + std::to_string(tiles.size() + 1);
        tile.image = artworkUrl(library->source.server + "/", media.logo);
        tile.key = mediaKey(media);
        tile.progressMedia = media;
        tile.style = VodStyle::Episode;
        tile.select = [library, group, index] { playEpisode(library, group, index); };
        favoriteAction(tile, library->source, media, true);
        tiles.push_back(std::move(tile));
    }
    page->setTiles(std::move(tiles));
    page->restoreFocusOnShow();
}
void episodeInfo(Lib library, size_t group) {
    const auto &set = *library->episodes;
    const auto index = preferredEpisode(*set.raw, set.groups[group],
                                        [](const LiveM3u8 &media) { return watchStore().progress(media); });
    const auto media = (*set.raw)[index];
    auto *d = new brls::Dialog(media.title + "\n\n" + media.description);
    d->addButton(tr("continue"), [library, group] { playEpisode(library, group); });
    d->addButton(tr("options"), [library, group, index, media] {
        auto *options = new brls::Dialog(media.title);
        options->addButton(tr(libraryPreferences().isFavorite(library->source.scope, media, true)
                                  ? "remove_favorite"
                                  : "add_favorite"),
                           [library, media] { toggleFavorite(library->source, media, true); });
        options->addButton(tr("start_over"), [library, group, index] {
            for (auto variant : library->episodes->groups[group].variants)
                watchStore().clear((*library->episodes->raw)[variant]);
            playEpisode(library, group, index);
        });
        if (library->episodes->groups[group].variants.size() > 1)
            options->addButton(tr("episode_sources"),
                               [library, group] { showEpisodeSources(library, group); });
        else
            options->addButton(brls::getStr("hints/back"), [] {});
        options->open();
    });
    d->addButton(brls::getStr("hints/back"), [] {});
    d->open();
}

Tiles playableTiles(Lib library, SharedIndices indices, const std::string &title);
void showItems(Lib library, Indices indices, const std::string &title, bool favoriteShelf = false) {
    auto *page = openVodGrid(title);
    auto list = std::make_shared<const Indices>(std::move(indices));
    auto build = [library, list, title, favoriteShelf] {
        if (!favoriteShelf)
            return playableTiles(library, list, title);
        Indices filtered;
        for (auto i : *list)
            if (libraryPreferences().isFavorite(library->source.scope, (*library->items)[i], false))
                filtered.push_back(i);
        return playableTiles(library, std::make_shared<const Indices>(std::move(filtered)), title);
    };
    if (favoriteShelf)
        page->setBuilder(build, [] { return libraryPreferences().version(); });
    else
        page->setBuilder(build);
}
Tiles playableTiles(Lib library, SharedIndices indices, const std::string &title) {
    Tiles tiles;
    tiles.reserve(indices->size() + 1);
    if (!indices->empty())
        tiles.push_back(actionTile(brls::getStr("hints/search"), "search", [library, indices, title] {
            openSearchInput(brls::getStr("hints/search"), "", [library, indices, title](std::string query) {
                Indices filtered;
                query = normalizeSearchText(query);
                for (auto i : *indices)
                    if (normalizeSearchText((*library->items)[i].title).find(query) != std::string::npos)
                        filtered.push_back(i);
                showItems(library, std::move(filtered), title + " / " + brls::getStr("hints/search"));
            });
        }));
    for (auto i : *indices) {
        const auto &media = (*library->items)[i];
        VodTile tile;
        tile.title = media.title;
        tile.subtitle = media.season.empty()
                            ? media.groupTitle
                            : tr("season") + " " + media.season + " • " + tr("episode") + " " + media.episode;
        tile.image = artworkUrl(library->source.server + "/", artworkValue(Json(media.logo)));
        tile.key = mediaKey(media);
        tile.progressMedia = media;
        tile.style = media.contentType == "series" ? VodStyle::Episode : VodStyle::Poster;
        tile.select = [queue = library->items, i] { play(queue, i); };
        tile.info = [source = library->source, queue = library->items, i] { info(source, queue, i); };
        if (library->episodes) {
            const auto set = library->episodes;
            tile.key = set->groups[i].key;
            tile.select = [library, i] { playEpisode(library, i); };
            tile.info = [library, i] { episodeInfo(library, i); };
            tile.progressState = [set, i] {
                const auto chosen = preferredEpisode(*set->raw, set->groups[i], [](const LiveM3u8 &item) {
                    return watchStore().progress(item);
                });
                return watchStore().progress((*set->raw)[chosen]);
            };
            if (set->groups[i].variants.size() > 1)
                tile.subtitle += " · " + std::to_string(set->groups[i].variants.size()) + " " + tr("sources");
            const LiveM3u8 *display = &media;
            if (brls::Application::getLocale() == "he")
                for (auto variant : set->groups[i].variants)
                    if ((*set->raw)[variant].title.find("\xd7") != std::string::npos) {
                        display = &(*set->raw)[variant];
                        break;
                    }
            static const std::regex prefix(R"(^.*?[sS][0-9]+[ ._-]*[eE][0-9]+[ .:_-]*)");
            auto caption = std::regex_replace(display->title, prefix, "");
            tile.title = tr("episode") + " " + media.episode;
            if (!caption.empty() && asciiLower(caption).find("episode ") != 0)
                tile.title += " · " + caption;
        }
        favoriteAction(tile, library->source, media, media.contentType == "series");
        tiles.push_back(std::move(tile));
    }
    return tiles;
}
void showSeasons(VodGridPage *page, const Source &source, const LiveM3u8 &series, SharedItems episodes) {
    auto collection = std::make_shared<EpisodeCollection>();
    collection->raw = episodes;
    collection->groups = groupEpisodes(*episodes);
    for (size_t group = 0; group < collection->groups.size(); ++group)
        for (auto variant : collection->groups[group].variants)
            collection->byMedia[mediaKey((*episodes)[variant])] = group;
    auto canonical = std::make_shared<const Items>(episodeQueue(
        *episodes, collection->groups, [](const LiveM3u8 &item) { return watchStore().progress(item); }));
    auto library = std::make_shared<Library>(source, canonical, false);
    library->episodes = collection;
    episodes = canonical;
    auto seasons = std::make_shared<std::map<std::string, Indices>>();
    for (size_t i = 0; i < episodes->size(); ++i)
        (*seasons)[(*episodes)[i].season.empty() ? "1" : (*episodes)[i].season].push_back(i);
    auto keys = std::make_shared<std::vector<std::string>>();
    for (const auto &entry : *seasons)
        keys->push_back(entry.first);
    std::stable_sort(keys->begin(), keys->end(), [](const auto &a, const auto &b) {
        try {
            return std::stoi(a) < std::stoi(b);
        } catch (...) {
            return a < b;
        }
    });
    page->setBuilder(
        [library, series, seasons, keys] {
            Tiles tiles;
            auto fav = actionTile(
                tr(libraryPreferences().isFavorite(library->source.scope, series, true) ? "remove_favorite"
                                                                                        : "add_favorite"),
                "favorite-series",
                [source = library->source, series] { toggleFavorite(source, series, true); });
            tiles.push_back(std::move(fav));
            for (const auto &watched : watchStore().recent("series"))
                if (seriesKey(watched) == seriesKey(series) && watchStore().progress(watched).resume() > 0) {
                    auto found = library->episodes->byMedia.find(mediaKey(watched));
                    if (found != library->episodes->byMedia.end()) {
                        auto list = std::make_shared<const Indices>(Indices{found->second});
                        auto resume = playableTiles(library, list, series.title);
                        resume[1].title = tr("continue") + " • " + watched.title;
                        tiles.push_back(std::move(resume[1]));
                    }
                    break;
                }
            for (const auto &key : *keys) {
                auto tile =
                    actionTile(tr("season") + " " + key, "season:" + key, [library, series, seasons, key] {
                        showItems(library, seasons->at(key), series.title + " / " + tr("season") + " " + key);
                    });
                tile.image = artworkUrl(library->source.server + "/", artworkValue(Json(series.logo)));
                tile.subtitle = std::to_string(seasons->at(key).size()) + " " + tr("s7");
                tiles.push_back(std::move(tile));
            }
            return tiles;
        },
        [] { return uint64_t(watchStore().version()) + libraryPreferences().version(); });
    page->restoreFocusOnShow();
}
void openSeries(Lib library, LiveM3u8 series) {
    auto *page = openVodGrid(series.title);
    const auto source = library->source;
    auto alive = page->alive;
    if (source.mode == 0) {
        Items episodes;
        for (const auto &item : *library->items)
            if (seriesKey(item) == seriesKey(series))
                episodes.push_back(item);
        sortEpisodes(episodes);
        showSeasons(page, source, series, std::make_shared<const Items>(std::move(episodes)));
        return;
    }
    const auto id = series.seriesId.empty() ? series.id : series.seriesId;
    auto fetch = [source, series, id](std::function<void(Items)> done, Fail fail) {
        request(
            source, "get_series_info", "&series_id=" + encodeUrlComponent(id),
            [source, series, id, done, fail](const Json &data) {
                if (!data.is_object() || !data.contains("episodes") || !data["episodes"].is_object()) {
                    fail();
                    return;
                }
                Items episodes;
                for (auto it = data["episodes"].begin(); it != data["episodes"].end(); ++it) {
                    if (!it.value().is_array())
                        continue;
                    for (const auto &e : it.value()) {
                        LiveM3u8 media;
                        media.id = text(e, "id");
                        if (media.id.empty())
                            continue;
                        media.providerId = series.providerId;
                        media.seriesId = id;
                        media.seriesTitle = series.seriesTitle.empty() ? series.title : series.seriesTitle;
                        media.title = text(e, "title");
                        media.season = text(e, "season");
                        if (media.season.empty())
                            media.season = it.key();
                        media.episode = text(e, "episode_num");
                        if (media.title.empty())
                            media.title = media.seriesTitle + " • " + media.episode;
                        media.url = stream(source, "series", media.id, text(e, "container_extension"));
                        media.contentType = "series";
                        media.logo = series.logo;
                        media.addedAt = numberField(e, "added");
                        if (e.contains("info")) {
                            media.description = description(e["info"]);
                            media.durationHint = numberField(e["info"], "duration_secs");
                            auto image = contentArtwork(e, source.server + "/");
                            if (!image.empty())
                                media.logo = image;
                        }
                        episodes.push_back(std::move(media));
                    }
                }
                sortEpisodes(episodes);
                done(std::move(episodes));
            },
            fail);
    };
    auto load = [page, alive, source, series, id, fetch](bool force) {
        if (!*alive)
            return;
        fetchCached(
            source.cache + "/episodes/" + id, fetch,
            [page, alive, source, series](SharedItems episodes) {
                if (*alive)
                    showSeasons(page, source, series, episodes);
            },
            [page, alive] {
                if (*alive)
                    page->setTiles({actionTile(tr("retry"), "retry", [page, alive] {
                        if (*alive)
                            page->reload();
                    })});
            },
            force);
    };
    page->setRefreshAction([load] { load(true); });
    page->setTiles({actionTile(tr("s16"), "loading", {})});
    load(false);
}
Tiles seriesTiles(Lib library, SharedIndices indices) {
    Tiles tiles;
    auto recent = watchStore().recent("series");
    std::unordered_map<std::string, LiveM3u8> progress;
    for (const auto &item : recent)
        progress[seriesKey(item)] = item;
    for (auto i : *indices) {
        auto series = (*library->items)[i];
        if (series.seriesTitle.empty())
            series.seriesTitle = series.title;
        series.title = series.seriesTitle;
        VodTile tile;
        tile.title = series.title;
        tile.style = VodStyle::Poster;
        tile.subtitle = series.groupTitle;
        tile.key = seriesKey(series);
        tile.image = artworkUrl(library->source.server + "/", artworkValue(Json(series.logo)));
        auto it = progress.find(tile.key);
        if (it != progress.end())
            tile.progressMedia = it->second;
        tile.select = [library, series] { openSeries(library, series); };
        favoriteAction(tile, library->source, series, true);
        tile.info = [source = library->source, series] {
            auto *d = new brls::Dialog(series.title + "\n\n" + series.description);
            d->addButton(tr(libraryPreferences().isFavorite(source.scope, series, true) ? "remove_favorite"
                                                                                        : "add_favorite"),
                         [source, series] { toggleFavorite(source, series, true); });
            d->addButton(brls::getStr("hints/back"), [] {});
            d->open();
        };
        tiles.push_back(std::move(tile));
    }
    return tiles;
}
void showSeries(Lib library, Indices indices, const std::string &title, bool favorites = false) {
    auto *page = openVodGrid(title);
    auto list = std::make_shared<const Indices>(std::move(indices));
    page->setBuilder(
        [library, list, title, favorites] {
            auto visible = list;
            if (favorites) {
                Indices current;
                for (auto i : *list)
                    if (libraryPreferences().isFavorite(library->source.scope, (*library->items)[i], true))
                        current.push_back(i);
                visible = std::make_shared<const Indices>(std::move(current));
            }
            auto tiles = seriesTiles(library, visible);
            if (!visible->empty())
                tiles.insert(tiles.begin(),
                             actionTile(brls::getStr("hints/search"), "search", [library, visible, title] {
                                 openSearchInput(
                                     brls::getStr("hints/search"), "",
                                     [library, visible, title](std::string query) {
                                         Indices filtered;
                                         query = normalizeSearchText(query);
                                         for (auto i : *visible)
                                             if (normalizeSearchText((*library->items)[i].seriesTitle + " " +
                                                                     (*library->items)[i].title)
                                                     .find(query) != std::string::npos)
                                                 filtered.push_back(i);
                                         showSeries(library, std::move(filtered),
                                                    title + " / " + brls::getStr("hints/search"));
                                     });
                             }));
            return tiles;
        },
        [] { return uint64_t(watchStore().version()) + libraryPreferences().version(); });
}
void showShelf(Lib library, bool series, const std::string &kind, const std::string &title) {
    auto *page = openVodGrid(title);
    page->setBuilder(
        [library, series, kind, title] {
            Indices indices;
            if (kind == "added") {
                indices = library->all;
                std::unordered_map<size_t, int64_t> dates;
                for (auto i : indices)
                    dates[i] = watchStore().added((*library->items)[i]);
                std::stable_sort(indices.begin(), indices.end(),
                                 [&](size_t a, size_t b) { return dates[a] > dates[b]; });
                if (indices.size() > 200)
                    indices.resize(200);
            } else if (kind == "favorites") {
                for (auto i : library->all)
                    if (libraryPreferences().isFavorite(library->source.scope, (*library->items)[i], series))
                        indices.push_back(i);
            } else {
                for (const auto &item : watchStore().recent(series ? "series" : "movie")) {
                    auto found = library->byKey.find(series ? seriesKey(item) : mediaKey(item));
                    if (found != library->byKey.end())
                        indices.push_back(found->second);
                }
            }
            auto list = std::make_shared<const Indices>(std::move(indices));
            return series ? seriesTiles(library, list) : playableTiles(library, list, title);
        },
        [] { return uint64_t(watchStore().version()) + libraryPreferences().version(); });
}
void setCatalogue(VodGridPage *page, const Source &source, bool series, SharedItems items) {
    watchStore().observe(*items);
    auto library = std::make_shared<Library>(source, items, series);
    const auto title = tr(series ? "s1" : "s0");
    page->setBuilder([library, series, title] {
        Tiles tiles;
        for (const auto *shelf : {"favorites", "recent_added", "recent_watched"}) {
            const std::string kind = std::string(shelf) == "recent_added"     ? "added"
                                     : std::string(shelf) == "recent_watched" ? "watched"
                                                                              : "favorites";
            tiles.push_back(
                actionTile(tr(shelf), kind, [library, series, title, shelf = std::string(shelf), kind] {
                    showShelf(library, series, kind, title + " / " + tr(shelf));
                }));
        }
        tiles.push_back(actionTile(tr("all_items"), "all", [library, series, title] {
            if (series)
                showSeries(library, library->all, title);
            else
                showItems(library, library->all, title);
        }));
        for (const auto &category : library->categories) {
            auto tile = actionTile(category.first, "category:" + category.first,
                                   [library, series, title, name = category.first] {
                                       const auto &indices = library->categories.at(name);
                                       if (series)
                                           showSeries(library, indices, title + " / " + name);
                                       else
                                           showItems(library, indices, title + " / " + name);
                                   });
            if (!category.second.empty())
                tile.image = (*library->items)[category.second.front()].logo;
            tile.subtitle = std::to_string(category.second.size());
            tiles.push_back(std::move(tile));
        }
        return tiles;
    });
    page->restoreFocusOnShow();
}
void openCatalogue(bool series) {
    Source source;
    const auto key = source.cache + (series ? "/series" : "/movies");
    auto *page = openVodGrid(tr(series ? "s1" : "s0"));
    auto alive = page->alive;
    auto load = [source, series, key, page, alive](bool force) {
        if (!*alive)
            return;
        Fetch fetch = [source, series, force](std::function<void(Items)> done, Fail fail) {
            rawCatalogue(source, series, done, fail, force);
        };
        fetchCached(
            key, fetch,
            [source, series, page, alive](SharedItems items) {
                if (*alive)
                    setCatalogue(page, source, series, items);
            },
            [page, alive] {
                if (*alive)
                    page->setTiles({actionTile(tr("retry"), "retry", [page, alive] {
                        if (*alive)
                            page->reload();
                    })});
            },
            force);
    };
    page->setRefreshAction([load] { load(true); });
    page->setTiles({actionTile(tr("s16"), "loading", {})});
    load(false);
}
Tiles rootTiles() {
    return {actionTile(tr("s0"), "movies", [] { openCatalogue(false); }),
            actionTile(tr("s1"), "series", [] { openCatalogue(true); }),
            actionTile(tr("s3"), "epg", [] { configureEpgSource(); })};
}
} // namespace
brls::View *createIptvLibrary() {
    auto *page = new VodGridPage(tr("s4"), true);
    page->setBuilder(rootTiles);
    return page;
}
void openIptvBrowser() {
    auto *page = openVodGrid(tr("s4"));
    page->setBuilder(rootTiles);
}
} // namespace tsvitch

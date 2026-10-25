#pragma once
#include "utils/playlist.hpp"
#include "utils/vod_helpers.hpp"
#include <unordered_map>
#include <unordered_set>
#include <functional>
namespace tsvitch {
inline std::string artworkValue(const nlohmann::json &value) {
    if (value.is_string()) {
        auto s = trim(value.get<std::string>());
        if (s == "null" || s == "N/A" || s == "0")
            return {};
        if (!s.empty() && (s.front() == '[' || s.front() == '{')) {
            auto j = nlohmann::json::parse(s, nullptr, false);
            if (!j.is_discarded())
                return artworkValue(j);
        }
        return s;
    }
    if (value.is_array()) {
        for (const auto &item : value) {
            auto s = artworkValue(item);
            if (!s.empty())
                return s;
        }
    }
    if (value.is_object())
        for (const auto *key : {"url", "src", "cover_big", "movie_image", "stream_icon", "cover", "image"})
            if (value.contains(key)) {
                auto s = artworkValue(value[key]);
                if (!s.empty())
                    return s;
            }
    return {};
}
inline std::string artworkUrl(const std::string &base, std::string url) {
    url = trim(url);
    for (size_t at = 0; (at = url.find("&amp;", at)) != std::string::npos;)
        url.replace(at, 5, "&");
    if (url.empty() || url == "null" || url == "N/A")
        return {};
    url = resolvePlaylistUrl(base, url);
    std::string encoded;
    for (char c : url)
        encoded += c == ' ' ? "%20" : std::string(1, c);
    return encoded;
}
inline std::string contentArtwork(const nlohmann::json &data, const std::string &base) {
    for (const auto *key :
         {"stream_icon", "cover_big", "cover", "movie_image", "poster", "image", "backdrop_path"})
        if (data.contains(key)) {
            auto value = artworkValue(data[key]);
            if (!value.empty())
                return artworkUrl(base, value);
        }
    if (data.contains("info") && data["info"].is_object())
        return contentArtwork(data["info"], base);
    return {};
}
inline int episodeNumber(const std::string &value) {
    try {
        size_t end = 0;
        const auto number = std::stoi(value, &end);
        return end == value.size() && number > 0 ? number : 0;
    } catch (...) {
        return 0;
    }
}
inline int seasonNumber(const std::string &value) {
    if (value == "0" || value == "00")
        return 0;
    const int number = episodeNumber(value);
    return number > 0 ? number : -1;
}
inline void inferEpisodeNumbers(LiveM3u8 &item) {
    static const std::regex pattern(R"([sS]([0-9]+)[ ._-]*[eE]([0-9]+))");
    std::smatch match;
    if (std::regex_search(item.title, match, pattern)) {
        if (seasonNumber(item.season) < 0)
            item.season = std::to_string(episodeNumber(match[1]));
        if (!episodeNumber(item.episode))
            item.episode = std::to_string(episodeNumber(match[2]));
    }
}
struct EpisodeGroup {
    std::string key;
    std::vector<size_t> variants;
    int season = 0, episode = 0;
    std::string series;
};
inline std::vector<EpisodeGroup> groupEpisodes(const LiveM3u8ListResult &items) {
    std::vector<EpisodeGroup> result;
    std::unordered_map<std::string, size_t> groups;
    std::unordered_set<std::string> urls;
    for (size_t i = 0; i < items.size(); ++i) {
        auto media = items[i];
        inferEpisodeNumbers(media);
        if (!media.url.empty() && !urls.insert(media.url).second)
            continue;
        const auto parsedSeason = seasonNumber(media.season);
        const auto season = parsedSeason < 0 ? 1 : parsedSeason, episode = episodeNumber(media.episode);
        const auto key =
            episode ? seriesKey(media) + ":s" + std::to_string(season) + ":e" + std::to_string(episode)
                    : mediaKey(media) + ":" + media.url;
        auto found = groups.find(key);
        if (found == groups.end()) {
            groups[key] = result.size();
            result.push_back({key, {i}, season, episode, seriesKey(media)});
        } else
            result[found->second].variants.push_back(i);
    }
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.series != b.series   ? a.series < b.series
               : a.season == b.season ? a.episode < b.episode
                                      : a.season < b.season;
    });
    return result;
}
inline size_t preferredEpisode(const LiveM3u8ListResult &items, const EpisodeGroup &group,
                               const std::function<WatchProgress(const LiveM3u8 &)> &progress) {
    auto chosen = group.variants.front();
    auto recent = progress(items[chosen]).updated;
    for (auto index : group.variants) {
        auto stamp = progress(items[index]).updated;
        if (stamp > recent) {
            recent = stamp;
            chosen = index;
        }
    }
    return chosen;
}
inline LiveM3u8ListResult episodeQueue(const LiveM3u8ListResult &items,
                                       const std::vector<EpisodeGroup> &groups,
                                       const std::function<WatchProgress(const LiveM3u8 &)> &progress,
                                       size_t selected = SIZE_MAX, size_t variant = SIZE_MAX) {
    LiveM3u8ListResult result;
    for (size_t i = 0; i < groups.size(); ++i) {
        auto index =
            i == selected && variant != SIZE_MAX ? variant : preferredEpisode(items, groups[i], progress);
        auto item = items[index];
        inferEpisodeNumbers(item);
        result.push_back(std::move(item));
    }
    return result;
}
} // namespace tsvitch

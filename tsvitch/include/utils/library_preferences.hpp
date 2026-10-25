#pragma once
#include "utils/json_file.hpp"
#include "utils/watch_store.hpp"
#include <unordered_set>
#include <unordered_map>
namespace tsvitch {
class LibraryPreferences {
    using Json = nlohmann::json;
    std::filesystem::path file;
    Json data = Json::object();
    std::unordered_set<std::string> favoriteKeys;
    uint64_t revision = 0;
    static std::string key(const std::string &scope, const LiveM3u8 &item, bool series) {
        return scope + "\n" + (series ? "series:" : "movie:") + (series ? seriesKey(item) : mediaKey(item));
    }
    void index() {
        favoriteKeys.clear();
        for (auto scope = data.begin(); scope != data.end(); ++scope) {
            for (const auto *kind : {"movie", "series"}) {
                if (!scope.value().contains(kind) || !scope.value()[kind].is_object())
                    continue;
                for (auto it = scope.value()[kind].begin(); it != scope.value()[kind].end(); ++it)
                    favoriteKeys.insert(scope.key() + "\n" + kind + ":" + it.key());
            }
        }
    }
    bool commit(const Json &previous) {
        if (!writeJsonFile(file, data)) {
            data = previous;
            index();
            return false;
        }
        ++revision;
        return true;
    }

  public:
    explicit LibraryPreferences(std::filesystem::path file) : file(std::move(file)) {
        data = readJsonFile(this->file);
        if (!data.is_object())
            data = readJsonFile(this->file.string() + ".bak");
        if (!data.is_object())
            data = Json::object();
        index();
    }
    uint64_t version() const { return revision; }
    bool isFavorite(const std::string &scope, const LiveM3u8 &item, bool series) const {
        return favoriteKeys.count(key(scope, item, series)) != 0;
    }
    bool toggleFavorite(const std::string &scope, LiveM3u8 item, bool series) {
        const auto previous = data;
        const auto id = series ? seriesKey(item) : mediaKey(item);
        const auto kind = series ? "series" : "movie";
        auto &entries = data[scope][kind];
        if (!entries.is_object())
            entries = Json::object();
        if (entries.contains(id)) {
            entries.erase(id);
            favoriteKeys.erase(key(scope, item, series));
        } else {
            if (series) {
                if (item.providerId.empty()) {
                    const auto start = item.url.find("://");
                    if (start != std::string::npos)
                        item.providerId = item.url.substr(0, item.url.find('/', start + 3));
                }
                item.contentType = "series";
                item.url.clear();
                if (item.seriesTitle.empty())
                    item.seriesTitle = item.title;
                item.title = item.seriesTitle;
                item.season.clear();
                item.episode.clear();
            }
            entries[id] = item;
            favoriteKeys.insert(scope + "\n" + kind + ":" + id);
        }
        return commit(previous);
    }
    LiveM3u8ListResult favorites(const std::string &scope, bool series) const {
        LiveM3u8ListResult result;
        const auto kind = series ? "series" : "movie";
        if (!data.contains(scope) || !data[scope].contains(kind) || !data[scope][kind].is_object())
            return result;
        for (const auto &entry : data[scope][kind])
            try {
                result.push_back(entry.get<LiveM3u8>());
            } catch (...) {
            }
        std::stable_sort(result.begin(), result.end(),
                         [](const LiveM3u8 &a, const LiveM3u8 &b) { return a.title < b.title; });
        return result;
    }
    std::vector<std::string> pins(const std::string &scope) const {
        try {
            return data.at(scope).value("pins", std::vector<std::string>{});
        } catch (...) {
            return {};
        }
    }
    bool pinned(const std::string &scope, const std::string &group) const {
        const auto names = pins(scope);
        return std::find(names.begin(), names.end(), group) != names.end();
    }
    bool togglePin(const std::string &scope, const std::string &group) {
        const auto previous = data;
        auto names = pins(scope);
        auto it = std::find(names.begin(), names.end(), group);
        if (it == names.end())
            names.push_back(group);
        else
            names.erase(it);
        data[scope]["pins"] = names;
        return commit(previous);
    }
    void sortGroups(const std::string &scope, std::vector<std::string> &groups) const {
        std::unordered_map<std::string, size_t> rank;
        const auto names = pins(scope);
        for (size_t i = 0; i < names.size(); ++i)
            rank[names[i]] = i;
        std::stable_sort(groups.begin(), groups.end(), [&](const std::string &a, const std::string &b) {
            const auto x = rank.count(a) ? rank.at(a) : names.size(),
                       y = rank.count(b) ? rank.at(b) : names.size();
            return x == y ? a < b : x < y;
        });
    }
    std::string selected(const std::string &scope) const {
        try {
            return data.at(scope).value("selected", std::string{});
        } catch (...) {
            return {};
        }
    }
    void select(const std::string &scope, const std::string &group) {
        if (selected(scope) == group)
            return;
        const auto previous = data;
        data[scope]["selected"] = group;
        commit(previous);
    }
};
} // namespace tsvitch

#pragma once
#include "utils/json_file.hpp"
#include "api/tsvitch/result/home_live_result.h"
#include <algorithm>
#include <memory>
#include <chrono>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
namespace tsvitch {
inline int64_t vodNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
struct VodSnapshot {
    std::shared_ptr<const LiveM3u8ListResult> items;
    int64_t saved = 0;
    bool fresh(int64_t now) const { return now >= saved && now - saved < 6 * 60 * 60; }
};
class VodCache {
    struct Entry {
        std::shared_ptr<const VodSnapshot> value;
        uint64_t used;
    };
    std::filesystem::path directory;
    std::mutex mutex, diskMutex;
    std::map<std::string, Entry> memory;
    uint64_t clock = 0;
    std::filesystem::path path(const std::string &key) const {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : key) {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        std::ostringstream name;
        name << std::hex << std::setw(16) << std::setfill('0') << hash;
        return directory / (name.str() + ".json");
    }

  public:
    explicit VodCache(std::filesystem::path directory) : directory(std::move(directory)) {}
    std::shared_ptr<const VodSnapshot> find(const std::string &key) {
        std::lock_guard<std::mutex> guard(mutex);
        auto it = memory.find(key);
        if (it == memory.end())
            return {};
        it->second.used = ++clock;
        return it->second.value;
    }
    void remember(const std::string &key, std::shared_ptr<const VodSnapshot> value) {
        std::lock_guard<std::mutex> guard(mutex);
        memory[key] = {std::move(value), ++clock};
        while (memory.size() > 14) {
            auto oldest = memory.begin();
            for (auto it = memory.begin(); it != memory.end(); ++it)
                if (it->second.used < oldest->second.used)
                    oldest = it;
            memory.erase(oldest);
        }
    }
    std::shared_ptr<const VodSnapshot> read(const std::string &key) {
        std::lock_guard<std::mutex> guard(diskMutex);
        auto data = readJsonFile(path(key));
        try {
            if (!data.is_object() || data.value("format", 0) != 1 ||
                data.value("source", std::string{}) != key || !data.at("items").is_array())
                return {};
            auto snapshot = std::make_shared<VodSnapshot>();
            snapshot->items =
                std::make_shared<const LiveM3u8ListResult>(data["items"].get<LiveM3u8ListResult>());
            snapshot->saved = data.at("saved").get<int64_t>();
            return snapshot;
        } catch (...) {
            return {};
        }
    }
    bool persist(const std::string &key, const VodSnapshot &value) {
        std::lock_guard<std::mutex> guard(diskMutex);
        const bool result = writeJsonFile(
            path(key),
            nlohmann::json{{"format", 1}, {"source", key}, {"saved", value.saved}, {"items", *value.items}});
        // Bound the application's own disk cache; active pages retain shared memory snapshots.
        if (result)
            try {
                std::vector<std::filesystem::directory_entry> files;
                for (const auto &entry : std::filesystem::directory_iterator(directory))
                    if (entry.is_regular_file() && entry.path().extension() == ".json")
                        files.push_back(entry);
                if (files.size() > 64) {
                    std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
                        return a.last_write_time() < b.last_write_time();
                    });
                    for (size_t i = 0; i < files.size() - 64; ++i)
                        std::filesystem::remove(files[i]);
                }
            } catch (...) {
            }
        return result;
    }
};
} // namespace tsvitch

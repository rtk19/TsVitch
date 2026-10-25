#pragma once
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
namespace tsvitch {
inline nlohmann::json readJsonFile(const std::filesystem::path &path) {
    try {
        std::ifstream input(path);
        nlohmann::json data;
        if (input >> data)
            return data;
    } catch (...) {
    }
    return nullptr;
}
inline bool writeJsonFile(const std::filesystem::path &path, const nlohmann::json &data) noexcept {
    try {
        if (std::filesystem::exists(path) && !std::filesystem::is_regular_file(path))
            return false;
        std::filesystem::create_directories(path.parent_path());
        const auto staged = path.string() + ".new", backup = path.string() + ".bak";
        {
            std::ofstream out(staged, std::ios::binary | std::ios::trunc);
            out << data.dump();
            out.flush();
            if (!out.good())
                return false;
            out.close();
            if (out.fail())
                return false;
        }
        std::error_code error;
        std::filesystem::rename(staged, path, error);
        if (!error)
            return true;
        std::filesystem::remove(backup, error);
        error.clear();
        if (std::filesystem::exists(path))
            std::filesystem::rename(path, backup, error);
        if (error)
            return false;
        std::filesystem::rename(staged, path, error);
        if (!error)
            return true;
        std::filesystem::rename(backup, path, error);
        return false;
    } catch (...) {
        return false;
    }
}
} // namespace tsvitch

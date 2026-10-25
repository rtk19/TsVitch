#include <borealis/core/i18n.hpp>
#include "utils/app_updater.hpp"
#include "utils/config_helper.hpp"
#include "utils/release_version.hpp"
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include <curl/curl.h>
#include <atomic>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace tsvitch {
namespace {
std::string executablePath;
std::atomic<bool> installing{false};
std::atomic<bool> pending{false};
std::atomic<bool> stopping{false};
std::thread worker;
int progress(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t) { return stopping ? 1 : 0; }
constexpr size_t maximumSize = 128 * 1024 * 1024;
size_t receive(char* data, size_t size, size_t count, void* target) {
    auto& buffer = *static_cast<std::vector<unsigned char>*>(target);
    if (size && count > maximumSize / size) return 0;
    const auto bytes = size * count;
    if (bytes > maximumSize - buffer.size()) return 0;
    buffer.insert(buffer.end(), data, data + bytes);
    return bytes;
}
std::vector<unsigned char> download(const std::string& url) {
    std::vector<unsigned char> buffer;
    auto* handle = curl_easy_init();
    if (!handle) throw std::runtime_error("Cannot start download");
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "TsVitch-rtk19-updater");
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 180L);
    curl_easy_setopt(handle, CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(maximumSize));
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);
    const auto result = curl_easy_perform(handle);
    long status = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(handle);
    if (result != CURLE_OK || status != 200) throw std::runtime_error("Download failed; existing application preserved");
    return buffer;
}
}
void setApplicationPath(const std::string& path) { executablePath = path; }
void installLatestUpdate() {
#ifndef __SWITCH__
    brls::Application::notify("In-app installation is available on Nintendo Switch");
#else
    if (pending) { brls::Application::notify(brls::getStr("iptv/s37")); return; }
    if (installing.exchange(true)) return;
    const auto path = executablePath;
    if (path.rfind("sdmc:/", 0) != 0 || std::filesystem::path(path).extension() != ".nro") {
        installing = false;
        brls::Application::notify(brls::getStr("iptv/s38"));
        return;
    }
    brls::Application::notify(brls::getStr("iptv/s33"));
    if (worker.joinable()) worker.join();
    stopping = false;
    worker = std::thread([path] {
        try {
            const auto metadata = download("https://api.github.com/repos/rtk19/TsVitch/releases/latest");
            const auto release = nlohmann::json::parse(metadata.begin(), metadata.end());
            auto& version = APPVersion::instance();
            if (release.value("prerelease", true) || release.value("draft", true) ||
                !isNewerStableRelease(release.value("tag_name", ""), {version.major, version.minor, version.revision})) {
                brls::sync([] { brls::Application::notify(brls::getStr("iptv/s32")); });
                installing = false;
                return;
            }
            std::string url, digest;
            size_t expectedSize = 0;
            for (const auto& asset : release.at("assets")) {
                if (asset.value("name", "") == "TsVitch.nro") {
                    url = asset.value("browser_download_url", "");
                    if (asset.contains("digest") && asset["digest"].is_string()) digest = asset["digest"].get<std::string>();
                    expectedSize = asset.value("size", size_t{0});
                }
            }
            if (url.rfind("https://github.com/rtk19/TsVitch/releases/download/", 0) != 0 ||
                digest.rfind("sha256:", 0) != 0 || digest.size() != 71 || !expectedSize || expectedSize > maximumSize)
                throw std::runtime_error("Release has no verified TsVitch.nro asset");
            const auto data = download(url);
            if (data.size() != expectedSize || data.size() < 0x80 || std::memcmp(data.data() + 0x10, "NRO0", 4) != 0)
                throw std::runtime_error("Invalid or incomplete NRO; existing application preserved");
            unsigned char hash[32];
            sha256CalculateHash(hash, data.data(), data.size());
            std::string actual;
            const char* hex = "0123456789abcdef";
            for (unsigned char byte : hash) { actual += hex[byte >> 4]; actual += hex[byte & 15]; }
            if (actual != digest.substr(7)) throw std::runtime_error("Update checksum mismatch; existing application preserved");
            const auto staged = path + ".new";
            {
                std::ofstream output(staged, std::ios::binary | std::ios::trunc);
                output.write(reinterpret_cast<const char*>(data.data()), data.size());
                output.flush();
                if (!output.good()) throw std::runtime_error("Cannot write update; check free SD space");
                output.close();
                if (output.fail()) throw std::runtime_error("Cannot close update file");
            }
            pending = true;
            brls::sync([path] {
                auto* dialog = new brls::Dialog(brls::getStr("iptv/s39"));
                dialog->addButton(brls::getStr("iptv/s34"), [path] { if (envHasNextLoad()) envSetNextLoad(path.c_str(), path.c_str()); brls::Application::quit(); });
                dialog->addButton(brls::getStr("iptv/s35"), [] {});
                dialog->open();
            });
        } catch (const std::exception& error) {
            const std::string text = error.what();
            brls::sync([text] { brls::Application::notify(text); });
        }
        installing = false;
    });
#endif
}
void shutdownUpdater() {
    stopping = true;
    if (worker.joinable()) worker.join();
}
void applyPendingUpdate() {
#ifdef __SWITCH__
    if (!pending || executablePath.empty()) return;
    // This must be the last action before returning from main: no resource reads afterward.
    romfsExit();
    const auto staged = executablePath + ".new", backup = executablePath + ".bak";
    std::error_code ec;
    std::filesystem::remove(backup, ec);
    std::filesystem::rename(executablePath, backup, ec);
    if (ec) { brls::Logger::error("Cannot back up NRO; update remains staged"); return; }
    std::filesystem::rename(staged, executablePath, ec);
    if (ec) {
        std::filesystem::rename(backup, executablePath, ec);
        brls::Logger::error("Cannot replace NRO; previous version restored where possible");
    }
#endif
}

}

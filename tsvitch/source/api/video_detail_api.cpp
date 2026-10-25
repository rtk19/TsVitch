#include "utils/vod_cache_runtime.hpp"
#include "utils/library_state.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include "utils/url_encode.hpp"
#include "utils/playlist.hpp"
#include "utils/xmltv_time.hpp"
#include <sstream>
#include <utility>
#include <vector>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>

#include "tsvitch.h"
#include "tsvitch/util/http.hpp"
#include "borealis/core/thread.hpp"
#include "borealis/core/application.hpp"

#include "tsvitch/result/home_live_result.h"
#include "utils/config_helper.hpp"

namespace tsvitch {

/// Pulisce il testo rimuovendo caratteri che potrebbero causare problemi di rendering
// Helper function to safely get string value from JSON, handling null values
static std::string safeGetString(const nlohmann::json& json, const std::string& key, const std::string& defaultValue = "") {
    if (!json.contains(key) || json[key].is_null()) {
        return defaultValue;
    }
    if (json[key].is_string()) {
        return json[key].get<std::string>();
    }
    if (json[key].is_boolean()) return json[key].get<bool>() ? "1" : "0";
    if (json[key].is_number_integer() || json[key].is_number_unsigned()) return json[key].dump();
    return defaultValue;
}

// Helper function to sanitize text for safe font rendering
static std::string sanitizeText(const std::string& text) {
    if (text.empty()) return text;

    std::string cleaned = text;

    // Rimuovi caratteri di controllo ASCII (0-31, eccetto tab, newline, carriage return)
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), [](unsigned char c) {
        return (c < 32 && c != 9 && c != 10 && c != 13) || c == 127;
    }), cleaned.end());

    // Sostituisci caratteri problematici con versioni sicure
    std::replace(cleaned.begin(), cleaned.end(), '\n', ' ');
    std::replace(cleaned.begin(), cleaned.end(), '\r', ' ');
    std::replace(cleaned.begin(), cleaned.end(), '\t', ' ');

    // Rimuovi spazi multipli
    std::regex multiple_spaces("\\s+");
    cleaned = std::regex_replace(cleaned, multiple_spaces, " ");

    // Trim
    cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r\f\v"));
    cleaned.erase(cleaned.find_last_not_of(" \t\n\r\f\v") + 1);

    return cleaned;
}

/// Découpe une chaîne `a,b,c` -> {"a","b","c"}
static std::vector<std::string> split_csv(const std::string& csv)
{
    std::vector<std::string> out;
    std::stringstream        ss(csv);
    std::string              item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

nlohmann::json parse_m3u8_to_json(const std::string& content) {
    return nlohmann::json(parsePlaylist(content));
}

void TsVitchClient::get_file_m3u8(const std::function<void(LiveM3u8ListResult)>& callback,
                                const ErrorCallback& error) {
    const auto url = ProgramConfig::instance().getM3U8Url();
    const auto timeout = std::max(30000, ProgramConfig::instance().getIntOption(SettingItem::M3U8_TIMEOUT));
    const auto cacheKey=libraryCacheKey()+"/playlist";
    vodCacheStore();
    cpr::GetCallback([url, callback, error, cacheKey](cpr::Response response) {
        if (response.error || response.status_code != 200) {
            brls::sync([error] { if (error) error("Cannot download M3U playlist", -1); });
            return;
        }
        try {
            auto snapshot=std::make_shared<VodSnapshot>();
            snapshot->items=std::make_shared<const LiveM3u8ListResult>(parsePlaylist(response.text,url));snapshot->saved=vodNow();
            vodCacheStore().remember(cacheKey,snapshot);
            brls::sync([callback,snapshot]{if(callback)callback(*snapshot->items);});
            brls::async([cacheKey,snapshot]{vodCacheStore().persist(cacheKey,*snapshot);});
        } catch (const std::exception&) {
            brls::sync([error] { if (error) error("Invalid M3U playlist", -1); });
        }
    }, cpr::Url{url}, cpr::Timeout{timeout}, HTTP::HEADERS, HTTP::PROXIES, HTTP::VERIFY);
}

void TsVitchClient::get_xtream_channels(const std::function<void(LiveM3u8ListResult)>& callback,
                                       const ErrorCallback& error) {
    get_xtream_channels_with_retry(callback, error, 3); // Max 3 retry attempts
}

/**
 * Fetches live streams from Xtream Codes API with automatic retry logic.
 * Timeout: 45+ seconds (Xtream servers typically slower than M3U8 sources)
 * Retries: 3 attempts with exponential backoff (1s network, 3s server errors)
 * Error Handling: Network errors retry, server errors 502/503 retry, 4xx no retry
 */
void TsVitchClient::get_xtream_channels_with_retry(const std::function<void(LiveM3u8ListResult)>& callback,
                                                  const ErrorCallback& error, int maxRetries) {
    auto serverUrl = ProgramConfig::instance().getXtreamServerUrl();
    auto username = ProgramConfig::instance().getXtreamUsername();
    auto password = ProgramConfig::instance().getXtreamPassword();

    if (serverUrl.empty() || username.empty() || password.empty()) {
        if (error) {
            error("Xtream Codes credentials not configured properly", -1);
        }
        return;
    }

    // Construct Xtream API URL for getting all live streams
    std::string xtreamUrl = serverUrl;
    if (xtreamUrl.back() != '/') {
        xtreamUrl += "/";
    }
    xtreamUrl += "player_api.php?username=" + encodeUrlComponent(username) + "&password=" + encodeUrlComponent(password) + "&action=get_live_streams";

    brls::Logger::debug("Fetching Xtream channels (retries left: {})", maxRetries);

    auto timeoutMs = ProgramConfig::instance().getIntOption(SettingItem::M3U8_TIMEOUT);
    // Use longer timeout for Xtream API (typically slower than M3U8 sources)
    if (timeoutMs < 45000) timeoutMs = 45000; // Minimum 45 seconds for Xtream

    // Use cpr::GetCallback per migliori prestazioni asincrono
    cpr::GetCallback(
        [callback, error, maxRetries, xtreamUrl, timeoutMs, serverUrl, username, password](const cpr::Response& r) {
            try {
                brls::Logger::info("Xtream response: status={}, size={}KB", r.status_code, r.text.length()/1024);

                // Handle network errors
                if (r.error) {
                    brls::Logger::error("Xtream network error: {}", r.error.message);
                    if (maxRetries > 0) {
                        brls::Logger::info("Retrying Xtream request due to network error (retries left: {})", maxRetries - 1);
                        brls::Threading::async([callback, error, maxRetries]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                            TsVitchClient::get_xtream_channels_with_retry(callback, error, maxRetries - 1);
                        });
                        return;
                    }
                    if (error) {
                        error("Network error: " + r.error.message, -1);
                    }
                    return;
                }

                // Handle HTTP errors with retry for 503 and 502
                if ((r.status_code == 503 || r.status_code == 502) && maxRetries > 0) {
                    brls::Logger::warning("Xtream server returned {} - server temporarily unavailable, retrying in 3 seconds (retries left: {})", r.status_code, maxRetries - 1);
                    brls::Threading::async([callback, error, maxRetries]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait 3 seconds for server errors
                        TsVitchClient::get_xtream_channels_with_retry(callback, error, maxRetries - 1);
                    });
                    return;
                }

                if (r.status_code != 200) {
                    brls::Logger::error("Xtream API error: HTTP {}, body: {}", r.status_code, r.text.substr(0, 500));
                    if (error) {
                        error("HTTP error " + std::to_string(r.status_code) + ": " + r.text.substr(0, 200), r.status_code);
                    }
                    return;
                }

                if (r.text.empty()) {
                    brls::Logger::error("Xtream API returned empty response");
                    if (error) {
                        error("Empty response from Xtream server", -1);
                    }
                    return;
                }

                // Sposta il parsing JSON in un thread asincrono per non bloccare la UI
                brls::Threading::async([callback, error, responseText = std::move(r.text), serverUrl, username, password]() {
                    try {
                        auto parse_start = std::chrono::high_resolution_clock::now();

                        nlohmann::json json_result;
                        try {
                            json_result = nlohmann::json::parse(responseText);
                        } catch (const nlohmann::json::parse_error& e) {
                            brls::Logger::error("Failed to parse Xtream JSON: {}", e.what());
                            brls::sync([error]() {
                                if (error) error("Invalid JSON response from Xtream server", -1);
                            });
                            return;
                        }

                        if (!json_result.is_array()) {
                            brls::Logger::error("Xtream response is not an array, type: {}", json_result.type_name());
                            brls::sync([error]() {
                                if (error) error("Invalid response format from Xtream server", -1);
                            });
                            return;
                        }

                        // Providers normally return category_id on streams, with names in a separate endpoint.
                        // This runs on the parsing worker, never on the UI thread.
                        std::unordered_map<std::string, std::string> categories;
                        auto categoryUrl = serverUrl;
                        if (categoryUrl.back() != '/') categoryUrl += '/';
                        categoryUrl += "player_api.php?username=" + encodeUrlComponent(username) +
                            "&password=" + encodeUrlComponent(password) + "&action=get_live_categories";
                        const auto categoryResponse = cpr::Get(cpr::Url{categoryUrl}, cpr::Timeout{15000},
                            HTTP::HEADERS, HTTP::COOKIES, HTTP::PROXIES, HTTP::VERIFY);
                        if (!categoryResponse.error && categoryResponse.status_code == 200) {
                            const auto data = nlohmann::json::parse(categoryResponse.text, nullptr, false);
                            if (data.is_array()) {
                                for (const auto& category : data) {
                                    const auto id = safeGetString(category, "category_id");
                                    if (!id.empty()) categories[id] = safeGetString(category, "category_name");
                                }
                            }
                        }
                        int archiveCorrection=0;
                        const auto authUrl=categoryUrl.substr(0,categoryUrl.rfind("&action="));
                        const auto auth=cpr::Get(cpr::Url{authUrl},cpr::Timeout{10000},HTTP::HEADERS,HTTP::PROXIES,HTTP::VERIFY);
                        if(!auth.error&&auth.status_code==200) {
                            const auto account=nlohmann::json::parse(auth.text,nullptr,false);
                            if(account.is_object()&&account.contains("server_info")) {
                                const auto& info=account["server_info"];
                                auto civil=safeGetString(info,"time_now");
                                civil.erase(std::remove_if(civil.begin(),civil.end(),[](unsigned char c){return !std::isdigit(c);}),civil.end());
                                try {const auto delta=xmltvTimestamp(civil)-std::stoll(safeGetString(info,"timestamp_now"));
                                    if(delta>=-14*3600&&delta<=14*3600)archiveCorrection=delta;
                                }catch(...){}
                            }
                        }
                        LiveM3u8ListResult result;
                        result.reserve(json_result.size()); // Pre-allocazione per prestazioni

                        size_t processed = 0, skipped = 0;
                        for (size_t i = 0; i < json_result.size(); i++) {
                            const auto& item = json_result[i];
                            if (!item.is_object()) {
                                skipped++;
                                continue;
                            }

                            try {
                                LiveM3u8 live;

                                // Map Xtream fields con controlli ottimizzati
                                if (item.contains("stream_id") && !item["stream_id"].is_null()) {
                                    if (item["stream_id"].is_string()) {
                                        live.id = item["stream_id"].get<std::string>();
                                    } else if (item["stream_id"].is_number()) {
                                        live.id = std::to_string(item["stream_id"].get<int>());
                                    }
                                }

                                if (item.contains("num") && !item["num"].is_null()) {
                                    if (item["num"].is_string()) {
                                        live.chno = item["num"].get<std::string>();
                                    } else if (item["num"].is_number()) {
                                        live.chno = std::to_string(item["num"].get<int>());
                                    }
                                }

                                live.epgId = safeGetString(item, "epg_channel_id");
                                live.contentType = "live";
                                live.catchupCorrection=archiveCorrection;
                                if (safeGetString(item,"tv_archive") == "1") {
                                    live.catchupMode = "xc";
                                    try { live.catchupDays = std::stoi(safeGetString(item,"tv_archive_duration")); } catch (...) {}
                                }
                                auto epgBase = serverUrl;
                                while (!epgBase.empty() && epgBase.back() == '/') epgBase.pop_back();
                                live.epgSource = epgBase + "/xmltv.php?username=" + encodeUrlComponent(username) + "&password=" + encodeUrlComponent(password);
                                live.title = sanitizeText(safeGetString(item, "name"));
                                live.logo = safeGetString(item, "stream_icon");

                                std::string categoryName = safeGetString(item, "category_name");
                                const auto categoryId = safeGetString(item, "category_id");
                                if (categoryName.empty() && categories.count(categoryId)) categoryName = categories.at(categoryId);
                                if (categoryName.empty()) categoryName = categoryId.empty() ? "Live TV" : "Category " + categoryId;
                                live.groupTitle = sanitizeText(categoryName);

                                // Construct the stream URL for Xtream
                                if (!live.id.empty()) {
                                    std::string streamUrl = serverUrl;
                                    if (streamUrl.back() != '/') {
                                        streamUrl += "/";
                                    }
                                    streamUrl += "live/" + encodeUrlComponent(username) + "/" + encodeUrlComponent(password) + "/" + live.id + ".ts";
                                    live.url = streamUrl;
                                }

                                // Only add channels that have required fields
                                if (!live.id.empty() && !live.title.empty() && !live.url.empty()) {
                                    result.push_back(std::move(live));
                                    processed++;
                                }

                            } catch (const std::exception& e) {
                                brls::Logger::error("Exception processing Xtream item at index {}: {}", i, e.what());
                                skipped++;
                                continue;
                            }
                        }

                        auto parse_end = std::chrono::high_resolution_clock::now();
                        auto parse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(parse_end - parse_start);
                        brls::Logger::info("Xtream parsing completed in {}ms - processed: {}, skipped: {}, total: {}",
                                         parse_duration.count(), processed, skipped, json_result.size());

                        brls::sync([callback, result = std::move(result)]() {
                            if (callback) {
                                callback(result);
                            }
                        });

                    } catch (const std::exception& e) {
                        brls::Logger::error("Exception in Xtream async processing: {}", e.what());
                        brls::sync([error, e]() {
                            if (error) {
                                error("Failed to parse Xtream channels response: " + std::string(e.what()), -1);
                            }
                        });
                    }
                });
            } catch (const std::exception& e) {
                brls::Logger::error("Exception in Xtream response handler: {}", e.what());
                if (error) {
                    error("Failed to process Xtream response: " + std::string(e.what()), -1);
                }
            }
        },
        cpr::Url{xtreamUrl},
        cpr::HttpVersion{cpr::HttpVersionCode::VERSION_2_0_TLS},
        cpr::Timeout{timeoutMs},
        HTTP::HEADERS,
        HTTP::COOKIES,
        HTTP::PROXIES,
        HTTP::VERIFY);
}

void TsVitchClient::get_live_channels(const std::function<void(LiveM3u8ListResult)>& callback,
                                     const ErrorCallback& error) {
    // Check IPTV mode and call appropriate function
    int iptvMode = ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE);

    if (iptvMode == 0) {
        // M3U8 Mode
        brls::Logger::debug("Using M3U8 mode for live channels");
        get_file_m3u8([callback](LiveM3u8ListResult items) {
            items.erase(std::remove_if(items.begin(),items.end(),[](const LiveM3u8& item) {
                return item.contentType == "movie" || item.contentType == "series";
            }),items.end());
            if(callback) callback(items);
        }, error);
    } else if (iptvMode == 1) {
        // Xtream Codes Mode
        brls::Logger::debug("Using Xtream mode for live channels");
        get_xtream_channels(callback, error);
    } else {
        // Unknown mode
        brls::Logger::error("Unknown IPTV mode: {}", iptvMode);
        if (error) {
            error("Unknown IPTV mode configured", -1);
        }
    }
}

} // namespace tsvitch

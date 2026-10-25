#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>

#include "activity/live_player_activity.hpp"
#include "utils/iptv_browser.hpp"
#include "utils/programme.hpp"
#include "utils/catchup.hpp"
#include "utils/watch_progress.hpp"
#include "fragment/player_setting.hpp"
#include "utils/vod_helpers.hpp"
#include "core/HistoryManager.hpp"
#include "utils/number_helper.hpp"
#include "core/DownloadManager.hpp"
#include "core/DownloadProgressManager.hpp"

#include <vector>
#include <chrono>
#include <algorithm>
#include <fmt/format.h>

#include "tsvitch.h"

#include "utils/shader_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/stream_helper.hpp"
#include "utils/playback_position_manager.hpp"

#include "view/video_view.hpp"

#include "view/grid_dropdown.hpp"
#include "view/qr_image.hpp"
#include "view/mpv_core.hpp"

#include "config/server_config.h"

#include "core/FavoriteManager.hpp"
#include "core/DownloadManager.hpp"

using namespace brls::literals;

LiveActivity::LiveActivity(const std::vector<tsvitch::LiveM3u8> &channels, size_t startIndex,
                           std::function<void()> onClose)
    : onCloseCallback(onClose), channelList(channels), currentChannelIndex(startIndex) {
    for (auto &media : channelList)
        if (media.contentType.empty() && !tsvitch::isLiveMedia(media))
            media.contentType = media.url.find("/series/") != std::string::npos ? "series" : "movie";
    this->liveData = channelList[currentChannelIndex];
    brls::Logger::debug("LiveActivity: create: {}", liveData.title);
    ShaderHelper::instance().clearShader(false);
}

void LiveActivity::onContentAvailable() {
    brls::Logger::debug("LiveActivity: onContentAvailable");

    installTrackRecovery();

    // Ottieni i riferimenti agli elementi UI
    video = dynamic_cast<VideoView *>(this->getView("video"));

    MPVCore::instance().setAspect(
        ProgramConfig::instance().getSettingItem(SettingItem::PLAYER_ASPECT, std::string{"-1"}));

    this->video->registerAction("", brls::BUTTON_B, [this](...) {
        if (this->video->isOSDLock()) {
            this->video->toggleOSD();
        } else {
            if (this->video->getTvControlMode() && this->video->isOSDShown()) {
                this->video->toggleOSD();
                return true;
            }
            brls::Logger::debug("exit live");
            brls::Application::popActivity();
        }
        return true;
    });

    this->video->registerAction(brls::getStr("iptv/player_options"), brls::BUTTON_X, [this](brls::View *) {
        if (!video->isOSDLock()) {
            video->hideOSD();
            brls::Application::pushActivity(new brls::Activity(new PlayerSetting));
        }
        return true;
    });
    this->video->registerAction(brls::getStr("iptv/player_info"), brls::BUTTON_RT, [this](brls::View *) {
        if (!video->isOSDLock())
            video->togglePlaybackInfo();
        return true;
    });
    this->video->registerAction(
        brls::getStr("iptv/s24"), brls::BUTTON_NAV_UP,
        [this](brls::View *) {
            if (!this->isAd && !this->video->isOSDLock())
                tsvitch::showProgrammeInfo(this->liveData);
            return true;
        },
        true);

    this->video->registerAction(brls::getStr("iptv/watch_live"), brls::BUTTON_LSB, [this](brls::View *) {
        if (liveData.contentType == "catchup")
            returnToLive();
        else
            tsvitch::showProgrammeInfo(liveData);
        return true;
    });
    // Button R go to next channel
    this->video->registerAction("hints/next_channel"_i18n, brls::BUTTON_RB, [this](...) {
        if (!this->isAd) {
            if (this->video->isOSDLock()) {
                this->video->toggleOSD();
            } else {
                if (currentChannelIndex + 1 < channelList.size()) {
                    savePlaybackPosition();
                    this->video->stop();

                    currentChannelIndex++;
                    this->liveData = channelList[currentChannelIndex];
                    this->video->setTitle(liveData.title);
                    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
                    this->getAdUrlFromServer([&](const std::string &adUrl) {
                        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                        if (!adUrl.empty()) {
                            this->startAd(adUrl);
                        } else {
                            this->startLive();
                        }
                    });
                } else {
                    // exit live
                    brls::Logger::debug("exit live");
                    brls::Application::popActivity();
                }
            }
        }
        return true;
    });
    // Button L go to previous channel
    this->video->registerAction("hints/previous_channel"_i18n, brls::BUTTON_LB, [this](...) {
        if (!this->isAd) {
            if (this->video->isOSDLock()) {
                this->video->toggleOSD();
            } else {
                if (currentChannelIndex > 0) {
                    savePlaybackPosition();
                    this->video->stop();

                    currentChannelIndex--;
                    this->liveData = channelList[currentChannelIndex];
                    this->video->setTitle(liveData.title);
                    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
                    this->getAdUrlFromServer([&](const std::string &adUrl) {
                        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                        if (!adUrl.empty()) {
                            this->startAd(adUrl);
                        } else {
                            this->startLive();
                        }
                    });
                } else {
                    // exit live
                    brls::Logger::debug("exit live");
                    brls::Application::popActivity();
                }
            }
        }
        return true;
    });

    this->video->hideSubtitleSetting();
    this->video->hideVideoRelatedSetting();
    this->video->hideBottomLineSetting();
    this->video->hideHighlightLineSetting();
    this->video->disableCloseOnEndOfFile();
    this->video->setFullscreenIcon(true);
    this->video->setTitle(liveData.title);
    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
    this->video->setStatusLabelLeft("");
    this->video->setFavoriteCallback([this](bool state) { FavoriteManager::get()->toggle(this->liveData); });

    this->getAdUrlFromServer([&](const std::string &adUrl) {
        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
        if (!adUrl.empty()) {
            this->startAd(adUrl);
        } else {
            this->startLive();
        }
    });

    GA("open_live", {
                        {"title", this->liveData.title},
                        {"url", this->liveData.url},
                        {"group", this->liveData.groupTitle},
                        {"index", std::to_string(currentChannelIndex)},
                    });
}
void LiveActivity::startAd(std::string adUrl) {
    ++playbackGeneration;
    brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
    this->isAd = true;
    this->video->setAdMode();
    this->video->showVideoProgressSlider();
    this->video->disableProgressSliderSeek(true); // Disabilita il seek durante gli annunci
    this->video->setUrl(adUrl);

    // Quando l'annuncio finisce normalmente, passa alla live
    setPlaybackEnd([this]() { this->startLive(); });

    // Se l'annuncio fallisce nel caricamento, passa comunque alla live
    // (Rimuoviamo il comportamento di chiusura dell'app per gli errori durante gli annunci)
    if (!mpvEventRegistered) {
        this->tl_event_id = MPVCore::instance().getEvent()->subscribe([this](MpvEventEnum event) {
            if (event == MpvEventEnum::MPV_FILE_ERROR && this->isAd) {
                brls::Logger::warning("LiveActivity: Ad failed to load, skipping to live content");
                this->startLive();
            }
        });
        mpvEventRegistered = true;
    }
}

void LiveActivity::startLive() {
    ++playbackGeneration;
    this->isAd = false;
    trackReloading = false;
    archiveReloading = false;
    if (tsvitch::isLiveMedia(liveData))
        tsvitch::lastLiveChannelUrl() = liveData.url;
    mediaFinished = false;
    restorePending = true;
    lastCheckpoint = tsvitch::watchNow();
    if (liveData.contentType == "movie" || liveData.contentType == "series")
        HistoryManager::get()->add(liveData);
    video->setActionAvailable(brls::BUTTON_LSB, liveData.contentType == "catchup");

    // Rileva il tipo di contenuto PRIMA di caricare l'URL
    // Questo imposta correttamente isLiveMode per eventuali errori di caricamento
    this->detectContentType();

    // Riabilita il seek quando non è più un annuncio
    this->video->disableProgressSliderSeek(false);

    this->video->setCustomToggleAction([this]() {
        if (MPVCore::instance().isStopped()) {
            this->onLiveData(this->liveData.url);
        } else if (MPVCore::instance().isPaused()) {
            MPVCore::instance().resume();
        } else {
            this->video->showOSD(false);
            MPVCore::instance().pause();
            savePlaybackPosition();
            brls::cancelDelay(toggleDelayIter);
            ASYNC_RETAIN
            toggleDelayIter = brls::delay(5000, [ASYNC_TOKEN]() {
                ASYNC_RELEASE
                if (MPVCore::instance().isPaused() && tsvitch::isLiveMedia(liveData)) {
                    MPVCore::instance().stop();
                }
            });
        }
    });

    archiveOffset = 0;
    if (liveData.contentType == "catchup") {
        const auto duration = liveData.archiveEnd - liveData.archiveStart;
        const auto saved = tsvitch::PlaybackPositionManager::getPosition(liveData.url);
        loadArchive((saved > 0 && saved < duration - 5) ? saved : 0, false);
        setPlaybackEnd([this] {
            savePlaybackPosition();
            if (currentChannelIndex + 1 < channelList.size()) {
                liveData = channelList[++currentChannelIndex];
                video->setTitle(liveData.title);
                startLive();
            } else
                returnToLive();
        });
    } else {
        this->video->setArchiveTimeline(0, 0, {});
        setPlaybackEnd([this] {
            mediaFinished = true;
            if (liveData.contentType == "movie" || liveData.contentType == "series") {
                const auto duration =
                    MPVCore::instance().duration > 0 ? MPVCore::instance().duration : liveData.durationHint;
                tsvitch::watchStore().update(liveData, duration, duration, true);
                if (tsvitch::hasNextEpisode(channelList, currentChannelIndex)) {
                    liveData = channelList[++currentChannelIndex];
                    video->setTitle(liveData.title);
                    video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
                    startLive();
                }
            }
        });
        MPVCore::instance().command_async("set", "end", "none");
        this->video->setUrl(liveData.url);
    }

    // Registra un listener per l'evento MPV_LOADED per ri-verificare il tipo con la durata effettiva
    // e per ripristinare la posizione salvata
    if (mpvEventRegistered)
        MPVCore::instance().getEvent()->unsubscribe(this->tl_event_id);
    this->tl_event_id = MPVCore::instance().getEvent()->subscribe([this](MpvEventEnum event) {
        if (event == MpvEventEnum::UPDATE_PROGRESS && tsvitch::watchNow() - lastCheckpoint >= 15) {
            savePlaybackPosition();
            lastCheckpoint = tsvitch::watchNow();
        }
        if (event == MPV_LOADED) {
            if (trackReloading) {
                auto &core = MPVCore::instance();
                trackReloading = false;
                core.command_async("set", "pause", trackPaused ? "yes" : "no");
                core.command_async("set", "speed", std::to_string(trackSpeed));
            }

            if (archiveReloading && MPVCore::instance().getString("path") == archiveUrl) {
                auto &core = MPVCore::instance();
                archiveReloading = false;
                core.command_async("set", "speed", std::to_string(archiveSpeed));
                core.command_async("set", "pause", archivePaused ? "yes" : "no");
            }

            // Ri-verifica il tipo di contenuto con la durata effettiva da MPV
            this->detectContentType();

            // Ripristina la posizione salvata solo per video on-demand (non per live stream)
            if (restorePending && liveData.contentType != "catchup" && !tsvitch::isLiveMedia(liveData)) {
                restorePending = false;
                const auto saved = tsvitch::watchStore().progress(liveData).resume();
                if (saved > 0)
                    MPVCore::instance().seek(saved);
            }
        }
    });
    mpvEventRegistered = true;
}

void LiveActivity::savePlaybackPosition() {
    if (isAd || !video || mediaFinished || archiveReloading || trackReloading ||
        tsvitch::isLiveMedia(liveData))
        return;
    const auto position = static_cast<int64_t>(video->playbackPosition());
    const auto duration = liveData.contentType == "catchup" ? liveData.archiveEnd - liveData.archiveStart
                                                            : MPVCore::instance().duration;
    if (position <= 0)
        return;
    if (liveData.contentType == "movie" || liveData.contentType == "series") {
        tsvitch::watchStore().update(liveData, position, duration);
        if (!tsvitch::watchStore().persisted && !progressWriteWarned) {
            progressWriteWarned = true;
            brls::Application::notify(brls::getStr("iptv/save_error"));
        }
    } else if (duration > 0)
        tsvitch::PlaybackPositionManager::savePosition(liveData.url, position, duration);
}
void LiveActivity::loadArchive(double seconds, bool preserve) {
    auto &core = MPVCore::instance();
    const auto plan = tsvitch::archiveSeekPlan(liveData, seconds, tsvitch::programmeNow());
    if (plan.url.empty()) {
        brls::Application::notify(brls::getStr("iptv/archive_unsupported"));
        return;
    }
    if (preserve && !archiveReloading) {
        archiveAudio = core.getString("aid");
        archiveSubtitle = core.getString("sid");
        archivePaused = core.getFlag("pause");
        archiveSpeed = core.getDouble("speed");
    }
    if (!preserve) {
        archiveAudio.clear();
        archiveSubtitle.clear();
        archivePaused = !MPVCore::AUTO_PLAY;
        archiveSpeed = 1;
    }
    archiveReloading = true;
    archiveUrl = plan.url;
    archiveTarget = plan.target;
    archiveOffset = plan.offset;
    core.playback_time = plan.start;
    video->setArchiveTimeline(liveData.archiveEnd - liveData.archiveStart, archiveOffset,
                              [this](double target) { seekArchive(target); });
    video->showLoading();
    // Per-file bounds refer to the stream's own zero; the OSD retains the whole programme timeline.
    core.preparePosition(plan.start, archivePaused, archiveSpeed);
    auto options = "start=" + std::to_string(plan.start) + ",end=" + std::to_string(plan.end);
    if (!archiveAudio.empty())
        options += ",aid=" + archiveAudio;
    if (!archiveSubtitle.empty())
        options += ",sid=" + archiveSubtitle;
    core.setUrl(plan.url, options);
}
void LiveActivity::seekArchive(double seconds) {
    if (liveData.contentType != "catchup")
        return;
    seconds = std::clamp(seconds, 0.0, double(liveData.archiveEnd - liveData.archiveStart) - .1);
    auto &core = MPVCore::instance();
    const double local = seconds - archiveOffset;
    if (!archiveReloading && local >= 0 && core.cachedSeek(local)) {
        core.command_async("seek", local, "absolute+exact");
        return;
    }
    loadArchive(seconds, true);
}
void LiveActivity::returnToLive() {
    if (liveData.liveUrl.empty())
        return;
    savePlaybackPosition();
    video->stop();
    liveData.url = liveData.liveUrl;
    liveData.title = liveData.liveTitle;
    liveData.liveUrl.clear();
    liveData.contentType = "live";
    liveData.archiveStart = liveData.archiveEnd = 0;
    video->setTitle(liveData.title);
    startLive();
}

void LiveActivity::detectContentType() {
    // Prima fase: analisi basata su URL e titolo (disponibile sempre)
    std::string url = liveData.url;
    std::string title = liveData.title;

    // Converti in lowercase per il confronto
    std::transform(url.begin(), url.end(), url.begin(), ::tolower);
    std::transform(title.begin(), title.end(), title.begin(), ::tolower);

    // Determina il tipo basandosi su URL e titolo
    bool isLiveStream = true; // Default: assume live stream

    // Indicatori di video on-demand negli URL
    if (url.find(".mp4") != std::string::npos || url.find(".mkv") != std::string::npos ||
        url.find(".avi") != std::string::npos || url.find("video") != std::string::npos) {
        isLiveStream = false;
    }
    // Indicatori di live stream negli URL e titoli
    else if (url.find("live") != std::string::npos || url.find("stream") != std::string::npos ||
             url.find(".m3u8") != std::string::npos || url.find(".ts") != std::string::npos ||
             title.find("live") != std::string::npos || title.find("diretta") != std::string::npos) {
        isLiveStream = true;
    }

    // Seconda fase: verifica con durata MPV (se disponibile)
    double duration = MPVCore::instance().duration;
    brls::Logger::debug("LiveActivity: detectContentType - duration: {}", duration);

    // Se MPV è già caricato, usa la durata per verificare/correggere la detection
    if (duration > 0) {
        // Se ha durata definita, è sicuramente un video
        isLiveStream = false;
        brls::Logger::debug("LiveActivity: MPV duration {} confirms VIDEO mode", duration);
    } else if (duration == 0 && MPVCore::instance().isPlaying()) {
        // Se sta riproducendo ma durata è 0, conferma che è live
        isLiveStream = true;
        brls::Logger::debug("LiveActivity: MPV duration 0 confirms LIVE mode");
    }
    // Altrimenti mantiene la detection basata su URL/titolo fatta prima

    brls::Logger::debug("LiveActivity: Content detected as: {}",
                        isLiveStream ? "LIVE STREAM" : "VIDEO WITH DURATION");

    // Xtream URLs provide an authoritative type, even before MPV loads the new file.
    if (url.find("/movie/") != std::string::npos || url.find("/series/") != std::string::npos)
        isLiveStream = false;
    else if (url.find("/live/") != std::string::npos)
        isLiveStream = true;

    if (liveData.contentType == "catchup" || liveData.contentType == "movie" ||
        liveData.contentType == "series")
        isLiveStream = false;
    else if (liveData.contentType == "live")
        isLiveStream = true;

    // Configura l'interfaccia in base al tipo di contenuto
    if (isLiveStream) {
        this->video->setLiveMode();
        this->video->hideVideoProgressSlider();
        brls::Logger::debug("LiveActivity: Configured for live stream mode");
    } else {
        this->video->setVideoMode();
        this->video->showVideoProgressSlider();
        brls::Logger::debug("LiveActivity: Configured for video mode with progress bar");
    }
}

void LiveActivity::startDownload() {
    if (this->isAd) {
        brls::Logger::debug("LiveActivity: Cannot download ads");
        return;
    }

    if (hasActiveDownload) {
        brls::Logger::debug("LiveActivity: Download already in progress");
        return;
    }

    // Controlla se è una live stream in corso
    if (tsvitch::isLiveMedia(liveData)) {
        brls::Logger::warning("LiveActivity: Cannot download live streams");
        tsvitch::showLiveStreamDownloadError();
        return;
    }

    // Debug: stampa i dati del download
    brls::Logger::debug("LiveActivity: Starting download for title='{}', url='{}'", this->liveData.title,
                        this->liveData.url);

    // Verifica che l'URL sia valido
    if (this->liveData.url.empty()) {
        brls::Logger::error("LiveActivity: Cannot download - URL is empty");
        return;
    }

    // Mostra l'overlay del progresso globale
    tsvitch::DownloadProgressManager::getInstance()->showDownloadProgress(
        "live_" + this->liveData.title, this->liveData.title, this->liveData.url);

    // Avvia il download del video corrente con callback
    currentDownloadId = DownloadManager::instance().startDownload(
        this->liveData.title, this->liveData.url,
        this->liveData.logo, // Usa il logo del canale come immagine
        [this](const std::string &downloadId, float progress, size_t downloaded, size_t total) {
            // Callback di progresso - aggiorna l'overlay globale
            if (!hasActiveDownload) {
                return; // Download annullato o completato
            }

            try {
                std::string progressText;
                std::string statusText = "Download in corso...";

                if (total > 0) {
                    std::string downloadedStr = formatFileSize(downloaded);
                    std::string totalStr = formatFileSize(total);
                    progressText = fmt::format("{:.1f}% ({}/{})", progress, downloadedStr, totalStr);
                } else {
                    progressText = fmt::format("{:.1f}%", progress);
                }

                // Aggiorna l'overlay globale
                tsvitch::DownloadProgressManager::getInstance()->updateProgress(
                    "live_" + this->liveData.title, progress, statusText, progressText);

            } catch (const std::exception &e) {
                brls::Logger::error("Error in download progress callback: {}", e.what());
                hasActiveDownload = false;
            }
        },
        [this](const std::string &downloadId, const std::string &filePath) {
            // Callback di completamento
            brls::Logger::info("Download completed: {}", downloadId);

            hasActiveDownload = false;

            try {
                // Aggiorna l'overlay globale con stato di completamento
                tsvitch::DownloadProgressManager::getInstance()->updateProgress(
                    "live_" + this->liveData.title, 100.0f, "Download completato!", "100%");

                // Nascondi l'overlay dopo 2 secondi
                brls::delay(2000, [this]() {
                    tsvitch::DownloadProgressManager::getInstance()->hideDownloadProgress(
                        "live_" + this->liveData.title);
                });

                // Mostra notifica di successo
                std::string message = fmt::format("Download di \"{}\" completato", this->liveData.title);
                brls::sync([message]() {
                    brls::Dialog *dialog = new brls::Dialog(message);
                    dialog->addButton(brls::getStr("iptv/s10"), []() {});
                    dialog->open();
                });
            } catch (const std::exception &e) {
                brls::Logger::error("Error in download completion callback: {}", e.what());
            }
        },
        [this](const std::string &downloadId, const std::string &error) {
            // Callback di errore
            brls::Logger::error("Download failed: {} - {}", downloadId, error);

            hasActiveDownload = false;

            try {
                // Aggiorna l'overlay globale con stato di errore
                tsvitch::DownloadProgressManager::getInstance()->updateProgress(
                    "live_" + this->liveData.title, 0.0f, "Errore nel download", "Fallito");

                // Nascondi l'overlay dopo 3 secondi
                brls::delay(3000, [this]() {
                    tsvitch::DownloadProgressManager::getInstance()->hideDownloadProgress(
                        "live_" + this->liveData.title);
                });

                // Mostra notifica di errore
                std::string message =
                    fmt::format("Errore nel download di \"{}\": {}", this->liveData.title, error);
                brls::sync([message]() {
                    brls::Dialog *dialog = new brls::Dialog(message);
                    dialog->addButton(brls::getStr("iptv/s10"), []() {});
                    dialog->open();
                });
            } catch (const std::exception &e) {
                brls::Logger::error("Error in download error callback: {}", e.what());
            }
        });

    if (!currentDownloadId.empty()) {
        hasActiveDownload = true;
        brls::Logger::info("LiveActivity: Started download {} for {}", currentDownloadId,
                           this->liveData.title);
    } else {
        // Download fallito immediatamente
        hasActiveDownload = false;

        brls::Dialog *dialog = new brls::Dialog("Impossibile avviare il download");
        dialog->addButton(brls::getStr("iptv/s10"), []() {});
        dialog->open();
    }
}

void LiveActivity::onLiveData(std::string url) {
    brls::Logger::debug("Live stream url: {}", url);
    this->getAdUrlFromServer([&](const std::string &adUrl) {
        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
        if (!adUrl.empty()) {
            this->video->setUrl(adUrl);
        } else {
            this->startLive();
        }
    });
    return;
}

void LiveActivity::onError(const std::string &error) {
    brls::Logger::error("ERROR request live data: {}", error);
    this->video->showOSD(false);
    this->retryRequestData();
}

void LiveActivity::retryRequestData() {
    brls::cancelDelay(errorDelayIter);
    errorDelayIter = brls::delay(2000, [this]() {
        if (!MPVCore::instance().isPlaying())
            static_cast<LiveDataRequest *>(this)->requestData(liveData.url);
    });
}

void LiveActivity::getAdUrlFromServer(std::function<void(const std::string &)> callback) {
    if (std::string(SERVER_URL_VALUE).empty()) {
        callback("");
        return;
    }
    CLIENT::get_ad(
        [callback](const std::string &adUrl, int statusCode) {
            if (statusCode == 200 && !adUrl.empty()) {
                brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                if (callback)
                    callback(adUrl);
            } else {
                brls::Logger::error("LiveActivity: Failed to get ad URL, status code: {}", statusCode);
                if (callback)
                    callback("");
            }
        },
        [callback](const std::string &error, int statusCode) {
            brls::Logger::error("LiveActivity: Error getting ad URL: {}, status code: {}", error, statusCode);
            if (callback)
                callback("");
        });
}

std::string LiveActivity::formatFileSize(size_t bytes) {
    const char *suffixes[] = {"B", "KB", "MB", "GB"};
    int suffixIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024 && suffixIndex < 3) {
        size /= 1024;
        suffixIndex++;
    }

    if (suffixIndex == 0) {
        return fmt::format("{} {}", static_cast<int>(size), suffixes[suffixIndex]);
    } else {
        return fmt::format("{:.1f} {}", size, suffixes[suffixIndex]);
    }
}

LiveActivity::~LiveActivity() {
    *playerAlive = false;
    auto &core = MPVCore::instance();
    if (core.trackChangeOwner == this) {
        core.onTrackChange = {};
        core.trackChangeOwner = nullptr;
    }
    brls::Logger::debug("LiveActivity: delete");

    // Salva la posizione di riproduzione prima di uscire (solo per video on-demand)
    savePlaybackPosition();

    // Cancella download attivo se presente
    if (hasActiveDownload && !currentDownloadId.empty()) {
        brls::Logger::debug("LiveActivity: Cancelling active download {}", currentDownloadId);
        DownloadManager::instance().cancelDownload(currentDownloadId);
        hasActiveDownload = false;
        currentDownloadId.clear();
    }

    if (this->video) {
        this->video->setArchiveTimeline(0, 0, {});
        setPlaybackEnd(nullptr); // Annulla la callback per evitare crash
        this->video->stop();
    }
    brls::cancelDelay(toggleDelayIter);
    brls::cancelDelay(errorDelayIter);

    // Pulisci gli eventi in modo sicuro per evitare callback dopo la distruzione
    try {
        if (mpvEventRegistered) {
            MPVCore::instance().getEvent()->unsubscribe(this->tl_event_id);
            mpvEventRegistered = false;
        }
    } catch (const std::exception &e) {
        brls::Logger::warning("LiveActivity: Error unsubscribing MPV event: {}", e.what());
    } catch (...) {
        brls::Logger::warning("LiveActivity: Unknown error unsubscribing MPV event");
    }

    try {
        if (customEventRegistered) {
            EventHelper::instance().getCustomEvent()->unsubscribe(this->event_id);
            customEventRegistered = false;
        }
    } catch (const std::exception &e) {
        brls::Logger::warning("LiveActivity: Error unsubscribing custom event: {}", e.what());
    } catch (...) {
        brls::Logger::warning("LiveActivity: Unknown error unsubscribing custom event");
    }

    if (onCloseCallback)
        onCloseCallback();
}

void LiveActivity::onResume() { installTrackRecovery(); }
void LiveActivity::installTrackRecovery() {
    auto &core = MPVCore::instance();
    core.trackChangeOwner = this;
    core.onTrackChange = [this] {
        if (isAd || !video)
            return;
        if (liveData.contentType == "catchup") {
            loadArchive(video->playbackPosition(), true);
            return;
        }
        savePlaybackPosition();
        auto &core = MPVCore::instance();
        trackAudio = core.getString("aid");
        trackSubtitle = core.getString("sid");
        trackPaused = core.getFlag("pause");
        trackSpeed = core.getDouble("speed");
        trackReloading = true;
        video->showLoading();
        const double position = core.getDouble("time-pos");
        core.preparePosition(tsvitch::isLiveMedia(liveData) ? 0 : position, trackPaused, trackSpeed);
        std::string options = "aid=" + trackAudio + ",sid=" + trackSubtitle;
        if (!tsvitch::isLiveMedia(liveData))
            options += ",start=" + std::to_string(position);
        core.setUrl(liveData.url, options);
    };
}

void LiveActivity::setPlaybackEnd(std::function<void()> action) {
    if (!action) {
        video->setOnEndCallback({});
        return;
    }
    const auto alive = playerAlive;
    const auto generation = playbackGeneration;
    video->setOnEndCallback([this, alive, generation, action] {
        if (!*alive || generation != playbackGeneration || MPVCore::instance().isPositioning())
            return;
        brls::sync([this, alive, generation, action] {
            if (*alive && generation == playbackGeneration)
                action();
        });
    });
}

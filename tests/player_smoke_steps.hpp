#pragma once
#include "view/mpv_core.hpp"
#include "view/nx_player_controls.hpp"
#include "view/grid_dropdown.hpp"
#include "view/video_view.hpp"
#include "view/video_profile.hpp"
#include "activity/live_player_activity.hpp"
#include "fragment/player_setting.hpp"
#include "utils/programme.hpp"
#include "utils/catchup.hpp"
inline bool playerSmokeStep(int step) {
    static int traced = -1;
    static auto stageStart = std::chrono::steady_clock::now();
    if (step != traced) {
        traced = step;
        stageStart = std::chrono::steady_clock::now();
        brls::Logger::info("PLAYER_STEP {}", step);
        std::fflush(nullptr);
    }
    if (!uiSmokeCapture(step))
        return false;
    auto current = []() { return brls::Application::getActivitiesStack().back(); };
    auto activate = [](brls::View *view, brls::ControllerButton button = brls::BUTTON_A) {
        if (!view)
            throw std::runtime_error("Player test: missing view");
        for (auto action : view->getActions())
            if (action->getButton() == button && action->isAvailable()) {
                auto cb = action->getActionListener();
                cb(view);
                return;
            }
        throw std::runtime_error("Player test: missing action");
    };
    auto &core = MPVCore::instance();
    static auto traceAt = std::chrono::steady_clock::now();
    if (std::chrono::steady_clock::now() - traceAt > std::chrono::seconds(5)) {
        traceAt = std::chrono::steady_clock::now();
        brls::Logger::info("PLAYER_WAIT {} pos={} sid={} text={}", step, core.getDouble("time-pos"),
                           core.getString("sid"), core.getString("sub-text"));
        brls::Logger::info("SUB_STATE vis={} codec={} start={} end={} lang={}",
                           core.getFlag("sub-visibility"), core.getString("sub-codec"),
                           core.getDouble("sub-start"), core.getDouble("sub-end"),
                           core.getString("current-tracks/sub/lang"));
        std::fflush(nullptr);
    }
    auto video = [&]() { return dynamic_cast<VideoView *>(current()->getView("video")); };
    auto heading = [&](const char *id) {
        auto *label = video() ? dynamic_cast<brls::Label *>(video()->getView(id)) : nullptr;
        if (!label)
            throw std::runtime_error("Player title label missing");
        return label->getFullText();
    };
    auto selected = [&](const std::string &lang) {
        for (const auto &t : core.tracks())
            if (t.type == "sub" && t.language == lang && t.selected)
                return true;
        return false;
    };
    if (step >= 15 && step <= 17) {
        if (std::chrono::steady_clock::now() - stageStart > std::chrono::seconds(12))
            throw std::runtime_error("Player test: catch-up seek did not settle promptly");
        auto *live = dynamic_cast<LiveActivity *>(current());
        if (core.isPositioning() || (live && live->archiveReloading))
            return false;
    }
    static double movieTarget = 0, movieSaved = 0;
    auto openMovie = [] {
        tsvitch::LiveM3u8 movie;
        movie.contentType = "movie";
        movie.title = "NX controls fixture";
        movie.url = ProgramConfig::instance().getXtreamServerUrl() + "/movie/fixture/fixture/1.mkv";
        Intent::openLive({movie}, 0, [] {});
    };
    switch (step) {
    case 0: {
        auto *grid = dynamic_cast<RecyclingGrid *>(current()->getView("home/live/recyclingGrid"));
        if (!grid || grid->getItemCount() < 40)
            return false;
        grid->setDefaultCellFocus(31);
        grid->reloadData();
        grid->selectRowAt(31, false);
        grid->getDataSource()->onItemSelected(grid, 31);
        return true;
    }
    case 1: {
        if (!video() || core.isPositioning() || core.tracks().size() < 5 || core.getDouble("time-pos") <= 0)
            return false;
        if (heading("nx/player/title-detail").find("השידור הנכון לפי הספק [32]") == std::string::npos)
            return false;
        if (heading("nx/player/title-text") != "Channel 31" || !video()->nxPlayer()->shown())
            throw std::runtime_error("Live header or automatic title reveal missing");
        activate(video(), brls::BUTTON_X);
        return true;
    }
    case 2: {
        auto *row = current()->getView("player/sub");
        if (!row)
            return false;
        auto *title = dynamic_cast<brls::Label *>(static_cast<brls::Box *>(row)->getChildren()[0]);
        if (!title || title->getFullText() != brls::getStr("iptv/s12") || title->getWidth() < 180)
            throw std::runtime_error("Player test: subtitle title is clipped");
        activate(row);
        return true;
    }
    case 3: {
        auto *grid = dynamic_cast<RecyclingGrid *>(current()->getView("brls/dropdown/recycler"));
        if (!grid) {
            auto *drop = dynamic_cast<BaseDropdown *>(current()->getContentView());
            if (drop)
                grid = drop->getRecyclingList();
        }
        if (!grid)
            throw std::runtime_error("Player test: subtitle choices missing");
        size_t index = 2;
        for (const auto &t : core.tracks())
            if (t.type == "sub") {
                if (t.language == "heb") {
                    grid->getDataSource()->onItemSelected(grid, index);
                    return true;
                }
                ++index;
            }
        throw std::runtime_error("Player test: embedded Hebrew subtitle not offered");
    }
    case 4:
        if (!selected("heb") || !core.getFlag("sub-visibility") ||
            core.getString("sub-text").find("שלום") == std::string::npos)
            return false;
        activate(current()->getView("player/sub-background"));
        return true;
    case 5: {
        auto *options = dynamic_cast<PlayerSetting *>(current()->getContentView());
        if (!options)
            throw std::runtime_error("Player test: settings not restored after selection");
        activate(options, brls::BUTTON_RB);
        if (std::abs(core.getDouble("sub-delay") - .1) > .02)
            throw std::runtime_error("Player test: subtitle delay failed");
        activate(current()->getView("player/audio"));
        return true;
    }
    case 6: {
        auto *drop = dynamic_cast<BaseDropdown *>(current()->getContentView());
        if (!drop)
            return false;
        auto *grid = drop->getRecyclingList();
        size_t index = 2;
        for (const auto &t : core.tracks())
            if (t.type == "audio") {
                if (t.language == "heb") {
                    grid->getDataSource()->onItemSelected(grid, index);
                    return true;
                }
                ++index;
            }
        throw std::runtime_error("Player test: Hebrew audio unavailable");
    }
    case 7:
        if (core.isPositioning() || core.getString("current-tracks/audio/lang") != "heb")
            return false;
        brls::Application::popActivity();
        return true;
    case 8:
        if (!video())
            return false;
        activate(video(), brls::BUTTON_RT);
        return true;
    case 9:
        if (core.statsNumber("video-params/w") != 320 || core.statsText("video-codec").empty())
            throw std::runtime_error("Async playback diagnostics missing");
        activate(video(), brls::BUTTON_RT);
        activate(video(), brls::BUTTON_LT);
        return true;
    case 10: {
        auto *grid = dynamic_cast<RecyclingGrid *>(current()->getView("programme/grid"));
        if (!grid)
            return false;
        if (grid->getDefaultCellFocus() != 1)
            throw std::runtime_error("Player test: provider current programme not focused");
        auto *row = grid->getGridItemByIndex(1);
        auto *label = row ? dynamic_cast<brls::Label *>(row->getView("programme/status")) : nullptr;
        if (!label || label->getFullText().find(brls::getStr("iptv/on_now")) == std::string::npos)
            throw std::runtime_error("Player test: current programme badge wrong");
        brls::Application::popActivity();
        return true;
    }
    case 11:
        activate(video(), brls::BUTTON_RB);
        return true;
    case 12:
        if (core.getString("path").find("/33.ts") == std::string::npos || core.getDouble("time-pos") <= 0)
            return false;
        if (heading("nx/player/title-text") != "Channel 32" ||
            heading("nx/player/title-detail") != brls::getStr("iptv/epg_unavailable"))
            throw std::runtime_error("Channel change retained the previous programme title");
        brls::Application::popActivity();
        return true;
    case 13: {
        auto *grid = dynamic_cast<RecyclingGrid *>(current()->getView("home/live/recyclingGrid"));
        if (!grid)
            return false;
        if (grid->getDefaultCellFocus() != 32 || !grid->getGridItemByIndex(32))
            throw std::runtime_error("Player test: channel scroll/focus not restored");
        tsvitch::LiveM3u8 channel;
        channel.title = "Catch-up fixture";
        channel.id = "50";
        channel.url = ProgramConfig::instance().getXtreamServerUrl() + "/live/fixture/fixture/50.ts";
        channel.contentType = "live";
        channel.catchupMode = "xc";
        channel.catchupDays = 7;
        const auto start = std::stoll(std::getenv("TSVITCH_ARCHIVE_START"));
        auto media = tsvitch::archiveMedia(channel, "Recording", "", start, start + 180, std::time(nullptr));
        Intent::openLive({media}, 0, [] {});
        return true;
    }
    case 14:
        if (!video() || core.getDouble("time-pos") < 1)
            return false;
        if (!video()->nxPlayer() || video()->nxPlayer()->isLive())
            throw std::runtime_error("Recording treated as live");
        activate(video(), brls::BUTTON_RB);
        static_cast<LiveActivity *>(current())->seekArchive(72.5);
        return true;
    case 15:
        if (!video() || std::abs(video()->playbackPosition() - 73.5) > 3)
            return false;
        if (std::abs(core.getSpeed() - 1.25) > .01)
            throw std::runtime_error("Recording R did not change speed");
        core.setProperty("pause", "yes");
        core.setProperty("speed", "1.25");
        static_cast<LiveActivity *>(current())->seekArchive(14.5);
        return true;
    case 16:
        if (!video() || std::abs(video()->playbackPosition() - 14.5) > 1.5 || !core.getFlag("pause"))
            return false;
        if (std::abs(core.getDouble("speed") - 1.25) > .01)
            throw std::runtime_error("Player test: catch-up speed lost");
        static_cast<LiveActivity *>(current())->seekArchive(125);
        static_cast<LiveActivity *>(current())->seekArchive(32);
        static_cast<LiveActivity *>(current())->seekArchive(90);
        return true;
    case 17:
        if (!video() || std::abs(video()->playbackPosition() - 90) > 1.5 || !core.getFlag("pause"))
            return false;
        brls::Application::popActivity();
        return true;
    case 18: {
        tsvitch::LiveM3u8 first;
        first.title = "ההתחלה";
        first.contentType = "series";
        first.seriesTitle = "סדרת בדיקה";
        first.season = "1";
        first.episode = "1";
        first.url = ProgramConfig::instance().getXtreamServerUrl() + "/series/fixture/fixture/1.mkv";
        auto second = first;
        second.title = "ההמשך";
        second.episode = "2";
        second.url = ProgramConfig::instance().getXtreamServerUrl() + "/series/fixture/fixture/2.mkv";
        Intent::openLive({first, second}, 0, [] {});
        return true;
    }
    case 19:
        if (std::chrono::steady_clock::now() - stageStart > std::chrono::seconds(12))
            throw std::runtime_error("Player test: next episode did not start automatically");
        if (!video() || core.isPositioning() ||
            core.getString("path").find("/series/fixture/fixture/2.mkv") == std::string::npos ||
            core.getDouble("time-pos") <= 0)
            return false;
        if (heading("nx/player/title-text") != "סדרת בדיקה" ||
            heading("nx/player/title-detail").find("עונה 1 · פרק 2 · ההמשך") == std::string::npos)
            throw std::runtime_error("Series title/episode header did not follow autoplay");
        brls::Application::popActivity();
        return true;
    case 20:
        openMovie();
        return true;
    case 21:
        if (!video() || core.isPositioning() || core.playback_time <= 0)
            return false;
        if (video()->nxPlayer()->isLive())
            throw std::runtime_error("Movie treated as live");
        if (heading("nx/player/title-text") != "NX controls fixture" ||
            !heading("nx/player/title-detail").empty())
            throw std::runtime_error("Movie inherited live or episode metadata");
        video()->nxPlayer()->show(true);
        activate(video());
        return true;
    case 22: {
        if (!core.isPaused())
            return false;
        auto *activity = current();
        auto *controls = video()->nxPlayer();
        activate(video(), brls::BUTTON_Y);
        if (!controls->locked())
            throw std::runtime_error("Y did not lock");
        for (auto button :
             {brls::BUTTON_B, brls::BUTTON_X, brls::BUTTON_RB, brls::BUTTON_LT, brls::BUTTON_RIGHT})
            activate(video(), button);
        if (current() != activity || controls->scrubbing() || !controls->locked())
            throw std::runtime_error("Locked input leaked");
        return true;
    }
    case 23: {
        if (core.isPaused())
            return false;
        activate(video(), brls::BUTTON_Y);
        auto *controls = video()->nxPlayer();
        if (controls->locked())
            throw std::runtime_error("Y did not unlock");
        movieSaved = core.playback_time;
        activate(video(), brls::BUTTON_RIGHT);
        if (!controls->scrubbing() || controls->scrubTarget() < movieSaved + 4)
            throw std::runtime_error("D-pad did not scrub");
        activate(video(), brls::BUTTON_B); // cancel, do not close or seek
        return true;
    }
    case 24: {
        if (!video() || core.isPaused() || video()->nxPlayer()->scrubbing() ||
            core.playback_time > movieSaved + 3)
            throw std::runtime_error("B did not cancel scrub and restore playback");
        activate(video(), brls::BUTTON_RIGHT);
        movieTarget = video()->nxPlayer()->scrubTarget();
        activate(video()); // A commits once, resumes
        return true;
    }
    case 25:
        if (core.isPositioning() || std::abs(core.playback_time - movieTarget) > 2)
            return false;
        if (core.isPaused() || video()->nxPlayer()->scrubbing())
            throw std::runtime_error("A did not commit/resume");
        activate(video(), brls::BUTTON_RB);
        activate(video(), brls::BUTTON_RB);
        activate(video(), brls::BUTTON_LB);
        activate(video(), brls::BUTTON_NAV_UP);
        if (!video())
            throw std::runtime_error("Up unexpectedly opened programme details");
        activate(video(), brls::BUTTON_X);
        return true;
    case 26: {
        if (core.isPositioning() || std::abs(core.getSpeed() - 1.25) > .01)
            return false;
        if (!current()->getView("player/speed"))
            throw std::runtime_error("Movie speed options missing");
        // The settings panel has focus: neither stick axis may scrub underneath.
        if (video())
            throw std::runtime_error("Player settings did not take focus");
        brls::Application::popActivity();
        return true;
    }
    case 27: {
        auto *controls = video()->nxPlayer();
        if (controls->scrubbing())
            throw std::runtime_error("Settings caused a seek");
        const double before = core.playback_time;
        controls->scrubAxis(.3, .1);
        if (!controls->scrubbing() || controls->scrubTarget() <= before ||
            controls->scrubTarget() > before + 1)
            throw std::runtime_error("Fine stick scrub is not proportional");
        controls->cancelSeek();
        movieSaved = core.playback_time;
        brls::Application::popActivity();
        return true;
    }
    case 28:
        openMovie();
        return true;
    case 29:
        if (!video() || core.isPositioning() || core.playback_time < movieSaved - 1)
            return false;
        if (core.playback_time > movieSaved + 4)
            throw std::runtime_error("VOD resume position drifted");
        activate(video(), brls::BUTTON_X);
        return true;
    case 30:
        activate(current()->getView("player/audio-boost"));
        brls::Application::popActivity();
        return true;
    case 31:
        if (core.getString("af").find("nx-boost") == std::string::npos)
            return false;
        brls::Application::popActivity();
        return true;
    case 32: {
        tsvitch::LiveM3u8ListResult episodes;
        for (int i = 11; i <= 13; ++i) {
            tsvitch::LiveM3u8 episode;
            episode.contentType = "series";
            episode.seriesTitle = "Manual next fixture";
            episode.season = "1";
            episode.episode = std::to_string(i);
            episode.title = "Episode " + episode.episode;
            episode.url = ProgramConfig::instance().getXtreamServerUrl() + "/series/fixture/fixture/" +
                          episode.episode + ".mkv";
            episodes.push_back(episode);
        }
        Intent::openLive(episodes, 0, [] {});
        return true;
    }
    case 33:
        if (!video() || core.isPositioning() || !video()->nxPlayer()->nextShown())
            return false;
        activate(video());
        return true;
    case 34:
        if (core.filepath.find("/13.mkv") != std::string::npos)
            throw std::runtime_error("Next card advanced twice");
        if (core.isPositioning() || core.filepath.find("/12.mkv") == std::string::npos ||
            core.playback_time <= 0)
            return false;
        for (const auto &action : video()->getActions())
            if (action->getButton() == brls::BUTTON_RSB)
                throw std::runtime_error("Right stick click is bound");
        brls::Application::popActivity();
        return true;
    default:
        brls::Logger::info("PLAYER_SMOKE_PASSED");
        brls::Application::quit();
        return true;
    }
}

#pragma once
#include "view/mpv_core.hpp"
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
        activate(video(), brls::BUTTON_RT);
        activate(video(), brls::BUTTON_NAV_UP);
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
        static_cast<LiveActivity *>(current())->seekArchive(72.5);
        return true;
    case 15:
        if (!video() || std::abs(video()->playbackPosition() - 73.5) > 3)
            return false;
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
        first.title = "Autoplay fixture · episode 1";
        first.contentType = "series";
        first.seriesTitle = "Autoplay fixture";
        first.season = "1";
        first.episode = "1";
        first.url = ProgramConfig::instance().getXtreamServerUrl() + "/series/fixture/fixture/1.mkv";
        auto second = first;
        second.title = "Autoplay fixture · episode 2";
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
        brls::Application::popActivity();
        return true;
    default:
        brls::Logger::info("PLAYER_SMOKE_PASSED");
        brls::Application::quit();
        return true;
    }
}

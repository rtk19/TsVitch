#pragma once
#include "utils/library_state.hpp"
#include "utils/search_text.hpp"
#include <borealis/views/image.hpp>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <borealis/views/label.hpp>
inline bool uiSmokeCapture(int step) {
    const char *folder = std::getenv("TSVITCH_UI_CAPTURE");
    if (!folder)
        return true;
    static int captured = -1;
    static std::chrono::steady_clock::time_point ready;
    if (captured != step) {
        captured = step;
        ready = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
        std::ofstream(std::filesystem::path(folder) / "stage") << step;
    }
    return std::chrono::steady_clock::now() >= ready;
}
// Desktop-only CLI fixture driver: invokes the same grid actions as controller input.
inline bool uiSmokeStep(int step, bool cacheOnly) {
    if (!cacheOnly && !uiSmokeCapture(step))
        return false;
    auto current = []() { return brls::Application::getActivitiesStack().back(); };
    auto grid = [&]() { return dynamic_cast<RecyclingGrid *>(current()->getView("vod/grid")); };
    auto select = [&](size_t index) {
        auto *g = grid();
        if (!g || g->getItemCount() <= index)
            return false;
        g->getDefaultFocus();
        g->getDataSource()->onItemSelected(g, index);
        return true;
    };
    auto toggle = [&](RecyclingGrid *g, size_t index) {
        g->getDefaultFocus();
        g->setDefaultCellFocus(index);
        g->selectRowAt(index, false);
        brls::Application::giveFocus(g->getDefaultFocus());
        auto *cell = g->getGridItemByIndex(index);
        if (!cell)
            return false;
        for (auto action : cell->getActions())
            if (action->getButton() == brls::BUTTON_Y && action->isAvailable()) {
                auto callback = action->getActionListener();
                callback(cell);
                return true;
            }
        throw std::runtime_error("Smoke: Y action missing");
    };
    auto pop = []() { brls::Application::popActivity(); };
    const auto scope = tsvitch::librarySourceKey();
    auto &preferences = tsvitch::libraryPreferences();
    if (step == 0) {
        if (brls::Application::getLocale() !=
            (ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE) == 1 ? "he" : "en-US"))
            throw std::runtime_error("Smoke: configured language was not applied");
        if (cacheOnly &&
            (!preferences.pinned(scope, "Group 1") || preferences.favorites(scope, true).size() != 1))
            throw std::runtime_error("Smoke: preferences did not survive restart");
        if (!preferences.pinned(scope, "Group 1")) {
            auto *groups = dynamic_cast<RecyclingGrid *>(current()->getView("dynamic/up/recyclingGrid"));
            if (!groups || groups->getItemCount() < 2)
                return false;
            if (!toggle(groups, 1))
                return false;
            if (!preferences.pinned(scope, "Group 1"))
                throw std::runtime_error("Smoke: category pin failed");
            auto *first = groups->getGridItemByIndex(0);
            auto *label = first ? dynamic_cast<brls::Label *>(first->getView("title")) : nullptr;
            if (!label || label->getFullText().find("Group 1") == std::string::npos)
                throw std::runtime_error("Smoke: pinned category was not moved to top");
        }
        auto *channels = dynamic_cast<RecyclingGrid *>(current()->getView("home/live/recyclingGrid"));
        auto *card = channels ? channels->getGridItemByIndex(0) : nullptr;
        auto *programme =
            card ? dynamic_cast<brls::Label *>(card->getView("video/card/label/programme")) : nullptr;
        if (!programme || programme->getFullText() != "חדשות הערב")
            return false;
        auto *logo = dynamic_cast<brls::Image *>(card->getView("video/card/picture"));
        if (!logo || logo->getTexture() <= 0)
            return false;
        auto *tabs = dynamic_cast<AutoTabFrame *>(current()->getView("main/tabFrame"));
        if (!tabs)
            throw std::runtime_error("Smoke: sidebar missing");
        tabs->focusTab(4);
        return true;
    }
    if (cacheOnly) {
        if (step == 1)
            return select(0);
        if (step == 2) {
            if (!grid() || grid()->getItemCount() < 5)
                return false;
            pop();
            return true;
        }
        if (step == 3)
            return select(1);
        if (step == 4) {
            if (!grid() || grid()->getItemCount() < 5)
                return false;
            select(4);
            return true;
        }
        if (step == 5)
            return select(0);
        if (step == 6) {
            if (!grid() || grid()->getItemCount() < 2)
                return false;
            brls::Logger::info("UI_SMOKE_PASSED");
            brls::Application::quit();
            return true;
        }
    }
    switch (step) {
    case 1:
        return select(0); // Movies.
    case 2:
        return select(4); // Provider category.
    case 3:
        if (!grid() || grid()->getItemCount() < 2)
            return false;
        return toggle(grid(), 0);
    case 4:
        if (preferences.favorites(scope, false).size() != 1)
            throw std::runtime_error("Smoke: movie favorite missing");
        pop();
        return true;
    case 5:
        return select(0); // Favorites shelf.
    case 6:
        if (!grid() || grid()->getItemCount() < 1)
            return false;
        return toggle(grid(), 0);
    case 7:
        if (!preferences.favorites(scope, false).empty())
            throw std::runtime_error("Smoke: movie favorite removal failed");
        pop();
        return true;
    case 8:
        pop();
        return true;
    case 9:
        return select(0); // Reopen movies: memory cache.
    case 10:
        if (!grid() || grid()->getItemCount() < 5)
            return false;
        // Explicit refresh followed by Back must update the cache safely in the background.
        if (auto *page = dynamic_cast<VodGridPage *>(grid()->getParent()))
            page->reload();
        else
            throw std::runtime_error("Smoke: refresh page missing");
        pop();
        return true;
    case 11:
        return select(1); // Series.
    case 12:
        return select(4); // Provider category.
    case 13:
        if (!grid() || grid()->getItemCount() < 2)
            return false;
        if (!toggle(grid(), 0))
            return false;
        return select(0);
    case 14:
        if (!grid() || grid()->getItemCount() < 2)
            return false;
        return select(1); // Season.
    case 15: {
        if (!grid())
            return false;
        if (grid()->getItemCount() != 2)
            throw std::runtime_error("Smoke: duplicate episodes were not grouped");
        auto *cell = grid()->getGridItemByIndex(0);
        if (!cell)
            return false;
        for (auto action : cell->getActions())
            if (action->getButton() == brls::BUTTON_X) {
                auto callback = action->getActionListener();
                callback(cell);
                return true;
            }
        throw std::runtime_error("Smoke: episode details missing");
    }
    case 16:
    case 17: {
        auto *button = current()->getView(step == 16 ? "brls/dialog/button2" : "brls/dialog/button3");
        if (!button)
            throw std::runtime_error("Smoke: episode source option missing");
        for (auto action : button->getActions())
            if (action->getButton() == brls::BUTTON_A) {
                auto callback = action->getActionListener();
                callback(button);
                return true;
            }
        throw std::runtime_error("Smoke: episode source option inaccessible");
    }
    case 18:
        if (!grid())
            return false;
        if (grid()->getItemCount() != 2)
            throw std::runtime_error("Smoke: alternate episode sources were lost");
        pop();
        return true;
    case 19:
        pop();
        return true;
    case 20:
        if (preferences.favorites(scope, true).size() != 1)
            throw std::runtime_error("Smoke: whole-series favorite missing");
        pop();
        return true;
    case 21:
        return select(0); // Reopen the same series: episode cache.
    case 22:
        if (!grid() || grid()->getItemCount() < 2)
            return false;
        pop();
        return true;
    case 23:
        pop();
        return true;
    case 24:
        pop();
        return true;
    case 25:
        Intent::openSettings();
        return true;
    case 26:
        if (!current()->getView("tools/release_checker"))
            throw std::runtime_error("Smoke: Check for updates missing");
        pop();
        return true;
    case 27:
        return select(0);
    case 28:
        return select(4);
    case 29: {
        if (!grid() || grid()->getItemCount() < 2)
            return false;
        auto *page = grid()->getParent();
        for (auto action : page->getActions())
            if (action->getButton() == brls::BUTTON_LB && action->isAvailable()) {
                auto callback = action->getActionListener();
                callback(page);
                return true;
            }
        throw std::runtime_error("Smoke: VOD search inaccessible from controller");
    }
    case 30: {
        auto *keys = dynamic_cast<RecyclingGrid *>(current()->getView("search/keys"));
        if (!keys)
            return false;
        keys->getDefaultFocus();
        if (brls::Application::getLocale() != "he")
            for (int i = 0; i < 2; ++i)
                for (auto action : keys->getParent()->getActions())
                    if (action->getButton() == brls::BUTTON_Y) {
                        auto callback = action->getActionListener();
                        callback(keys->getParent());
                        break;
                    }
        const auto letters = tsvitch::utf8Characters("/'קראטוןםפשדגכעיחלךףזסבהנמצתץ.");
        for (const auto &letter : tsvitch::utf8Characters("שלום")) {
            const auto index = std::find(letters.begin(), letters.end(), letter) - letters.begin();
            keys->getDataSource()->onItemSelected(keys, index);
        }
        auto *preview = dynamic_cast<brls::Label *>(current()->getView("search/input"));
        if (!preview || preview->getFullText() != "שלום")
            throw std::runtime_error("Smoke: Hebrew keyboard input failed");
        auto *page = keys->getParent();
        for (auto action : page->getActions())
            if (action->getButton() == brls::BUTTON_START) {
                auto callback = action->getActionListener();
                callback(page);
                return true;
            }
        throw std::runtime_error("Smoke: search submit missing");
    }
    case 31: {
        if (!grid())
            return false;
        if (grid()->getItemCount() != 1)
            throw std::runtime_error("Smoke: Hebrew search did not find the unique movie");
        auto *item = grid()->getGridItemByIndex(0);
        auto *title = item ? dynamic_cast<brls::Label *>(item->getView("vod/tile/title")) : nullptr;
        if (!title || title->getFullText().find("שלום") == std::string::npos)
            throw std::runtime_error("Smoke: wrong Hebrew search result");
        auto *image = dynamic_cast<brls::Image *>(item->getView("vod/tile/image"));
        if (!image || image->getTexture() <= 0)
            return false;
        return true;
    }
    default:
        brls::Logger::info("UI_SMOKE_PASSED");
        brls::Application::quit();
        return true;
    }
}

#pragma once
#include "utils/library_state.hpp"
#include <borealis/views/label.hpp>
// Desktop-only CLI fixture driver: invokes the same grid actions as controller input.
inline bool uiSmokeStep(int step, bool cacheOnly) {
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
            return select(1);
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
        return toggle(grid(), 1);
    case 4:
        if (preferences.favorites(scope, false).size() != 1)
            throw std::runtime_error("Smoke: movie favorite missing");
        pop();
        return true;
    case 5:
        return select(0); // Favorites shelf.
    case 6:
        if (!grid() || grid()->getItemCount() < 2)
            return false;
        return toggle(grid(), 1);
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
        if (!toggle(grid(), 1))
            return false;
        return select(1);
    case 14:
        if (!grid() || grid()->getItemCount() < 2)
            return false;
        return select(1); // Season.
    case 15:
        if (!grid() || grid()->getItemCount() < 3)
            return false;
        pop();
        return true; // Episodes.
    case 16:
        if (preferences.favorites(scope, true).size() != 1)
            throw std::runtime_error("Smoke: whole-series favorite missing");
        pop();
        return true;
    case 17:
        return select(1); // Reopen the same series: episode cache.
    case 18:
        if (!grid() || grid()->getItemCount() < 2)
            return false;
        pop();
        return true;
    case 19:
        pop();
        return true;
    case 20:
        pop();
        return true;
    case 21:
        Intent::openSettings();
        return true;
    case 22:
        if (!current()->getView("tools/release_checker"))
            throw std::runtime_error("Smoke: Check for updates missing");
        pop();
        return true;
    default:
        brls::Logger::info("UI_SMOKE_PASSED");
        brls::Application::quit();
        return true;
    }
}

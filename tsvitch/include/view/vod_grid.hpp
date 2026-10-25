#pragma once
#include "view/recycling_grid.hpp"
#include "api/tsvitch/result/home_live_result.h"
#include <atomic>
#include "utils/watch_store.hpp"
enum class VodStyle { Folder, Poster, Episode };
struct VodTile {
    std::string title, subtitle, image, key;
    tsvitch::LiveM3u8 progressMedia;
    std::function<void()> select, info;
    std::function<void()> favorite;
    std::function<bool()> isFavorite;
    VodStyle style = VodStyle::Folder;
    std::function<tsvitch::WatchProgress()> progressState;
};
class VodGridPage : public brls::Box {
  public:
    explicit VodGridPage(const std::string &title, bool embedded = false);
    ~VodGridPage() override;
    brls::View *getDefaultFocus() override;
    void willAppear(bool resetState = false) override {
        retiring = false;
        brls::Box::willAppear(resetState);
    }
    void willDisappear(bool resetState = false) override {
        retiring = true;
        brls::Box::willDisappear(resetState);
    }
    void setTiles(std::vector<VodTile> tiles, bool focus = false);
    void setBuilder(std::function<std::vector<VodTile>()> builder, std::function<uint64_t()> revision = {});
    void refresh();
    void setRefreshAction(std::function<void()> action);
    void setSearchAction(std::function<void()> action);
    void reload() {
        if (refreshCallback)
            refreshCallback();
    }
    void restoreFocusOnShow() { restoreFocus = true; }
    void draw(NVGcontext *, float, float, float, float, brls::Style, brls::FrameContext *) override;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

  private:
    RecyclingGrid *grid = nullptr;
    bool restoreFocus = false;
    bool retiring = false;
    bool built = false;
    uint64_t builtRevision = 0;
    std::function<uint64_t()> revision;
    std::function<void()> refreshCallback;
    std::function<std::vector<VodTile>()> builder;
};
VodGridPage *openVodGrid(const std::string &title, std::vector<VodTile> tiles = {});

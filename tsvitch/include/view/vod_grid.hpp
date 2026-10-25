#pragma once
#include "view/recycling_grid.hpp"
#include "api/tsvitch/result/home_live_result.h"
#include <atomic>
struct VodTile {
    std::string title,subtitle,image,key;
    tsvitch::LiveM3u8 progressMedia;
    std::function<void()> select,info;
};
class VodGridPage : public brls::Box {
public:
    explicit VodGridPage(const std::string& title,bool embedded=false);
    ~VodGridPage() override;
    brls::View* getDefaultFocus() override;
    void setTiles(std::vector<VodTile> tiles,bool focus=false);
    void setBuilder(std::function<std::vector<VodTile>()> builder);
    void refresh();
    void restoreFocusOnShow(){restoreFocus=true;}
    void draw(NVGcontext*,float,float,float,float,brls::Style,brls::FrameContext*) override;
    std::shared_ptr<std::atomic<bool>> alive=std::make_shared<std::atomic<bool>>(true);
private:
    RecyclingGrid* grid=nullptr;
    bool restoreFocus=false;
    std::function<std::vector<VodTile>()> builder;
};
VodGridPage* openVodGrid(const std::string& title,std::vector<VodTile> tiles={});

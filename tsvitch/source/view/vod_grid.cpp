#include "view/vod_grid.hpp"
#include "utils/watch_progress.hpp"
#include "utils/image_helper.hpp"
#include "utils/number_helper.hpp"
#include "view/auto_tab_frame.hpp"
#include <borealis/core/activity.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/image.hpp>
#include <borealis/views/label.hpp>

namespace {
class VodCell : public RecyclingGridItem {
    BRLS_BIND(brls::Image, image,"vod/tile/image");
    BRLS_BIND(brls::Label, title,"vod/tile/title");
    BRLS_BIND(brls::Label, subtitle,"vod/tile/subtitle");
    BRLS_BIND(brls::Label, progress,"vod/tile/progress");
    tsvitch::LiveM3u8 media;
    int64_t shownPosition=-1;bool shownCompleted=false;
public:
    VodCell(){inflateFromXMLRes("xml/views/vod_tile.xml");}
    ~VodCell() override{ImageHelper::clear(image);}
    void set(const VodTile& tile) {
        title->setText(tile.title);subtitle->setText(tile.subtitle);media=tile.progressMedia;shownPosition=-1;
        ImageHelper::clear(image);image->setImageFromRes("pictures/video-card-bg.png");
        if(!tile.image.empty())ImageHelper::with(image)->load(tile.image);
        registerAction(brls::getStr("iptv/details"),brls::BUTTON_X,[action=tile.info](brls::View*){if(action)action();return true;});
        setActionAvailable(brls::BUTTON_X,static_cast<bool>(tile.info));
        progress->setText("");
    }
    void cacheForReuse() override{ImageHelper::clear(image);}
    void draw(NVGcontext* vg,float x,float y,float width,float height,brls::Style style,brls::FrameContext* ctx) override {
        const auto state=media.url.empty()?tsvitch::WatchProgress{}:tsvitch::watchStore().progress(media);
        if(state.position!=shownPosition||state.completed!=shownCompleted){
            shownPosition=state.position;shownCompleted=state.completed;
            progress->setText(state.completed?brls::getStr("iptv/watched"):state.position>0?brls::getStr("iptv/continue")+"  "+tsvitch::sec2Time(state.position):"");
        }
        RecyclingGridItem::draw(vg,x,y,width,height,style,ctx);
        if(state.position>0||state.completed){
            nvgBeginPath(vg);nvgFillColor(vg,nvgRGBA(128,128,128,90));nvgRect(vg,x,y+height-5,width,5);nvgFill(vg);
            nvgBeginPath(vg);nvgFillColor(vg,nvgRGB(27,164,107));nvgRect(vg,x,y+height-5,std::max(3.f,width*state.fraction()),5);nvgFill(vg);
        }
    }
};
class Tiles : public RecyclingGridDataSource {
public:
    std::vector<VodTile> items;
    explicit Tiles(std::vector<VodTile> tiles):items(std::move(tiles)){}
    size_t getItemCount() override{return items.size();}
    RecyclingGridItem* cellForRow(RecyclingGrid* grid,size_t index) override {
        auto* cell=static_cast<VodCell*>(grid->dequeueReusableCell("Vod"));cell->set(items[index]);return cell;
    }
    void onItemSelected(RecyclingGrid* grid,size_t index) override {
        if(index>=items.size())return;
        grid->setDefaultCellFocus(index);auto action=items[index].select;if(action)action();
    }
    void clearData() override{items.clear();}
};
class GridActivity : public brls::Activity {
    VodGridPage* page;
public:
    explicit GridActivity(VodGridPage* page):brls::Activity(new brls::AppletFrame(page)),page(page){
    }
    void onContentAvailable() override{static_cast<brls::AppletFrame*>(getContentView())->setHeaderVisibility(brls::Visibility::GONE);}
    void onResume() override {page->refresh();page->restoreFocusOnShow();}
};
}
VodGridPage::VodGridPage(const std::string& heading,bool embedded) {
    inflateFromXMLRes("xml/views/vod_grid.xml");
    grid=dynamic_cast<RecyclingGrid*>(getView("vod/grid"));
    dynamic_cast<brls::Label*>(getView("vod/title"))->setText(heading);
    auto* back=dynamic_cast<brls::DetailCell*>(getView("vod/back"));
    back->setText(brls::getStr("hints/back"));
    auto cancel=[this,embedded](brls::View*){if(embedded)AutoTabFrame::focus2Sidebar(this);else brls::Application::popActivity();return true;};
    back->registerClickAction(cancel);if(embedded)back->setVisibility(brls::Visibility::GONE);
    registerAction(brls::getStr("hints/back"),brls::BUTTON_B,cancel);
    grid->registerCell("Vod",[]{return new VodCell;});
}
VodGridPage::~VodGridPage(){*alive=false;}
brls::View* VodGridPage::getDefaultFocus(){return grid?grid->getDefaultFocus():nullptr;}
void VodGridPage::draw(NVGcontext* vg,float x,float y,float width,float height,brls::Style style,brls::FrameContext* ctx) {
    if(restoreFocus){
        const auto stack=brls::Application::getActivitiesStack();
        // A fading outgoing activity remains on the stack until its animation ends.
        // Restore only when this page is actually visible and active, never over a player/dialog.
        if(!stack.empty()&&stack.back()==getParentActivity()){
            restoreFocus=false;brls::Application::giveFocus(getDefaultFocus());
        }
    }
    brls::Box::draw(vg,x,y,width,height,style,ctx);
}
void VodGridPage::setTiles(std::vector<VodTile> tiles,bool focus) {
    std::string focused;
    auto* old=dynamic_cast<Tiles*>(grid->getDataSource());const auto index=grid->getDefaultCellFocus();
    if(old&&index<old->items.size())focused=old->items[index].key;
    if(tiles.empty())tiles.push_back({brls::getStr("iptv/empty"),brls::getStr("hints/back"),"","empty",{},{},{}});
    size_t selected=0;
    if(!focused.empty())for(size_t i=0;i<tiles.size();++i)if(tiles[i].key==focused){selected=i;break;}
    grid->setDefaultCellFocus(selected);grid->setDataSource(new Tiles(std::move(tiles)));
    if(focus)brls::Application::giveFocus(getDefaultFocus());
}
void VodGridPage::setBuilder(std::function<std::vector<VodTile>()> value){builder=std::move(value);refresh();}
void VodGridPage::refresh(){if(builder)setTiles(builder());}
VodGridPage* openVodGrid(const std::string& title,std::vector<VodTile> tiles) {
    auto* page=new VodGridPage(title);page->setTiles(std::move(tiles));
    brls::Application::pushActivity(new GridActivity(page),brls::TransitionAnimation::NONE);return page;
}

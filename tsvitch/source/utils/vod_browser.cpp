#include <borealis/core/activity.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include "utils/iptv_browser.hpp"
#include "utils/xmltv.hpp"
#include "utils/activity_helper.hpp"
#include "utils/playlist.hpp"
#include "utils/vod_helpers.hpp"
#include "utils/watch_progress.hpp"
#include "utils/url_encode.hpp"
#include "view/vod_grid.hpp"
#include "tsvitch.h"
#include "tsvitch/util/http.hpp"
#include <map>

namespace tsvitch {
namespace {
using Json=nlohmann::json;
using Items=LiveM3u8ListResult;
using Tiles=std::vector<VodTile>;
using Builder=std::function<Tiles()>;
using Done=std::function<void(Items)>;
std::string text(const Json& j,const std::string& key) {
    if(!j.is_object()||!j.contains(key)||j[key].is_null())return "";
    return j[key].is_string()?j[key].get<std::string>():j[key].is_number()?j[key].dump():"";
}
std::string server() {auto s=ProgramConfig::instance().getXtreamServerUrl();while(!s.empty()&&s.back()=='/')s.pop_back();return s;}
std::string provider() {return server()+"#"+ProgramConfig::instance().getXtreamUsername();}
void request(const std::string& action,const std::string& args,std::function<void(const Json&)> done,std::function<void()> fail) {
    auto& c=ProgramConfig::instance();
    const auto url=server()+"/player_api.php?username="+encodeUrlComponent(c.getXtreamUsername())+"&password="+encodeUrlComponent(c.getXtreamPassword())+"&action="+action+args;
    cpr::GetCallback([done,fail](cpr::Response r){
        auto data=Json::parse(r.text,nullptr,false);const bool ok=!r.error&&r.status_code==200&&!data.is_discarded();
        brls::sync([data=std::move(data),ok,done,fail]{if(ok)done(data);else fail();});
    },cpr::Url{url},cpr::Timeout{45000},HTTP::HEADERS,HTTP::PROXIES,HTTP::VERIFY);
}
std::string stream(const std::string& kind,const std::string& id,std::string extension) {
    if(extension.empty()||extension.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")!=std::string::npos)extension="mp4";
    auto& c=ProgramConfig::instance();
    return server()+"/"+kind+"/"+encodeUrlComponent(c.getXtreamUsername())+"/"+encodeUrlComponent(c.getXtreamPassword())+"/"+encodeUrlComponent(id)+"."+extension;
}
std::string description(const Json& j) {
    std::string result;
    for(const auto* key:{"plot","description","genre","releasedate","rating","director","cast"}){
        auto value=text(j,key);if(!value.empty())result+=value+"\n\n";
    }return result;
}
VodTile actionTile(const std::string& title,const std::string& key,std::function<void()> action) {
    VodTile tile;tile.title=title;tile.key=key;tile.select=std::move(action);return tile;
}
void play(const Items& queue,size_t index) {
    if(index>=queue.size()||queue[index].url.empty())return;
    if(queue[index].contentType=="movie")Intent::openLive({queue[index]},0,[]{});else Intent::openLive(queue,index,[]{});
}
void info(const Items& queue,size_t index) {
    if(index>=queue.size())return;const auto media=queue[index];
    const auto playback=std::make_shared<Items>(media.contentType=="movie"?Items{media}:queue);
    if(media.contentType=="movie")index=0;
    auto* dialog=new brls::Dialog(media.title+"\n\n"+(media.description.empty()?brls::getStr("iptv/s18"):media.description));
    dialog->addButton(brls::getStr("iptv/continue"),[playback,index]{play(*playback,index);});
    dialog->addButton(brls::getStr("iptv/start_over"),[playback,index]{watchStore().clear((*playback)[index]);play(*playback,index);});
    dialog->addButton(brls::getStr("iptv/s9"),[]{});dialog->open();
}
Tiles playableTiles(const Items& displayed,const Items& queue,const std::string& title);
void showItems(const std::string& title,Items displayed,Items queue={}) {
    if(queue.empty())queue=displayed;
    auto* page=openVodGrid(title);
    page->setBuilder([displayed,queue,title]{return playableTiles(displayed,queue,title);});
}
Tiles playableTiles(const Items& displayed,const Items& queue,const std::string& title) {
    Tiles tiles;
    if(!displayed.empty())tiles.push_back(actionTile(brls::getStr("hints/search"),"search",[displayed,queue,title]{
        brls::Application::getImeManager()->openForText([displayed,queue,title](std::string query){
            Items filtered;query=asciiLower(query);
            for(const auto& media:displayed)if(asciiLower(media.title).find(query)!=std::string::npos)filtered.push_back(media);
            showItems(title+" / "+brls::getStr("hints/search"),filtered,queue);
        },brls::getStr("hints/search"),"",128,"");
    }));
    const auto sharedQueue=std::make_shared<Items>(queue);
    for(const auto& media:displayed) {
        size_t selected=0;while(selected<queue.size()&&mediaKey(queue[selected])!=mediaKey(media))++selected;
        auto playback=sharedQueue;if(selected==queue.size()){playback=std::make_shared<Items>(Items{media});selected=0;}
        VodTile tile;tile.title=media.title;tile.subtitle=media.season.empty()?media.groupTitle:brls::getStr("iptv/season")+" "+media.season+" • "+brls::getStr("iptv/episode")+" "+media.episode;
        tile.image=media.logo;tile.key=mediaKey(media);tile.progressMedia=media;
        tile.select=[playback,selected]{play(*playback,selected);};tile.info=[playback,selected]{info(*playback,selected);};tiles.push_back(std::move(tile));
    }return tiles;
}
void showSeasons(const LiveM3u8& series,Items episodes) {
    sortEpisodes(episodes);auto* page=openVodGrid(series.title);
    page->setBuilder([series,episodes]{
        Tiles tiles;
        for(const auto& watched:watchStore().recent("series"))if(seriesKey(watched)==seriesKey(series)&&watchStore().progress(watched).resume()>0){
            auto resume=playableTiles({watched},episodes,series.title);if(resume.size()>1){resume[1].title=brls::getStr("iptv/continue")+" • "+watched.title;tiles.push_back(resume[1]);}break;
        }
        std::map<std::string,Items> seasons;for(const auto& episode:episodes)seasons[episode.season.empty()?"1":episode.season].push_back(episode);
        std::vector<std::string> keys;for(const auto& entry:seasons)keys.push_back(entry.first);
        std::stable_sort(keys.begin(),keys.end(),[](const std::string& a,const std::string& b){try{return std::stoi(a)<std::stoi(b);}catch(...){return a<b;}});
        for(const auto& key:keys){auto tile=actionTile(brls::getStr("iptv/season")+" "+key,"season:"+key,[series,episodes,items=seasons.at(key),key]{showItems(series.title+" / "+brls::getStr("iptv/season")+" "+key,items,episodes);});
            tile.image=series.logo;tile.subtitle=std::to_string(seasons.at(key).size())+" "+brls::getStr("iptv/s7");tiles.push_back(tile);
        }return tiles;
    });
}
void loadPage(VodGridPage* page,std::function<void(Done,std::function<void()>)> fetch,std::function<void(Items)> ready) {
    auto alive=page->alive;
    page->setTiles({actionTile(brls::getStr("iptv/s16"),"loading",{})},true);
    fetch([alive,ready](Items items){if(*alive)ready(std::move(items));},[alive,page,fetch,ready]{
        if(!*alive)return;
        page->setTiles({actionTile(brls::getStr("iptv/retry"),"retry",[alive,page,fetch,ready]{if(*alive)loadPage(page,fetch,ready);})},true);
    });
}
void openSeries(LiveM3u8 series,Items catalogue) {
    if(ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE)==0) {
        Items episodes;for(const auto& item:catalogue)if(seriesKey(item)==seriesKey(series))episodes.push_back(item);
        showSeasons(series,episodes);return;
    }
    auto* page=openVodGrid(series.title);
    auto fetch=[series](Done done,std::function<void()> fail){
        const auto id=series.seriesId.empty()?series.id:series.seriesId;
        request("get_series_info","&series_id="+encodeUrlComponent(id),[series,id,done,fail](const Json& data){
            if(!data.is_object()||!data.contains("episodes")||!data["episodes"].is_object()){fail();return;}
            Items episodes;
            for(auto it=data["episodes"].begin();it!=data["episodes"].end();++it){if(!it.value().is_array())continue;
                for(const auto& e:it.value()){
                    LiveM3u8 media;media.id=text(e,"id");if(media.id.empty())continue;
                    media.providerId=series.providerId;media.seriesId=id;media.seriesTitle=series.seriesTitle.empty()?series.title:series.seriesTitle;
                    media.title=text(e,"title");media.season=text(e,"season");if(media.season.empty())media.season=it.key();media.episode=text(e,"episode_num");
                    if(media.title.empty())media.title=media.seriesTitle+" • "+media.episode;
                    media.url=stream("series",media.id,text(e,"container_extension"));media.contentType="series";media.logo=series.logo;media.addedAt=numberField(e,"added");
                    if(e.contains("info")){media.description=description(e["info"]);media.durationHint=numberField(e["info"],"duration_secs");auto image=text(e["info"],"movie_image");if(!image.empty())media.logo=image;}
                    episodes.push_back(media);
                }
            }
            sortEpisodes(episodes);done(episodes);
        },fail);
    };
    // Replace the loading activity with a season page only while it is still alive.
    loadPage(page,fetch,[page,series](Items episodes){
        sortEpisodes(episodes);
        page->setBuilder([series,episodes]{
            Tiles tiles;std::map<std::string,Items> seasons;
            for(const auto& watched:watchStore().recent("series"))if(seriesKey(watched)==seriesKey(series)&&watchStore().progress(watched).resume()>0){
                auto resume=playableTiles({watched},episodes,series.title);if(resume.size()>1){resume[1].title=brls::getStr("iptv/continue")+" • "+watched.title;tiles.push_back(resume[1]);}break;
            }
            for(const auto& e:episodes)seasons[e.season].push_back(e);
            std::vector<std::string> keys;for(const auto& entry:seasons)keys.push_back(entry.first);
            std::stable_sort(keys.begin(),keys.end(),[](const std::string& a,const std::string& b){try{return std::stoi(a)<std::stoi(b);}catch(...){return a<b;}});
            for(const auto& key:keys){auto tile=actionTile(brls::getStr("iptv/season")+" "+key,"season:"+key,[series,episodes,items=seasons.at(key),key]{showItems(series.title+" / "+brls::getStr("iptv/season")+" "+key,items,episodes);});tile.image=series.logo;tiles.push_back(tile);}return tiles;
        });
        brls::Application::giveFocus(page->getDefaultFocus());
    });
}
Tiles seriesTiles(const Items& entries,const Items& catalogue) {
    const auto sharedCatalogue=std::make_shared<Items>(catalogue);
    Tiles tiles;
    std::vector<std::string> seen;
    for(auto series:entries){const auto key=seriesKey(series);if(std::find(seen.begin(),seen.end(),key)!=seen.end())continue;seen.push_back(key);
        auto progress=series;
        if(series.seriesTitle.empty())series.seriesTitle=series.title;
        series.title=series.seriesTitle;
        VodTile tile;tile.title=series.title;tile.subtitle=series.groupTitle;tile.key=key;tile.image=series.logo;
        if(!progress.url.empty())tile.progressMedia=progress;
        tile.select=[series,sharedCatalogue]{openSeries(series,*sharedCatalogue);};tiles.push_back(tile);
    }return tiles;
}
void showSeries(const std::string& title,Items entries,Items catalogue) {
    auto* page=openVodGrid(title);page->setBuilder([title,entries,catalogue]{
        auto tiles=seriesTiles(entries,catalogue);
        if(!entries.empty())tiles.insert(tiles.begin(),actionTile(brls::getStr("hints/search"),"search",[title,entries,catalogue]{
            brls::Application::getImeManager()->openForText([title,entries,catalogue](std::string query){Items filtered;query=asciiLower(query);
                for(const auto& item:entries)if(asciiLower(item.seriesTitle+" "+item.title).find(query)!=std::string::npos)filtered.push_back(item);
                showSeries(title+" / "+brls::getStr("hints/search"),filtered,catalogue);
            },brls::getStr("hints/search"),"",128,"");
        }));return tiles;
    });
}
void fetchCatalogue(bool series,Done done,std::function<void()> fail) {
    if(ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE)==0){
        TsVitchClient::get_file_m3u8([series,done](Items items){items.erase(std::remove_if(items.begin(),items.end(),[series](const LiveM3u8& item){return item.contentType!=(series?"series":"movie");}),items.end());done(items);},[fail](const std::string&,int){fail();});return;
    }
    const auto source=provider();
    request(series?"get_series":"get_vod_streams","",[series,source,done,fail](const Json& data){
        if(!data.is_array()){fail();return;}
        Items items;
        for(const auto& entry:data){LiveM3u8 item;item.id=text(entry,series?"series_id":"stream_id");if(item.id.empty())continue;
            item.title=text(entry,"name");item.providerId=source;item.contentType=series?"series":"movie";item.groupTitle=text(entry,"category_id");
            item.logo=text(entry,series?"cover":"stream_icon");item.addedAt=numberField(entry,"added");if(!item.addedAt)item.addedAt=numberField(entry,"last_modified");
            item.description=description(entry);
            if(series){item.seriesId=item.id;item.seriesTitle=item.title;}else item.url=stream("movie",item.id,text(entry,"container_extension"));items.push_back(item);
        }done(items);
    },fail);
}
void openCatalogue(bool series) {
    const auto title=brls::getStr(series?"iptv/s1":"iptv/s0");auto* page=openVodGrid(title);
    auto fetch=[series](Done done,std::function<void()> fail){
        fetchCatalogue(series,[series,done](Items items){
            if(ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE)==0){done(items);return;}
            // Category names are separate from stream records on most Xtream providers.
            request(series?"get_series_categories":"get_vod_categories","",[items,done](const Json& categories)mutable{
                std::map<std::string,std::string> names;if(categories.is_array())for(const auto& category:categories)names[text(category,"category_id")]=text(category,"category_name");
                for(auto& item:items)if(names.count(item.groupTitle))item.groupTitle=names.at(item.groupTitle);done(items);
            },[items,done]{done(items);});
        },fail);
    };
    loadPage(page,fetch,[page,series,title](Items items){
        watchStore().observe(items);
        page->setBuilder([items,series,title]{
            Tiles tiles;
            tiles.push_back(actionTile(brls::getStr("iptv/recent_added"),"added",[items,series,title]{
                auto added=items;std::stable_sort(added.begin(),added.end(),[](const LiveM3u8& a,const LiveM3u8& b){return watchStore().added(a)>watchStore().added(b);});
                if(added.size()>200)added.resize(200);
                if(series)showSeries(title+" / "+brls::getStr("iptv/recent_added"),added,items);else showItems(title+" / "+brls::getStr("iptv/recent_added"),added);
            }));
            tiles.push_back(actionTile(brls::getStr("iptv/recent_watched"),"watched",[items,series,title]{
                auto* recentPage=openVodGrid(title+" / "+brls::getStr("iptv/recent_watched"));
                recentPage->setBuilder([items,series,title]{auto recent=recentForCatalogue(watchStore().recent(series?"series":"movie"),items,series);return series?seriesTiles(recent,items):playableTiles(recent,recent,title);});
            }));
            tiles.push_back(actionTile(brls::getStr("iptv/all_items"),"all",[items,series,title]{if(series)showSeries(title,items,items);else showItems(title,items);}));
            const auto sharedCatalogue=std::make_shared<Items>(items);
            std::map<std::string,Items> categories;for(const auto& item:items)categories[item.groupTitle.empty()?brls::getStr("iptv/all_items"):item.groupTitle].push_back(item);
            for(const auto& category:categories){auto tile=actionTile(category.first,"category:"+category.first,[filtered=category.second,sharedCatalogue,series,title,name=category.first]{if(series)showSeries(title+" / "+name,filtered,*sharedCatalogue);else showItems(title+" / "+name,filtered);});
                if(!category.second.empty())tile.image=category.second.front().logo;tile.subtitle=std::to_string(category.second.size());tiles.push_back(tile);
            }return tiles;
        });brls::Application::giveFocus(page->getDefaultFocus());
    });
}
Tiles rootTiles() {
    return {
        actionTile(brls::getStr("iptv/s0"),"movies",[]{openCatalogue(false);}),
        actionTile(brls::getStr("iptv/s1"),"series",[]{openCatalogue(true);}),
        actionTile(brls::getStr("iptv/s3"),"epg",[]{configureEpgSource();})
    };
}
}
brls::View* createIptvLibrary(){auto* page=new VodGridPage(brls::getStr("iptv/s4"),true);page->setBuilder(rootTiles);return page;}
void openIptvBrowser(){auto* page=openVodGrid(brls::getStr("iptv/s4"));page->setBuilder(rootTiles);}
}

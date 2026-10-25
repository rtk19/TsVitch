#pragma once
#include "api/tsvitch/result/home_live_result.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>

namespace tsvitch {
inline int64_t watchNow() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
inline std::string mediaKey(const LiveM3u8& media) {
    if(media.contentType=="catchup")return media.url;
    auto origin=media.providerId;
    if(origin.empty()) {const auto start=media.url.find("://");origin=media.url.substr(0,start==std::string::npos?0:media.url.find('/',start+3));}
    if(media.url.empty())return origin+":series-folder:"+(media.seriesId.empty()?media.id:media.seriesId);
    if(!media.id.empty()&&(media.url.find("/movie/")!=std::string::npos||media.url.find("/series/")!=std::string::npos))return origin+":"+media.contentType+":"+media.id;
    if(!media.id.empty()&&media.id!=media.url)
        return origin+":"+media.contentType+":"+media.id+":"+media.seriesTitle+":"+media.season+":"+media.episode;
    return media.url;
}
inline std::string seriesKey(const LiveM3u8& media) {
    auto source=media.providerId;
    if(source.empty()){auto start=media.url.find("://");source=media.url.substr(0,start==std::string::npos?0:media.url.find('/',start+3));}
    return source+":"+(media.seriesId.empty()?media.seriesTitle:media.seriesId);
}
struct WatchProgress {
    int64_t position=0,duration=0,updated=0;
    bool completed=false;
    int64_t resume() const {return completed?0:position;}
    float fraction() const {return completed?1.f:duration>0?std::min(1.f,static_cast<float>(position)/duration):0.f;}
};
class WatchStore {
    using Json=nlohmann::json;
    std::filesystem::path path;
    Json data=Json::object(),legacy=Json::object(),discovered=Json::object();
    int64_t lastUpdated=0;
    static Json read(const std::filesystem::path& path) {
        try {std::ifstream f(path);Json j;if(f>>j&&j.is_object())return j;}catch(...){}return Json::object();
    }
    static bool write(const std::filesystem::path& path,const Json& value) noexcept {
        try {
        const auto staged=path.string()+".new",backup=path.string()+".bak";
        if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
        {std::ofstream out(staged,std::ios::trunc);out<<value.dump();out.flush();if(!out.good())return false;out.close();if(out.fail())return false;}
        std::error_code ec;std::filesystem::rename(staged,path,ec);
        if(!ec)return true;
        std::filesystem::remove(backup,ec);ec.clear();
        if(std::filesystem::exists(path))std::filesystem::rename(path,backup,ec);
        if(ec)return false;
        std::filesystem::rename(staged,path,ec);
        if(ec){std::filesystem::rename(backup,path,ec);return false;}
        return true;
        }catch(...){return false;}
    }
    LiveM3u8 normalized(LiveM3u8 media) const {
        const Json* saved=nullptr;const auto key=mediaKey(media);
        if(data["entries"].contains(key))saved=&data["entries"][key];
        else for(const auto& entry:data["entries"])try{if(entry.contains("media")&&entry["media"].value("url",std::string{})==media.url){saved=&entry;break;}}catch(...){}
        if(saved)try{
            const auto known=saved->at("media").get<LiveM3u8>();
            if(media.providerId.empty())media.providerId=known.providerId;
            if(media.seriesId.empty())media.seriesId=known.seriesId;
            if(media.seriesTitle.empty())media.seriesTitle=known.seriesTitle;
            if(media.season.empty())media.season=known.season;
            if(media.episode.empty())media.episode=known.episode;
            if(media.contentType.empty())media.contentType=known.contentType;
            if(media.logo.empty())media.logo=known.logo;
            if(media.description.empty())media.description=known.description;
        }catch(...){}
        return media;
    }
    void persist() { persisted=write(path,data); }
public:
    bool persisted=true;
    explicit WatchStore(std::filesystem::path file,std::filesystem::path legacyFile={}) : path(std::move(file)) {
        data=read(path);if(data.empty())data=read(path.string()+".bak");
        if(!legacyFile.empty())legacy=read(legacyFile);
        if(!data.contains("entries")||!data["entries"].is_object())data["entries"]=Json::object();
        discovered=read(path.string()+".catalogue");
        for(const auto& entry:data["entries"])try{if(entry.is_object())lastUpdated=std::max(lastUpdated,entry.value("updated",int64_t{0}));}catch(...){}
    }
    WatchProgress progress(const LiveM3u8& media) const {
        const auto key=mediaKey(media);const Json* entry=nullptr;
        if(data["entries"].contains(key))entry=&data["entries"][key];
        else {
            for(const auto& candidate:data["entries"])try{if(candidate.contains("media")&&candidate["media"].value("url",std::string{})==media.url){entry=&candidate;break;}}catch(...){}
            if(!entry&&legacy.contains(media.url))entry=&legacy[media.url];
        }
        if(!entry||!entry->is_object())return {};
        try {return {entry->value("position",int64_t{0}),entry->value("duration",int64_t{0}),entry->value("updated",int64_t{0}),entry->value("completed",false)};}catch(...){return {};}
    }
    void update(const LiveM3u8& media,int64_t position,int64_t duration,bool completed=false) {
        if(media.url.empty()||position<0)return;
        auto old=progress(media);if(duration<=0)duration=old.duration>0?old.duration:media.durationHint;
        if(duration>0)position=std::min(position,duration);
        lastUpdated=std::max(watchNow()*1000,lastUpdated+1);
        const auto canonical=normalized(media);const auto key=mediaKey(canonical);
        if(key!=mediaKey(media)||!data["entries"].contains(key)){
            std::vector<std::string> aliases;
            for(auto it=data["entries"].begin();it!=data["entries"].end();++it)try{if(it.key()!=key&&it.value().contains("media")&&it.value()["media"].value("url",std::string{})==media.url)aliases.push_back(it.key());}catch(...){}
            for(const auto& alias:aliases)data["entries"].erase(alias);
        }
        data["entries"][key]={{"position",position},{"duration",duration},{"completed",completed},{"updated",lastUpdated},{"media",canonical}};
        persist();
    }
    void clear(const LiveM3u8& media) {
        const auto canonical=normalized(media);
        const auto duration=progress(media).duration;
        std::vector<std::string> aliases;
        for(auto it=data["entries"].begin();it!=data["entries"].end();++it)try{
            if(it.key()==mediaKey(media)||(it.value().contains("media")&&it.value()["media"].value("url",std::string{})==media.url))aliases.push_back(it.key());
        }catch(...){}
        for(const auto& key:aliases)data["entries"].erase(key);
        legacy.erase(media.url);
        // Persist a zero-position record so a legacy cache cannot resurrect old progress.
        update(canonical,0,duration,false);
    }
    void observe(const LiveM3u8ListResult& items) {
        bool changed=false;const auto now=watchNow();
        for(const auto& item:items){const auto key=mediaKey(item);if(!discovered.contains(key)){discovered[key]=now;changed=true;}}
        if(changed)write(path.string()+".catalogue",discovered);
    }
    int64_t added(const LiveM3u8& media) const {
        if(media.addedAt>0)return media.addedAt;
        const auto key=mediaKey(media);try{return discovered.value(key,int64_t{0});}catch(...){return 0;}
    }
    LiveM3u8ListResult recent(const std::string& type) const {
        std::vector<std::pair<int64_t,LiveM3u8>> all;
        for(const auto& entry:data["entries"])try{
            if(!entry.contains("media")||(entry.value("position",int64_t{0})==0&&!entry.value("completed",false)))continue;
            auto item=entry["media"].get<LiveM3u8>();if(item.contentType!=type)continue;
            all.emplace_back(entry.value("updated",int64_t{0}),std::move(item));
        }catch(...){}
        std::stable_sort(all.begin(),all.end(),[](const auto& a,const auto& b){return a.first>b.first;});
        LiveM3u8ListResult result;std::vector<std::string> seen;
        for(const auto& pair:all){const auto key=type=="series"?seriesKey(pair.second):mediaKey(pair.second);
            if(std::find(seen.begin(),seen.end(),key)!=seen.end())continue;
            seen.push_back(key);result.push_back(pair.second);if(result.size()==200)break;
        }return result;
    }
};
}

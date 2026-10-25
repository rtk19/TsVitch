#pragma once
#include "utils/watch_store.hpp"
#include <map>
namespace tsvitch {
inline int64_t numberField(const nlohmann::json& j,const std::string& key) {
    if(!j.is_object()||!j.contains(key)||j[key].is_null())return 0;
    try {return j[key].is_number()?j[key].get<int64_t>():std::stoll(j[key].get<std::string>());}catch(...){return 0;}
}
inline void sortEpisodes(LiveM3u8ListResult& items) {
    auto value=[](const std::string& s){try{return std::stoi(s);}catch(...){return 0;}};
    std::stable_sort(items.begin(),items.end(),[value](const LiveM3u8& a,const LiveM3u8& b){
        if(value(a.season)!=value(b.season))return value(a.season)<value(b.season);
        return value(a.episode)<value(b.episode);
    });
}
inline bool hasNextEpisode(const LiveM3u8ListResult& items,size_t index) {
    return index+1<items.size()&&items[index].contentType=="series"&&items[index+1].contentType=="series"&&seriesKey(items[index])==seriesKey(items[index+1]);
}
inline LiveM3u8ListResult recentForCatalogue(const LiveM3u8ListResult& recent,const LiveM3u8ListResult& catalogue,bool series) {
    std::map<std::string,LiveM3u8> current;
    for(const auto& item:catalogue)current[series?seriesKey(item):mediaKey(item)]=item;
    LiveM3u8ListResult result;
    for(auto item:recent){auto found=current.find(series?seriesKey(item):mediaKey(item));
        if(found==current.end()&&item.providerId.empty()&&!item.url.empty()){
            for(auto it=current.begin();it!=current.end();++it)if(it->second.url==item.url){found=it;break;}
        }
        if(found==current.end())continue;
        if(!series)item=found->second;
        else if(!found->second.logo.empty())item.logo=found->second.logo;
        result.push_back(std::move(item));
    }return result;
}

}

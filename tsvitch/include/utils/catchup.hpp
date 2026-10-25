#pragma once
#include "utils/playlist.hpp"
#include "utils/url_encode.hpp"
#include <ctime>

namespace tsvitch {
inline bool hasCatchup(const LiveM3u8& c) {
    if(c.contentType=="movie"||c.contentType=="series"||c.contentType=="catchup")return false;
    if(c.catchupMode=="xc")return c.url.find("://")!=std::string::npos;
    if(c.catchupMode=="shift"||c.catchupMode=="flussonic")return true;
    return (c.catchupMode=="default"||c.catchupMode=="append")&&!c.catchupSource.empty();
}
inline bool archiveAvailable(const LiveM3u8& c,int64_t start,int64_t end,int64_t now) {
    return hasCatchup(c)&&start>0&&end>start&&start<now&&(!c.catchupDays||start>=now-static_cast<int64_t>(c.catchupDays)*86400);
}
inline void replaceToken(std::string& s,const std::string& token,const std::string& value) {
    size_t at=0;while((at=s.find(token,at))!=std::string::npos){s.replace(at,token.size(),value);at+=value.size();}
}
inline std::string catchupUrl(const LiveM3u8& channel,int64_t start,int64_t end,int64_t now) {
    if(!hasCatchup(channel)||end<=start||start<=0)return "";
    std::string source=channel.catchupSource;
    const auto adjusted=start+channel.catchupCorrection;
    if(channel.catchupMode=="xc" && source.empty()) {
        const auto scheme=channel.url.find("://"),path=channel.url.find('/',scheme+3);
        if(path==std::string::npos)return "";
        auto parts=channel.url.substr(path+1);if(parts.rfind("live/",0)==0)parts.erase(0,5);
        const auto user=parts.find('/'),password=parts.find('/',user==std::string::npos?0:user+1);
        if(user==std::string::npos||password==std::string::npos)return "";
        auto id=parts.substr(password+1);id=id.substr(0,id.find_first_of(".?#/"));
        const auto aligned=adjusted-adjusted%60;
        const std::time_t epoch=aligned;std::tm tm{};gmtime_r(&epoch,&tm);char date[32];std::strftime(date,sizeof(date),"%Y-%m-%d:%H-%M",&tm);
        return channel.url.substr(0,path)+"/timeshift/"+parts.substr(0,password)+"/"+std::to_string((end+channel.catchupCorrection-aligned+59)/60)+"/"+date+"/"+id+".ts";
    }
    if(source.empty()&&channel.catchupMode=="shift") source=(channel.url.find('?')==std::string::npos?"?":"&")+std::string("utc={utc}&lutc={lutc}");
    if(source.empty()&&channel.catchupMode=="flussonic") {
        auto url=channel.url.substr(0,channel.url.find('?'));const auto slash=url.rfind('/');
        if(slash==std::string::npos)return "";
        source=url.substr(0,slash+1)+"timeshift_abs-{utc}."+(url.find(".m3u8")!=std::string::npos?"m3u8":"ts");
    }
    if((channel.catchupMode=="append"||channel.catchupMode=="shift")&&source.find("://")==std::string::npos) {
        if(!source.empty()&&source.front()=='?'&&channel.url.find('?')!=std::string::npos)source.front()='&';
        source=channel.url+source;
    }
    else source=resolvePlaylistUrl(channel.url,source);
    replaceToken(source,"${start}",std::to_string(adjusted));replaceToken(source,"${end}",std::to_string(end+channel.catchupCorrection));
    replaceToken(source,"${duration}",std::to_string(end-start));
    for(const auto& token:{std::string("{utc}"),std::string("{start}")})replaceToken(source,token,std::to_string(adjusted));
    for(const auto& token:{std::string("{utcend}"),std::string("{end}")})replaceToken(source,token,std::to_string(end+channel.catchupCorrection));
    replaceToken(source,"{duration}",std::to_string(end-start));replaceToken(source,"{lutc}",std::to_string(now));replaceToken(source,"{offset}",std::to_string(now-start));
    const std::time_t epoch=adjusted;std::tm tm{};gmtime_r(&epoch,&tm);
    for(const auto& code:{"Y","m","d","H","M","S"}){char formatted[16];std::strftime(formatted,sizeof(formatted),("%"+std::string(code)).c_str(),&tm);replaceToken(source,"{"+std::string(code)+"}",formatted);}
    // Unknown placeholders must never be sent to a provider as if supported.
    if(source.find('{')!=std::string::npos||source.find('}')!=std::string::npos)return "";
    return source;
}
inline int64_t archiveCursor(const LiveM3u8& media,int64_t target) {
    target=std::max(media.archiveStart,std::min(media.archiveEnd-1,target));
    if(media.catchupMode=="xc"&&media.catchupSource.empty()) {
        const auto adjusted=target+media.catchupCorrection;
        target=adjusted-adjusted%60-media.catchupCorrection;
    }
    return std::max(media.archiveStart,target);
}
inline std::vector<int64_t> archiveDaySlots(int64_t midnight,int64_t nextMidnight) {
    std::vector<int64_t> slots;
    if(nextMidnight<=midnight || nextMidnight-midnight>26*3600)return slots;
    for(auto start=midnight;start<nextMidnight;start+=1800)slots.push_back(start);
    return slots;
}
inline LiveM3u8 archiveMedia(const LiveM3u8& channel,const std::string& title,const std::string& description,int64_t start,int64_t end,int64_t now) {
    auto media=channel;media.liveUrl=channel.url;media.liveTitle=channel.title;end=std::min(end,now);media.url=catchupUrl(channel,start,end,now);
    media.title=title;media.description=description;media.contentType="catchup";media.archiveStart=start;media.archiveEnd=end;return media;
}
}

#pragma once
#include "api/tsvitch/result/home_live_result.h"
#include <sstream>
#include <map>
#include <regex>
#include <algorithm>

namespace tsvitch {
inline std::string asciiLower(std::string s) {
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);}); return s;
}
inline std::string trim(std::string s) {
    const auto first=s.find_first_not_of(" \t\r\n");
    return first==std::string::npos ? "" : s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
inline std::map<std::string,std::string> playlistAttributes(const std::string& s) {
    std::map<std::string,std::string> result;
    static const std::regex attribute(R"attr(([a-zA-Z0-9_-]+)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s,]+)))attr");
    for (auto it=std::sregex_iterator(s.begin(),s.end(),attribute);it!=std::sregex_iterator();++it)
        result[asciiLower((*it)[1])] = (*it)[2].matched ? (*it)[2].str() : (*it)[3].matched ? (*it)[3].str() : (*it)[4].str();
    return result;
}
inline std::string resolvePlaylistUrl(const std::string& base,const std::string& reference) {
    if(reference.empty() || reference.find("://")!=std::string::npos || base.find("://")==std::string::npos) return reference;
    const auto originEnd=base.find('/',base.find("://")+3);
    const auto origin=originEnd==std::string::npos?base:base.substr(0,originEnd);
    if(reference.rfind("//",0)==0) return base.substr(0,base.find(':')+1)+reference;
    if(reference.front()=='?') return base.substr(0,base.find('?'))+reference;
    if(reference.front()=='/') return origin+reference;
    const auto clean=base.substr(0,base.find_first_of("?#"));
    return (originEnd==std::string::npos?base+"/":clean.substr(0,clean.rfind('/')+1))+reference;
}
inline void classifyMedia(LiveM3u8& item) {
    auto type=asciiLower(item.contentType),url=asciiLower(item.url),group=asciiLower(item.groupTitle);
    static const std::regex movieExtension(R"(\.(mp4|mkv|avi|mov)(\?|$))");
    std::smatch match;
    static const std::regex episode(R"((.*?)\s*[Ss]([0-9]+)[ ._-]*[Ee]([0-9]+).*)");
    if(std::regex_match(item.title,match,episode)) {
        if(item.seriesTitle.empty()) item.seriesTitle=trim(match[1]);
        if(item.season.empty()) item.season=match[2];
        if(item.episode.empty()) item.episode=match[3];
    }
    if(type=="series" || type=="episode" || !item.seriesTitle.empty() || url.find("/series/")!=std::string::npos || group.find("series")!=std::string::npos || group.find("סדרות")!=std::string::npos) item.contentType="series";
    else if(type=="movie" || type=="vod" || url.find("/movie/")!=std::string::npos || group.find("movie")!=std::string::npos || group.find("סרטים")!=std::string::npos || std::regex_search(url,movieExtension)) item.contentType="movie";
    else item.contentType="live";
}
inline LiveM3u8ListResult parsePlaylist(const std::string& text,const std::string& base="") {
    LiveM3u8ListResult result;
    std::istringstream stream(text);std::string line;
    std::map<std::string,std::string> defaults,attrs;
    LiveM3u8 current;bool pending=false;
    while(std::getline(stream,line)) {
        line=trim(line);
        if(line.rfind("\xef\xbb\xbf",0)==0) line.erase(0,3);
        if(line.rfind("#EXTM3U",0)==0) { defaults=playlistAttributes(line);continue; }
        if(line.rfind("#EXTINF:",0)==0) {
            current=LiveM3u8{};attrs=defaults;
            bool quoted=false;char quote=0;size_t comma=std::string::npos;
            for(size_t i=8;i<line.size();++i) {
                if((line[i]=='"'||line[i]=='\'')&&(!quoted||line[i]==quote)){quoted=!quoted;quote=line[i];}
                else if(line[i]==','&&!quoted){comma=i;break;}
            }
            const auto entry=playlistAttributes(line.substr(0,comma));attrs.insert(entry.begin(),entry.end());
            for(const auto& a:entry) attrs[a.first]=a.second;
            try { if(std::stod(line.substr(8))>0 && attrs["type"].empty() && attrs["media-type"].empty())attrs["type"]="movie"; } catch(...) {}
            current.title=comma==std::string::npos?attrs["tvg-name"]:trim(line.substr(comma+1));pending=true;continue;
        }
        if(line.rfind("#EXTGRP:",0)==0) {attrs["group-title"]=trim(line.substr(8));continue;}
        if(line.empty()||line[0]=='#'||!pending) continue;
        current.url=resolvePlaylistUrl(base,line);current.id=attrs["tvg-id"];
        current.epgId=current.id; if(current.id.empty())current.id=current.url;
        current.logo=resolvePlaylistUrl(base,attrs["tvg-logo"]);current.chno=attrs["tvg-chno"];
        current.groupTitle=attrs["group-title"];current.description=attrs["description"];
        if(current.description.empty()) current.description=attrs["tvg-description"];
        current.epgSource=attrs["x-tvg-url"].empty()?attrs["url-tvg"]:attrs["x-tvg-url"];
        current.epgSource=resolvePlaylistUrl(base,current.epgSource);
        current.contentType=attrs["type"];if(current.contentType.empty())current.contentType=attrs["media-type"];
        current.seriesTitle=attrs["series-title"].empty()?attrs["tvg-series"]:attrs["series-title"];current.season=attrs["season"].empty()?attrs["tvg-season"]:attrs["season"];
        current.episode=attrs["episode"].empty()?attrs["tvg-episode"]:attrs["episode"];current.catchupMode=asciiLower(attrs["catchup"]);
        current.catchupSource=attrs["catchup-source"];
        if(current.catchupMode=="fs")current.catchupMode="flussonic";
        if(current.catchupMode.empty()&&!attrs["timeshift"].empty())current.catchupMode="shift";
        if(current.catchupMode.empty()&&!attrs["tvg-rec"].empty()&&current.url.find("/live/")!=std::string::npos)current.catchupMode="xc";
        if(current.catchupMode.empty()&&!current.catchupSource.empty())current.catchupMode="default";
        try { current.catchupDays=std::max(0,std::stoi(attrs["catchup-days"].empty()?(attrs["timeshift"].empty()?attrs["tvg-rec"]:attrs["timeshift"]):attrs["catchup-days"])); } catch(...) {}
        try { current.catchupCorrection=static_cast<int>(std::stod(attrs["catchup-correction"])*3600); } catch(...) {}
        classifyMedia(current);result.push_back(std::move(current));pending=false;
    }
    return result;
}
}

#include "utils/catchup.hpp"
#include "utils/epg_metadata.hpp"
#include "utils/subtitle_style.hpp"
#include <cassert>
#include <iostream>
using namespace tsvitch;
int main(){
    LiveM3u8 channel;channel.url="https://h/live/u/p/1.ts";channel.contentType="live";channel.catchupMode="xc";channel.catchupDays=7;
    const int64_t start=1700000017,now=start+3600;
    auto media=archiveMedia(channel,"Programme","",start,start+1800,now);
    auto initial=archiveSeekPlan(media,0,now);assert(initial.offset<0&&initial.offset>-60&&initial.start==-initial.offset&&initial.end==1800-initial.offset);
    for(double target:{0.,.3,12.5,59.9,60.,181.5,1799.5}){
        auto plan=archiveSeekPlan(media,target,now);assert(!plan.url.empty());assert(std::abs(plan.offset+plan.start-target)<.001);assert(plan.start>=0&&plan.start<60);assert(std::abs(plan.end+plan.offset-1800)<.001);
    }
    media.catchupCorrection=3600;auto corrected=archiveSeekPlan(media,73.25,now);assert(std::abs(corrected.offset+corrected.start-73.25)<.001);
    channel.catchupMode="default";channel.catchupSource="https://h/archive?start={utc}&duration={duration}";media=archiveMedia(channel,"P","",start,start+1800,now);auto seconds=archiveSeekPlan(media,13.5,now);assert(seconds.offset==13&&seconds.start==.5);
    Programme a,b,c;a.title="Past";a.start=100;a.end=200;b.title="Current according to provider";b.start=200;b.end=300;b.nowPlaying=true;c.title="Device clock is ahead";c.start=300;c.end=400;
    assert(currentProgrammeIndex({a,b,c},350)==1);b.nowPlaying=false;assert(currentProgrammeIndex({a,b,c},350)==2);assert(currentProgrammeIndex({a,b,c},450)==-1);
    nlohmann::json payload={{"epg_listings",nlohmann::json::array({{{"title","Past"},{"start_timestamp","1700000000000"},{"stop_timestamp","1700000060000"}},{{"title","Now"},{"start","2026-10-25 09:55:00"},{"end","2026-10-25 11:00:00"},{"now_playing",1}}})}};
    auto parsed=parseShortEpg(payload);assert(parsed.size()==2&&parsed[0].start==1700000000&&parsed[1].nowPlaying);assert(currentProgrammeIndex(parsed,1700000000)==1);assert(programmeTime(parsed[1]).find("2026-")==std::string::npos);
    auto style=subtitle_style::background(true);assert(std::string(style[0].second)=="background-box"&&std::string(style[1].second)=="#99000000");
    std::cout<<"Catch-up minute boundaries/subsecond targets, provider current EPG flags and subtitle style passed\n";
}

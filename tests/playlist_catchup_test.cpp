#include "utils/playlist.hpp"
#include "utils/catchup.hpp"
#include "utils/xmltv_time.hpp"
#include <cassert>
int main() {
    assert(tsvitch::archiveDaySlots(1000,1000+23*3600).size()==46);
    assert(tsvitch::archiveDaySlots(1000,1000+25*3600).size()==50);
    assert(tsvitch::archiveDaySlots(1000,1000).empty());
    tsvitch::LiveM3u8 liveMovie;liveMovie.groupTitle="UK Movies";liveMovie.url="https://p.test/live/u/p/15.ts";
    tsvitch::classifyMedia(liveMovie);assert(liveMovie.contentType=="live");
    liveMovie.contentType="live";liveMovie.url="https://p.test/hls/movies.m3u8";
    tsvitch::classifyMedia(liveMovie);assert(liveMovie.contentType=="live");
    const auto list=tsvitch::parsePlaylist(R"m3u(#EXTM3U x-tvg-url="guide.xml.gz" catchup="append" catchup-source="?utc={utc}&duration={duration}" catchup-days="7"
#EXTINF:-1 tvg-id="news.il" tvg-logo="/logo.png" group-title="חדשות, ישראל",ערוץ חדשות
https://provider.test/live/u/p/15.ts
#EXTINF:7200 type="movie" group-title="Movies" description="סרט לדוגמה",סרט
movies/film.m3u8
#EXTINF:-1 catchup="none" group-title='Series',Show S02E03
https://provider.test/series/u/p/33.mkv
#EXTINF:-1,No ID
last.ts
)m3u","https://provider.test/playlist/list.m3u");
    assert(list.size()==4);
    assert(list[0].title=="ערוץ חדשות"&&list[0].groupTitle=="חדשות, ישראל");
    assert(list[0].epgSource=="https://provider.test/playlist/guide.xml.gz");
    assert(list[0].logo=="https://provider.test/logo.png"&&list[0].epgId=="news.il");
    assert(list[1].contentType=="movie"&&list[1].description=="סרט לדוגמה");
    assert(list[2].contentType=="series"&&list[2].seriesTitle=="Show"&&list[2].season=="02"&&list[2].episode=="03");
    assert(!list[3].id.empty()&&list[3].url=="https://provider.test/playlist/last.ts");
    const int64_t now=tsvitch::xmltvTimestamp("20261025120000 +0000"),start=now-3600,end=now-1800;
    assert(tsvitch::archiveAvailable(list[0],start,end,now));
    assert(!tsvitch::archiveAvailable(list[0],now-9*86400,now-8*86400,now));
    assert(!tsvitch::archiveAvailable(list[0],now+1,now+100,now));
    assert(!tsvitch::hasCatchup(list[1])&&!tsvitch::hasCatchup(list[2]));
    const auto url=tsvitch::catchupUrl(list[0],start,end,now);
    assert(url==list[0].url+"?utc="+std::to_string(start)+"&duration=1800");
    auto xc=list[0];xc.catchupMode="xc";xc.catchupSource.clear();
    assert(tsvitch::catchupUrl(xc,start,end,now)=="https://provider.test/timeshift/u/p/30/2026-10-25:11-00/15.ts");
    xc.archiveStart=start;xc.archiveEnd=end;
    assert(tsvitch::archiveCursor(xc,start+95)==start+60);
    assert(tsvitch::archiveCursor(xc,start-50)==start);
    assert(tsvitch::archiveCursor(xc,end+100)<end);
    xc.catchupCorrection=7200;
    assert(tsvitch::catchupUrl(xc,start,end,now).find("2026-10-25:13-00")!=std::string::npos);
    auto custom=list[0];custom.catchupMode="default";custom.catchupSource="/archive/{Y}/{m}/{d}/{H}/{M}/{S}?end={utcend}&now={lutc}";
    assert(tsvitch::catchupUrl(custom,start,end,now).find("https://provider.test/archive/2026/10/25/11/00/00?")==0);
    custom.catchupSource="https://provider.test/{unknown}";
    assert(tsvitch::catchupUrl(custom,start,end,now).empty());
    const auto recording=tsvitch::archiveMedia(list[0],"Programme","Summary",start,end,now);
    assert(recording.contentType=="catchup"&&recording.liveUrl==list[0].url&&recording.archiveStart==start&&recording.archiveEnd==end);
    const nlohmann::json saved=recording;
    const auto restored=saved.get<tsvitch::LiveM3u8>();
    assert(restored.catchupSource==recording.catchupSource&&restored.archiveStart==start&&restored.liveTitle==list[0].title);
}

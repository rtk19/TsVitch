#include "utils/content_metadata.hpp"
#include "utils/epg_metadata.hpp"
#include <cassert>
#include <iostream>
using namespace tsvitch;
int main() {
    SearchText text("שלום");
    assert(text.position() == 4);
    text.move(-1);
    text.insert("!");
    assert(text.value() == "שלו!ם");
    text.backspace();
    assert(text.value() == "שלום");
    text.clear();
    text.backspace();
    assert(text.value().empty());
    text.insert("שָׁ");
    text.backspace();
    assert(text.value().empty());
    text.set("A😀ב");
    text.move(-1);
    text.backspace();
    assert(text.value() == "Aב");
    SearchText bounded("אב", 3);
    bounded.insert("גד");
    assert(bounded.value() == "אבג");
    assert(searchMatches("שָׁלוֹם עולם", "שלום"));
    assert(searchMatches("מלך", "מלכ"));
    assert(searchMatches("HELLO Мир", "hello мир"));
    assert(utf8Characters("א😀z").size() == 3);
    assert(utf8Characters(std::string("\xf0\x80", 2)).size() == 2);
    using Json = nlohmann::json;
    assert(contentArtwork(Json{{"cover", Json::array({nullptr, "/poster.webp?size=big"})}},
                          "https://host:80/api/") == "https://host:80/poster.webp?size=big");
    assert(contentArtwork(Json{{"stream_icon", nullptr},
                               {"info", {{"movie_image", {{"url", "//cdn/poster one.jpg?a=1&amp;b=2"}}}}}},
                          "https://host/") == "https://cdn/poster%20one.jpg?a=1&b=2");
    assert(artworkValue("[\"https://a/cover.png\"]") == "https://a/cover.png");
    assert(artworkValue("null").empty());
    LiveM3u8 one;
    one.id = "1";
    one.url = "https://host/series/u/p/1.mkv";
    one.title = "Last Seen S01E01 - First";
    one.contentType = "series";
    one.seriesId = "9";
    one.seriesTitle = "Last Seen";
    auto alternate = one;
    alternate.id = "2";
    alternate.url = "https://host/series/u/p/2.mkv";
    alternate.title = "נראתה לאחרונה - המוקד";
    alternate.season = "1";
    alternate.episode = "1";
    auto two = one;
    two.id = "3";
    two.url = "https://host/series/u/p/3.mkv";
    two.title = "Last Seen S01E02";
    auto season = two;
    season.id = "4";
    season.url += "?season=2";
    season.title = "Last Seen S02E01";
    auto unknown = one;
    unknown.id = "5";
    unknown.url += "?special";
    unknown.title = "Special";
    auto other = alternate;
    other.seriesId = "10";
    other.id = "6";
    other.url += "?other";
    LiveM3u8ListResult items = {two, one, alternate, season, unknown, other, alternate};
    auto groups = groupEpisodes(items);
    assert(groups.size() == 5);
    size_t first = 0;
    while (groups[first].variants.size() != 2)
        ++first;
    assert(groups[first].variants[0] == 1 && groups[first].variants[1] == 2);
    auto progress = [](const LiveM3u8 &m) {
        return WatchProgress{m.id == "2" ? 120 : 0, 300, m.id == "2" ? 100 : 0, false};
    };
    assert(preferredEpisode(items, groups[first], progress) == 2);
    auto queue = episodeQueue(items, groups, progress);
    assert(queue.size() == 5 && queue[first].id == "2" && queue[first].episode == "1");
    assert(queue[first + 1].id == "3");
    auto override = episodeQueue(items, groups, progress, first, 1);
    assert(override[first].id == "1" && override[first + 1].id == "3");
    auto special = alternate;
    special.season = "0";
    special.id = "special";
    special.url += "?special-season";
    auto regular = alternate;
    regular.season = "1";
    assert(groupEpisodes({special, regular}).size() == 2);
    assert(epgText("16nXnNeV150=") == "שלום");
    assert(epgText("News") == "News");
    assert(epgText("חדשות") == "חדשות");
    assert(epgText("////") == "////");
    assert(epgText("YWJj") == "abc");
    auto epg = parseShortEpg(
        Json{{"epg_listings",
              Json::array({Json{{"title", "16nXnNeV150="},
                                {"description", "Plain description"},
                                {"start_timestamp", "100"},
                                {"end_timestamp", 200}},
                           Json{{"title", "News"}, {"start_timestamp", 300}, {"stop_timestamp", "400"}}})}});
    assert(epg.size() == 2 && epg[0].title == "שלום" && epg[0].description == "Plain description" &&
           epg[0].start == 100 && epg[0].end == 200 && epg[1].end == 400);
    assert(parseShortEpg(Json::array()).empty());
    std::cout << "Unicode editing/search, artwork metadata, episode alternatives/resume/autoplay and EPG "
                 "parsing passed\n";
}

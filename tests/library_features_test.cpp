#include "utils/library_preferences.hpp"
#include "utils/vod_cache.hpp"
#include "../cmake/font_weight.h"
#include <cassert>
#include <iostream>
#include <chrono>
namespace fs = std::filesystem;
using namespace tsvitch;
int main(int argc, char **argv) {
    assert(argc == 2);
    fs::path directory = argv[1];
    fs::create_directories(directory);
    LiveM3u8 movie;
    movie.id = "1";
    movie.url = "https://host/movie/u/p/1.mp4";
    movie.title = "Movie";
    movie.contentType = "movie";
    movie.providerId = "host#u";
    auto episode = movie;
    episode.id = "7";
    episode.url = "https://host/series/u/p/7.mkv";
    episode.contentType = "series";
    episode.title = "Episode 1";
    episode.seriesId = "2";
    episode.seriesTitle = "Series";
    episode.season = "1";
    episode.episode = "1";
    const auto file = directory / "preferences.json";
    {
        LibraryPreferences preferences(file);
        assert(preferences.toggleFavorite("provider", movie, false));
        assert(preferences.isFavorite("provider", movie, false));
        assert(!preferences.isFavorite("other", movie, false));
        auto rotated = movie;
        rotated.url = "https://host/movie/u/new/1.mp4";
        assert(preferences.isFavorite("provider", rotated, false));
        assert(preferences.toggleFavorite("provider", episode, true));
        auto another = episode;
        another.id = "8";
        another.episode = "2";
        another.url = "https://host/series/u/p/8.mkv";
        assert(preferences.isFavorite("provider", another, true));
        const auto favorites = preferences.favorites("provider", true);
        assert(favorites.size() == 1 && favorites[0].title == "Series" && favorites[0].url.empty() &&
               favorites[0].episode.empty());
        assert(preferences.togglePin("provider", "Sports"));
        assert(preferences.togglePin("provider", "News"));
        std::vector<std::string> groups = {"Movies", "News", "Drama", "Sports"};
        preferences.sortGroups("provider", groups);
        assert((groups == std::vector<std::string>{"Sports", "News", "Drama", "Movies"}));
        preferences.select("provider", "Drama");
        assert(preferences.selected("provider") == "Drama");
        assert(preferences.pinned("provider", "Sports"));
        assert(!preferences.pinned("other", "Sports"));
        assert(preferences.togglePin("provider", "Sports"));
        preferences.sortGroups("provider", groups);
        assert(groups.front() == "News");
        assert(preferences.selected("provider") == "Drama");
    }
    {
        LibraryPreferences reopened(file);
        assert(reopened.isFavorite("provider", movie, false));
        assert(reopened.isFavorite("provider", episode, true));
        assert(reopened.pinned("provider", "News"));
        assert(reopened.selected("provider") == "Drama");
        assert(reopened.toggleFavorite("provider", episode, true));
        assert(!reopened.isFavorite("provider", episode, true));
    }
    fs::create_directories(directory / "blocked");
    std::ofstream(directory / "blocked" / "child") << "x";
    LibraryPreferences broken(directory / "blocked");
    assert(!broken.toggleFavorite("provider", movie, false));
    assert(!broken.isFavorite("provider", movie, false));
    VodCache cache(directory / "cache");
    auto data = std::make_shared<VodSnapshot>();
    data->items = std::make_shared<const LiveM3u8ListResult>(LiveM3u8ListResult{movie, episode});
    data->saved = 1000;
    cache.remember("source/movies", data);
    assert(cache.find("source/movies")->items == data->items);
    assert(!cache.find("different/movies"));
    assert(data->fresh(1001));
    assert(!data->fresh(1000 + 6 * 60 * 60));
    assert(!data->fresh(999));
    assert(cache.persist("source/movies", *data));
    VodCache reopened(directory / "cache");
    auto fromDisk = reopened.read("source/movies");
    assert(fromDisk && fromDisk->items->size() == 2 && (*fromDisk->items)[1].seriesId == "2");
    assert(!reopened.read("other/movies"));
    auto empty = std::make_shared<VodSnapshot>();
    empty->items = std::make_shared<const LiveM3u8ListResult>();
    empty->saved = 2000;
    assert(cache.persist("empty", *empty));
    assert(reopened.read("empty")->items->empty());
    for (int i = 0; i < 15; ++i)
        cache.remember("scope/" + std::to_string(i), data);
    assert(!cache.find("source/movies"));
    assert(cache.read("source/movies"));
    unsigned char regular[] = {0, 0, 255, 128, 0, 0, 0};
    unsigned char icon[] = {0, 0, 255, 128, 0, 0, 0};
    tsvitchEmbolden(regular, 7, 1, 7, 'A');
    assert(regular[2] == 255 && regular[3] > 128 && regular[3] < 200 && regular[0] == 0 && regular[6] == 0);
    for (auto codepoint : {0x05d0u, 0x0627u, 0x3042u, 0xac00u, 0x4e00u, 0x20000u}) {
        unsigned char glyph[] = {0, 0, 255, 128, 0, 0, 0};
        tsvitchEmbolden(glyph, 7, 1, 7, codepoint);
        assert(glyph[3] > 128);
    }
    tsvitchEmbolden(icon, 7, 1, 7, 0xe001);
    assert(icon[3] == 128);
    std::cout << "Persistent movie/series favorites, provider isolation, pin ordering, catalogue cache and "
                 "fractional text weight passed\n";
}

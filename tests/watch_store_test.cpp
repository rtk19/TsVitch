#include "utils/watch_store.hpp"
#include "utils/vod_helpers.hpp"
#include <cassert>
#include <unistd.h>
int main(){
    const auto dir=std::filesystem::temp_directory_path()/("tsvitch-watch-"+std::to_string(getpid()));
    std::filesystem::create_directories(dir);
    auto movie=tsvitch::LiveM3u8{};movie.contentType="movie";movie.id="7";movie.providerId="provider/account";movie.url="https://p/movie/u/p/7.mp4";
    {std::ofstream legacy(dir/"legacy.json");legacy<<nlohmann::json{{movie.url,{{"position",120},{"duration",300},{"timestamp",1}}}};}
    tsvitch::WatchStore store(dir/"watch.json",dir/"legacy.json");
    assert(store.progress(movie).resume()==120); // Old saved progress remains usable after 30 days.
    store.update(movie,2,300);assert(store.progress(movie).resume()==2);
    store.update(movie,285,300);assert(store.progress(movie).resume()==285); // No stale earlier position near the end.
    store.update(movie,300,300,true);assert(store.progress(movie).resume()==0&&store.progress(movie).fraction()==1);
    tsvitch::WatchStore restored(dir/"watch.json");assert(restored.progress(movie).completed);
    restored.clear(movie);assert(restored.progress(movie).position==0);
    tsvitch::WatchStore restarted(dir/"watch.json",dir/"legacy.json");assert(restarted.progress(movie).resume()==0);
    restored.update(movie,22,0);assert(restored.progress(movie).resume()==22);
    auto episode=movie;episode.contentType="series";episode.seriesId="show";episode.seriesTitle="Show";episode.season="1";episode.episode="1";
    auto second=episode;second.id="8";second.episode="2";second.url="https://p/series/u/p/8.mkv";
    restored.update(episode,30,90);restored.update(second,40,90);
    const auto recent=restored.recent("series");assert(recent.size()==1&&recent[0].id=="8");
    assert(restored.recent("movie").size()==1);
    auto other=second;other.providerId="other/account";assert(tsvitch::mediaKey(other)!=tsvitch::mediaKey(second));
    auto nextSeason=second;nextSeason.id="9";nextSeason.season="2";nextSeason.episode="1";
    tsvitch::LiveM3u8ListResult queue={nextSeason,second,episode};tsvitch::sortEpisodes(queue);
    assert(queue[0].id=="7"&&queue[1].id=="8"&&queue[2].id=="9");
    assert(tsvitch::hasNextEpisode(queue,0)&&tsvitch::hasNextEpisode(queue,1)&&!tsvitch::hasNextEpisode(queue,2));
    const auto filtered=tsvitch::recentForCatalogue(recent,{other},true);assert(filtered.empty());
    auto fresh=movie;fresh.url="https://p/movie/new-user/new-password/7.mp4";
    const auto renewed=tsvitch::recentForCatalogue({movie},{fresh},false);assert(renewed.size()==1&&renewed[0].url==fresh.url);
    queue.push_back(movie);assert(!tsvitch::hasNextEpisode(queue,2));
    restored.observe({movie,episode});const auto discovered=restored.added(movie);assert(discovered>0);
    tsvitch::WatchStore reloaded(dir/"watch.json");assert(reloaded.added(movie)==discovered);
    auto dated=movie;dated.addedAt=1234567890;assert(reloaded.added(dated)==1234567890);
    {std::ofstream broken(dir/"not-a-directory");broken<<"x";}
    tsvitch::WatchStore unwritable(dir/"not-a-directory"/"watch.json");unwritable.update(movie,100,300);assert(!unwritable.persisted);
    std::filesystem::remove_all(dir);
}

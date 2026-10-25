#include "api/tsvitch/result/home_live_result.h"
#include <cassert>
int main() {
    const auto legacy = nlohmann::json::parse(R"({"id":"7","chno":"1","title":"ערוץ","logo":"","groupTitle":"חדשות","url":"https://example.test/live/7.ts"})");
    const auto media = legacy.get<tsvitch::LiveM3u8>();
    assert(media.title == "ערוץ" && media.description.empty());
    auto nullMetadata = legacy;
    nullMetadata["description"] = nullptr; nullMetadata["epgId"] = nullptr;
    assert(nullMetadata.get<tsvitch::LiveM3u8>().description.empty());
    auto episode = media;
    episode.description = "תקציר פרק עם English 123";
    const nlohmann::json serialized = episode;
    const auto restored = serialized.get<tsvitch::LiveM3u8>();
    assert(restored.description == episode.description && restored.url == media.url);
}

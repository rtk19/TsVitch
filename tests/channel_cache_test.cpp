#include "core/ChannelManager.hpp"
#include <cassert>
#include <fstream>
#include <unistd.h>
int main() {
    const auto dir = std::filesystem::temp_directory_path() / ("tsvitch-cache-test-" + std::to_string(getpid()));
    std::filesystem::create_directories(dir);
    ChannelManager manager(dir);
    tsvitch::LiveM3u8 channel;
    channel.id = "8"; channel.title = "ערוץ"; channel.url = "https://example.test/live.ts";
    channel.epgId = "channel.il"; channel.description = "תקציר";
    channel.catchupMode="append";channel.catchupSource="?utc={utc}";channel.catchupDays=7;
    manager.saveBinary({channel});
    const auto restored = manager.loadBinary();
    assert(restored.size() == 1 && restored[0].epgId == channel.epgId && restored[0].description == channel.description);
    assert(restored[0].catchupMode=="append"&&restored[0].catchupDays==7&&restored[0].catchupSource==channel.catchupSource);
    const auto path = dir / "channels-v3.bin";
    std::filesystem::resize_file(path, 9);
    assert(manager.loadBinary().empty());
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        uint32_t count = 1, maliciousLength = 0xffffffff;
        output.write(reinterpret_cast<const char*>(&count), 4);
        output.write(reinterpret_cast<const char*>(&maliciousLength), 4);
    }
    assert(manager.loadBinary().empty());
    std::filesystem::remove_all(dir);
}

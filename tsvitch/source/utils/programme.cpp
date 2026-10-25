#include <borealis/core/activity.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/views/dialog.hpp>
#include "utils/programme.hpp"
#include "utils/catchup.hpp"
#include "utils/activity_helper.hpp"
#include "view/grid_dropdown.hpp"
#include "core/HistoryManager.hpp"

namespace tsvitch {
void chooseArchiveTime(const LiveM3u8 &channel) {
    if (!hasCatchup(channel))
        return;
    const auto now = std::time(nullptr);
    std::tm today{};
    localtime_r(&now, &today);
    today.tm_hour = today.tm_min = today.tm_sec = 0;
    std::vector<std::string> dates;
    std::vector<int64_t> days, dayEnds;
    const int count = std::min(366, channel.catchupDays > 0 ? channel.catchupDays : 7);
    for (int day = 0; day <= count; ++day) {
        auto date = today;
        date.tm_mday -= day;
        date.tm_isdst = -1;
        const auto epoch = std::mktime(&date);
        char label[32];
        std::strftime(label, sizeof(label), "%d/%m/%Y", &date);
        dates.push_back(label);
        days.push_back(epoch);
        ++date.tm_mday;
        date.tm_isdst = -1;
        dayEnds.push_back(std::mktime(&date));
    }
    BaseDropdown::text(
        brls::getStr("iptv/archive_date"), dates,
        [channel, days, dayEnds](int day) {
            if (day < 0 || static_cast<size_t>(day) >= days.size())
                return;
            const auto now = std::time(nullptr);
            std::vector<std::string> labels;
            std::vector<int64_t> starts;
            for (const auto start : archiveDaySlots(days[day], dayEnds[day])) {
                if (!archiveAvailable(channel, start, start + 1800, now))
                    continue;
                const std::time_t epoch = start;
                std::tm time{};
                localtime_r(&epoch, &time);
                char label[16];
                std::strftime(label, sizeof(label), "%H:%M", &time);
                labels.push_back(label);
                starts.push_back(start);
            }
            if (starts.empty()) {
                brls::Application::notify(brls::getStr("iptv/s26"));
                return;
            }
            BaseDropdown::text(
                brls::getStr("iptv/archive_time"), labels,
                [channel, starts](int index) {
                    if (index < 0 || static_cast<size_t>(index) >= starts.size())
                        return;
                    BaseDropdown::text(
                        brls::getStr("iptv/archive_duration"), {"30 min", "60 min", "90 min", "120 min"},
                        [channel, start = starts[index]](int choice) {
                            const int minutes[] = {30, 60, 90, 120};
                            if (choice < 0 || choice > 3)
                                return;
                            const auto now = std::time(nullptr);
                            auto media =
                                archiveMedia(channel, channel.title, brls::getStr("iptv/archive_by_time"),
                                             start, start + minutes[choice] * 60, now);
                            if (media.url.empty()) {
                                brls::Application::notify(brls::getStr("iptv/archive_unsupported"));
                                return;
                            }
                            HistoryManager::get()->add(media);
                            Intent::openLive({media}, 0, [] {});
                        },
                        1);
                },
                -1);
        },
        0);
}
void openProgrammeGrid(const LiveM3u8 &, const std::vector<Programme> &);
void presentProgrammeGuide(const LiveM3u8 &channel, const std::vector<Programme> &programmes) {
    if (programmes.empty()) {
        if (hasCatchup(channel))
            chooseArchiveTime(channel);
        else
            brls::Application::notify(brls::getStr("iptv/s26"));
        return;
    }
    openProgrammeGrid(channel, programmes);
}
} // namespace tsvitch

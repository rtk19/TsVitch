#include <borealis/core/activity.hpp>
#include "utils/programme.hpp"
#include "utils/epg_metadata.hpp"
#include "utils/catchup.hpp"
#include "utils/activity_helper.hpp"
#include "view/recycling_grid.hpp"
#include "view/icon_button.hpp"
#include "core/HistoryManager.hpp"
#include <borealis/core/activity.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/dialog.hpp>
namespace tsvitch {
namespace {
std::string tr(const char *key) { return brls::getStr(std::string("iptv/") + key); }
LiveM3u8 liveSource(LiveM3u8 channel) {
    if (!channel.liveUrl.empty()) {
        channel.url = channel.liveUrl;
        channel.title = channel.liveTitle;
    }
    channel.contentType = "live";
    channel.archiveStart = channel.archiveEnd = 0;
    return channel;
}
class ProgrammeRow : public RecyclingGridItem {
    brls::Label *title, *time, *status;

  public:
    ProgrammeRow() {
        setAxis(brls::Axis::COLUMN);
        setPadding(14);
        setCornerRadius(8);
        setHideHighlightBackground(true);
        setBackgroundColor(nvgRGB(27, 60, 99));
        title = new brls::Label;
        title->setId("programme/title");
        title->setFontSize(27);
        title->setHeight(40);
        title->setTextColor(nvgRGB(255, 255, 255));
        addView(title);
        auto *meta = new brls::Box(brls::Axis::ROW);
        addView(meta);
        time = new brls::Label;
        time->setId("programme/time");
        time->setFontSize(23);
        time->setGrow(1);
        time->setTextColor(nvgRGB(206, 224, 244));
        meta->addView(time);
        status = new brls::Label;
        status->setId("programme/status");
        status->setFontSize(22);
        status->setTextColor(nvgRGB(255, 220, 104));
        meta->addView(status);
    }
    void set(const Programme &p, const std::string &badge) {
        title->setText(p.title);
        time->setText(programmeTime(p));
        status->setText(badge);
    }
};
class ProgrammeSource : public RecyclingGridDataSource {
    LiveM3u8 channel;
    std::vector<Programme> programmes;
    int current;

  public:
    ProgrammeSource(LiveM3u8 channel, std::vector<Programme> programmes)
        : channel(std::move(channel)), programmes(std::move(programmes)),
          current(currentProgrammeIndex(this->programmes, programmeNow())) {}
    size_t getItemCount() override { return programmes.size(); }
    void clearData() override { programmes.clear(); }
    RecyclingGridItem *cellForRow(RecyclingGrid *grid, size_t index) override {
        auto *cell = static_cast<ProgrammeRow *>(grid->dequeueReusableCell("programme"));
        const auto &p = programmes[index];
        std::string badge = int(index) == current      ? tr("on_now")
                            : p.start > programmeNow() ? tr("upcoming")
                            : archiveAvailable(liveSource(channel), p.start, p.end, programmeNow())
                                ? tr("archive_badge")
                                : "";
        if (channel.archiveStart > 0 && channel.archiveStart == p.start)
            badge += (badge.empty() ? "" : " · ") + tr("playing_archive");
        cell->set(p, badge);
        return cell;
    }
    void onItemSelected(RecyclingGrid *, size_t index) override {
        if (index >= programmes.size())
            return;
        const auto p = programmes[index];
        auto *dialog = new brls::Dialog(p.title + "\n" + programmeTime(p) + "\n\n" + p.description);
        const auto source = liveSource(channel);
        if (archiveAvailable(source, p.start, p.end, programmeNow())) {
            LiveM3u8ListResult queue;
            size_t selected = 0;
            for (size_t i = 0; i < programmes.size(); ++i)
                if (archiveAvailable(source, programmes[i].start, programmes[i].end, programmeNow())) {
                    if (i == index)
                        selected = queue.size();
                    auto media = archiveMedia(source, programmes[i].title, programmes[i].description,
                                              programmes[i].start, programmes[i].end, programmeNow());
                    if (!media.url.empty())
                        queue.push_back(std::move(media));
                }
            dialog->addButton(tr("watch_archive"), [queue, selected] {
                if (selected >= queue.size())
                    return;
                HistoryManager::get()->add(queue[selected]);
                Intent::openLive(queue, selected, [] {});
            });
        }
        dialog->addButton(tr("watch_live"), [source] { Intent::openLive({source}, 0, [] {}); });
        dialog->addButton(brls::getStr("hints/back"), [] {});
        dialog->open();
    }
};
class GuidePage : public brls::Box {
    RecyclingGrid *grid = nullptr;

  public:
    GuidePage(const LiveM3u8 &channel, const std::vector<Programme> &programmes)
        : brls::Box(brls::Axis::COLUMN) {
        setPadding(18);
        setBackgroundColor(nvgRGB(14, 33, 56));
        auto *header = new brls::Box(brls::Axis::ROW);
        header->setHeight(72);
        header->setAlignItems(brls::AlignItems::CENTER);
        addView(header);
        auto *back = new IconButton;
        back->setText(brls::getStr("hints/back"));
        back->setIcon("back");
        back->registerClickAction([](brls::View *) {
            brls::Application::popActivity();
            return true;
        });
        header->addView(back);
        auto *title = new brls::Label;
        title->setText(channel.title + " · EPG");
        title->setFontSize(28);
        title->setGrow(1);
        title->setMarginLeft(18);
        title->setTextColor(nvgRGB(255, 255, 255));
        header->addView(title);
        if (hasCatchup(liveSource(channel))) {
            auto *archive = new IconButton;
            archive->setWidth(340);
            archive->setIcon("clock");
            archive->setText(tr("archive_by_time"));
            archive->registerClickAction([channel](brls::View *) {
                chooseArchiveTime(liveSource(channel));
                return true;
            });
            header->addView(archive);
        }
        grid = new RecyclingGrid;
        grid->setId("programme/grid");
        grid->spanCount = 1;
        grid->estimatedRowHeight = 110;
        grid->estimatedRowSpace = 10;
        grid->setGrow(1);
        grid->releaseDataSourceOnDestroy();
        grid->registerCell("programme", [] { return new ProgrammeRow; });
        addView(grid);
        int selected = currentProgrammeIndex(programmes, programmeNow());
        if (channel.archiveStart > 0)
            for (size_t i = 0; i < programmes.size(); ++i)
                if (programmes[i].start == channel.archiveStart)
                    selected = i;
        grid->setDefaultCellFocus(selected < 0 ? 0 : selected);
        grid->setDataSource(new ProgrammeSource(channel, programmes));
        registerAction(brls::getStr("hints/back"), brls::BUTTON_B, [](brls::View *) {
            brls::Application::popActivity();
            return true;
        });
    }
    brls::View *getDefaultFocus() override { return grid ? grid->getDefaultFocus() : nullptr; }
};
} // namespace
void openProgrammeGrid(const LiveM3u8 &channel, const std::vector<Programme> &programmes) {
    auto *frame = new brls::AppletFrame(new GuidePage(channel, programmes));
    frame->setHeaderVisibility(brls::Visibility::GONE);
    brls::Application::pushActivity(new brls::Activity(frame));
}
} // namespace tsvitch

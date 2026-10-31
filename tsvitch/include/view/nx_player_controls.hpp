#pragma once
#include <borealis/core/box.hpp>
#include <chrono>
#include <functional>
#include "utils/nx_player_controls.hpp"
namespace brls {
class Label;
}
class VideoView;
class SVGImage;

// The NX player overlay and input state machine, adapted to the shared IPTV mpv
// instance. Source opening, watch state and archive URL ownership stay with the
// activity; a scrub commits exactly one request through that owner.
class NxPlayerControls : public brls::Box {
  public:
    explicit NxPlayerControls(VideoView *host);
    ~NxPlayerControls() override;
    void configure(bool live, const std::string &title, const std::string &next,
                   std::function<void()> nextAction);
    void tick();
    void setBuffering(bool value);
    void show(bool visible);
    void toggleLock();
    void playPause();
    void openOptions();
    void scrubBy(double seconds);
    void scrubAxis(double axis, double dt);
    void commitSeek(double target);
    bool cancelSeek();
    bool blocked();
    bool isLive() const { return live; }
    bool locked() const { return controlsLocked; }
    bool shown() const { return controlsShown; }
    bool scrubbing() const { return seek.active; }
    double scrubTarget() const { return seek.target; }
    bool nextShown() const { return nextCardShown; }
    void setTitle(const std::string &title);
    void setTitleDetail(const std::string &detail);
    void flash(const std::string &text);
    std::function<void()> onBack, onProgramme;
    std::function<void()> onHeadingRefresh;
    std::function<void(int)> onChannel;
    std::function<void(double)> onSeek;

  private:
    using Clock = std::chrono::steady_clock;
    VideoView *host;
    bool boostApplied = false, buffering = false, speedPending = false;
    double requestedSpeed = 1;
    bool revealTitlePending = false;
    Clock::time_point headingRefreshAt{};
    brls::Box *bufferOverlay;
    bool live = true, controlsLocked = false, controlsShown = false, userPaused = false;
    bool nextCardShown = false, stickWasPushed = false, panelActive = false;
    nxplayer::Scrub seek;
    Clock::time_point hideAt{}, flashUntil{}, lockUntil{}, lastFrame = Clock::now();
    std::function<void()> nextAction;
    brls::Box *pauseOverlay, *pauseTitleBox, *optionsHint, *lockHint, *hintPill, *nextCard, *seekOverlay,
        *seekTrack, *seekFill, *seekCursor;
    brls::Label *titleLabel, *titleDetailLabel, *pillLabel, *nextCardSub, *seekCur, *seekTotal;
    SVGImage *lockIcon;
    bool ready() const;
    bool foreground() const;
    void beginSeek();
    void flashLock();
    void advance();
    void updateSeekBar();
};

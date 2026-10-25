// Player controls/UI ported from rtk19/nx-torrent-player/app/player.cpp,
// Apache-2.0. See resources/licenses/nx-torrent-player-LICENSE.txt.
#include "view/nx_player_controls.hpp"
#include "view/video_view.hpp"
#include "view/mpv_core.hpp"
#include "view/video_profile.hpp"
#include "view/svg_image.hpp"
#include "fragment/player_setting.hpp"
#include "utils/number_helper.hpp"
#include "utils/config_helper.hpp"
#ifdef __SWITCH__
#include <switch.h>
#endif
#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/touch/pan_gesture.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/progress_spinner.hpp>
#include <algorithm>
#include <cmath>
#include <fmt/format.h>
namespace {
std::string tr(const char *key) { return brls::getStr(std::string("iptv/") + key); }
class PlayPauseButton : public brls::Box {
  public:
    const bool *paused = nullptr;
    void draw(NVGcontext *vg, float x, float y, float w, float h, brls::Style style,
              brls::FrameContext *ctx) override {
        brls::Box::draw(vg, x, y, w, h, style, ctx); // the circular background
        float cx = x + w / 2.0f, cy = y + h / 2.0f;
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 240));
        if (paused && *paused) {
            // Play: a right-pointing triangle.
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx - 15.0f, cy - 27.0f);
            nvgLineTo(vg, cx + 28.0f, cy);
            nvgLineTo(vg, cx - 15.0f, cy + 27.0f);
            nvgClosePath(vg);
            nvgFill(vg);
        } else {
            // Pause: two rounded bars.
            const float bw = 13.0f, bh = 52.0f, gap = 16.0f;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx - gap / 2 - bw, cy - bh / 2, bw, bh, 3.0f);
            nvgRoundedRect(vg, cx + gap / 2, cy - bh / 2, bw, bh, 3.0f);
            nvgFill(vg);
        }
    }
};

} // namespace
NxPlayerControls::NxPlayerControls(VideoView *video) : host(video) {
    MPVCore::instance().nxNavigationActive = true;
    setId("nx/player/controls");
    setPositionType(brls::PositionType::ABSOLUTE);
    setPositionLeft(0);
    setPositionTop(0);
    setWidthPercentage(100);
    setHeightPercentage(100);
    // Centre play/pause button, shown with the controls overlay. The overlay box
    // spans the screen (to centre the button) but is NOT tappable itself -- only
    // the bounded button below is, so a tap anywhere else falls through to the
    // video's show/hide-overlay tap.
    pauseOverlay = new brls::Box();
    pauseOverlay->setAxis(brls::Axis::ROW);
    pauseOverlay->setJustifyContent(brls::JustifyContent::CENTER);
    pauseOverlay->setAlignItems(brls::AlignItems::CENTER);
    pauseOverlay->setGrow(1.0f);
    pauseOverlay->setPositionType(brls::PositionType::ABSOLUTE);
    pauseOverlay->setPositionTop(0.0f);
    pauseOverlay->setPositionLeft(0.0f);
    pauseOverlay->setWidthPercentage(100.0f);
    pauseOverlay->setHeightPercentage(100.0f);
    pauseOverlay->setVisibility(brls::Visibility::GONE);
    {
        auto *btn = new PlayPauseButton();
        btn->paused = &userPaused; // draws play vs pause from the live flag
        btn->setDimensions(140.0f, 140.0f);
        btn->setCornerRadius(70.0f);
        btn->setBackgroundColor(nvgRGBA(0, 0, 0, 90));
        // Tap the centre button to play/pause (works like the A button). Bounded,
        // so taps elsewhere toggle the overlay instead.
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn, [this]() {
            if (blocked())
                return;
            playPause();
        }));
        pauseOverlay->addView(btn);
    }
    this->addView(pauseOverlay);

    // Title, top-left, shown alongside the pause icon. Semi-transparent pill so
    // it reads over any frame.
    pauseTitleBox = new brls::Box();
    pauseTitleBox->setPositionType(brls::PositionType::ABSOLUTE);
    pauseTitleBox->setPositionTop(48.0f);
    pauseTitleBox->setPositionLeft(60.0f);
    pauseTitleBox->setPadding(12.0f, 22.0f, 12.0f, 22.0f);
    pauseTitleBox->setCornerRadius(8.0f);
    pauseTitleBox->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
    pauseTitleBox->setVisibility(brls::Visibility::GONE);
    {
        auto *tl = titleLabel = new brls::Label();
        tl->setText(std::string{});
        tl->setFontSize(26.0f);
        tl->setTextColor(nvgRGB(255, 255, 255));
        tl->setSingleLine(true);
        // Cap the width so a long title is cut with an ellipsis instead of
        // running across the screen into the top-right hints. Shortened to clear
        // the wider top-right cluster (Options + the Y-lock hint). Short titles
        // keep their natural width (the pill shrinks to fit).
        tl->setMaxWidth(760.0f);
        pauseTitleBox->addView(tl);
    }
    this->addView(pauseTitleBox);

    // Top-right cluster shown while paused: the Options hint, then the Y-lock
    // hint at the far right. Right-anchored so they stay together against the
    // edge, and each pill toggles its own visibility (the lock one also flashes
    // on a blocked input) without the other moving.
    auto *topRight = new brls::Box();
    topRight->setPositionType(brls::PositionType::ABSOLUTE);
    topRight->setPositionTop(48.0f);
    topRight->setPositionRight(60.0f);
    topRight->setAxis(brls::Axis::ROW);
    topRight->setAlignItems(brls::AlignItems::CENTER);
    this->addView(topRight);

    // The X button glyph + a gear: X opens the audio/subtitle options. Same
    // show/hide as the pause overlays. Nudged left of the Y-lock pill.
    optionsHint = new brls::Box();
    // Fixed height (not content-driven): keeps both pills the same thickness and
    // stops the cluster from reflowing -- and the lock pill from jumping a pixel
    // -- when Options is hidden on lock. Vertical space comes from centring.
    optionsHint->setHeight(56.0f);
    optionsHint->setPadding(4.0f, 20.0f, 0.0f, 20.0f);
    optionsHint->setMarginRight(16.0f);
    optionsHint->setCornerRadius(8.0f);
    optionsHint->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
    optionsHint->setAxis(brls::Axis::ROW);
    optionsHint->setAlignItems(brls::AlignItems::CENTER);
    optionsHint->setVisibility(brls::Visibility::GONE);
    {
        auto *x = new brls::Label();
        x->setText("X"); // borealis font: the X button glyph
        x->setFontSize(26.0f);
        x->setTextColor(nvgRGB(255, 255, 255));
        x->setMargins(0.0f, 12.0f, 0.0f, 0.0f);
        optionsHint->addView(x);

        // Gear from the Material Icons font bundled in romfs (settings glyph).
        auto *gear = new SVGImage;
        gear->setDimensions(26, 26);
        gear->setImageFromSVGRes("svg/bpx-svg-sprite-setting.svg");
        optionsHint->addView(gear);
    }
    topRight->addView(optionsHint);

    // The Y button glyph + "Lock": a hint that Y locks the controls while paused,
    // and the indicator that flashes when a blocked input is attempted (its label
    // then reads "Locked", see flashLock). Far right of the cluster.
    lockHint = new brls::Box();
    lockHint->setHeight(56.0f); // match optionsHint exactly (see the note there)
    lockHint->setPadding(4.0f, 20.0f, 0.0f, 20.0f);
    lockHint->setCornerRadius(8.0f);
    lockHint->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
    lockHint->setAxis(brls::Axis::ROW);
    lockHint->setAlignItems(brls::AlignItems::CENTER);
    lockHint->setVisibility(brls::Visibility::GONE);
    {
        auto *y = new brls::Label();
        y->setText("Y"); // borealis font: the Y button glyph (U+E0E3)
        y->setFontSize(26.0f);
        y->setTextColor(nvgRGB(255, 255, 255));
        y->setMargins(0.0f, 12.0f, 0.0f, 0.0f);
        lockHint->addView(y);

        // Padlock glyph (Material Icons): open = a hint that Y locks; closed =
        // locked. Nudged down a touch like the gear, which sits high in its box.
        lockIcon = new SVGImage;
        lockIcon->setDimensions(26, 26);
        lockIcon->setImageFromSVGRes("svg/player-unlock.svg");
        lockHint->addView(lockIcon);
    }
    topRight->addView(lockHint);

    // The L/R readout, top centre: shown for a moment on a speed or a
    // subtitle-delay change and hidden again by updatePill. Its own absolute,
    // full-width row so centring it does not disturb the pills above -- and
    // away from the bottom, where the subtitles it can be about are drawn.
    {
        auto *row = new brls::Box();
        row->setPositionType(brls::PositionType::ABSOLUTE);
        row->setPositionTop(48.0f);
        row->setPositionLeft(0.0f);
        row->setWidthPercentage(100.0f);
        row->setAxis(brls::Axis::ROW);
        row->setJustifyContent(brls::JustifyContent::CENTER);

        hintPill = new brls::Box();
        hintPill->setHeight(56.0f);
        hintPill->setPadding(0.0f, 24.0f, 0.0f, 24.0f);
        hintPill->setCornerRadius(8.0f);
        hintPill->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
        hintPill->setAxis(brls::Axis::ROW);
        hintPill->setAlignItems(brls::AlignItems::CENTER);
        hintPill->setVisibility(brls::Visibility::GONE);
        {
            // Words, not a glyph. The pill says two different things
            // ("Speed 1.25x", "Subtitles +0.10 s") and no one icon covers both
            // -- and a caption glyph could not be drawn anyway: every Material
            // one lives in U+E000-E152, which the Nintendo Extended shared font
            // claims ahead of Material in the fallback chain.
            pillLabel = new brls::Label();
            pillLabel->setText("");
            pillLabel->setFontSize(24.0f);
            pillLabel->setTextColor(nvgRGB(255, 255, 255));
            hintPill->addView(pillLabel);
        }
        row->addView(hintPill);
        this->addView(row);
    }

    // "Next episode", bottom right, over the last kNextCardSecs of an episode
    // that has one. Above the seek bar so the two never overlap, and on the
    // right so it stays clear of the title pill on the left.
    nextCard = new brls::Box();
    nextCard->setAxis(brls::Axis::COLUMN);
    nextCard->setPositionType(brls::PositionType::ABSOLUTE);
    nextCard->setPositionBottom(132.0f);
    nextCard->setPositionRight(70.0f);
    nextCard->setPadding(14.0f, 22.0f, 16.0f, 22.0f);
    nextCard->setCornerRadius(10.0f);
    nextCard->setBackgroundColor(nvgRGBA(0, 0, 0, 170));
    nextCard->setVisibility(brls::Visibility::GONE);
    {
        // The A button glyph, then the words. Two labels rather than one
        // string so the glyph can carry its own size -- it sits small next to
        // 23pt text otherwise. Same font as the X and Y glyphs on the pills
        // above (Nintendo Extended, which owns U+E000-E152).
        auto *headRow = new brls::Box();
        headRow->setAxis(brls::Axis::ROW);
        headRow->setAlignItems(brls::AlignItems::CENTER);

        auto *glyph = new brls::Label();
        glyph->setText("A"); // U+E0E0, the A button
        glyph->setFontSize(26.0f);
        glyph->setTextColor(nvgRGB(255, 255, 255));
        glyph->setMarginRight(10.0f);
        headRow->addView(glyph);

        auto *head = new brls::Label();
        head->setText(tr("next_episode"));
        head->setFontSize(23.0f);
        head->setTextColor(nvgRGB(255, 255, 255));
        headRow->addView(head);

        nextCard->addView(headRow);

        nextCardSub = new brls::Label();
        nextCardSub->setText("");
        nextCardSub->setFontSize(18.0f);
        nextCardSub->setTextColor(nvgRGB(186, 186, 192));
        nextCardSub->setMarginTop(4.0f);
        nextCard->addView(nextCardSub);
    }
    // Touch works whatever the playback state, which is the way through while
    // paused (A resumes then).
    nextCardSub->setMaxWidth(560);
    nextCardSub->setSingleLine(true);
    nextCard->addGestureRecognizer(new brls::TapGestureRecognizer(nextCard, [this]() {
        if (blocked())
            return;
        advance();
    }));
    this->addView(nextCard);

    // Seek bar at the bottom while paused: elapsed | progress | total.
    seekOverlay = new brls::Box();
    seekOverlay->setAxis(brls::Axis::ROW);
    seekOverlay->setAlignItems(brls::AlignItems::CENTER);
    seekOverlay->setPositionType(brls::PositionType::ABSOLUTE);
    seekOverlay->setPositionLeft(70.0f);
    seekOverlay->setPositionRight(70.0f);
    seekOverlay->setPositionBottom(56.0f);
    seekOverlay->setHeight(64.0f);
    seekOverlay->setPadding(0, 26, 0, 26);
    seekOverlay->setCornerRadius(10.0f);
    seekOverlay->setBackgroundColor(nvgRGBA(0, 0, 0, 165));
    seekOverlay->setVisibility(brls::Visibility::GONE);

    // Fixed width, or the bar jitters: a Label sizes to its text, the track
    // grows into whatever is left, so every "9:59" -> "10:00" widened the label
    // and shoved the track a few pixels. While scrubbing the timer changes
    // constantly, which made the whole bar shiver. Wide enough for "H:MM:SS".
    seekCur = new brls::Label();
    seekCur->setText("0:00");
    seekCur->setFontSize(22);
    seekCur->setTextColor(nvgRGB(255, 255, 255));
    seekCur->setWidth(104.0f);
    seekCur->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    seekCur->setMargins(0, 20, 0, 0);
    seekOverlay->addView(seekCur);

    seekTrack = new brls::Box();
    seekTrack->setAxis(brls::Axis::ROW);
    seekTrack->setAlignItems(brls::AlignItems::CENTER);
    seekTrack->setGrow(1.0f);
    seekTrack->setHeight(8.0f);
    seekTrack->setCornerRadius(4.0f);
    seekTrack->setBackgroundColor(nvgRGBA(255, 255, 255, 45));

    seekFill = new brls::Box();
    seekFill->setHeight(8.0f);
    seekFill->setWidth(0.0f); // driven in pixels from the measured track width
    seekFill->setCornerRadius(4.0f);
    seekFill->setBackgroundColor(brls::Application::getTheme().getColor("brls/accent"));
    seekTrack->addView(seekFill);

    // Cursor: absolutely positioned, so it is NOT part of the row's flex flow.
    // As a normal child it sat next to the fill and competed for width, so the
    // fill got squeezed by the cursor's 6 px and could never reach 100% -- the
    // cursor visibly pushed the bar instead of riding on it. Its left offset is
    // driven from the same percentage as the fill (see updateSeekBar).
    seekCursor = new brls::Box();
    seekCursor->setPositionType(brls::PositionType::ABSOLUTE);
    seekCursor->setPositionLeft(0.0f);
    seekCursor->setDimensions(6.0f, 26.0f);
    seekCursor->setCornerRadius(3.0f);
    seekCursor->setBackgroundColor(nvgRGB(255, 255, 255));
    seekTrack->addView(seekCursor);

    seekOverlay->addView(seekTrack);

    seekTotal = new brls::Label();
    seekTotal->setText("0:00");
    seekTotal->setFontSize(22);
    seekTotal->setTextColor(nvgRGB(255, 255, 255));
    seekTotal->setWidth(104.0f); // fixed for the same reason as seekCur
    seekTotal->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    seekTotal->setMargins(0, 0, 0, 20);
    seekOverlay->addView(seekTotal);
    this->addView(seekOverlay);

    bufferOverlay = new brls::Box(brls::Axis::ROW);
    bufferOverlay->setPositionType(brls::PositionType::ABSOLUTE);
    bufferOverlay->setPositionTop(124);
    bufferOverlay->setPositionRight(60);
    bufferOverlay->setPadding(12, 20, 12, 20);
    bufferOverlay->setCornerRadius(8);
    bufferOverlay->setAlignItems(brls::AlignItems::CENTER);
    bufferOverlay->setBackgroundColor(nvgRGBA(0, 0, 0, 170));
    bufferOverlay->setVisibility(brls::Visibility::GONE);
    auto *spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::NORMAL);
    spinner->setDimensions(28, 28);
    spinner->animate(true);
    bufferOverlay->addView(spinner);
    auto *caption = new brls::Label;
    caption->setText(tr("player_buffering"));
    caption->setFontSize(22);
    caption->setTextColor(nvgRGB(255, 255, 255));
    caption->setMarginLeft(12);
    bufferOverlay->addView(caption);
    addView(bufferOverlay);
    pauseTitleBox->setId("nx/player/title");
    seekOverlay->setId("nx/player/seek");
    hintPill->setId("nx/player/feedback");
    nextCard->setId("nx/player/next");
    optionsHint->addGestureRecognizer(new brls::TapGestureRecognizer(optionsHint, [this] {
        if (!blocked())
            openOptions();
    }));
    lockHint->addGestureRecognizer(new brls::TapGestureRecognizer(lockHint, [this] { toggleLock(); }));
    seekOverlay->addGestureRecognizer(
        new brls::TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound *) {
            if (blocked() || live || !ready() || status.state != brls::GestureState::END)
                return;
            const float w = seekTrack->getWidth();
            if (w > 0)
                commitSeek(std::clamp(double((status.position.x - seekTrack->getX()) / w), 0., 1.) *
                           host->getRealDuration());
        }));
    seekOverlay->addGestureRecognizer(new brls::PanGestureRecognizer(
        [this](brls::PanGestureStatus status, brls::Sound *) {
            if (blocked() || live || !ready())
                return;
            const float w = seekTrack->getWidth();
            if (w <= 0)
                return;
            if (status.state == brls::GestureState::FAILED) {
                cancelSeek();
                return;
            }
            if (!seek.active)
                beginSeek();
            if (!seek.active)
                return;
            seek.target =
                std::clamp(double((status.position.x - seekTrack->getX()) / w), 0., 1.) * seek.duration;
            if (status.state == brls::GestureState::END)
                commitSeek(seek.target);
            else
                show(true);
        },
        brls::PanAxis::HORIZONTAL));
    auto action = [this](brls::ControllerButton button, const std::string &text, brls::ActionListener cb,
                         bool repeat = false) {
        host->registerAction(text, button, cb, true, repeat, brls::SOUND_NONE);
    };
    action(brls::BUTTON_Y, tr("player_lock"), [this](brls::View *) {
        toggleLock();
        return true;
    });
    action(brls::BUTTON_B, brls::getStr("hints/back"), [this](brls::View *) {
        if (blocked())
            return true;
        if (!cancelSeek() && onBack)
            onBack();
        return true;
    });
    action(brls::BUTTON_A, tr("player_pause"), [this](brls::View *) {
        if (blocked())
            return true;
        if (nextCardShown && !MPVCore::instance().isPaused() && !seek.active)
            advance();
        else
            playPause();
        return true;
    });
    action(
        brls::BUTTON_LEFT, tr("player_seek_back"),
        [this](brls::View *) {
            scrubBy(-5);
            return true;
        },
        true);
    action(
        brls::BUTTON_RIGHT, tr("player_seek_forward"),
        [this](brls::View *) {
            scrubBy(5);
            return true;
        },
        true);
    for (auto button :
         {brls::BUTTON_NAV_UP, brls::BUTTON_NAV_DOWN, brls::BUTTON_NAV_LEFT, brls::BUTTON_NAV_RIGHT})
        action(button, "", [this](brls::View *) {
            if (!blocked())
                show(true);
            return true;
        });
    auto shoulder = [this](int direction) {
        if (blocked())
            return;
        if (live) {
            if (onChannel)
                onChannel(direction);
            return;
        }
        if (!ready())
            return;
        auto &core = MPVCore::instance();
        const auto speed = nxplayer::stepSpeed(speedPending ? requestedSpeed : core.getSpeed(), direction);
        requestedSpeed = speed;
        speedPending = true;
        core.setSpeed(speed);
        flash(tr("playback_speed") + fmt::format("  {:.2f}×", speed));
    };
    action(brls::BUTTON_LB, tr("player_slower"), [shoulder](brls::View *) {
        shoulder(-1);
        return true;
    });
    action(brls::BUTTON_RB, tr("player_faster"), [shoulder](brls::View *) {
        shoulder(1);
        return true;
    });
    action(brls::BUTTON_X, tr("player_options"), [this](brls::View *) {
        if (!blocked())
            openOptions();
        return true;
    });
    action(brls::BUTTON_RT, tr("player_info"), [this](brls::View *) {
        if (!blocked())
            host->togglePlaybackInfo();
        return true;
    });
    action(brls::BUTTON_LT, tr("s24"), [this](brls::View *) {
        if (!blocked() && onProgramme)
            onProgramme();
        return true;
    });
    action(brls::BUTTON_BACK, tr("player_favourite"), [this](brls::View *) {
        if (!blocked())
            host->toggleFavorite();
        return true;
    });
    // Pointer input is independent of focus. Children interrupt this recognizer.
    host->addGestureRecognizer(new brls::TapGestureRecognizer(host, [this] {
        if (!blocked() && ready())
            show(!controlsShown);
    }));
    show(false);
}
NxPlayerControls::~NxPlayerControls() {
    MPVCore::instance().nxNavigationActive = false;
    if (boostApplied && MPVCore::instance().isValid())
        MPVCore::instance().command_async("af", "remove", "@nx-boost");
}
bool NxPlayerControls::foreground() const {
    const auto &activities = brls::Application::getActivitiesStack();
    return !activities.empty() && activities.back()->getView("video") == host;
}
bool NxPlayerControls::ready() const {
    auto &core = MPVCore::instance();
    return !core.isStopped() && !core.isPositioning();
}
void NxPlayerControls::setTitle(const std::string &title) { titleLabel->setText(title); }
void NxPlayerControls::configure(bool isLive, const std::string &title, const std::string &next,
                                 std::function<void()> action) {
    speedPending = false;
    buffering = false;
    bufferOverlay->setVisibility(brls::Visibility::GONE);
    live = isLive;
    setTitle(title);
    seek = {};
    nextAction = std::move(action);
    nextCardSub->setText(next);
    nextCardShown = false;
    nextCard->setVisibility(brls::Visibility::GONE);
    if (live)
        MPVCore::instance().setSpeed(1);
    show(false);
}
void NxPlayerControls::show(bool visible) {
    controlsShown = visible;
    const auto v = visible ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
    for (auto *view : {pauseOverlay, pauseTitleBox, optionsHint})
        view->setVisibility(v);
    seekOverlay->setVisibility(visible && !live ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (Clock::now() >= lockUntil)
        lockHint->setVisibility(v);
    if (visible)
        hideAt = Clock::now() + std::chrono::seconds(4);
}
void NxPlayerControls::flashLock() {
    lockIcon->setImageFromSVGRes(controlsLocked ? "svg/player-lock.svg" : "svg/player-unlock.svg");
    lockHint->setVisibility(brls::Visibility::VISIBLE);
    lockUntil = Clock::now() + std::chrono::milliseconds(1400);
}
bool NxPlayerControls::blocked() {
    if (controlsLocked) {
        flashLock();
        return true;
    }
    return false;
}
void NxPlayerControls::toggleLock() {
    controlsLocked = !controlsLocked;
    if (controlsLocked) {
        const bool wasScrubbing = seek.active;
        seek.active = false;
        if (wasScrubbing || MPVCore::instance().isPaused())
            MPVCore::instance().resume();
        show(false);
        host->videoProfile->setVisibility(brls::Visibility::INVISIBLE);
    }
    flashLock();
}
void NxPlayerControls::playPause() {
    if (blocked() || !ready())
        return;
    if (seek.active) {
        commitSeek(seek.target);
        return;
    }
    auto &core = MPVCore::instance();
    if (core.isPaused())
        core.resume();
    else
        core.pause();
    show(!core.isPaused());
}
void NxPlayerControls::beginSeek() {
    if (live || !ready() || host->getRealDuration() <= 0)
        return;
    seek.begin(host->playbackPosition(), host->getRealDuration(), MPVCore::instance().isPaused());
    MPVCore::instance().pause();
    show(true);
}
void NxPlayerControls::scrubBy(double seconds) {
    if (blocked() || live || !ready())
        return;
    if (!seek.active)
        beginSeek();
    if (seek.active) {
        seek.move(seconds);
        show(true);
    }
}
void NxPlayerControls::scrubAxis(double axis, double dt) {
    if (blocked() || live || !ready())
        return;
    if (std::abs(axis) >= .15 && !seek.active)
        beginSeek();
    if (seek.active) {
        seek.stick(axis, dt);
        show(true);
    }
}
bool NxPlayerControls::cancelSeek() {
    if (!seek.active)
        return false;
    const bool paused = seek.wasPaused;
    seek.active = false;
    MPVCore::instance().command_async("set", "pause", paused ? "yes" : "no");
    show(paused);
    return true;
}
void NxPlayerControls::commitSeek(double target) {
    if (blocked() || live || !ready())
        return;
    target = std::clamp(target, 0., std::max(0., double(host->getRealDuration()) - .1));
    seek.active = false;
    MPVCore::instance().setProperty("pause", "no");
    if (onSeek)
        onSeek(target);
    else if (host->archiveSeek)
        host->archiveSeek(target);
    else
        MPVCore::instance().seek(std::to_string(target));
    show(false);
}
void NxPlayerControls::advance() {
    if (blocked() || !nextAction)
        return;
    nextCardShown = false;
    nextCard->setVisibility(brls::Visibility::GONE);
    auto action = nextAction;
    nextAction = {};
    brls::sync(action);
}
void NxPlayerControls::openOptions() {
    if (!ready())
        return;
    show(false);
    panelActive = true;
    brls::Application::pushActivity(new brls::Activity(new PlayerSetting));
}
void NxPlayerControls::flash(const std::string &text) {
    pillLabel->setText(text);
    hintPill->setVisibility(brls::Visibility::VISIBLE);
    flashUntil = Clock::now() + std::chrono::milliseconds(1600);
}
void NxPlayerControls::updateSeekBar() {
    const double duration = seek.active ? seek.duration : host->getRealDuration();
    const double position = host->playbackPosition(), target = seek.active ? seek.target : position;
    seekCur->setText(tsvitch::sec2Time(std::max(0., target)));
    seekTotal->setText(tsvitch::sec2Time(std::max(0., duration)));
    const float width = seekTrack->getWidth();
    if (width <= 0)
        return;
    auto frac = [duration](double value) { return duration > 0 ? std::clamp(value / duration, 0., 1.) : 0.; };
    seekFill->setWidth(width * frac(position));
    seekCursor->setPositionLeft(std::clamp(float(width * frac(target) - 3), 0.f, std::max(0.f, width - 6)));
}
void NxPlayerControls::setBuffering(bool value) {
    buffering = value;
    bufferOverlay->setVisibility(value && !MPVCore::instance().isPositioning() ? brls::Visibility::VISIBLE
                                                                               : brls::Visibility::GONE);
}
void NxPlayerControls::tick() {
    auto now = Clock::now();
    auto &core = MPVCore::instance();
    bool wantBoost = ProgramConfig::instance().getSettingItem(SettingItem::AUDIO_BOOST, false);
#ifdef __SWITCH__
    wantBoost = wantBoost && appletGetOperationMode() == AppletOperationMode_Handheld;
#endif
    if (!core.isPositioning() && wantBoost != boostApplied) {
        boostApplied = wantBoost;
        if (wantBoost)
            core.command_async("af", "add", "@nx-boost:lavfi=[dynaudnorm=f=200:g=11:p=0.9:m=10]");
        else
            core.command_async("af", "remove", "@nx-boost");
    }
    if (speedPending && !core.isPositioning() && std::abs(core.getSpeed() - requestedSpeed) < .001)
        speedPending = false;
    setBuffering(buffering);
    const bool active = foreground();
    const double dt = std::chrono::duration<double>(now - lastFrame).count();
    lastFrame = now;
    if (!active) {
        if (controlsShown)
            show(false);
        nextCard->setVisibility(brls::Visibility::GONE);
        return;
    }
    if (panelActive) {
        panelActive = false;
        show(core.isPaused() || seek.active);
    }
    userPaused = core.isPaused() || seek.active;
    brls::ControllerState state{};
    brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&state);
    // Digital navigation already takes 5-second steps; don't also process a
    // synthetic stick navigation press. Physical axes use NX's squared tilt.
    const float lx = state.axes[brls::ControllerAxis::LEFT_X], rx = state.axes[brls::ControllerAxis::RIGHT_X];
    const float axis = std::abs(lx) >= std::abs(rx) ? lx : rx;
    const bool pushed = std::abs(axis) >= .15;
    if (controlsLocked) {
        if (pushed && !stickWasPushed)
            flashLock();
    } else if (!live && ready()) {
        if (pushed)
            scrubAxis(axis, dt);
        else if (seek.active)
            seek.stick(0, dt);
    }
    stickWasPushed = pushed;
    if (controlsShown && !userPaused && !seek.active && now >= hideAt)
        show(false);
    if (now >= flashUntil)
        hintPill->setVisibility(brls::Visibility::GONE);
    if (now >= lockUntil)
        lockHint->setVisibility(controlsShown && !controlsLocked ? brls::Visibility::VISIBLE
                                                                 : brls::Visibility::GONE);
    const double left = host->getRealDuration() - host->playbackPosition();
    nextCardShown = !live && !controlsLocked && ready() && bool(nextAction) && host->getRealDuration() > 0 &&
                    left > 0 && left <= 30;
    nextCard->setVisibility(nextCardShown ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (controlsShown && !live)
        updateSeekBar();
}

#include <borealis/core/i18n.hpp>
#include <borealis/views/label.hpp>
#include <algorithm>
#include <cmath>

#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_slider.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include <borealis/views/cells/cell_detail.hpp>

#include "utils/config_helper.hpp"
#include "utils/shader_helper.hpp"
#include "utils/number_helper.hpp"
#include "utils/activity_helper.hpp"

#include "fragment/player_setting.hpp"

#include "view/button_close.hpp"

#include "view/video_view.hpp"
#include "view/selector_cell.hpp"
#include "view/svg_image.hpp"
#include "view/mpv_core.hpp"

using namespace brls::literals;

PlayerSetting::PlayerSetting() {
    this->inflateFromXMLRes("xml/fragment/player_setting.xml");
    brls::Logger::debug("Fragment PlayerSetting: create");

    setupCommonSetting();

    this->registerAction("hints/cancel"_i18n, brls::BUTTON_B, [](...) {
        brls::Application::popActivity();
        return true;
    });

    this->cancel->registerClickAction([](...) {
        brls::Application::popActivity();
        return true;
    });
    this->cancel->addGestureRecognizer(new brls::TapGestureRecognizer(this->cancel));

    closebtn->registerClickAction([](...) {
        brls::Application::popActivity();
        return true;
    });
}

PlayerSetting::~PlayerSetting() {
    *alive = false;
    brls::Logger::debug("Fragment PlayerSetting: delete");
}

brls::View *PlayerSetting::create() { return new PlayerSetting(); }

bool PlayerSetting::isTranslucent() { return true; }

brls::View *PlayerSetting::getDefaultFocus() { return this->settings->getDefaultFocus(); }

void PlayerSetting::setupCommonSetting() {
    auto locale = brls::Application::getLocale();
    setupPlaybackOptions();

    btnMirror->init("tsvitch/player/setting/common/mirror"_i18n, MPVCore::VIDEO_MIRROR, [](bool value) {
        MPVCore::instance().setMirror(!MPVCore::VIDEO_MIRROR);
        GA("player_setting", {{"mirror", value ? "true" : "false"}});

        if (MPVCore::HARDWARE_DEC) {
            std::string hwdec = MPVCore::VIDEO_MIRROR ? "auto-copy" : MPVCore::PLAYER_HWDEC_METHOD;
            MPVCore::instance().command_async("set", "hwdec", hwdec);
            brls::Logger::info("MPV hardware decode: {}", hwdec);
        }
    });

    btnSleep->setText("tsvitch/setting/app/playback/sleep"_i18n);
    updateCountdown(tsvitch::getUnixTime());
    btnSleep->registerClickAction([this](View *view) {
        std::vector<int> timeList = {15, 30, 60, 90, 120};
        std::string min = "tsvitch/home/common/min"_i18n;
        std::vector<std::string> optionList = {"15 " + min, "30 " + min, "60 " + min, "90 " + min,
                                               "120 " + min};
        bool countdownStarted = MPVCore::CLOSE_TIME != 0 && tsvitch::getUnixTime() < MPVCore::CLOSE_TIME;
        if (countdownStarted) {
            timeList.insert(timeList.begin(), -1);
            optionList.insert(optionList.begin(), "hints/off"_i18n);
        }
        BaseDropdown::text(
            "tsvitch/setting/app/playback/sleep"_i18n, optionList,
            [this, timeList, countdownStarted](int data) {
                if (countdownStarted && data == 0) {
                    MPVCore::CLOSE_TIME = 0;
                    GA("player_setting", {{"sleep", "-1"}});
                } else {
                    MPVCore::CLOSE_TIME = tsvitch::getUnixTime() + timeList[data] * 60;
                    GA("player_setting", {{"sleep", timeList[data]}});
                }
                updateCountdown(tsvitch::getUnixTime());
            },
            -1);
        return true;
    });

#ifdef ALLOW_FULLSCREEN
    auto &conf = ProgramConfig::instance();
    btnFullscreen->init("tsvitch/setting/app/others/fullscreen"_i18n,
                        conf.getBoolOption(SettingItem::FULLSCREEN), [](bool value) {
                            ProgramConfig::instance().setSettingItem(SettingItem::FULLSCREEN, value);

                            VideoContext::FULLSCREEN = value;

                            brls::Application::getPlatform()->getVideoContext()->fullScreen(value);
                            GA("player_setting", {{"fullscreen", value ? "true" : "false"}});
                        });

    auto setOnTopCell = [this](bool enabled) {
        if (enabled) {
            btnOnTopMode->setDetailTextColor(brls::Application::getTheme()["brls/list/listItem_value_color"]);
        } else {
            btnOnTopMode->setDetailTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        }
    };
    setOnTopCell(conf.getIntOptionIndex(SettingItem::ON_TOP_MODE) != 0);
    int onTopModeIndex = conf.getIntOption(SettingItem::ON_TOP_MODE);
    btnOnTopMode->setText("tsvitch/setting/app/others/always_on_top"_i18n);
    std::vector<std::string> onTopOptionList = {"hints/off"_i18n, "hints/on"_i18n,
                                                "tsvitch/player/setting/aspect/auto"_i18n};
    btnOnTopMode->setDetailText(onTopOptionList[onTopModeIndex]);
    btnOnTopMode->registerClickAction([this, onTopOptionList, setOnTopCell](brls::View *view) {
        BaseDropdown::text(
            "tsvitch/setting/app/others/always_on_top"_i18n, onTopOptionList,
            [this, onTopOptionList, setOnTopCell](int data) {
                btnOnTopMode->setDetailText(onTopOptionList[data]);
                ProgramConfig::instance().setSettingItem(SettingItem::ON_TOP_MODE, data);
                ProgramConfig::instance().checkOnTop();
                setOnTopCell(data != 0);
                GA("player_setting", {{"on_top_mode", data}});
            },
            ProgramConfig::instance().getIntOption(SettingItem::ON_TOP_MODE),
            "tsvitch/setting/app/others/always_on_top_hint"_i18n);
        return true;
    });

#else
    btnFullscreen->setVisibility(brls::Visibility::GONE);
    btnOnTopMode->setVisibility(brls::Visibility::GONE);
#endif

    btnEqualizerReset->registerClickAction([this](View *view) {
        btnEqualizerBrightness->slider->setProgress(0.5f);
        btnEqualizerContrast->slider->setProgress(0.5f);
        btnEqualizerSaturation->slider->setProgress(0.5f);
        btnEqualizerGamma->slider->setProgress(0.5f);
        btnEqualizerHue->slider->setProgress(0.5f);
        return true;
    });
    registerHideBackground(btnEqualizerReset);

    setupEqualizerSetting(btnEqualizerBrightness, "tsvitch/player/setting/equalizer/brightness"_i18n,
                          SettingItem::PLAYER_BRIGHTNESS, MPVCore::instance().getBrightness());
    setupEqualizerSetting(btnEqualizerContrast, "tsvitch/player/setting/equalizer/contrast"_i18n,
                          SettingItem::PLAYER_CONTRAST, MPVCore::instance().getContrast());
    setupEqualizerSetting(btnEqualizerSaturation, "tsvitch/player/setting/equalizer/saturation"_i18n,
                          SettingItem::PLAYER_SATURATION, MPVCore::instance().getSaturation());
    setupEqualizerSetting(btnEqualizerGamma, "tsvitch/player/setting/equalizer/gamma"_i18n,
                          SettingItem::PLAYER_GAMMA, MPVCore::instance().getGamma());
    setupEqualizerSetting(btnEqualizerHue, "tsvitch/player/setting/equalizer/hue"_i18n,
                          SettingItem::PLAYER_HUE, MPVCore::instance().getHue());
}

void PlayerSetting::setupEqualizerSetting(brls::SliderCell *cell, const std::string &title, SettingItem item,
                                          int initValue) {
    if (initValue < -100)
        initValue = -100;
    if (initValue > 100)
        initValue = 100;
    cell->detail->setWidth(50);
    cell->title->setWidth(116);
    cell->title->setMarginRight(0);
    cell->slider->setStep(0.05f);
    cell->slider->setMarginRight(0);
    cell->slider->setPointerSize(20);
    cell->setDetailText(std::to_string(initValue));
    cell->init(title, (initValue + 100) * 0.005f, [cell, item](float value) {
        int data = (int)(value * 200 - 100);
        if (data < -100)
            data = -100;
        if (data > 100)
            data = 100;
        cell->detail->setText(std::to_string(data));
        switch (item) {
        case SettingItem::PLAYER_BRIGHTNESS:
            MPVCore::instance().setBrightness(data);
            break;
        case SettingItem::PLAYER_CONTRAST:
            MPVCore::instance().setContrast(data);
            break;
        case SettingItem::PLAYER_SATURATION:
            MPVCore::instance().setSaturation(data);
            break;
        case SettingItem::PLAYER_GAMMA:
            MPVCore::instance().setGamma(data);
            break;
        case SettingItem::PLAYER_HUE:
            MPVCore::instance().setHue(data);
            break;
        default:
            break;
        }
        static size_t iter = 0;
        brls::cancelDelay(iter);
        iter = brls::delay(200, []() {
            ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_BRIGHTNESS,
                                                     MPVCore::VIDEO_BRIGHTNESS, false);
            ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_CONTRAST, MPVCore::VIDEO_CONTRAST,
                                                     false);
            ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_SATURATION,
                                                     MPVCore::VIDEO_SATURATION, false);
            ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_GAMMA, MPVCore::VIDEO_GAMMA, false);
            ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_HUE, MPVCore::VIDEO_HUE, false);
            ProgramConfig::instance().save();
        });
    });
    registerHideBackground(cell->getDefaultFocus());
}

void PlayerSetting::registerHideBackground(brls::View *view) {
    view->getFocusEvent()->subscribe(
        [this](...) { this->setBackgroundColor(nvgRGBAf(0.0f, 0.0f, 0.0f, 0.0f)); });

    view->getFocusLostEvent()->subscribe(
        [this](...) { this->setBackgroundColor(brls::Application::getTheme().getColor("brls/backdrop")); });
}

void PlayerSetting::draw(NVGcontext *vg, float x, float y, float width, float height, brls::Style style,
                         brls::FrameContext *ctx) {
    static size_t updateTime = 0;
    size_t now = tsvitch::getUnixTime();
    if (now != updateTime) {
        updatePlaybackOptions();
        updateTime = now;
        updateCountdown(now);
    }
    Box::draw(vg, x, y, width, height, style, ctx);
}

void PlayerSetting::updateCountdown(size_t now) {
    if (MPVCore::CLOSE_TIME == 0 || now > MPVCore::CLOSE_TIME) {
        btnSleep->setDetailTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        btnSleep->setDetailText("hints/off"_i18n);
    } else {
        btnSleep->setDetailTextColor(brls::Application::getTheme()["brls/list/listItem_value_color"]);
        btnSleep->setDetailText(tsvitch::sec2Time(MPVCore::CLOSE_TIME - now));
    }
}

namespace {
class PlayerOption : public brls::Box {
  public:
    brls::Label *value;
    PlayerOption(const std::string &id, const std::string &title) : brls::Box(brls::Axis::COLUMN) {
        setId(id);
        setFocusable(true);
        setPadding(12);
        setHeight(90);
        setMarginBottom(8);
        setCornerRadius(8);
        auto *caption = new brls::Label;
        caption->setText(title);
        caption->setFontSize(24);
        caption->setHeight(30);
        addView(caption);
        value = new brls::Label;
        value->setFontSize(22);
        value->setHeight(30);
        value->setTextColor(brls::Application::getTheme()["brls/list/listItem_value_color"]);
        addView(value);
        addGestureRecognizer(new brls::TapGestureRecognizer(this));
    }
    void onFocusGained() override {
        brls::Box::onFocusGained();
        value->setAnimated(true);
    }
    void onFocusLost() override {
        brls::Box::onFocusLost();
        value->setAnimated(false);
    }
    void detail(const std::string &text) {
        if (value->getFullText() != text)
            value->setText(text);
    }
};
std::string trackLabel(const MediaTrack &track) {
    std::string name = track.language;
    if (name == "he" || name == "heb" || name == "iw")
        name = "עברית";
    else if (name == "en" || name == "eng")
        name = "English";
    else if (name == "ar" || name == "ara")
        name = "العربية";
    if (!track.title.empty())
        name += (name.empty() ? "" : " · ") + track.title;
    if (name.empty())
        name = brls::getStr("iptv/track") + " " + std::to_string(track.id);
    if (!track.codec.empty())
        name += " · " + track.codec;
    return name;
}
} // namespace
void PlayerSetting::setupPlaybackOptions() {
    auto *parent = static_cast<brls::Box *>(btnSleep->getParent());
    auto add = [&](const std::string &id, const std::string &key, std::function<void()> action) {
        auto *row = new PlayerOption(id, brls::getStr("iptv/" + key));
        row->registerClickAction([action](brls::View *) {
            action();
            return true;
        });
        parent->addView(row);
        return row;
    };
    for (const auto &type : {std::string("audio"), std::string("sub")})
        add("player/" + type, type == "audio" ? "s11" : "s12", [this, type] {
            auto &core = MPVCore::instance();
            std::vector<std::string> labels = {brls::getStr("iptv/s13"), brls::getStr("iptv/s14")},
                                     ids = {"auto", "no"};
            int selected = core.getString(type == "audio" ? "aid" : "sid") == "no" ? 1 : 0;
            for (const auto &track : core.tracks())
                if (track.type == type) {
                    if (track.selected)
                        selected = labels.size();
                    labels.push_back(trackLabel(track));
                    ids.push_back(std::to_string(track.id));
                }
            auto valid = alive;
            BaseDropdown::text(
                brls::getStr(type == "audio" ? "iptv/s11" : "iptv/s12"), labels,
                [this, valid, type, ids, labels](int pick) {
                    if (!*valid || pick < 0 || size_t(pick) >= ids.size())
                        return;
                    static_cast<PlayerOption *>(getView("player/" + type))->detail(labels[pick]);
                    optionsBlockedUntil = brls::getCPUTimeUsec() + 500000;
                    if (!MPVCore::instance().selectTrack(type, ids[pick]))
                        brls::Application::notify(brls::getStr("iptv/track_error"));
                    updatePlaybackOptions();
                },
                selected);
        });
    add("player/sub-background", "subtitle_background", [this] {
        auto &config = ProgramConfig::instance();
        const bool enabled = !config.getSettingItem(SettingItem::SUBTITLE_BACKGROUND, false);
        MPVCore::instance().applySubtitleStyle(enabled);
        config.setSettingItem(SettingItem::SUBTITLE_BACKGROUND, enabled);
        updatePlaybackOptions();
    });
    add("player/sub-size", "subtitle_size", [] {
        const std::vector<std::string> labels = {"75%", "90%", "100%", "115%", "130%", "150%"};
        const std::vector<double> values = {.75, .9, 1, 1.15, 1.3, 1.5};
        int selected = 2;
        const auto scale = MPVCore::instance().getDouble("sub-scale");
        for (size_t i = 0; i < values.size(); ++i)
            if (std::abs(values[i] - scale) < .01)
                selected = i;
        BaseDropdown::text(
            brls::getStr("iptv/subtitle_size"), labels,
            [values](int i) {
                if (i >= 0 && size_t(i) < values.size())
                    MPVCore::instance().setProperty("sub-scale", std::to_string(values[i]));
            },
            selected);
    });
    add("player/sub-delay", "subtitle_delay", [] { MPVCore::instance().setProperty("sub-delay", "0"); });
    registerAction(brls::getStr("iptv/subtitle_earlier"), brls::BUTTON_LB, [this](brls::View *) {
        auto &core = MPVCore::instance();
        core.setProperty("sub-delay",
                         std::to_string(std::clamp(core.getDouble("sub-delay") - .1, -600., 600.)));
        updatePlaybackOptions();
        return true;
    });
    registerAction(brls::getStr("iptv/subtitle_later"), brls::BUTTON_RB, [this](brls::View *) {
        auto &core = MPVCore::instance();
        core.setProperty("sub-delay",
                         std::to_string(std::clamp(core.getDouble("sub-delay") + .1, -600., 600.)));
        updatePlaybackOptions();
        return true;
    });
    add("player/audio-delay", "audio_delay", [] {
        std::vector<std::string> labels;
        std::vector<double> values;
        int selected = 10;
        auto delay = MPVCore::instance().getDouble("audio-delay");
        for (int i = -10; i <= 10; ++i) {
            const double v = i * .1;
            values.push_back(v);
            labels.push_back(fmt::format("{:+.1f} s", v));
            if (std::abs(v - delay) < .05)
                selected = values.size() - 1;
        }
        BaseDropdown::text(
            brls::getStr("iptv/audio_delay"), labels,
            [values](int i) {
                if (i >= 0 && size_t(i) < values.size())
                    MPVCore::instance().setProperty("audio-delay", std::to_string(values[i]));
            },
            selected);
    });
    add("player/speed", "playback_speed", [] {
        const std::vector<std::string> labels = {"0.5×", "0.75×", "1×", "1.25×", "1.5×", "1.75×", "2×"};
        const std::vector<double> values = {.5, .75, 1, 1.25, 1.5, 1.75, 2};
        int selected = 2;
        auto speed = MPVCore::instance().getDouble("speed");
        for (size_t i = 0; i < values.size(); ++i)
            if (std::abs(values[i] - speed) < .01)
                selected = i;
        BaseDropdown::text(
            brls::getStr("iptv/playback_speed"), labels,
            [values](int i) {
                if (i >= 0 && size_t(i) < values.size())
                    MPVCore::instance().setProperty("speed", std::to_string(values[i]));
            },
            selected);
    });
    add("player/deinterlace", "deinterlace", [this] {
        auto &core = MPVCore::instance();
        core.setProperty("deinterlace", core.getFlag("deinterlace") ? "no" : "yes");
        updatePlaybackOptions();
    });
    add("player/decoder", "recover_video", [] { MPVCore::instance().setProperty("hwdec", "no"); });
    updatePlaybackOptions();
}
void PlayerSetting::updatePlaybackOptions() {
    if (brls::getCPUTimeUsec() < optionsBlockedUntil)
        return;
    auto &core = MPVCore::instance();
    if (core.isPositioning())
        return;
    for (const auto &type : {std::string("audio"), std::string("sub")}) {
        std::string label = brls::getStr("iptv/no_tracks");
        for (const auto &track : core.tracks())
            if (track.type == type && track.selected)
                label = trackLabel(track);
        if (core.getString(type == "audio" ? "aid" : "sid") == "no" ||
            (type == "sub" && !core.getFlag("sub-visibility")))
            label = brls::getStr("iptv/s14");
        static_cast<PlayerOption *>(getView("player/" + type))->detail(label);
    }
    auto set = [this](const char *id, const std::string &text) {
        static_cast<PlayerOption *>(getView(id))->detail(text);
    };
    set("player/sub-background",
        brls::getStr(ProgramConfig::instance().getSettingItem(SettingItem::SUBTITLE_BACKGROUND, false)
                         ? "hints/on"
                         : "hints/off"));
    set("player/sub-size", fmt::format("{:.0f}%", core.getDouble("sub-scale") * 100));
    set("player/sub-delay", fmt::format("{:+.2f} s · L / R", core.getDouble("sub-delay")));
    set("player/audio-delay", fmt::format("{:+.2f} s", core.getDouble("audio-delay")));
    set("player/speed", fmt::format("{:.2f}×", core.getDouble("speed")));
    set("player/deinterlace", brls::getStr(core.getFlag("deinterlace") ? "hints/on" : "hints/off"));
}

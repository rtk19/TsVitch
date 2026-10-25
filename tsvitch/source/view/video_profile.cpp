



#include <borealis/core/application.hpp>
#include <borealis/views/label.hpp>

#include "view/video_profile.hpp"
#include "view/mpv_core.hpp"

VideoProfile::VideoProfile() {
    this->inflateFromXMLRes("xml/views/video_profile.xml");
    brls::Logger::debug("View VideoProfile: create");
}

void VideoProfile::update() {
    auto mpvCore = &MPVCore::instance();
    if(mpvCore->isPositioning())return;

    
    auto origin=mpvCore->filepath;const auto scheme=origin.find("://");if(scheme!=std::string::npos)origin=origin.substr(0,origin.find('/',scheme+3));if(origin!=labelUrl->getFullText())labelUrl->setText(origin);
    labelSize->setText(fmt::format("{:.2f}MB", mpvCore->getInt("file-size") / 1048576.0));
    labelFormat->setText(mpvCore->getString("file-format"));
    auto cache=mpvCore->getNodeMap("demuxer-cache-state");
    auto number=[&](const char* key){auto it=cache.find(key);if(it==cache.end())return 0.;const auto& n=it->second;return n.format==MPV_FORMAT_INT64?double(n.u.int64):n.format==MPV_FORMAT_DOUBLE?n.u.double_:0.;};
    labelCache->setText(fmt::format("{:.2f} MB · {:.1f} s · {:.0f} KB/s",number("fw-bytes")/1048576.,mpvCore->getDouble("demuxer-cache-duration"),number("raw-input-rate")/1024.));

    
    labelVideoRes->setText(fmt::format(
        "{} x {}@{} (window: {} x {} framebuffer: {} x {})", mpvCore->getInt("video-params/w"),
        mpvCore->getInt("video-params/h"), mpvCore->getDouble("container-fps"), brls::Application::contentWidth,
        brls::Application::contentHeight, brls::Application::windowWidth, brls::Application::windowHeight));
    labelVideoCodec->setText(mpvCore->getString("video-codec"));
    labelVideoPixel->setText(mpvCore->getString("video-params/pixelformat"));
    labelVideoHW->setText(mpvCore->getString("hwdec-current"));
    labelVideoBitrate->setText(std::to_string(mpvCore->getInt("video-bitrate") / 1024) + "kbps");
    labelVideoDrop->setText(fmt::format("{} (decoder) {} (output)", mpvCore->getInt("decoder-frame-drop-count"),
                                        mpvCore->getInt("frame-drop-count")));
    labelVideoSync->setText(fmt::format("{:.5f}", mpvCore->getDouble("avsync")));

    
    labelAudioCodec->setText(mpvCore->getString("audio-codec"));
    labelAudioChannel->setText(mpvCore->getString("audio-params/channel-count"));
    labelAudioSampleRate->setText(std::to_string(mpvCore->getInt("audio-params/samplerate") / 1000) + "kHz");
    labelAudioBitrate->setText(std::to_string(mpvCore->getInt("audio-bitrate") / 1024) + "kbps");
}

void VideoProfile::draw(NVGcontext *vg, float x, float y, float width, float height, brls::Style style,
                        brls::FrameContext *ctx) {
    static brls::Time time = 0;
    brls::Time now         = brls::getCPUTimeUsec();
    if ((now - time) > 1000000) {
        this->update();
        time = now;
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

VideoProfile::~VideoProfile() { brls::Logger::debug("View VideoProfile: delete"); }

brls::View *VideoProfile::create() { return new VideoProfile(); }


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
    if (mpvCore->isPositioning())
        return;

    auto origin = mpvCore->filepath;
    const auto scheme = origin.find("://");
    if (scheme != std::string::npos)
        origin = origin.substr(0, origin.find('/', scheme + 3));
    if (origin != labelUrl->getFullText())
        labelUrl->setText(origin);
    labelSize->setText(fmt::format("{:.2f}MB", mpvCore->statsNumber("file-size") / 1048576.0));
    labelFormat->setText(mpvCore->statsText("file-format"));
    labelCache->setText(fmt::format(
        "{:.2f} MB · {:.1f} s · {:.0f} KB/s", mpvCore->statsNumber("demuxer-cache-state/fw-bytes") / 1048576.,
        mpvCore->statsNumber("demuxer-cache-duration"), mpvCore->cache_speed / 1024.));

    labelVideoRes->setText(fmt::format("{:.0f} × {:.0f} @ {:.2f}", mpvCore->statsNumber("video-params/w"),
                                       mpvCore->statsNumber("video-params/h"),
                                       mpvCore->statsNumber("container-fps")));
    labelVideoCodec->setText(mpvCore->statsText("video-codec"));
    labelVideoPixel->setText(mpvCore->statsText("video-params/pixelformat"));
    labelVideoHW->setText(mpvCore->statsText("hwdec-current"));
    labelVideoBitrate->setText(fmt::format("{:.0f} kbps", mpvCore->statsNumber("video-bitrate") / 1024));
    labelVideoDrop->setText(fmt::format("{} (decoder) {} (output)",
                                        mpvCore->statsNumber("decoder-frame-drop-count"),
                                        mpvCore->statsNumber("frame-drop-count")));
    labelVideoSync->setText(fmt::format("{:.5f}", mpvCore->statsNumber("avsync")));

    labelAudioCodec->setText(mpvCore->statsText("audio-codec"));
    const auto channels = mpvCore->statsText("audio-params/channel-count");
    labelAudioChannel->setText(channels.empty() ? "—" : channels);
    labelAudioSampleRate->setText(
        fmt::format("{:.1f} kHz", mpvCore->statsNumber("audio-params/samplerate") / 1000));
    labelAudioBitrate->setText(fmt::format("{:.0f} kbps", mpvCore->statsNumber("audio-bitrate") / 1024));
}

void VideoProfile::draw(NVGcontext *vg, float x, float y, float width, float height, brls::Style style,
                        brls::FrameContext *ctx) {
    static brls::Time time = 0;
    brls::Time now = brls::getCPUTimeUsec();
    if ((now - time) > 1000000) {
        this->update();
        time = now;
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

VideoProfile::~VideoProfile() { brls::Logger::debug("View VideoProfile: delete"); }

brls::View *VideoProfile::create() { return new VideoProfile(); }
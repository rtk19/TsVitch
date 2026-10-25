

#include "view/video_card.hpp"
#include "utils/programme.hpp"
#include "utils/content_metadata.hpp"
#include <ctime>
#include "utils/catchup.hpp"
#include <borealis/core/thread.hpp>
#include "view/svg_image.hpp"
#include "view/text_box.hpp"
#include "utils/number_helper.hpp"
#include "utils/image_helper.hpp"
#include "core/FavoriteManager.hpp"
#include <pystring.h>

using namespace brls::literals;

void BaseVideoCard::prepareForReuse() { ImageHelper::clear(this->picture); }

void BaseVideoCard::cacheForReuse() { ImageHelper::clear(this->picture); }

RecyclingGridItemLiveVideoCard::RecyclingGridItemLiveVideoCard() {
    this->inflateFromXMLRes("xml/views/video_card_live.xml");
}

RecyclingGridItemLiveVideoCard::~RecyclingGridItemLiveVideoCard() {
    brls::cancelDelay(programmeTimer);
    ImageHelper::clear(this->picture);
}

void RecyclingGridItemLiveVideoCard::setChannel(tsvitch::LiveM3u8 liveData) {
    brls::cancelDelay(programmeTimer);
    this->liveData = liveData;
    archiveBadgeText->setText(brls::getStr("iptv/archive_badge"));
    archiveBadge->setVisibility(tsvitch::hasCatchup(liveData) ? brls::Visibility::VISIBLE
                                                              : brls::Visibility::GONE);
    updateProgramme();
    this->labelGroup->setText(liveData.groupTitle);
    this->labelTitle->setIsWrapping(true);
    this->labelTitle->setText(liveData.title);
    ImageHelper::clear(picture);
    const auto logo = tsvitch::artworkUrl(liveData.url, tsvitch::artworkValue(nlohmann::json(liveData.logo)));
    if (!logo.empty())
        ImageHelper::with(picture)->load(logo);

    bool isFavorite = FavoriteManager::get()->isFavorite(liveData.url);

    if (isFavorite) {
        this->svgFavoriteIcon->setImageFromSVGRes("svg/ui-favorite.svg");
        this->svgFavoriteIcon->setVisibility(brls::Visibility::VISIBLE);
    } else
        this->svgFavoriteIcon->setVisibility(brls::Visibility::GONE);

    this->labelChno->setText(liveData.chno);
}

void RecyclingGridItemLiveVideoCard::cacheForReuse() {
    brls::cancelDelay(programmeTimer);
    BaseVideoCard::cacheForReuse();
}
void RecyclingGridItemLiveVideoCard::updateProgramme() {
    programmeFraction = 0;
    if (liveData.contentType == "movie" || liveData.contentType == "series" ||
        liveData.contentType == "catchup") {
        labelProgramme->setText("");
        programmeTime->setText("");
        return;
    }
    tsvitch::primeProgrammeGuide(liveData);
    const auto programme = tsvitch::currentProgrammeData(liveData);
    labelProgramme->setText(programme.title.empty() ? brls::getStr("iptv/epg_unavailable") : programme.title);
    std::string time;
    if (programme.start > 0 && programme.end > programme.start) {
        const auto now = tsvitch::programmeNow();
        programmeFraction =
            std::clamp(float(now - programme.start) / (programme.end - programme.start), 0.f, 1.f);
        std::time_t begin = programme.start, end = programme.end;
        std::tm first{}, last{};
        localtime_r(&begin, &first);
        localtime_r(&end, &last);
        char buffer[16];
        std::strftime(buffer, sizeof(buffer), "%H:%M", &first);
        time = buffer;
        std::strftime(buffer, sizeof(buffer), "%H:%M", &last);
        time += " - " + std::string(buffer);
    }
    programmeTime->setText(time);
    programmeTimer = brls::delay(1000, [this] { updateProgramme(); });
}
void RecyclingGridItemLiveVideoCard::draw(NVGcontext *vg, float x, float y, float width, float height,
                                          brls::Style style, brls::FrameContext *ctx) {
    const auto state = picture->getTexture() > 0 ? brls::Visibility::GONE : brls::Visibility::VISIBLE;
    if (logoFallback->getVisibility() != state)
        logoFallback->setVisibility(state);
    BaseVideoCard::draw(vg, x, y, width, height, style, ctx);
    if (programmeFraction > 0) {
        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGB(248, 217, 107));
        nvgRect(vg, x + 10, y + height - 5, (width - 20) * programmeFraction, 4);
        nvgFill(vg);
    }
}

tsvitch::LiveM3u8 RecyclingGridItemLiveVideoCard::getChannel() { return this->liveData; }

void RecyclingGridItemLiveVideoCard::setFavoriteIcon(bool isFavorite) {
    if (isFavorite) {
        this->svgFavoriteIcon->setImageFromSVGRes("svg/ui-favorite.svg");
        this->svgFavoriteIcon->setVisibility(brls::Visibility::VISIBLE);
    } else
        this->svgFavoriteIcon->setVisibility(brls::Visibility::GONE);
}

RecyclingGridItemLiveVideoCard *RecyclingGridItemLiveVideoCard::create() {
    return new RecyclingGridItemLiveVideoCard();
}

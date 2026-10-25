

#include "view/video_card.hpp"
#include "utils/programme.hpp"
#include "utils/catchup.hpp"
#include <borealis/core/thread.hpp>
#include "view/svg_image.hpp"
#include "view/text_box.hpp"
#include "utils/number_helper.hpp"
#include "utils/image_helper.hpp"
#include "core/FavoriteManager.hpp"
#include <pystring.h>

using namespace brls::literals;

void BaseVideoCard::prepareForReuse() { this->picture->setImageFromRes("pictures/video-card-bg.png"); }

void BaseVideoCard::cacheForReuse() { ImageHelper::clear(this->picture); }

RecyclingGridItemLiveVideoCard::RecyclingGridItemLiveVideoCard() {
    this->inflateFromXMLRes("xml/views/video_card_live.xml");
}

RecyclingGridItemLiveVideoCard::~RecyclingGridItemLiveVideoCard() { brls::cancelDelay(programmeTimer); ImageHelper::clear(this->picture); }

void RecyclingGridItemLiveVideoCard::setChannel(tsvitch::LiveM3u8 liveData) {
    brls::cancelDelay(programmeTimer);
    this->liveData = liveData;
    archiveBadgeText->setText(brls::getStr("iptv/archive_badge"));
    archiveBadge->setVisibility(tsvitch::hasCatchup(liveData)?brls::Visibility::VISIBLE:brls::Visibility::GONE);
    updateProgramme();
    this->labelGroup->setText(liveData.groupTitle);
    this->labelTitle->setIsWrapping(false);
    this->labelTitle->setText(liveData.title);
    ImageHelper::with(this->picture)->load(liveData.logo);

    bool isFavorite = FavoriteManager::get()->isFavorite(liveData.url);

    if (isFavorite) {
        this->svgFavoriteIcon->setImageFromSVGRes("svg/ico-favorite-activate.svg");
        this->svgFavoriteIcon->setVisibility(brls::Visibility::VISIBLE);
    } else
        this->svgFavoriteIcon->setVisibility(brls::Visibility::GONE);

    this->labelChno->setText(liveData.chno);
}

void RecyclingGridItemLiveVideoCard::cacheForReuse() {
    brls::cancelDelay(programmeTimer);BaseVideoCard::cacheForReuse();
}
void RecyclingGridItemLiveVideoCard::updateProgramme() {
    if(liveData.contentType=="movie"||liveData.contentType=="series"||liveData.contentType=="catchup") {
        labelProgramme->setText("");return;
    }
    tsvitch::primeProgrammeGuide(liveData);
    labelProgramme->setText(tsvitch::currentProgramme(liveData));
    programmeTimer=brls::delay(5000,[this]{updateProgramme();});
}

tsvitch::LiveM3u8 RecyclingGridItemLiveVideoCard::getChannel() { return this->liveData; }

void RecyclingGridItemLiveVideoCard::setFavoriteIcon(bool isFavorite) {
    if (isFavorite) {
        this->svgFavoriteIcon->setImageFromSVGRes("svg/ico-favorite-activate.svg");
        this->svgFavoriteIcon->setVisibility(brls::Visibility::VISIBLE);
    } else
        this->svgFavoriteIcon->setVisibility(brls::Visibility::GONE);
}

RecyclingGridItemLiveVideoCard* RecyclingGridItemLiveVideoCard::create() {
    return new RecyclingGridItemLiveVideoCard();
}

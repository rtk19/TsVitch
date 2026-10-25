

#pragma once

#include "view/recycling_grid.hpp"
#include "api/tsvitch/result/home_live_result.h" 

class SVGImage;
class TextBox;

class BaseVideoCard : public RecyclingGridItem {
public:
    void prepareForReuse() override;

    void cacheForReuse() override;

protected:
    BRLS_BIND(brls::Image, picture, "video/card/picture");
};

class RecyclingGridItemLiveVideoCard : public BaseVideoCard {
public:
    RecyclingGridItemLiveVideoCard();

    ~RecyclingGridItemLiveVideoCard() override;

    void setChannel(tsvitch::LiveM3u8 liveData);
    void updateProgramme();
    void cacheForReuse() override;

   tsvitch::LiveM3u8 getChannel();

                                             void setFavoriteIcon(bool isFavorite);

    static RecyclingGridItemLiveVideoCard* create();

private:
    size_t programmeTimer = 0;
    BRLS_BIND(brls::Label, labelProgramme, "video/card/label/programme");
    BRLS_BIND(brls::Box, archiveBadge, "video/card/archive/badge");
    BRLS_BIND(brls::Label, archiveBadgeText, "video/card/archive");
tsvitch::LiveM3u8 liveData;
    BRLS_BIND(TextBox, labelTitle, "video/card/label/title");
    BRLS_BIND(brls::Label, labelGroup, "video/card/label/group");
    BRLS_BIND(brls::Label, labelChno, "video/card/label/chno");
    BRLS_BIND(brls::Box, boxPic, "video/card/pic_box");
    BRLS_BIND(brls::Box, boxHint, "video/card/hint");
    BRLS_BIND(SVGImage, svgUp, "video/svg/up");
    BRLS_BIND(SVGImage, svgFavoriteIcon, "video/card/ico/favorite");
};

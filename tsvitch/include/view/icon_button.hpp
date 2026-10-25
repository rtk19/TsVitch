#pragma once
#include <borealis/core/box.hpp>
#include <borealis/views/label.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include "view/svg_image.hpp"
class IconButton : public brls::Box {
    SVGImage *symbol;
    brls::Label *caption;

  public:
    IconButton() : brls::Box(brls::Axis::ROW) {
        setFocusable(true);
        setHideHighlightBackground(true);
        setDimensions(156, 56);
        setPadding(10);
        setAlignItems(brls::AlignItems::CENTER);
        setCornerRadius(8);
        setBackgroundColor(nvgRGB(32, 66, 104));
        symbol = new SVGImage();
        symbol->setDimensions(26, 26);
        symbol->setShrink(0);
        addView(symbol);
        caption = new brls::Label();
        caption->setFontSize(24);
        caption->setMarginLeft(10);
        caption->setTextColor(nvgRGB(255, 255, 255));
        caption->setGrow(1);
        addView(caption);
        registerStringXMLAttribute("title", [this](const std::string &value) { setText(value); });
        registerStringXMLAttribute("icon", [this](const std::string &value) { setIcon(value); });
        addGestureRecognizer(new brls::TapGestureRecognizer(this));
    }
    void setText(const std::string &value) { caption->setText(value); }
    void setIcon(const std::string &name) { symbol->setImageFromSVGRes("svg/ui-" + name + ".svg"); }
    static brls::View *create() { return new IconButton; }
};

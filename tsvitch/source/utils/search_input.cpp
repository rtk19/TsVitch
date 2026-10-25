#include "utils/search_input.hpp"
#include "utils/search_text.hpp"
#include "view/icon_button.hpp"
#include "view/recycling_grid.hpp"
#include <borealis/core/activity.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/label.hpp>
namespace tsvitch {
namespace {
int lastKeyboardLanguage = -1;
class KeyCell : public RecyclingGridItem {
    brls::Label *text;

  public:
    KeyCell() {
        setHideHighlightBackground(true);
        setBackgroundColor(nvgRGB(39, 77, 121));
        setCornerRadius(8);
        setJustifyContent(brls::JustifyContent::CENTER);
        setAlignItems(brls::AlignItems::CENTER);
        text = new brls::Label();
        text->setFontSize(29);
        text->setTextColor(nvgRGB(255, 255, 255));
        addView(text);
    }
    void setText(const std::string &value) { text->setText(value); }
};
class Keys : public RecyclingGridDataSource {
    std::vector<std::string> keys;
    std::function<void(std::string)> insert;

  public:
    Keys(std::string letters, std::function<void(std::string)> insert)
        : keys(utf8Characters(letters)), insert(std::move(insert)) {}
    size_t getItemCount() override { return keys.size(); }
    RecyclingGridItem *cellForRow(RecyclingGrid *grid, size_t row) override {
        auto *key = static_cast<KeyCell *>(grid->dequeueReusableCell("key"));
        key->setText(keys[row]);
        return key;
    }
    void onItemSelected(RecyclingGrid *, size_t row) override {
        if (row < keys.size())
            insert(keys[row]);
    }
    void clearData() override { keys.clear(); }
};
class SearchPage : public brls::Box {
    SearchText input;
    int language = 0;
    brls::Label *preview;
    RecyclingGrid *keys = nullptr;
    IconButton *languageButton;
    std::function<void(std::string)> submit;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::string title;
    void update() {
        preview->setText(input.value().empty() ? brls::getStr("iptv/search_hint") : input.value());
        preview->setCursor(input.value().empty() ? -1 : static_cast<int>(input.position()));
    }
    IconButton *button(brls::Box *row, const std::string &label, const std::string &icon,
                       std::function<void()> action, float width = 170) {
        auto *b = new IconButton;
        b->setWidth(width);
        b->setMarginRight(8);
        b->setText(label);
        b->setIcon(icon);
        b->registerClickAction([action](brls::View *) {
            action();
            return true;
        });
        row->addView(b);
        return b;
    }
    void keyboard() {
        lastKeyboardLanguage = language;
        const std::string letters = language == 0 ? "/'קראטוןםפשדגכעיחלךףזסבהנמצתץ."
                                    : language == 1 ? "qwertyuiopasdfghjkl'zxcvbnm.,?"
                                                    : "1234567890.,:;!?-+/@#%&()[]_'\"";
        keys->setDataSource(new Keys(letters, [this](std::string letter) {
            input.insert(letter);
            update();
        }));
        languageButton->setText(language == 0 ? "עברית" : language == 1 ? "English" : "123");
    }

  public:
    SearchPage(std::string title, std::string initial, std::function<void(std::string)> submit)
        : brls::Box(brls::Axis::COLUMN), input(std::move(initial)), submit(std::move(submit)),
          title(std::move(title)) {
        language = lastKeyboardLanguage >= 0                ? lastKeyboardLanguage
                   : brls::Application::getLocale() == "he" ? 0
                                                            : 1;
        setPadding(24);
        setBackgroundColor(nvgRGB(14, 31, 54));
        auto *heading = new brls::Label;
        heading->setText(this->title);
        heading->setFontSize(30);
        heading->setTextColor(nvgRGB(255, 255, 255));
        heading->setMarginBottom(14);
        addView(heading);
        preview = new brls::Label;
        preview->setId("search/input");
        preview->setFontSize(32);
        preview->setHeight(72);
        preview->setTextColor(nvgRGB(255, 255, 255));
        preview->setBackgroundColor(nvgRGB(25, 55, 89));
        preview->setMarginBottom(18);
        addView(preview);
        keys = new RecyclingGrid;
        keys->setId("search/keys");
        keys->spanCount = 10;
        keys->estimatedRowHeight = 62;
        keys->estimatedRowSpace = 10;
        keys->setHeight(216);
        keys->setWidthPercentage(100);
        keys->releaseDataSourceOnDestroy();
        keys->registerCell("key", [] { return new KeyCell; });
        addView(keys);
        auto *editing = new brls::Box(brls::Axis::ROW);
        editing->setMarginTop(14);
        addView(editing);
        button(editing, brls::getStr("iptv/key_space"), "space", [this] {
            input.insert(" ");
            update();
        });
        button(editing, brls::getStr("iptv/key_delete"), "delete", [this] {
            input.backspace();
            update();
        });
        languageButton = button(editing, "עברית", "globe", [this] {
            language = (language + 1) % 3;
            keyboard();
        });
        button(
            editing, brls::getStr("iptv/key_system"), "keyboard",
            [this] {
                auto valid = alive;
                brls::Application::getImeManager()->openForText(
                    [this, valid](std::string text) {
                        if (*valid) {
                            input.set(text);
                            update();
                        }
                    },
                    this->title, "", 128, input.value());
            },
            270);
        auto *actions = new brls::Box(brls::Axis::ROW);
        actions->setMarginTop(16);
        addView(actions);
        button(actions, brls::getStr("hints/search"), "search", [this] { finish(); }, 215);
        button(
            actions, brls::getStr("iptv/key_clear"), "delete",
            [this] {
                input.clear();
                update();
            },
            185);
        button(actions, brls::getStr("hints/back"), "back", [] { brls::Application::popActivity(); }, 185);
        auto *help = new brls::Label;
        help->setText(brls::getStr("iptv/keyboard_help"));
        help->setFontSize(22);
        help->setTextColor(nvgRGB(210, 224, 242));
        help->setMarginTop(16);
        addView(help);
        registerAction(brls::getStr("hints/back"), brls::BUTTON_B, [](brls::View *) {
            brls::Application::popActivity();
            return true;
        });
        registerAction(brls::getStr("iptv/key_delete"), brls::BUTTON_X, [this](brls::View *) {
            input.backspace();
            update();
            return true;
        });
        registerAction(brls::getStr("iptv/key_language"), brls::BUTTON_Y, [this](brls::View *) {
            language = (language + 1) % 3;
            keyboard();
            return true;
        });
        registerAction(brls::getStr("hints/search"), brls::BUTTON_START, [this](brls::View *) {
            finish();
            return true;
        });
        registerAction(
            brls::getStr("iptv/cursor_left"), brls::BUTTON_LB,
            [this](brls::View *) {
                input.move(-1);
                update();
                return true;
            },
            true);
        registerAction(
            brls::getStr("iptv/cursor_right"), brls::BUTTON_RB,
            [this](brls::View *) {
                input.move(1);
                update();
                return true;
            },
            true);
        keyboard();
        update();
    }
    ~SearchPage() override { *alive = false; }
    brls::View *getDefaultFocus() override { return keys ? keys->getDefaultFocus() : nullptr; }
    void finish() {
        auto callback = submit;
        auto query = input.value();
        brls::Application::popActivity(brls::TransitionAnimation::FADE,
                                       [callback, query] { callback(query); });
    }
};
} // namespace
void openSearchInput(const std::string &title, const std::string &initial,
                     std::function<void(std::string)> submit) {
    auto *frame = new brls::AppletFrame(new SearchPage(title, initial, std::move(submit)));
    frame->setHeaderVisibility(brls::Visibility::GONE);
    brls::Application::pushActivity(new brls::Activity(frame));
}
} // namespace tsvitch

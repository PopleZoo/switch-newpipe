#include "activity/main_activity.hpp"

#include "newpipe/i18n.hpp"
#include "newpipe/settings_store.hpp"

namespace {

size_t startup_tab_index(const std::string& tab_id) {
    if (tab_id == "search") {
        return 1;
    }
    if (tab_id == "subscriptions") {
        return 2;
    }
    if (tab_id == "library") {
        return 3;
    }
    if (tab_id == "settings") {
        return 4;
    }
    return 0;
}

class TabMenuRow : public brls::Box {
public:
    TabMenuRow(const std::string& text, std::function<void()> on_click) {
        this->setAxis(brls::Axis::ROW);
        this->setFocusable(true);
        this->setWidth(360);
        this->setHeight(52);
        this->setMarginBottom(8);
        this->setCornerRadius(6);
        this->setHighlightCornerRadius(6);
        this->setBackgroundColor(brls::Application::getTheme().getColor("color/grey_2"));
        this->setJustifyContent(brls::JustifyContent::CENTER);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setFocusSound(brls::SOUND_FOCUS_SIDEBAR);
        this->registerClickAction([on_click](brls::View*) {
            on_click();
            return true;
        });
        this->addGestureRecognizer(new brls::TapGestureRecognizer(this));

        auto* label = new brls::Label();
        label->setText(text);
        label->setFontSize(20);
        this->addView(label);
    }
};

}  // namespace

void MainActivity::onContentAvailable() {
    this->registerAction(newpipe::tr("common/info"), brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        auto* dialog = new brls::Dialog(newpipe::tr("app/info_body", APP_VERSION));
        dialog->addButton(newpipe::tr("hints/ok"), [dialog]() { dialog->close(); });
        dialog->setCancelable(true);
        dialog->open();
        return true;
    });

    // ZR (right trigger) opens the tab menu dialog
    this->registerAction(newpipe::tr("main/zr_open_menu"), brls::ControllerButton::BUTTON_RT, [this](brls::View*) {
        this->openTabMenu();
        return true;
    });

    if (this->tabsFrame) {
        const size_t index = startup_tab_index(newpipe::SettingsStore::instance().settings().startup_tab);
        this->tabsFrame->setDefaultTabIndex(index);
        this->tabsFrame->hideSidebar();
        this->tabsFrame->showTab(static_cast<int>(index));
    }
}

void MainActivity::openTabMenu() {
    const std::string titles[] = {
        newpipe::tr("app/home"),
        newpipe::tr("app/search"),
        newpipe::tr("app/subscriptions"),
        newpipe::tr("app/library"),
        newpipe::tr("app/settings"),
    };

    auto* panel = new brls::Box(brls::Axis::COLUMN);
    panel->setBackgroundColor(brls::Application::getTheme().getColor("brls/background"));
    panel->setCornerRadius(10);
    panel->setPaddingTop(24);
    panel->setPaddingBottom(16);
    panel->setPaddingLeft(10);
    panel->setPaddingRight(10);
    panel->setAlignItems(brls::AlignItems::CENTER);
    panel->setJustifyContent(brls::JustifyContent::CENTER);

    auto* title = new brls::Label();
    title->setText(newpipe::tr("main/tab_menu_title"));
    title->setFontSize(24);
    title->setMarginBottom(20);
    panel->addView(title);

    for (size_t i = 0; i < 5; i++) {
        const size_t index = i;
        panel->addView(new TabMenuRow(titles[i], [this, index]() {
            brls::Application::popActivity(brls::TransitionAnimation::FADE, [this, index]() {
                if (tabsFrame) tabsFrame->showTab(static_cast<int>(index));
            });
        }));
    }

    auto* back_hint = new brls::Label();
    back_hint->setText(newpipe::tr("hints/back"));
    back_hint->setFontSize(15);
    back_hint->setTextColor(nvgRGBA(128, 128, 128, 255));
    back_hint->setMarginTop(8);
    panel->addView(back_hint);

    auto* backdrop = new brls::Box(brls::Axis::COLUMN);
    backdrop->setWidth(1280);
    backdrop->setHeight(720);
    backdrop->setBackgroundColor(brls::Application::getTheme().getColor("brls/backdrop"));
    backdrop->setJustifyContent(brls::JustifyContent::CENTER);
    backdrop->setAlignItems(brls::AlignItems::CENTER);
    backdrop->addView(panel);

    backdrop->registerAction(newpipe::tr("hints/back"), brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    brls::Application::pushActivity(new brls::Activity(backdrop));
}

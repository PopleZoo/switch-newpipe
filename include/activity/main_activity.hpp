#pragma once

#include <borealis.hpp>

#include "newpipe/settings_store.hpp"
#include "view/auto_tab_frame.hpp"

class MainActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/main.xml");

    void onContentAvailable() override;

private:
    void openTabMenu();

    BRLS_BIND(brls::Label, zrHintLabel, "main/zr_hint");
    BRLS_BIND(AutoTabFrame, tabsFrame, "main/tabs");
};

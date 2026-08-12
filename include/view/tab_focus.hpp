#pragma once

#include <borealis.hpp>

#include "view/auto_tab_frame.hpp"

namespace newpipe {

// Call this before clearing a card grid.
//
// borealis clears Application::currentFocus when the focused view is destroyed
// (View::~View focus sanity check), and Application::navigate() bails out while
// there is no focus, so deleting the focused card leaves the whole UI unable to
// move. Handing focus to the scrolling frame does not help: its default focus
// resolves through lastFocusedView back to the very card that is about to be
// deleted, which makes giveFocus() a no-op.
//
// The sidebar item is always focusable and never owned by the grid, so it is a
// safe place to park focus. Giving it focus does not switch tabs: the item is
// already the active one, and AutoSidebarItem::setActive() ignores no-op
// changes.
inline void release_grid_focus(AttachedView* tab, brls::Box* gridBox) {
    if (!gridBox) {
        return;
    }

    for (brls::View* view = brls::Application::getCurrentFocus(); view; view = view->getParent()) {
        if (view != gridBox) {
            continue;
        }

        brls::View* sidebarItem = tab ? tab->getTabBar() : nullptr;
        if (sidebarItem) {
            brls::Application::giveFocus(sidebarItem);
        }
        return;
    }
}

// Call this after a card grid has been rebuilt.
//
// With the sidebar hidden (see AutoTabFrame::hideSidebar), focus may have been
// parked on an invisible sidebar item by release_grid_focus(). That leaves the
// user with no visible focus, so once the grid actually has cards again the
// focus is handed to the first card.
inline void refocus_grid(AttachedView* tab, brls::Box* gridBox) {
    if (!tab || !gridBox || gridBox->getChildren().empty()) {
        return;
    }

    // The tab bar is only linked by AutoSidebarItem::createAttachedView()
    // AFTER the attached view's constructor ran. Tabs that build their grid
    // synchronously in the constructor (LibraryTab::refresh) would otherwise
    // call getParent() on a null pointer here.
    brls::View* tabBar = tab->getTabBar();
    if (!tabBar) {
        return;
    }
    AutoTabFrame* frame = dynamic_cast<AutoTabFrame*>(tabBar->getParent());
    if (!frame || frame->getSidebar()->getVisibility() == brls::Visibility::VISIBLE) {
        return;
    }

    brls::View* focus = brls::Application::getCurrentFocus();
    if (focus) {
        bool parked = false;
        for (brls::View* view = focus; view; view = view->getParent()) {
            if (view == frame->getSidebar() || view == frame) {
                parked = true;
                break;
            }
        }
        if (!parked) {
            return;
        }
    }

    brls::Application::giveFocus(gridBox);
}

}  // namespace newpipe

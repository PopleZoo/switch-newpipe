#include "tab/home_tab.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_helper.hpp"
#include "newpipe/runtime.hpp"
#include "newpipe/settings_store.hpp"
#include "view/stream_card.hpp"
#include "view/tab_focus.hpp"

namespace {
constexpr size_t kGridColumns = 2;
}

HomeTab::HomeTab() : service_() {
    this->inflateFromXMLRes("xml/tabs/home.xml");
    newpipe::log_line("home: construct");

    kiosks_ = service_.list_kiosks();
    newpipe::logf("home: kiosks=%zu", kiosks_.size());
    const newpipe::AppSettings settings = newpipe::SettingsStore::instance().settings();
    for (size_t i = 0; i < kiosks_.size(); i++) {
        if (kiosks_[i].id == settings.home_kiosk) {
            kioskIndex_ = i;
            break;
        }
    }

    if (statusLabel) {
        statusLabel->setText(newpipe::tr("home/preparing"));
    }
    brls::delay(700, [this]() {
        interactionReady_.store(true);
        newpipe::log_line("home: interaction ready");
    });
    scheduleLoadHome(250);
}

void HomeTab::onCreate() {
    // Tab actions are mirrored on the sidebar item so they stay usable while the
    // sidebar holds focus, and when the feed is empty and nothing here is focusable.
    this->registerTabAction(newpipe::tr("common/refresh"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        loadHome();
        return true;
    });
    this->registerTabAction(newpipe::tr("home/category_action"), brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        cycleKiosk();
        return true;
    });
}

void HomeTab::loadHome() {
    newpipe::logf("home: loadHome index=%zu", kioskIndex_);
    if (!initialLoadCompleted_) {
        initialLoadAttempts_++;
    }
    if (spinner) {
        spinner->setVisibility(brls::Visibility::VISIBLE);
    }

    if (!service_.is_loaded()) {
        if (statusLabel) {
            statusLabel->setText(newpipe::tr("common/service_init_failed", service_.error_message()));
        }
        if (spinner) {
            spinner->setVisibility(brls::Visibility::GONE);
        }
        return;
    }

    if (kiosks_.empty()) {
        if (statusLabel) {
            statusLabel->setText(newpipe::tr("home/no_kiosk"));
        }
        if (spinner) {
            spinner->setVisibility(brls::Visibility::GONE);
        }
        return;
    }

    kioskIndex_ %= kiosks_.size();
    const auto feed = service_.get_home_feed(kiosks_[kioskIndex_].id);
    if (!feed.has_value()) {
        if (statusLabel) {
            if (service_.error_message().empty()) {
                statusLabel->setText(newpipe::tr("home/load_failed"));
            } else {
                statusLabel->setText(service_.error_message());
            }
        }
        if (spinner) {
            spinner->setVisibility(brls::Visibility::GONE);
        }
        if (!initialLoadCompleted_ && items_.empty() && initialLoadAttempts_ < 4) {
            if (statusLabel) {
                statusLabel->setText(newpipe::tr("home/preparing"));
            }
            newpipe::logf("home: auto retry attempt=%d", initialLoadAttempts_);
            scheduleLoadHome(350);
        }
        return;
    }

    initialLoadCompleted_ = true;
    items_ = feed->items;
    hasMore_ = !feed->continuation_token.empty();
    newpipe::logf("home: feed=%s items=%zu hasMore=%d", feed->kiosk.id.c_str(), items_.size(), hasMore_ ? 1 : 0);
    buildGrid();

    if (statusLabel) {
        std::string title = feed->kiosk.title;
        const std::string option_key = "settings/home_kiosk/options/" + feed->kiosk.id;
        const std::string translated = newpipe::tr(option_key);
        if (!translated.empty() && translated != option_key) {
            title = translated;
        }
        statusLabel->setText(newpipe::tr("common/count_with_title", title, items_.size()));
    }
    if (spinner) {
        spinner->setVisibility(brls::Visibility::GONE);
    }
}

void HomeTab::scheduleLoadHome(long delay_ms) {
    brls::delay(delay_ms, [this]() { loadHome(); });
}

void HomeTab::buildGrid() {
    if (!gridBox) {
        newpipe::log_line("home: buildGrid gridBox missing");
        return;
    }

    newpipe::logf("home: buildGrid items=%zu", items_.size());
    newpipe::release_grid_focus(this, gridBox);
    loadMoreSubscriptions_.clear();
    gridBox->clearViews();
    for (size_t i = 0; i < items_.size(); i += kGridColumns) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8);

        for (size_t j = i; j < i + kGridColumns && j < items_.size(); j++) {
            auto* card = new StreamCard();
            card->setData(items_[j]);
            const size_t idx = j;
            card->registerClickAction([this, idx](brls::View*) {
                playStream(items_[idx]);
                return true;
            });
            card->registerAction(newpipe::tr("common/info"), brls::ControllerButton::BUTTON_Y, [this, idx](brls::View*) {
                openStream(items_[idx]);
                return true;
            });
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
            row->addView(card);
        }

        gridBox->addView(row);
    }

    attachLoadMoreTriggers();
}

void HomeTab::appendGridRows(size_t start_index) {
    if (!gridBox) {
        return;
    }

    for (size_t i = start_index; i < items_.size(); i += kGridColumns) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8);

        for (size_t j = i; j < i + kGridColumns && j < items_.size(); j++) {
            auto* card = new StreamCard();
            card->setData(items_[j]);
            const size_t idx = j;
            card->registerClickAction([this, idx](brls::View*) {
                playStream(items_[idx]);
                return true;
            });
            card->registerAction(newpipe::tr("common/info"), brls::ControllerButton::BUTTON_Y, [this, idx](brls::View*) {
                openStream(items_[idx]);
                return true;
            });
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
            row->addView(card);
        }

        gridBox->addView(row);
    }

    attachLoadMoreTriggers();
}

void HomeTab::attachLoadMoreTriggers() {
    if (!gridBox || !hasMore_ || loadingMore_) {
        return;
    }

    auto& children = gridBox->getChildren();
    if (children.size() < 2) {
        return;
    }

    // Focus reaching the last two rows means the user has scrolled near the
    // bottom, so the next page is fetched then. New rows append below and get
    // their own triggers, so scrolling can continue indefinitely.
    for (size_t r = children.size() - 2; r < children.size(); r++) {
        auto* row = dynamic_cast<brls::Box*>(children[r]);
        if (!row) {
            continue;
        }
        for (brls::View* card : row->getChildren()) {
            loadMoreSubscriptions_.push_back(card->getFocusEvent()->subscribe([this](brls::View*) {
                this->loadMore();
            }));
        }
    }
}

void HomeTab::loadMore() {
    if (loadingMore_ || !hasMore_ || !initialLoadCompleted_) {
        return;
    }
    if (kiosks_.empty()) {
        return;
    }

    loadingMore_ = true;
    newpipe::log_line("home: loadMore start");
    const auto feed = service_.get_home_feed_more(kiosks_[kioskIndex_].id);
    if (!feed.has_value()) {
        hasMore_ = false;
        loadingMore_ = false;
        newpipe::logf("home: loadMore failed error=%s", service_.error_message().c_str());
        return;
    }

    const size_t old_size = items_.size();
    items_ = feed->items;
    hasMore_ = !feed->continuation_token.empty();
    newpipe::logf("home: loadMore done old=%zu new=%zu hasMore=%d",
                  old_size, items_.size(), hasMore_ ? 1 : 0);
    appendGridRows(old_size);

    if (statusLabel) {
        std::string title = feed->kiosk.title;
        const std::string option_key = "settings/home_kiosk/options/" + feed->kiosk.id;
        const std::string translated = newpipe::tr(option_key);
        if (!translated.empty() && translated != option_key) {
            title = translated;
        }
        statusLabel->setText(newpipe::tr("common/count_with_title", title, items_.size()));
    }
    loadingMore_ = false;
}

void HomeTab::cycleKiosk() {
    if (!allowInitialInput()) {
        return;
    }
    if (kiosks_.empty()) {
        return;
    }

    kioskIndex_ = (kioskIndex_ + 1) % kiosks_.size();
    newpipe::logf("home: cycleKiosk newIndex=%zu id=%s", kioskIndex_, kiosks_[kioskIndex_].id.c_str());
    loadHome();
}

bool HomeTab::allowInitialInput() const {
    if (interactionReady_.load()) {
        return true;
    }

    newpipe::log_line("home: ignored startup input");
    return false;
}

void HomeTab::playStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("home: playStream url=%s", item.url.c_str());
    const auto detail = service_.get_stream_detail(item.url);
    const auto request = newpipe::build_playback_request(item, detail);
    if (!request.has_value()) {
        openStream(item);
        return;
    }

    std::string ignored_error;
    newpipe::LibraryStore::instance().add_history(detail.has_value() ? detail->item : item, &ignored_error);
    newpipe::logf("home: queue playback url=%s", request->url.c_str());
    newpipe::queue_playback(*request);
    brls::Application::quit();
}

void HomeTab::openStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("home: openStream url=%s", item.url.c_str());
    brls::Application::pushActivity(new StreamDetailActivity(item));
}

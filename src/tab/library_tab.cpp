#include "tab/library_tab.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_helper.hpp"
#include "newpipe/runtime.hpp"
#include "view/stream_card.hpp"
#include "view/tab_focus.hpp"

namespace {
constexpr size_t kGridColumns = 2;
}

LibraryTab::LibraryTab() : service_() {
    this->inflateFromXMLRes("xml/tabs/library.xml");
    brls::delay(700, [this]() { interactionReady_.store(true); });

    // Kept on the content only: clearing is destructive and needs no sidebar
    // shortcut, an empty section has nothing to clear anyway.
    this->registerAction(newpipe::tr("library/clear_action"), brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
        this->clearCurrentSection();
        return true;
    });

    this->refresh();
}

void LibraryTab::onCreate() {
    // Mirrored on the sidebar item: an empty history/favorites list has no
    // focusable child, so a content-only action could never be triggered.
    this->registerTabAction(newpipe::tr("common/refresh"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        this->refresh();
        return true;
    });
    this->registerTabAction(newpipe::tr("library/section_action"), brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
        this->toggleSection();
        return true;
    });
}

bool LibraryTab::allowInitialInput() const {
    return interactionReady_.load();
}

void LibraryTab::refresh() {
    if (this->spinner) {
        this->spinner->setVisibility(brls::Visibility::VISIBLE);
    }

    std::string error;
    newpipe::LibraryStore::instance().load(&error);
    this->items_ = this->showingFavorites_ ? newpipe::LibraryStore::instance().favorite_items()
                                           : newpipe::LibraryStore::instance().history_items();
    this->buildGrid();

    if (this->statusLabel) {
        this->statusLabel->setText(
            newpipe::tr(
                "common/count_with_title",
                this->showingFavorites_ ? newpipe::tr("library/favorites")
                                        : newpipe::tr("library/history"),
                this->items_.size()));
    }
    if (this->bodyLabel) {
        if (this->items_.empty()) {
            this->bodyLabel->setText(
                this->showingFavorites_
                    ? newpipe::tr("library/favorites_empty")
                    : newpipe::tr("library/history_empty"));
        } else {
            this->bodyLabel->setText(newpipe::tr("library/controls"));
        }
    }
    if (this->spinner) {
        this->spinner->setVisibility(brls::Visibility::GONE);
    }
}

void LibraryTab::buildGrid() {
    if (!this->gridBox) {
        return;
    }

    newpipe::release_grid_focus(this, this->gridBox);
    this->gridBox->clearViews();
    for (size_t i = 0; i < this->items_.size(); i += kGridColumns) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8);

        for (size_t j = i; j < i + kGridColumns && j < this->items_.size(); j++) {
            auto* card = new StreamCard();
            card->setData(this->items_[j]);
            const size_t idx = j;
            card->registerClickAction([this, idx](brls::View*) {
                this->playStream(this->items_[idx]);
                return true;
            });
            card->registerAction(newpipe::tr("common/info"), brls::ControllerButton::BUTTON_Y, [this, idx](brls::View*) {
                this->openStream(this->items_[idx]);
                return true;
            });
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
            row->addView(card);
        }

        this->gridBox->addView(row);
    }
}

void LibraryTab::toggleSection() {
    this->showingFavorites_ = !this->showingFavorites_;
    this->refresh();
}

void LibraryTab::clearCurrentSection() {
    std::string error;
    const bool ok = this->showingFavorites_ ? newpipe::LibraryStore::instance().clear_favorites(&error)
                                            : newpipe::LibraryStore::instance().clear_history(&error);
    if (!ok) {
        brls::Application::notify(error.empty() ? newpipe::tr("library/clear_failed") : error);
        return;
    }

    brls::Application::notify(this->showingFavorites_ ? newpipe::tr("library/favorites_cleared")
                                                      : newpipe::tr("library/history_cleared"));
    this->refresh();
}

void LibraryTab::playStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    const auto detail = this->service_.get_stream_detail(item.url);
    const auto request = newpipe::build_playback_request(item, detail);
    if (!request.has_value()) {
        this->openStream(item);
        return;
    }

    std::string ignored_error;
    newpipe::LibraryStore::instance().add_history(detail.has_value() ? detail->item : item, &ignored_error);
    newpipe::queue_playback(*request);
    brls::Application::quit();
}

void LibraryTab::openStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    brls::Application::pushActivity(new StreamDetailActivity(item));
}

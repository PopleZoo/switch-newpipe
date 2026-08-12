#include "tab/search_tab.hpp"

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

SearchTab::SearchTab() : service_() {
    this->inflateFromXMLRes("xml/tabs/search.xml");
    newpipe::log_line("search: construct");
    if (statusLabel) {
        statusLabel->setText(newpipe::tr("search/prompt"));
    }
    brls::delay(700, [this]() {
        interactionReady_.store(true);
        newpipe::log_line("search: interaction ready");
    });
}

void SearchTab::onCreate() {
    // Registered on the sidebar item as well: until a query returns results this
    // tab has no focusable child, so focus can never leave the sidebar and a
    // content-only action would be unreachable.
    this->registerTabAction(newpipe::tr("search/action"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        brls::Application::getImeManager()->openForText(
            [this](const std::string& text) { doSearch(text); },
            newpipe::tr("search/ime_title"),
            newpipe::tr("search/ime_subtitle"),
            80,
            lastQuery_);
        return true;
    });
}

void SearchTab::doSearch(const std::string& query) {
    lastQuery_ = query;
    newpipe::logf("search: doSearch query=%s", query.c_str());

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

    if (query.empty()) {
        items_.clear();
        hasMore_ = false;
        loadingMore_ = false;
        if (gridBox) {
            newpipe::release_grid_focus(this, gridBox);
            gridBox->clearViews();
        }
        if (statusLabel) {
            statusLabel->setText(newpipe::tr("search/prompt"));
        }
        if (spinner) {
            spinner->setVisibility(brls::Visibility::GONE);
        }
        return;
    }

    const auto results = service_.search(query);
    items_ = results.items;
    hasMore_ = !results.continuation_token.empty();
    newpipe::logf("search: results=%zu hasMore=%d", items_.size(), hasMore_ ? 1 : 0);
    buildGrid();

    if (statusLabel) {
        if (items_.empty()) {
            if (service_.error_message().empty()) {
                statusLabel->setText(newpipe::tr("search/no_results", query));
            } else {
                statusLabel->setText(service_.error_message());
            }
        } else {
            statusLabel->setText(newpipe::tr("search/results_count", query, items_.size()));
        }
    }
    if (spinner) {
        spinner->setVisibility(brls::Visibility::GONE);
    }
}

void SearchTab::buildGrid() {
    if (!gridBox) {
        newpipe::log_line("search: buildGrid gridBox missing");
        return;
    }

    newpipe::logf("search: buildGrid items=%zu", items_.size());
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
    newpipe::refocus_grid(this, gridBox);
}

void SearchTab::appendGridRows(size_t start_index) {
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

void SearchTab::attachLoadMoreTriggers() {
    if (!gridBox || !hasMore_ || loadingMore_) {
        return;
    }

    auto& children = gridBox->getChildren();
    if (children.size() < 2) {
        return;
    }

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

void SearchTab::loadMore() {
    if (loadingMore_ || !hasMore_) {
        return;
    }
    if (lastQuery_.empty()) {
        return;
    }

    loadingMore_ = true;
    newpipe::log_line("search: loadMore start");
    const auto results = service_.search_more(lastQuery_);
    if (results.items.empty()) {
        hasMore_ = false;
        loadingMore_ = false;
        newpipe::logf("search: loadMore failed error=%s", service_.error_message().c_str());
        return;
    }

    const size_t old_size = items_.size();
    items_.insert(items_.end(), results.items.begin(), results.items.end());
    hasMore_ = !results.continuation_token.empty();
    newpipe::logf("search: loadMore done old=%zu new=%zu hasMore=%d",
                  old_size, items_.size(), hasMore_ ? 1 : 0);
    appendGridRows(old_size);

    if (statusLabel) {
        statusLabel->setText(newpipe::tr("search/results_count", lastQuery_, items_.size()));
    }
    loadingMore_ = false;
}

bool SearchTab::allowInitialInput() const {
    if (interactionReady_.load()) {
        return true;
    }

    newpipe::log_line("search: ignored startup input");
    return false;
}

void SearchTab::playStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("search: playStream url=%s", item.url.c_str());
    const auto detail = service_.get_stream_detail(item.url);
    const auto request = newpipe::build_playback_request(item, detail);
    if (!request.has_value()) {
        openStream(item);
        return;
    }

    std::string ignored_error;
    newpipe::LibraryStore::instance().add_history(detail.has_value() ? detail->item : item, &ignored_error);
    newpipe::logf("search: queue playback url=%s", request->url.c_str());
    newpipe::queue_playback(*request);
    brls::Application::quit();
}

void SearchTab::openStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("search: openStream url=%s", item.url.c_str());
    brls::Application::pushActivity(new StreamDetailActivity(item));
}

#pragma once

#include <atomic>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "newpipe/models.hpp"
#include "newpipe/youtube_catalog_service.hpp"
#include "view/auto_tab_frame.hpp"

class SearchTab : public AttachedView {
public:
    SearchTab();

    void onCreate() override;

    static brls::View* create() { return new SearchTab(); }

private:
    void doSearch(const std::string& query);
    void buildGrid();
    void appendGridRows(size_t start_index);
    void attachLoadMoreTriggers();
    void loadMore();
    bool allowInitialInput() const;
    void playStream(const newpipe::StreamItem& item);
    void openStream(const newpipe::StreamItem& item);

    BRLS_BIND(brls::Label, statusLabel, "search/status");
    BRLS_BIND(brls::ProgressSpinner, spinner, "search/spinner");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "search/scroll");
    BRLS_BIND(brls::Box, gridBox, "search/grid");

    newpipe::YouTubeCatalogService service_;
    std::vector<newpipe::StreamItem> items_;
    std::vector<brls::GenericEvent::Subscription> loadMoreSubscriptions_;
    std::string lastQuery_;
    bool hasMore_ = false;
    bool loadingMore_ = false;
    std::atomic<bool> interactionReady_{false};
};

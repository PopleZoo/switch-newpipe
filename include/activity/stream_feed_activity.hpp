#pragma once

#include <atomic>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "newpipe/models.hpp"
#include "newpipe/youtube_catalog_service.hpp"

class StreamFeedActivity : public brls::Activity {
public:
    StreamFeedActivity(std::string title, std::vector<newpipe::StreamItem> items);
    StreamFeedActivity(std::string title, std::vector<newpipe::StreamItem> items,
                       std::function<std::optional<newpipe::HomeFeed>()> load_more_fn);

    CONTENT_FROM_XML_RES("activity/stream_feed.xml");

    void onContentAvailable() override;

private:
    void buildGrid();
    void appendGridRows(size_t start_index);
    void attachLoadMoreTriggers();
    void loadMore();
    bool allowInitialInput() const;
    void playStream(const newpipe::StreamItem& item);
    void openStream(const newpipe::StreamItem& item);

    std::string title_;
    std::vector<newpipe::StreamItem> items_;
    newpipe::YouTubeCatalogService service_;
    std::atomic<bool> interactionReady_{false};
    bool hasMore_ = false;
    bool loadingMore_ = false;
    std::function<std::optional<newpipe::HomeFeed>()> loadMoreFn_;
    std::vector<brls::GenericEvent::Subscription> loadMoreSubscriptions_;

    BRLS_BIND(brls::Label, statusLabel, "feed/status");
    BRLS_BIND(brls::Label, subtitleLabel, "feed/subtitle");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "feed/scroll");
    BRLS_BIND(brls::Box, gridBox, "feed/grid");
};

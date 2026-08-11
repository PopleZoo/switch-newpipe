#include "view/stream_card.hpp"

#include "newpipe/image_loader.hpp"
#include "newpipe/i18n.hpp"

namespace {
constexpr const char* kMetaSeparator = "  \xc2\xb7  ";
}

StreamCard::StreamCard() {
    this->inflateFromXMLRes("xml/views/stream_card.xml");
}

void StreamCard::setData(const newpipe::StreamItem& item) {
    if (titleLabel) {
        titleLabel->setText(item.title);
    }
    if (channelLabel) {
        channelLabel->setText(item.channel_name);
    }
    if (metaLabel) {
        std::string meta;
        if (!item.view_count_text.empty()) {
            meta = item.view_count_text;
        }
        if (item.is_live) {
            if (!meta.empty()) {
                meta += kMetaSeparator;
            }
            meta += newpipe::tr("stream/live_badge");
        } else {
            if (!item.published_text.empty()) {
                if (!meta.empty()) {
                    meta += kMetaSeparator;
                }
                meta += item.published_text;
            }
            if (!item.duration_text.empty()) {
                if (!meta.empty()) {
                    meta += kMetaSeparator;
                }
                meta += item.duration_text;
            }
        }
        metaLabel->setText(meta);
    }
    if (thumbnail && !item.thumbnail_url.empty()) {
        newpipe::ImageLoader::instance().load(item.thumbnail_url, thumbnail);
    }
}
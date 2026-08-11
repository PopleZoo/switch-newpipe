#pragma once

#include <optional>
#include <string>

#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/models.hpp"
#include "newpipe/switch_player.hpp"

namespace newpipe {

inline std::optional<PlaybackRequest> build_playback_request(
    const StreamItem& item,
    const std::optional<StreamDetail>& detail) {
    std::string url = item.url;
    if (url.empty() && detail.has_value() && detail->playback_url.has_value()) {
        url = *detail->playback_url;
    }

    if (url.empty()) {
        return std::nullopt;
    }

    PlaybackRequest request;
    request.title = item.title;
    request.url = url;
    request.referer = item.url;
    request.video_id = item.id;
    if (item.url.empty()) {
        request.http_header_fields =
            "Accept: */*,Accept-Encoding: identity,Connection: close,Cache-Control: no-cache";
    }

    // Continue Watching: start from the last checkpoint when a meaningful amount
    // of the video is still unwatched.
    if (!request.video_id.empty()) {
        if (const auto saved = LibraryStore::instance().history_position(request.video_id)) {
            if (saved->first >= 15.0
                && (saved->second <= 0.0 || saved->first <= saved->second - 15.0)) {
                request.start_position_seconds = saved->first;
                logf("playback: resume id=%s from %.1fs of %.1fs",
                     request.video_id.c_str(),
                     saved->first,
                     saved->second);
            }
        }
    }
    return request;
}

}  // namespace newpipe

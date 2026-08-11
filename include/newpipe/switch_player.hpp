#pragma once

#include <string>

namespace newpipe {

struct PlaybackRequest {
    std::string title;
    std::string url;
    std::string referer;
    std::string http_header_fields;
    std::string video_id;
    double start_position_seconds = 0.0;
};

bool run_switch_player(const PlaybackRequest& request, std::string& error);

}  // namespace newpipe

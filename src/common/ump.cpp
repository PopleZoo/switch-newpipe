#include "newpipe/ump.hpp"

#include <array>
#include <limits>
#include <utility>

namespace newpipe {
namespace {

std::optional<std::pair<uint64_t, size_t>> read_ump_varint(
    const uint8_t* data,
    size_t size,
    size_t offset) {
    if (!data || offset >= size) {
        return std::nullopt;
    }

    const uint8_t first = data[offset];
    const size_t length = first < 128 ? 1 : first < 192 ? 2 : first < 224 ? 3 : first < 240 ? 4 : 5;
    if (length > size - offset) {
        return std::nullopt;
    }

    uint64_t value = 0;
    switch (length) {
        case 1:
            value = first;
            break;
        case 2:
            value = (first & 0x3fU) + 64ULL * data[offset + 1];
            break;
        case 3:
            value = (first & 0x1fU)
                + 32ULL * (data[offset + 1] + 256ULL * data[offset + 2]);
            break;
        case 4:
            value = (first & 0x0fU)
                + 16ULL * (data[offset + 1]
                    + 256ULL * (data[offset + 2] + 256ULL * data[offset + 3]));
            break;
        default:
            value = static_cast<uint64_t>(data[offset + 1])
                | (static_cast<uint64_t>(data[offset + 2]) << 8U)
                | (static_cast<uint64_t>(data[offset + 3]) << 16U)
                | (static_cast<uint64_t>(data[offset + 4]) << 24U);
            break;
    }
    return std::make_pair(value, offset + length);
}

std::optional<std::pair<uint64_t, size_t>> read_proto_varint(
    const uint8_t* data,
    size_t size,
    size_t offset) {
    uint64_t value = 0;
    unsigned shift = 0;
    while (offset < size && shift < 64) {
        const uint8_t byte = data[offset++];
        value |= static_cast<uint64_t>(byte & 0x7fU) << shift;
        if ((byte & 0x80U) == 0) {
            return std::make_pair(value, offset);
        }
        shift += 7;
    }
    return std::nullopt;
}

std::string decode_first_proto_string(const uint8_t* data, size_t size) {
    if (!data || size < 2 || data[0] != 0x0a) {
        return {};
    }

    const auto length = read_proto_varint(data, size, 1);
    if (!length.has_value()
        || length->first > size - length->second
        || length->first > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return {};
    }

    return std::string(
        reinterpret_cast<const char*>(data + length->second),
        static_cast<size_t>(length->first));
}

std::optional<int> decode_stream_protection_status(const uint8_t* data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        const auto key = read_proto_varint(data, size, offset);
        if (!key.has_value()) {
            return std::nullopt;
        }
        offset = key->second;
        const uint64_t field = key->first >> 3U;
        const uint64_t wire = key->first & 7U;
        if (wire == 0) {
            const auto value = read_proto_varint(data, size, offset);
            if (!value.has_value()) {
                return std::nullopt;
            }
            offset = value->second;
            if (field == 1) {
                return static_cast<int>(value->first);
            }
            continue;
        }
        if (wire == 2) {
            const auto length = read_proto_varint(data, size, offset);
            if (!length.has_value() || length->first > size - length->second) {
                return std::nullopt;
            }
            offset = length->second + static_cast<size_t>(length->first);
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace

UmpParseResult parse_ump_response(const uint8_t* data, size_t size) {
    UmpParseResult result;
    size_t offset = 0;
    while (offset < size) {
        const auto type = read_ump_varint(data, size, offset);
        if (!type.has_value()) {
            result.complete = false;
            break;
        }
        const auto part_size = read_ump_varint(data, size, type->second);
        if (!part_size.has_value()
            || part_size->first > size - part_size->second
            || part_size->first > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
            result.complete = false;
            break;
        }

        const auto* payload = data + part_size->second;
        const size_t payload_size = static_cast<size_t>(part_size->first);
        offset = part_size->second + payload_size;

        switch (type->first) {
            case 21:  // MEDIA: first byte is the media field discriminator.
                if (payload_size > 1) {
                    result.media_data.insert(
                        result.media_data.end(), payload + 1, payload + payload_size);
                }
                break;
            case 43:  // SABR_REDIRECT
                result.redirect_url = decode_first_proto_string(payload, payload_size);
                break;
            case 58: {  // STREAM_PROTECTION_STATUS
                const auto status = decode_stream_protection_status(payload, payload_size);
                if (status.has_value()) {
                    result.stream_protection_statuses.push_back(*status);
                }
                break;
            }
            default:
                break;
        }
    }
    return result;
}

std::optional<std::string> find_url_query_parameter(
    const std::string& url,
    const std::string& key) {
    const std::string marker = key + "=";
    const size_t query = url.find('?');
    if (query == std::string::npos) {
        return std::nullopt;
    }

    size_t position = query + 1;
    while (position <= url.size()) {
        const size_t end = url.find_first_of("&#", position);
        const size_t item_end = end == std::string::npos ? url.size() : end;
        if (item_end >= position + marker.size()
            && url.compare(position, marker.size(), marker) == 0) {
            return url.substr(position + marker.size(), item_end - position - marker.size());
        }
        if (end == std::string::npos || url[end] == '#') {
            break;
        }
        position = end + 1;
    }
    return std::nullopt;
}

std::string set_url_query_parameter(
    const std::string& url,
    const std::string& key,
    const std::string& value) {
    const std::string marker = key + "=";
    const size_t fragment = url.find('#');
    const size_t query = url.find('?');
    const size_t url_end = fragment == std::string::npos ? url.size() : fragment;

    if (query != std::string::npos && query < url_end) {
        size_t position = query + 1;
        while (position <= url_end) {
            const size_t separator = url.find('&', position);
            const size_t item_end = separator == std::string::npos || separator > url_end
                ? url_end
                : separator;
            if (item_end >= position + marker.size()
                && url.compare(position, marker.size(), marker) == 0) {
                return url.substr(0, position + marker.size()) + value + url.substr(item_end);
            }
            if (separator == std::string::npos || separator >= url_end) {
                break;
            }
            position = separator + 1;
        }
    }

    const std::string addition = (query == std::string::npos || query > url_end ? "?" : "&")
        + marker + value;
    return url.substr(0, url_end) + addition + url.substr(url_end);
}

std::string build_ump_request_url(
    const std::string& base_url,
    uint64_t range_start,
    uint64_t range_end,
    int request_number) {
    std::string result = set_url_query_parameter(
        base_url, "range", std::to_string(range_start) + "-" + std::to_string(range_end));
    result = set_url_query_parameter(result, "ump", "1");
    result = set_url_query_parameter(result, "srfvp", "1");
    result = set_url_query_parameter(result, "alr", "yes");
    return set_url_query_parameter(result, "rn", std::to_string(request_number));
}

std::string merge_ump_redirect_parameters(
    const std::string& redirect_url,
    const std::string& request_url) {
    std::string result = redirect_url;
    constexpr std::array<const char*, 6> kParameters = {
        "range", "ump", "srfvp", "alr", "rn", "pot"
    };
    for (const char* key : kParameters) {
        if (find_url_query_parameter(result, key).has_value()) {
            continue;
        }
        const auto value = find_url_query_parameter(request_url, key);
        if (value.has_value()) {
            result = set_url_query_parameter(result, key, *value);
        }
    }
    return result;
}

}  // namespace newpipe

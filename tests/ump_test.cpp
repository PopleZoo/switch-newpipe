#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "newpipe/ump.hpp"

int main() {
    const std::vector<uint8_t> envelope = {
        21, 4, 0, 'f', 't', 'y',
        58, 2, 8, 1,
        43, 11, 10, 9, 'h', 't', 't', 'p', 's', ':', '/', '/', 'x',
    };
    const auto parsed = newpipe::parse_ump_response(envelope.data(), envelope.size());
    assert(parsed.complete);
    assert(std::string(parsed.media_data.begin(), parsed.media_data.end()) == "fty");
    assert(parsed.stream_protection_statuses == std::vector<int>{1});
    assert(parsed.redirect_url == "https://x");

    // A real 1 MiB media response uses the three-byte UMP length encoding.
    constexpr size_t kMediaBytes = 1024 * 1024;
    constexpr size_t kPayloadBytes = kMediaBytes + 1;
    std::vector<uint8_t> large_envelope = {
        21,
        static_cast<uint8_t>((kPayloadBytes & 0x1fU) | 0xc0U),
        static_cast<uint8_t>((kPayloadBytes >> 5U) & 0xffU),
        static_cast<uint8_t>(kPayloadBytes >> 13U),
        0,
    };
    large_envelope.resize(large_envelope.size() + kMediaBytes, 0x5a);
    const auto large_parsed = newpipe::parse_ump_response(
        large_envelope.data(), large_envelope.size());
    assert(large_parsed.complete);
    assert(large_parsed.media_data.size() == kMediaBytes);
    assert(large_parsed.media_data.front() == 0x5a);
    assert(large_parsed.media_data.back() == 0x5a);

    const std::vector<uint8_t> truncated = {21, 4, 0, 'f'};
    assert(!newpipe::parse_ump_response(truncated.data(), truncated.size()).complete);

    const std::string request = newpipe::build_ump_request_url(
        "https://cdn.example/videoplayback?pot=token#fragment", 10, 99, 7);
    assert(newpipe::find_url_query_parameter(request, "pot") == "token");
    assert(newpipe::find_url_query_parameter(request, "range") == "10-99");
    assert(newpipe::find_url_query_parameter(request, "ump") == "1");
    assert(newpipe::find_url_query_parameter(request, "srfvp") == "1");
    assert(newpipe::find_url_query_parameter(request, "alr") == "yes");
    assert(newpipe::find_url_query_parameter(request, "rn") == "7");

    const std::string redirected = newpipe::merge_ump_redirect_parameters(
        "https://redirect.example/videoplayback?foo=bar", request);
    assert(newpipe::find_url_query_parameter(redirected, "foo") == "bar");
    assert(newpipe::find_url_query_parameter(redirected, "pot") == "token");
    assert(newpipe::find_url_query_parameter(redirected, "range") == "10-99");

    std::cout << "ump tests passed\n";
    return 0;
}

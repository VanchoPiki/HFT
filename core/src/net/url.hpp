#pragma once

#include <string>
#include <string_view>

namespace hft::net {

struct Url {
    std::string scheme;  // http, https, ws, wss
    std::string host;
    std::string port;
    std::string target;  // path + query, at least "/"

    bool tls() const { return scheme == "https" || scheme == "wss"; }

    // Throws std::invalid_argument on malformed or unsupported URLs.
    static Url parse(std::string_view text);
};

}  // namespace hft::net

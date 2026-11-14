#pragma once

#include <string>
#include <string_view>

namespace hft::net {

struct Url {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;

    bool tls() const { return scheme == "https" || scheme == "wss"; }

    static Url parse(std::string_view text);
};

}

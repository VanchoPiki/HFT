#include "net/url.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace hft::net {

Url Url::parse(std::string_view text) {
    const auto sep = text.find("://");
    if (sep == std::string_view::npos) {
        throw std::invalid_argument("url: missing scheme in '" + std::string(text) + "'");
    }
    Url url;
    url.scheme = std::string(text.substr(0, sep));
    std::transform(url.scheme.begin(), url.scheme.end(), url.scheme.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (url.scheme != "http" && url.scheme != "https" && url.scheme != "ws" &&
        url.scheme != "wss") {
        throw std::invalid_argument("url: unsupported scheme '" + url.scheme + "'");
    }

    const std::string_view rest = text.substr(sep + 3);
    const auto path_start = rest.find_first_of("/?");
    const std::string_view authority = rest.substr(0, path_start);
    if (path_start == std::string_view::npos) {
        url.target = "/";
    } else {
        url.target = std::string(rest.substr(path_start));
        if (url.target.front() == '?') {
            url.target.insert(url.target.begin(), '/');
        }
    }

    const auto colon = authority.rfind(':');
    if (colon == std::string_view::npos) {
        url.host = std::string(authority);
        url.port = url.tls() ? "443" : "80";
    } else {
        url.host = std::string(authority.substr(0, colon));
        url.port = std::string(authority.substr(colon + 1));
    }
    if (url.host.empty() || url.port.empty()) {
        throw std::invalid_argument("url: missing host or port in '" + std::string(text) + "'");
    }
    return url;
}

}  // namespace hft::net

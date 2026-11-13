#pragma once

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace hft::net {

struct HttpStatusError : std::runtime_error {
    HttpStatusError(int status, const std::string& body);
    int status;
};

// Blocking HTTP/1.1 client over one kept-alive connection (http or https, TCP_NODELAY).
// Not thread-safe: use one client per thread.
class HttpClient {
public:
    explicit HttpClient(const std::string& url,
                        std::chrono::milliseconds timeout = std::chrono::seconds(10));
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // POSTs JSON to the URL and returns the response body. Throws HttpStatusError on
    // non-200 responses and std::exception on transport errors or timeouts.
    std::string post_json(std::string_view body);

    // GET of a path on the same host, e.g. "/api/config".
    std::string get(std::string_view target);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hft::net

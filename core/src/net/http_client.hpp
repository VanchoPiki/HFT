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

class HttpClient {
public:
    explicit HttpClient(const std::string& url,
                        std::chrono::milliseconds timeout = std::chrono::seconds(10));
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    std::string post_json(std::string_view body);

    std::string get(std::string_view target);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}

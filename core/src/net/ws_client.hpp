#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace hft::net {

class WsClient {
public:
    explicit WsClient(const std::string& url);
    ~WsClient();
    WsClient(const WsClient&) = delete;
    WsClient& operator=(const WsClient&) = delete;

    void connect();
    void send(std::string_view text);
    std::string read();
    std::optional<std::string> read_for(std::chrono::microseconds timeout);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}

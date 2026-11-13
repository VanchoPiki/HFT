#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace hft::net {

// Blocking WebSocket client (ws or wss) meant to be owned by one network thread.
// Sends keep-alive pings while reading and fails read() if the peer goes silent.
class WsClient {
public:
    explicit WsClient(const std::string& url);
    ~WsClient();
    WsClient(const WsClient&) = delete;
    WsClient& operator=(const WsClient&) = delete;

    void connect();
    void send(std::string_view text);
    std::string read();  // next text message; throws on close, error or timeout
    // Like read(), but gives up after `timeout` and returns nullopt. The read stays pending
    // (nothing is cancelled), so the next call simply keeps waiting for the same message.
    std::optional<std::string> read_for(std::chrono::microseconds timeout);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hft::net

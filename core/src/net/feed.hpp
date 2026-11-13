#pragma once

#include <atomic>
#include <chrono>
#include <string>

#include "net/rpc_types.hpp"

namespace hft::net {

// Receives chain events on the feed's thread.
class FeedHandler {
public:
    virtual ~FeedHandler() = default;
    // Subscriptions are (re)established. Events may have been missed: resync state.
    virtual void on_connected() {}
    virtual void on_block(const BlockHeader& header) = 0;
    virtual void on_log(const Log& log) = 0;
    // The feed has had nothing to deliver for a moment: a burst of logs is complete.
    virtual void on_idle() {}
};

class Feed {
public:
    virtual ~Feed() = default;
    // Blocks until `stop` is set. Reconnects on errors.
    virtual void run(FeedHandler& handler, const std::atomic<bool>& stop) = 0;
};

// eth_subscribe over WebSocket: newHeads + logs matching the filter. Lowest latency.
class WsFeed final : public Feed {
public:
    WsFeed(std::string ws_url, LogFilter filter);
    void run(FeedHandler& handler, const std::atomic<bool>& stop) override;

private:
    std::string url_;
    LogFilter filter_;
};

// Polls eth_blockNumber + eth_getLogs over HTTP, for endpoints without WebSocket.
class PollingFeed final : public Feed {
public:
    PollingFeed(std::string http_url, LogFilter filter, std::chrono::milliseconds interval);
    void run(FeedHandler& handler, const std::atomic<bool>& stop) override;

private:
    std::string url_;
    LogFilter filter_;
    std::chrono::milliseconds interval_;
};

}  // namespace hft::net

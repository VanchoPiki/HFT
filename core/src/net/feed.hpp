#pragma once

#include <atomic>
#include <chrono>
#include <string>

#include "net/rpc_types.hpp"

namespace hft::net {

class FeedHandler {
public:
    virtual ~FeedHandler() = default;
    virtual void on_connected() {}
    virtual void on_block(const BlockHeader& header) = 0;
    virtual void on_log(const Log& log) = 0;
    virtual void on_idle() {}
};

class Feed {
public:
    virtual ~Feed() = default;
    virtual void run(FeedHandler& handler, const std::atomic<bool>& stop) = 0;
};

class WsFeed final : public Feed {
public:
    WsFeed(std::string ws_url, LogFilter filter);
    void run(FeedHandler& handler, const std::atomic<bool>& stop) override;

private:
    std::string url_;
    LogFilter filter_;
};

class PollingFeed final : public Feed {
public:
    PollingFeed(std::string http_url, LogFilter filter, std::chrono::milliseconds interval);
    void run(FeedHandler& handler, const std::atomic<bool>& stop) override;

private:
    std::string url_;
    LogFilter filter_;
    std::chrono::milliseconds interval_;
};

}

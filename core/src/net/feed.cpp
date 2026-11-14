#include "net/feed.hpp"

#include <simdjson.h>

#include <thread>

#include "net/json_rpc.hpp"
#include "net/url.hpp"
#include "net/ws_client.hpp"
#include "util/log.hpp"

namespace hft::net {
namespace {

std::string redacted(const std::string& url) {
    const Url u = Url::parse(url);
    return u.scheme + "://" + u.host + (u.target.size() > 1 ? "/…" : "");
}

constexpr auto kReconnectDelay = std::chrono::seconds(1);
constexpr auto kBurstGap = std::chrono::milliseconds(2);

void sleep_unless_stopped(std::chrono::milliseconds total, const std::atomic<bool>& stop) {
    const auto deadline = std::chrono::steady_clock::now() + total;
    while (!stop && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

}

WsFeed::WsFeed(std::string ws_url, LogFilter filter)
    : url_(std::move(ws_url)), filter_(std::move(filter)) {}

void WsFeed::run(FeedHandler& handler, const std::atomic<bool>& stop) {
    simdjson::dom::parser parser;
    while (!stop) {
        try {
            WsClient ws(url_);
            ws.connect();
            ws.send(R"({"jsonrpc":"2.0","id":1,"method":"eth_subscribe","params":["newHeads"]})");
            ws.send(R"({"jsonrpc":"2.0","id":2,"method":"eth_subscribe","params":["logs",)" +
                    filter_.to_json() + "]}");
            log::info("ws feed: connected to {}", redacted(url_));

            std::string heads_sub;
            std::string logs_sub;
            bool announced = false;
            bool idle_reported = true;
            while (!stop) {
                const auto message = ws.read_for(kBurstGap);
                if (!message) {
                    if (!idle_reported) {
                        idle_reported = true;
                        handler.on_idle();
                    }
                    continue;
                }
                idle_reported = false;
                simdjson::dom::element doc = parser.parse(*message);
                simdjson::dom::element params;
                if (doc["params"].get(params) == simdjson::SUCCESS) {
                    const std::string_view sub = params["subscription"];
                    const simdjson::dom::element result = params["result"];
                    if (sub == logs_sub) {
                        handler.on_log(parse_log(result));
                    } else if (sub == heads_sub) {
                        handler.on_block(parse_block_header(result));
                    }
                    continue;
                }
                throw_if_rpc_error(doc);
                const std::uint64_t id = doc["id"];
                const std::string_view sub = doc["result"];
                (id == 1 ? heads_sub : logs_sub) = sub;
                if (!announced && !heads_sub.empty() && !logs_sub.empty()) {
                    announced = true;
                    handler.on_connected();
                }
            }
        } catch (const std::exception& e) {
            log::warn("ws feed: {} - reconnecting", e.what());
            sleep_unless_stopped(kReconnectDelay, stop);
        }
    }
}

PollingFeed::PollingFeed(std::string http_url, LogFilter filter, std::chrono::milliseconds interval)
    : url_(std::move(http_url)), filter_(std::move(filter)), interval_(interval) {}

void PollingFeed::run(FeedHandler& handler, const std::atomic<bool>& stop) {
    JsonRpcClient rpc(url_);
    std::uint64_t last = 0;
    bool connected = false;
    while (!stop) {
        try {
            const std::uint64_t head = rpc.block_number();
            if (!connected) {
                connected = true;
                last = head;
                log::info("polling feed: {} every {} ms from block {}", redacted(url_),
                          interval_.count(), head);
                handler.on_connected();
            } else if (head > last) {
                LogFilter range = filter_;
                range.from_block = last + 1;
                range.to_block = head;
                for (const Log& log : rpc.get_logs(range)) {
                    handler.on_log(log);
                }
                last = head;
                handler.on_block(BlockHeader{.number = head});
            }
        } catch (const std::exception& e) {
            log::warn("polling feed: {}", e.what());
            connected = false;
        }
        sleep_unless_stopped(interval_, stop);
    }
}

}

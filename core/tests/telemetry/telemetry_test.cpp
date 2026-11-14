#include <gtest/gtest.h>
#include <simdjson.h>

#include <chrono>
#include <thread>
#include <zmq.hpp>

#include "telemetry/events.hpp"
#include "telemetry/publisher.hpp"

namespace hft::telemetry {
namespace {

const eth::Address kWeth = *eth::Address::from_hex("0x82aF49447D8a07e3bd95BD0d56f35241523fBab1");
const eth::Address kUsdc = *eth::Address::from_hex("0xaf88d065e77c8cC2239327C5EDb3A432268e5831");

simdjson::dom::element parse(simdjson::dom::parser& parser, const std::string& json) {
    return parser.parse(json);
}

TEST(TelemetryEventsTest, PoolSnapshotIsValidJson) {
    dex::Market m;
    m.add_token({kWeth, "WETH", 18});
    m.add_token({kUsdc, "USDC", 6});
    eth::Address address;
    address.bytes[19] = 1;
    dex::V3Pool& pool = m.add_pool({address, kWeth, kUsdc, 500, 10});
    pool.reset(eth::u256("3833558700599279129460736"), -198736, eth::u256("3883490737149040689"), 7);

    simdjson::dom::parser parser;
    const auto e = parse(parser, pool_snapshot_event(m, pool, 7));
    EXPECT_EQ(std::string_view(e["type"]), "pool_snapshot");
    EXPECT_EQ(std::uint64_t(e["block"]), 7u);
    EXPECT_EQ(std::string_view(e["pair"]), "WETH/USDC");
    EXPECT_EQ(std::int64_t(e["tick"]), -198736);
    EXPECT_NEAR(double(e["price"]), 2341.2, 0.1);
    EXPECT_EQ(std::string_view(e["liquidity"]), "3883490737149040689");
}

TEST(TelemetryEventsTest, HeartbeatIsValidJson) {
    simdjson::dom::parser parser;
    const auto e = parse(parser, heartbeat_event(42161, 100, 20, "monitor", 3));
    EXPECT_EQ(std::string_view(e["mode"]), "monitor");
    EXPECT_EQ(std::uint64_t(e["dropped_events"]), 3u);
    EXPECT_GT(std::int64_t(e["ts_ms"]), 1'700'000'000'000);
}

TEST(TelemetryPublisherTest, DeliversToASubscriber) {
    const std::string endpoint = "tcp://127.0.0.1:55871";
    Publisher publisher(endpoint);

    zmq::context_t context(1);
    zmq::socket_t sub(context, zmq::socket_type::sub);
    sub.set(zmq::sockopt::subscribe, "");
    sub.set(zmq::sockopt::rcvtimeo, 2000);
    sub.connect(endpoint);

    zmq::message_t message;
    bool received = false;
    for (int attempt = 0; attempt < 20 && !received; ++attempt) {
        publisher.publish(R"({"type":"heartbeat"})");
        sub.set(zmq::sockopt::rcvtimeo, 100);
        received = sub.recv(message, zmq::recv_flags::none).has_value();
    }
    ASSERT_TRUE(received);
    EXPECT_EQ(message.to_string(), R"({"type":"heartbeat"})");
}

TEST(TelemetryPublisherTest, DisabledPublisherIgnoresEvents) {
    Publisher publisher("");
    publisher.publish("{}");
    EXPECT_EQ(publisher.published(), 0u);
    EXPECT_EQ(publisher.dropped(), 0u);
}

TEST(BoundedQueueTest, DropsWhenFullInsteadOfBlocking) {
    BoundedQueue<int> q(2);
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_FALSE(q.try_push(3));
    int v = 0;
    EXPECT_TRUE(q.pop(v, std::chrono::milliseconds(1)));
    EXPECT_EQ(v, 1);
    q.close();
    EXPECT_TRUE(q.pop(v, std::chrono::milliseconds(1)));
    EXPECT_FALSE(q.pop(v, std::chrono::milliseconds(1)));
    EXPECT_TRUE(q.closed_and_empty());
}

}
}

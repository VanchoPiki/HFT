#include <gtest/gtest.h>
#include <simdjson.h>

#include <string>

#include "dex/v3_pool.hpp"

namespace hft::dex {
namespace {

eth::u256 u(simdjson::dom::element e) { return eth::u256(std::string(std::string_view(e))); }

// Pools snapshotted from Arbitrum together with QuoterV2's answers at the same block
// (written by `hft_verify --dump`). Local quotes must reproduce them to the wei.
TEST(V3QuoteFixtureTest, MatchesQuoterV2ToTheWei) {
    simdjson::dom::parser parser;
    const simdjson::padded_string json =
        simdjson::padded_string::load(HFT_FIXTURES_DIR "/v3_quotes.json");
    const simdjson::dom::element doc = parser.parse(json);
    const std::uint64_t block = doc["block"];

    int checked = 0;
    int crossed = 0;
    for (simdjson::dom::element p : doc["pools"].get_array()) {
        PoolInfo info;
        info.address = *eth::Address::from_hex(std::string_view(p["address"]));
        info.fee = static_cast<std::uint32_t>(std::uint64_t(p["fee"]));
        info.tick_spacing = static_cast<int>(std::int64_t(p["tick_spacing"]));
        V3Pool pool(info);
        pool.reset(u(p["sqrt_price_x96"]), static_cast<int>(std::int64_t(p["tick"])),
                   u(p["liquidity"]), block);
        pool.set_known_tick_range(static_cast<int>(std::int64_t(p["known_min_tick"])),
                                  static_cast<int>(std::int64_t(p["known_max_tick"])));
        for (simdjson::dom::array t : p["ticks"].get_array()) {
            pool.set_tick(static_cast<int>(std::int64_t(t.at(0))),
                          eth::i256(std::string(std::string_view(t.at(1)))), u(t.at(2)));
        }
        for (simdjson::dom::element q : p["quotes"].get_array()) {
            const auto quote = pool.quote_exact_input(bool(q["zero_for_one"]), u(q["amount_in"]));
            ASSERT_TRUE(quote.has_value());
            EXPECT_EQ(quote->amount_out, u(q["amount_out"])) << info.address.to_hex();
            EXPECT_EQ(quote->sqrt_price_after, u(q["sqrt_price_after"])) << info.address.to_hex();
            crossed += quote->ticks_crossed;
            ++checked;
        }
    }
    EXPECT_GE(checked, 10);
    EXPECT_GT(crossed, 0) << "fixture should exercise tick crossings";
}

}  // namespace
}  // namespace hft::dex

#include "net/url.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace hft::net {
namespace {

TEST(UrlTest, HttpsWithPathAndDefaultPort) {
    const Url u = Url::parse("https://arb-mainnet.g.alchemy.com/v2/KEY");
    EXPECT_EQ(u.scheme, "https");
    EXPECT_EQ(u.host, "arb-mainnet.g.alchemy.com");
    EXPECT_EQ(u.port, "443");
    EXPECT_EQ(u.target, "/v2/KEY");
    EXPECT_TRUE(u.tls());
}

TEST(UrlTest, PlainWithPortAndNoPath) {
    const Url u = Url::parse("ws://127.0.0.1:8545");
    EXPECT_EQ(u.host, "127.0.0.1");
    EXPECT_EQ(u.port, "8545");
    EXPECT_EQ(u.target, "/");
    EXPECT_FALSE(u.tls());
}

TEST(UrlTest, QueryWithoutPath) {
    EXPECT_EQ(Url::parse("HTTP://host?key=1").target, "/?key=1");
}

TEST(UrlTest, RejectsBadUrls) {
    EXPECT_THROW(Url::parse("localhost:8545"), std::invalid_argument);
    EXPECT_THROW(Url::parse("ftp://host"), std::invalid_argument);
    EXPECT_THROW(Url::parse("http://:80"), std::invalid_argument);
}

}  // namespace
}  // namespace hft::net

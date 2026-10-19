
#include "rolling_vwap.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using cmarket::RollingVWAP;

TEST(RollingVWAPTest, RejectsZeroWindowSize) {
    EXPECT_THROW(RollingVWAP(0), std::invalid_argument);
}

TEST(RollingVWAPTest, EmptyWindowHasNoVWAP) {
    RollingVWAP vwap(10);

    EXPECT_FALSE(vwap.get_vwap("AAPL").has_value());
    EXPECT_EQ(vwap.trade_count("AAPL"), 0U);
}

TEST(RollingVWAPTest, SingleTrade) {
    RollingVWAP vwap(10);

    vwap.add_trade("AAPL", 100.0, 5.0);

    ASSERT_TRUE(vwap.get_vwap("AAPL").has_value());
    EXPECT_DOUBLE_EQ(*vwap.get_vwap("AAPL"), 100.0);
    EXPECT_EQ(vwap.trade_count("AAPL"), 1U);
}

TEST(RollingVWAPTest, CalculatesVolumeWeightedAverage) {
    RollingVWAP vwap(10);

    vwap.add_trade("AAPL", 100.0, 10.0);
    vwap.add_trade("AAPL", 110.0, 30.0);

    // Verify VWAP uses traded quantities as weights.
    EXPECT_NEAR(*vwap.get_vwap("AAPL"), 107.5, 1e-9);
}

TEST(RollingVWAPTest, RemovesOldestTradeWhenWindowFills) {
    RollingVWAP vwap(2);

    vwap.add_trade("AAPL", 100.0, 10.0);
    vwap.add_trade("AAPL", 110.0, 10.0);
    vwap.add_trade("AAPL", 120.0, 10.0);

    EXPECT_EQ(vwap.trade_count("AAPL"), 2U);
    EXPECT_NEAR(*vwap.get_vwap("AAPL"), 115.0, 1e-9);
}

TEST(RollingVWAPTest, SupportsWindowSizeOne) {
    RollingVWAP vwap(1);

    vwap.add_trade("AAPL", 100.0, 5.0);
    vwap.add_trade("AAPL", 125.0, 10.0);

    EXPECT_EQ(vwap.trade_count("AAPL"), 1U);
    EXPECT_DOUBLE_EQ(*vwap.get_vwap("AAPL"), 125.0);
}

TEST(RollingVWAPTest, MaintainsIndependentSymbols) {
    RollingVWAP vwap(2);

    vwap.add_trade("AAPL", 100.0, 10.0);
    vwap.add_trade("MSFT", 200.0, 5.0);
    vwap.add_trade("AAPL", 120.0, 10.0);

    EXPECT_NEAR(*vwap.get_vwap("AAPL"), 110.0, 1e-9);
    EXPECT_DOUBLE_EQ(*vwap.get_vwap("MSFT"), 200.0);
}

TEST(RollingVWAPTest, RejectsInvalidTrades) {
    RollingVWAP vwap(10);

    EXPECT_THROW(
        vwap.add_trade("", 100.0, 10.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        vwap.add_trade("AAPL", 0.0, 10.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        vwap.add_trade("AAPL", -100.0, 10.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        vwap.add_trade("AAPL", 100.0, 0.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        vwap.add_trade("AAPL", 100.0, -5.0),
        std::invalid_argument
    );

    EXPECT_EQ(vwap.trade_count("AAPL"), 0U);
}

TEST(RollingVWAPTest, ClearsIndividualSymbol) {
    RollingVWAP vwap(10);

    vwap.add_trade("AAPL", 100.0, 10.0);
    vwap.add_trade("MSFT", 200.0, 10.0);

    vwap.clear_symbol("AAPL");

    EXPECT_FALSE(vwap.get_vwap("AAPL").has_value());
    EXPECT_TRUE(vwap.get_vwap("MSFT").has_value());
}

TEST(RollingVWAPTest, ClearsAllSymbols) {
    RollingVWAP vwap(10);

    vwap.add_trade("AAPL", 100.0, 10.0);
    vwap.add_trade("MSFT", 200.0, 10.0);

    vwap.clear();

    EXPECT_FALSE(vwap.get_vwap("AAPL").has_value());
    EXPECT_FALSE(vwap.get_vwap("MSFT").has_value());
}

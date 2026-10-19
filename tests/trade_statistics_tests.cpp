
#include "trade_statistics.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

using cmarket::TradeAggressorSide;
using cmarket::TradeStatistics;

TEST(TradeStatisticsTest, EmptyHistory) {
    TradeStatistics stats;

    EXPECT_FALSE(stats.get_statistics("AAPL").has_value());
    EXPECT_EQ(stats.trade_count("AAPL"), 0U);
}

TEST(TradeStatisticsTest, SingleTrade) {
    TradeStatistics stats;

    stats.add_trade("AAPL", 100.0, 10.0);

    auto result = stats.get_statistics("AAPL");
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->trade_count, 1U);
    EXPECT_DOUBLE_EQ(result->total_volume, 10.0);
    EXPECT_DOUBLE_EQ(result->average_trade_price, 100.0);
    EXPECT_DOUBLE_EQ(result->minimum_trade_price, 100.0);
    EXPECT_DOUBLE_EQ(result->maximum_trade_price, 100.0);
    EXPECT_DOUBLE_EQ(result->average_trade_size, 10.0);
    EXPECT_DOUBLE_EQ(result->unknown_volume, 10.0);
}

TEST(TradeStatisticsTest, MultipleTrades) {
    TradeStatistics stats;

    stats.add_trade("AAPL", 100.0, 10.0);
    stats.add_trade("AAPL", 110.0, 20.0);
    stats.add_trade("AAPL", 120.0, 30.0);

    auto result = stats.get_statistics("AAPL");
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->trade_count, 3U);
    EXPECT_DOUBLE_EQ(result->total_volume, 60.0);
    EXPECT_DOUBLE_EQ(result->average_trade_price, 110.0);
    EXPECT_DOUBLE_EQ(result->minimum_trade_price, 100.0);
    EXPECT_DOUBLE_EQ(result->maximum_trade_price, 120.0);
    EXPECT_DOUBLE_EQ(result->average_trade_size, 20.0);
}

TEST(TradeStatisticsTest, TracksAggressorVolumes) {
    TradeStatistics stats;

    stats.add_trade(
        "AAPL", 100.0, 10.0, TradeAggressorSide::Buy
    );

    stats.add_trade(
        "AAPL", 110.0, 20.0, TradeAggressorSide::Sell
    );

    stats.add_trade(
        "AAPL", 120.0, 5.0, TradeAggressorSide::Unknown
    );

    auto result = stats.get_statistics("AAPL");
    ASSERT_TRUE(result.has_value());

    EXPECT_DOUBLE_EQ(result->buy_volume, 10.0);
    EXPECT_DOUBLE_EQ(result->sell_volume, 20.0);
    EXPECT_DOUBLE_EQ(result->unknown_volume, 5.0);
    EXPECT_DOUBLE_EQ(result->total_volume, 35.0);
}

TEST(TradeStatisticsTest, MultipleSymbols) {
    TradeStatistics stats;

    stats.add_trade("AAPL", 100.0, 10.0);
    stats.add_trade("MSFT", 200.0, 20.0);
    stats.add_trade("AAPL", 120.0, 30.0);

    auto aapl = stats.get_statistics("AAPL");
    auto msft = stats.get_statistics("MSFT");

    ASSERT_TRUE(aapl.has_value());
    ASSERT_TRUE(msft.has_value());

    EXPECT_EQ(aapl->trade_count, 2U);
    EXPECT_EQ(msft->trade_count, 1U);

    EXPECT_DOUBLE_EQ(aapl->total_volume, 40.0);
    EXPECT_DOUBLE_EQ(msft->total_volume, 20.0);
}

TEST(TradeStatisticsTest, RejectsInvalidTrades) {
    TradeStatistics stats;

    EXPECT_THROW(
        stats.add_trade("", 100.0, 10.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        stats.add_trade("AAPL", 0.0, 10.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        stats.add_trade("AAPL", -10.0, 10.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        stats.add_trade("AAPL", 100.0, 0.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        stats.add_trade("AAPL", 100.0, -10.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        stats.add_trade(
            "AAPL",
            std::numeric_limits<double>::infinity(),
            10.0
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        stats.add_trade(
            "AAPL",
            100.0,
            std::numeric_limits<double>::quiet_NaN()
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        stats.add_trade(
            "AAPL",
            100.0,
            10.0,
            static_cast<TradeAggressorSide>(99)
        ),
        std::invalid_argument
    );

    EXPECT_EQ(stats.trade_count("AAPL"), 0U);
}

TEST(TradeStatisticsTest, ClearsOneSymbol) {
    TradeStatistics stats;

    stats.add_trade("AAPL", 100.0, 10.0);
    stats.add_trade("MSFT", 200.0, 20.0);

    stats.clear_symbol("AAPL");

    EXPECT_FALSE(stats.get_statistics("AAPL").has_value());
    EXPECT_TRUE(stats.get_statistics("MSFT").has_value());
}

TEST(TradeStatisticsTest, ClearsAllSymbols) {
    TradeStatistics stats;

    stats.add_trade("AAPL", 100.0, 10.0);
    stats.add_trade("MSFT", 200.0, 20.0);

    stats.clear();

    EXPECT_FALSE(stats.get_statistics("AAPL").has_value());
    EXPECT_FALSE(stats.get_statistics("MSFT").has_value());
}

TEST(TradeStatisticsTest, RepeatedSamePrice) {
    TradeStatistics stats;

    stats.add_trade("AAPL", 150.0, 10.0);
    stats.add_trade("AAPL", 150.0, 20.0);

    auto result = stats.get_statistics("AAPL");
    ASSERT_TRUE(result.has_value());

    EXPECT_DOUBLE_EQ(result->minimum_trade_price, 150.0);
    EXPECT_DOUBLE_EQ(result->maximum_trade_price, 150.0);
    EXPECT_DOUBLE_EQ(result->average_trade_price, 150.0);
    EXPECT_DOUBLE_EQ(result->average_trade_size, 15.0);
}

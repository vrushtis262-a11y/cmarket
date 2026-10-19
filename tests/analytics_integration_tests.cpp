
#include "analytics_execution_handler.hpp"
#include "matching_engine.hpp"
#include "position_execution_handler.hpp"
#include "position_manager.hpp"
#include "risk_limits.hpp"
#include "risk_managed_execution.hpp"
#include "risk_validator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

class AnalyticsIntegrationTest : public ::testing::Test {
protected:
    OrderBook order_book;
    MatchingEngine engine{order_book};

    cmarket::PositionManager positions;
    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        positions, limits
    };

    cmarket::PositionExecutionHandler position_handler{
        positions
    };

    cmarket::RollingVWAP vwap{2};
    cmarket::TradeStatistics statistics;

    cmarket::AnalyticsExecutionHandler analytics_handler{
        vwap, statistics
    };

    cmarket::RiskManagedExecution execution{
        engine,
        validator,
        position_handler,
        &analytics_handler
    };
};

TEST_F(AnalyticsIntegrationTest, BuyUpdatesAllComponents) {
    order_book.update_ask(100'000'000, 50);

    const auto result =
        execution.execute_market_buy("AAPL", 30);

    ASSERT_EQ(result.executed_quantity, 30);
    ASSERT_EQ(result.trades.size(), 1U);

    EXPECT_EQ(positions.position("AAPL").quantity(), 30);

    ASSERT_TRUE(vwap.get_vwap("AAPL").has_value());
    EXPECT_DOUBLE_EQ(
        *vwap.get_vwap("AAPL"), 100'000'000.0
    );

    const auto stats = statistics.get_statistics("AAPL");
    ASSERT_TRUE(stats.has_value());

    EXPECT_EQ(stats->trade_count, 1U);
    EXPECT_EQ(stats->total_volume, 30.0L);
    EXPECT_EQ(stats->buy_volume, 30.0L);
    EXPECT_EQ(stats->sell_volume, 0.0L);
}

TEST_F(AnalyticsIntegrationTest, SellUpdatesAllComponents) {
    order_book.update_bid(90'000'000, 50);

    const auto result =
        execution.execute_market_sell("AAPL", 20);

    ASSERT_EQ(result.executed_quantity, 20);

    EXPECT_EQ(positions.position("AAPL").quantity(), -20);

    ASSERT_TRUE(vwap.get_vwap("AAPL").has_value());
    EXPECT_DOUBLE_EQ(
        *vwap.get_vwap("AAPL"), 90'000'000.0
    );

    const auto stats = statistics.get_statistics("AAPL");
    ASSERT_TRUE(stats.has_value());

    EXPECT_EQ(stats->trade_count, 1U);
    EXPECT_EQ(stats->sell_volume, 20.0L);
}

TEST_F(AnalyticsIntegrationTest, MultiplePriceLevels) {
    order_book.update_ask(100'000'000, 10);
    order_book.update_ask(110'000'000, 30);

    const auto result =
        execution.execute_market_buy("AAPL", 40);

    ASSERT_EQ(result.executed_quantity, 40);
    ASSERT_EQ(result.trades.size(), 2U);

    EXPECT_EQ(positions.position("AAPL").quantity(), 40);

    ASSERT_TRUE(vwap.get_vwap("AAPL").has_value());
    EXPECT_NEAR(
        *vwap.get_vwap("AAPL"),
        107'500'000.0,
        1e-6
    );

    const auto stats = statistics.get_statistics("AAPL");
    ASSERT_TRUE(stats.has_value());

    EXPECT_EQ(stats->trade_count, 2U);
    EXPECT_EQ(stats->total_volume, 40.0L);
    EXPECT_EQ(stats->minimum_trade_price, 100'000'000.0L);
    EXPECT_EQ(stats->maximum_trade_price, 110'000'000.0L);
}

TEST_F(AnalyticsIntegrationTest, PartialFillUsesExecutedQuantity) {
    order_book.update_ask(100'000'000, 25);

    const auto result =
        execution.execute_market_buy("AAPL", 60);

    EXPECT_EQ(result.executed_quantity, 25);
    EXPECT_EQ(result.remaining_quantity, 35);

    EXPECT_EQ(positions.position("AAPL").quantity(), 25);

    const auto stats = statistics.get_statistics("AAPL");
    ASSERT_TRUE(stats.has_value());

    EXPECT_EQ(stats->total_volume, 25.0L);
    EXPECT_EQ(stats->trade_count, 1U);
}

TEST_F(AnalyticsIntegrationTest, RollingWindowExpires) {
    order_book.update_ask(100'000'000, 10);
    order_book.update_ask(110'000'000, 10);
    order_book.update_ask(120'000'000, 10);

    const auto result =
        execution.execute_market_buy("AAPL", 30);

    ASSERT_EQ(result.trades.size(), 3U);

    EXPECT_EQ(vwap.trade_count("AAPL"), 2U);

    ASSERT_TRUE(vwap.get_vwap("AAPL").has_value());
    EXPECT_DOUBLE_EQ(
        *vwap.get_vwap("AAPL"), 115'000'000.0
    );

    const auto stats = statistics.get_statistics("AAPL");
    ASSERT_TRUE(stats.has_value());

    EXPECT_EQ(stats->trade_count, 3U);
    EXPECT_EQ(stats->total_volume, 30.0L);
}

TEST_F(AnalyticsIntegrationTest, RiskRejectionLeavesAnalyticsUnchanged) {
    order_book.update_ask(100'000'000, 200);

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_buy("AAPL", 101)
        ),
        std::runtime_error
    );

    EXPECT_FALSE(positions.has_position("AAPL"));
    EXPECT_FALSE(vwap.get_vwap("AAPL").has_value());
    EXPECT_FALSE(
        statistics.get_statistics("AAPL").has_value()
    );

    EXPECT_TRUE(engine.trade_history().empty());

    ASSERT_TRUE(order_book.best_ask().has_value());
    EXPECT_EQ(order_book.best_ask()->quantity, 200);
}

TEST_F(AnalyticsIntegrationTest, UnfilledOrderLeavesAnalyticsUnchanged) {
    const auto result =
        execution.execute_market_buy("AAPL", 50);

    EXPECT_TRUE(result.unfilled());
    EXPECT_FALSE(positions.has_position("AAPL"));
    EXPECT_FALSE(vwap.get_vwap("AAPL").has_value());
    EXPECT_FALSE(
        statistics.get_statistics("AAPL").has_value()
    );
}

TEST_F(AnalyticsIntegrationTest, MultipleSymbolsRemainIndependent) {
    order_book.update_ask(100'000'000, 10);

    const auto first =
        execution.execute_market_buy("AAPL", 10);

    ASSERT_EQ(first.executed_quantity, 10);

    order_book.update_ask(200'000'000, 20);

    const auto second =
        execution.execute_market_buy("MSFT", 20);

    ASSERT_EQ(second.executed_quantity, 20);

    EXPECT_EQ(positions.position("AAPL").quantity(), 10);
    EXPECT_EQ(positions.position("MSFT").quantity(), 20);

    ASSERT_TRUE(vwap.get_vwap("AAPL").has_value());
    ASSERT_TRUE(vwap.get_vwap("MSFT").has_value());

    EXPECT_DOUBLE_EQ(
        *vwap.get_vwap("AAPL"), 100'000'000.0
    );
    EXPECT_DOUBLE_EQ(
        *vwap.get_vwap("MSFT"), 200'000'000.0
    );

    EXPECT_EQ(statistics.trade_count("AAPL"), 1U);
    EXPECT_EQ(statistics.trade_count("MSFT"), 1U);
}

} // namespace

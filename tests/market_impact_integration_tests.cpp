
#include "market_impact_execution_adapter.hpp"
#include "matching_engine.hpp"
#include "position_execution_handler.hpp"
#include "position_manager.hpp"
#include "risk_limits.hpp"
#include "risk_managed_execution.hpp"
#include "risk_validator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

class MarketImpactIntegrationTest : public ::testing::Test {
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

    cmarket::RiskManagedExecution execution{
        engine,
        validator,
        position_handler
    };

    cmarket::MarketImpactExecutionAdapter adapter{
        execution,
        order_book
    };
};

TEST_F(MarketImpactIntegrationTest, SingleBuyExecution) {
    order_book.update_bid(98'000'000, 50);
    order_book.update_ask(102'000'000, 50);

    const auto result =
        adapter.execute_market_buy("AAPL", 10);

    ASSERT_EQ(result.executed_quantity, 10);
    EXPECT_EQ(positions.position("AAPL").quantity(), 10);

    const auto impact = adapter.latest_impact("AAPL");

    ASSERT_TRUE(impact.has_value());
    ASSERT_TRUE(impact->average_execution_price_ticks);
    ASSERT_TRUE(impact->slippage_ticks);

    EXPECT_EQ(impact->fill_count, 1U);
    EXPECT_EQ(impact->liquidity_consumed, 10);

    EXPECT_NEAR(
        *impact->average_execution_price_ticks,
        102'000'000.0L,
        0.001L
    );

    EXPECT_NEAR(*impact->slippage_ticks, 0.0L, 0.001L);
}

TEST_F(MarketImpactIntegrationTest, MultiLevelBuyExecution) {
    order_book.update_bid(98'000'000, 50);
    order_book.update_ask(102'000'000, 10);
    order_book.update_ask(104'000'000, 20);

    const auto result =
        adapter.execute_market_buy("AAPL", 15);

    ASSERT_EQ(result.executed_quantity, 15);
    ASSERT_EQ(result.trades.size(), 2U);

    const auto impact = adapter.latest_impact("AAPL");

    ASSERT_TRUE(impact.has_value());
    ASSERT_TRUE(impact->average_execution_price_ticks);
    ASSERT_TRUE(impact->slippage_ticks);

    EXPECT_EQ(impact->fill_count, 2U);

    EXPECT_NEAR(
        *impact->average_execution_price_ticks,
        102'666'666.666667L,
        0.01L
    );

    EXPECT_NEAR(
        *impact->slippage_ticks,
        666'666.666667L,
        0.01L
    );

    ASSERT_TRUE(impact->liquidity_consumed_ratio);

    EXPECT_NEAR(
        *impact->liquidity_consumed_ratio,
        0.5L,
        0.000001L
    );
}

TEST_F(MarketImpactIntegrationTest, SellExecution) {
    order_book.update_bid(98'000'000, 50);
    order_book.update_ask(102'000'000, 50);

    const auto result =
        adapter.execute_market_sell("AAPL", 20);

    ASSERT_EQ(result.executed_quantity, 20);
    EXPECT_EQ(positions.position("AAPL").quantity(), -20);

    const auto impact = adapter.latest_impact("AAPL");

    ASSERT_TRUE(impact.has_value());
    ASSERT_TRUE(impact->slippage_ticks);
    ASSERT_TRUE(impact->effective_spread_ticks);

    EXPECT_NEAR(*impact->slippage_ticks, 0.0L, 0.001L);

    EXPECT_NEAR(
        *impact->effective_spread_ticks,
        4'000'000.0L,
        0.001L
    );
}

TEST_F(MarketImpactIntegrationTest, PartialFill) {
    order_book.update_ask(100'000'000, 20);

    const auto result =
        adapter.execute_market_buy("AAPL", 50);

    EXPECT_EQ(result.executed_quantity, 20);
    EXPECT_EQ(result.remaining_quantity, 30);

    const auto impact = adapter.latest_impact("AAPL");

    ASSERT_TRUE(impact.has_value());

    EXPECT_EQ(impact->executed_quantity, 20);
    EXPECT_EQ(impact->remaining_quantity, 30);
    EXPECT_EQ(impact->liquidity_consumed, 20);
}

TEST_F(MarketImpactIntegrationTest, RiskRejection) {
    order_book.update_ask(100'000'000, 200);

    EXPECT_THROW(
        static_cast<void>(
            adapter.execute_market_buy("AAPL", 101)
        ),
        std::runtime_error
    );

    EXPECT_FALSE(adapter.latest_impact("AAPL"));
    EXPECT_TRUE(engine.trade_history().empty());
    EXPECT_FALSE(positions.has_position("AAPL"));
}

TEST_F(MarketImpactIntegrationTest, NoLiquidity) {
    const auto result =
        adapter.execute_market_buy("AAPL", 10);

    EXPECT_TRUE(result.unfilled());
    EXPECT_FALSE(adapter.latest_impact("AAPL"));
    EXPECT_FALSE(positions.has_position("AAPL"));
}

TEST_F(MarketImpactIntegrationTest, MultipleSymbols) {
    order_book.update_ask(100'000'000, 10);

    const auto first =
        adapter.execute_market_buy("AAPL", 10);

    ASSERT_EQ(first.executed_quantity, 10);

    order_book.update_ask(200'000'000, 20);

    const auto second =
        adapter.execute_market_buy("MSFT", 20);

    ASSERT_EQ(second.executed_quantity, 20);

    const auto apple = adapter.latest_impact("AAPL");
    const auto microsoft = adapter.latest_impact("MSFT");

    ASSERT_TRUE(apple.has_value());
    ASSERT_TRUE(microsoft.has_value());

    ASSERT_TRUE(apple->average_execution_price_ticks);
    ASSERT_TRUE(microsoft->average_execution_price_ticks);

    EXPECT_NEAR(
        *apple->average_execution_price_ticks,
        100'000'000.0L,
        0.001L
    );

    EXPECT_NEAR(
        *microsoft->average_execution_price_ticks,
        200'000'000.0L,
        0.001L
    );
}

} // namespace

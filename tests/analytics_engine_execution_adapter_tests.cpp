
#include "analytics_engine_execution_adapter.hpp"

#include "matching_engine.hpp"
#include "position_execution_handler.hpp"
#include "position_manager.hpp"
#include "risk_limits.hpp"
#include "risk_managed_execution.hpp"
#include "risk_validator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

class AnalyticsEngineExecutionAdapterTest
    : public ::testing::Test {
protected:
    OrderBook book;
    MatchingEngine matching_engine{book};

    cmarket::PositionManager positions;
    const cmarket::RiskLimits limits{100};

    const cmarket::RiskValidator validator{
        positions, limits
    };

    cmarket::PositionExecutionHandler position_handler{
        positions
    };

    cmarket::RollingVWAP vwap{10};
    cmarket::TradeStatistics statistics;
    cmarket::OrderFlowImbalance ofi{10};

    cmarket::AnalyticsEngine analytics{
        vwap, statistics, ofi, 5
    };

    cmarket::RiskManagedExecution execution{
        matching_engine,
        validator,
        position_handler,
        &analytics.execution_handler()
    };

    cmarket::AnalyticsEngineExecutionAdapter adapter{
        execution,
        analytics,
        book
    };
};

TEST_F(
    AnalyticsEngineExecutionAdapterTest,
    BuyUpdatesAllFiveComponents
) {
    book.update_bid(98'000'000, 30);
    book.update_ask(102'000'000, 20);

    adapter.on_order_book_update("AAPL");

    const auto result =
        adapter.execute_market_buy("AAPL", 5);

    ASSERT_EQ(result.executed_quantity, 5);

    const auto snapshot = adapter.snapshot("AAPL");

    ASSERT_TRUE(snapshot.rolling_vwap);
    ASSERT_TRUE(snapshot.trade_statistics);
    ASSERT_TRUE(snapshot.latest_ofi);
    ASSERT_TRUE(snapshot.rolling_ofi);
    ASSERT_TRUE(snapshot.depth);
    ASSERT_TRUE(snapshot.market_impact);

    EXPECT_DOUBLE_EQ(
        *snapshot.rolling_vwap,
        102'000'000.0
    );

    EXPECT_EQ(
        snapshot.trade_statistics->trade_count,
        1U
    );

    EXPECT_EQ(
        snapshot.trade_statistics->total_volume,
        5.0L
    );

    EXPECT_EQ(*snapshot.latest_ofi, 5.0L);

    EXPECT_EQ(snapshot.depth->ask_depth, 15.0L);

    EXPECT_EQ(
        snapshot.market_impact->executed_quantity,
        5
    );

    EXPECT_EQ(positions.position("AAPL").quantity(), 5);

    EXPECT_TRUE(adapter.analytics_healthy());
}

TEST_F(
    AnalyticsEngineExecutionAdapterTest,
    MultiLevelExecutionDoesNotDoubleCount
) {
    book.update_bid(98'000'000, 20);
    book.update_ask(102'000'000, 10);
    book.update_ask(104'000'000, 20);

    adapter.on_order_book_update("AAPL");

    const auto result =
        adapter.execute_market_buy("AAPL", 15);

    ASSERT_EQ(result.trades.size(), 2U);

    const auto snapshot = adapter.snapshot("AAPL");

    ASSERT_TRUE(snapshot.trade_statistics);
    ASSERT_TRUE(snapshot.market_impact);

    EXPECT_EQ(
        snapshot.trade_statistics->trade_count,
        2U
    );

    EXPECT_EQ(
        snapshot.trade_statistics->total_volume,
        15.0L
    );

    EXPECT_EQ(
        snapshot.market_impact->fill_count,
        2U
    );

    ASSERT_TRUE(snapshot.market_impact->slippage_ticks);

    EXPECT_NEAR(
        *snapshot.market_impact->slippage_ticks,
        666'666.666667L,
        0.01L
    );

    EXPECT_EQ(positions.position("AAPL").quantity(), 15);
}

TEST_F(
    AnalyticsEngineExecutionAdapterTest,
    SellUpdatesAnalytics
) {
    book.update_bid(98'000'000, 30);
    book.update_ask(102'000'000, 30);

    adapter.on_order_book_update("AAPL");

    const auto result =
        adapter.execute_market_sell("AAPL", 10);

    ASSERT_EQ(result.executed_quantity, 10);

    const auto snapshot = adapter.snapshot("AAPL");

    ASSERT_TRUE(snapshot.trade_statistics);
    ASSERT_TRUE(snapshot.market_impact);
    ASSERT_TRUE(snapshot.rolling_ofi);

    EXPECT_EQ(snapshot.trade_statistics->sell_volume, 10.0L);
    EXPECT_EQ(*snapshot.rolling_ofi, -10.0L);

    EXPECT_EQ(snapshot.depth->bid_depth, 20.0L);
    EXPECT_EQ(positions.position("AAPL").quantity(), -10);
}

TEST_F(
    AnalyticsEngineExecutionAdapterTest,
    RiskRejectionDoesNotRecordTrades
) {
    book.update_ask(100'000'000, 200);

    adapter.on_order_book_update("AAPL");

    EXPECT_THROW(
        static_cast<void>(
            adapter.execute_market_buy("AAPL", 101)
        ),
        std::runtime_error
    );

    const auto snapshot = adapter.snapshot("AAPL");

    EXPECT_FALSE(snapshot.rolling_vwap);
    EXPECT_FALSE(snapshot.trade_statistics);
    EXPECT_FALSE(snapshot.market_impact);

    EXPECT_TRUE(matching_engine.trade_history().empty());
    EXPECT_FALSE(positions.has_position("AAPL"));
}

TEST_F(
    AnalyticsEngineExecutionAdapterTest,
    MultipleSymbolsRemainIndependent
) {
    book.update_ask(100'000'000, 10);
    adapter.on_order_book_update("AAPL");

    const auto first =
        adapter.execute_market_buy("AAPL", 10);

    ASSERT_EQ(first.executed_quantity, 10);

    book.update_ask(200'000'000, 20);
    adapter.on_order_book_update("MSFT");

    const auto second =
        adapter.execute_market_buy("MSFT", 20);

    ASSERT_EQ(second.executed_quantity, 20);

    const auto apple = adapter.snapshot("AAPL");
    const auto microsoft = adapter.snapshot("MSFT");

    ASSERT_TRUE(apple.rolling_vwap);
    ASSERT_TRUE(microsoft.rolling_vwap);

    EXPECT_DOUBLE_EQ(*apple.rolling_vwap, 100'000'000.0);
    EXPECT_DOUBLE_EQ(*microsoft.rolling_vwap, 200'000'000.0);

    EXPECT_EQ(apple.trade_statistics->trade_count, 1U);
    EXPECT_EQ(microsoft.trade_statistics->trade_count, 1U);

    EXPECT_EQ(apple.market_impact->executed_quantity, 10);
    EXPECT_EQ(microsoft.market_impact->executed_quantity, 20);
}

} // namespace


#include "analytics_engine.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

class AnalyticsEngineTest : public ::testing::Test {
protected:
    cmarket::RollingVWAP vwap{10};
    cmarket::TradeStatistics statistics;
    cmarket::OrderFlowImbalance ofi{10};

    cmarket::AnalyticsEngine analytics{
        vwap, statistics, ofi, 5
    };

    OrderBook book;

    static ExecutionResult make_buy_execution(
        std::int64_t price,
        std::int64_t quantity
    ) {
        return ExecutionResult{
            .side = OrderSide::Buy,
            .requested_quantity = quantity,
            .executed_quantity = quantity,
            .remaining_quantity = 0,
            .average_price_ticks = price,
            .trades = {
                Trade{OrderSide::Buy, price, quantity}
            }
        };
    }
};

TEST_F(AnalyticsEngineTest, EmptySymbolHasNoAnalytics) {
    const auto snapshot = analytics.snapshot("AAPL");

    EXPECT_FALSE(snapshot.rolling_vwap);
    EXPECT_FALSE(snapshot.trade_statistics);
    EXPECT_FALSE(snapshot.latest_ofi);
    EXPECT_FALSE(snapshot.rolling_ofi);
    EXPECT_FALSE(snapshot.depth);
    EXPECT_FALSE(snapshot.market_impact);
}

TEST_F(AnalyticsEngineTest, ExecutionUpdatesVWAPAndStatistics) {
    const auto execution =
        make_buy_execution(100'000'000, 10);

    analytics.record_execution("AAPL", execution);

    const auto snapshot = analytics.snapshot("AAPL");

    ASSERT_TRUE(snapshot.rolling_vwap);
    ASSERT_TRUE(snapshot.trade_statistics);

    EXPECT_DOUBLE_EQ(
        *snapshot.rolling_vwap,
        100'000'000.0
    );

    EXPECT_EQ(snapshot.trade_statistics->trade_count, 1U);
    EXPECT_EQ(snapshot.trade_statistics->total_volume, 10.0L);
    EXPECT_EQ(snapshot.trade_statistics->buy_volume, 10.0L);
}

TEST_F(AnalyticsEngineTest, BookUpdatesCalculateOFIAndDepth) {
    book.update_bid(98'000'000, 20);
    book.update_ask(102'000'000, 20);

    analytics.on_order_book_update("AAPL", book);

    book.update_bid(98'000'000, 30);

    analytics.on_order_book_update("AAPL", book);

    const auto snapshot = analytics.snapshot("AAPL");

    ASSERT_TRUE(snapshot.latest_ofi);
    ASSERT_TRUE(snapshot.rolling_ofi);
    ASSERT_TRUE(snapshot.depth);

    EXPECT_EQ(*snapshot.latest_ofi, 10.0L);
    EXPECT_EQ(*snapshot.rolling_ofi, 10.0L);

    EXPECT_EQ(snapshot.depth->bid_depth, 30.0L);
    EXPECT_EQ(snapshot.depth->ask_depth, 20.0L);

    ASSERT_TRUE(snapshot.depth->imbalance);

    EXPECT_NEAR(
        *snapshot.depth->imbalance,
        0.2L,
        0.000001L
    );
}

TEST_F(AnalyticsEngineTest, MarketImpactIsRecorded) {
    book.update_bid(98'000'000, 20);
    book.update_ask(102'000'000, 20);

    const auto before =
        cmarket::MarketImpact::capture_quote(book);

    const auto execution =
        make_buy_execution(102'000'000, 5);

    book.update_ask(102'000'000, 15);

    const auto after =
        cmarket::MarketImpact::capture_quote(book);

    analytics.record_market_impact(
        "AAPL",
        execution,
        before,
        after
    );

    const auto snapshot = analytics.snapshot("AAPL");

    ASSERT_TRUE(snapshot.market_impact);

    EXPECT_EQ(
        snapshot.market_impact->executed_quantity,
        5
    );

    ASSERT_TRUE(snapshot.market_impact->slippage_ticks);

    EXPECT_NEAR(
        *snapshot.market_impact->slippage_ticks,
        0.0L,
        0.000001L
    );
}

TEST_F(AnalyticsEngineTest, AllFiveComponentsWorkTogether) {
    book.update_bid(98'000'000, 20);
    book.update_ask(102'000'000, 20);

    analytics.on_order_book_update("AAPL", book);

    const auto before =
        cmarket::MarketImpact::capture_quote(book);

    const auto execution =
        make_buy_execution(102'000'000, 5);

    book.update_ask(102'000'000, 15);

    const auto after =
        cmarket::MarketImpact::capture_quote(book);

    analytics.record_execution("AAPL", execution);
    analytics.on_order_book_update("AAPL", book);

    analytics.record_market_impact(
        "AAPL",
        execution,
        before,
        after
    );

    const auto snapshot = analytics.snapshot("AAPL");

    EXPECT_TRUE(snapshot.rolling_vwap);
    EXPECT_TRUE(snapshot.trade_statistics);
    EXPECT_TRUE(snapshot.latest_ofi);
    EXPECT_TRUE(snapshot.rolling_ofi);
    EXPECT_TRUE(snapshot.depth);
    EXPECT_TRUE(snapshot.market_impact);

    EXPECT_EQ(*snapshot.rolling_ofi, 5.0L);
    EXPECT_EQ(snapshot.depth->ask_depth, 15.0L);
}

TEST_F(AnalyticsEngineTest, MultipleSymbolsRemainIndependent) {
    analytics.record_execution(
        "AAPL",
        make_buy_execution(100'000'000, 10)
    );

    analytics.record_execution(
        "MSFT",
        make_buy_execution(200'000'000, 20)
    );

    const auto apple = analytics.snapshot("AAPL");
    const auto microsoft = analytics.snapshot("MSFT");

    ASSERT_TRUE(apple.rolling_vwap);
    ASSERT_TRUE(microsoft.rolling_vwap);

    EXPECT_DOUBLE_EQ(*apple.rolling_vwap, 100'000'000.0);
    EXPECT_DOUBLE_EQ(*microsoft.rolling_vwap, 200'000'000.0);

    EXPECT_EQ(apple.trade_statistics->total_volume, 10.0L);
    EXPECT_EQ(microsoft.trade_statistics->total_volume, 20.0L);
}

TEST_F(AnalyticsEngineTest, InvalidConfigurationIsRejected) {
    EXPECT_THROW(
        (cmarket::AnalyticsEngine{
            vwap, statistics, ofi, 0
        }),
        std::invalid_argument
    );

    EXPECT_THROW(
        analytics.snapshot(""),
        std::invalid_argument
    );
}

} // namespace

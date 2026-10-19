
#include "analytics_execution_handler.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using cmarket::AnalyticsExecutionHandler;
using cmarket::RollingVWAP;
using cmarket::TradeStatistics;

namespace {

Trade make_trade(
    OrderSide side,
    std::int64_t price,
    std::int64_t quantity
) {
    Trade trade{};
    trade.aggressor_side = side;
    trade.price_ticks = price;
    trade.quantity = quantity;
    return trade;
}

ExecutionResult make_execution(
    OrderSide side,
    std::vector<Trade> trades
) {
    ExecutionResult result{};
    result.side = side;
    result.requested_quantity = 0;
    result.executed_quantity = 0;
    result.remaining_quantity = 0;

    for (const auto& trade : trades) {
        result.executed_quantity += trade.quantity;
    }

    result.requested_quantity =
        result.executed_quantity;

    result.trades = std::move(trades);

    return result;
}

} // namespace

TEST(AnalyticsExecutionHandlerTest, SingleBuyTrade) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    auto execution = make_execution(
        OrderSide::Buy,
        {make_trade(OrderSide::Buy, 100, 10)}
    );

    handler.apply_execution("AAPL", execution);

    ASSERT_TRUE(vwap.get_vwap("AAPL").has_value());

    EXPECT_DOUBLE_EQ(
        *vwap.get_vwap("AAPL"),
        100.0
    );

    auto snapshot = stats.get_statistics("AAPL");
    ASSERT_TRUE(snapshot.has_value());

    EXPECT_EQ(snapshot->trade_count, 1U);
    EXPECT_DOUBLE_EQ(snapshot->total_volume, 10.0L);
    EXPECT_DOUBLE_EQ(snapshot->buy_volume, 10.0L);
    EXPECT_DOUBLE_EQ(snapshot->sell_volume, 0.0L);
}

TEST(AnalyticsExecutionHandlerTest, SingleSellTrade) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    auto execution = make_execution(
        OrderSide::Sell,
        {make_trade(OrderSide::Sell, 200, 5)}
    );

    handler.apply_execution("AAPL", execution);

    auto snapshot = stats.get_statistics("AAPL");
    ASSERT_TRUE(snapshot.has_value());

    EXPECT_DOUBLE_EQ(snapshot->sell_volume, 5.0L);
    EXPECT_DOUBLE_EQ(snapshot->buy_volume, 0.0L);
    EXPECT_DOUBLE_EQ(*vwap.get_vwap("AAPL"), 200.0);
}

TEST(AnalyticsExecutionHandlerTest, MultipleFills) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    auto execution = make_execution(
        OrderSide::Buy,
        {
            make_trade(OrderSide::Buy, 100, 10),
            make_trade(OrderSide::Buy, 110, 30)
        }
    );

    handler.apply_execution("AAPL", execution);

    EXPECT_NEAR(
        *vwap.get_vwap("AAPL"),
        107.5,
        1e-9
    );

    auto snapshot = stats.get_statistics("AAPL");
    ASSERT_TRUE(snapshot.has_value());

    EXPECT_EQ(snapshot->trade_count, 2U);
    EXPECT_DOUBLE_EQ(snapshot->total_volume, 40.0L);
    EXPECT_DOUBLE_EQ(snapshot->buy_volume, 40.0L);
}

TEST(AnalyticsExecutionHandlerTest, RollingWindowExpires) {
    RollingVWAP vwap(2);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    handler.apply_execution(
        "AAPL",
        make_execution(
            OrderSide::Buy,
            {make_trade(OrderSide::Buy, 100, 10)}
        )
    );

    handler.apply_execution(
        "AAPL",
        make_execution(
            OrderSide::Buy,
            {make_trade(OrderSide::Buy, 110, 10)}
        )
    );

    handler.apply_execution(
        "AAPL",
        make_execution(
            OrderSide::Buy,
            {make_trade(OrderSide::Buy, 120, 10)}
        )
    );

    EXPECT_EQ(vwap.trade_count("AAPL"), 2U);
    EXPECT_DOUBLE_EQ(*vwap.get_vwap("AAPL"), 115.0);

    auto snapshot = stats.get_statistics("AAPL");
    ASSERT_TRUE(snapshot.has_value());

    EXPECT_EQ(snapshot->trade_count, 3U);
}

TEST(AnalyticsExecutionHandlerTest, MultipleSymbols) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    handler.apply_execution(
        "AAPL",
        make_execution(
            OrderSide::Buy,
            {make_trade(OrderSide::Buy, 100, 10)}
        )
    );

    handler.apply_execution(
        "MSFT",
        make_execution(
            OrderSide::Sell,
            {make_trade(OrderSide::Sell, 250, 20)}
        )
    );

    EXPECT_DOUBLE_EQ(*vwap.get_vwap("AAPL"), 100.0);
    EXPECT_DOUBLE_EQ(*vwap.get_vwap("MSFT"), 250.0);

    EXPECT_EQ(stats.trade_count("AAPL"), 1U);
    EXPECT_EQ(stats.trade_count("MSFT"), 1U);
}

TEST(AnalyticsExecutionHandlerTest, EmptyExecution) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    handler.apply_execution(
        "AAPL",
        make_execution(OrderSide::Buy, {})
    );

    EXPECT_FALSE(vwap.get_vwap("AAPL").has_value());
    EXPECT_FALSE(stats.get_statistics("AAPL").has_value());
}

TEST(AnalyticsExecutionHandlerTest, RejectsEmptySymbol) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    auto execution = make_execution(
        OrderSide::Buy,
        {make_trade(OrderSide::Buy, 100, 10)}
    );

    EXPECT_THROW(
        handler.apply_execution("", execution),
        std::invalid_argument
    );

    EXPECT_FALSE(stats.get_statistics("").has_value());
}

TEST(AnalyticsExecutionHandlerTest, RejectsInvalidPrice) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    auto execution = make_execution(
        OrderSide::Buy,
        {make_trade(OrderSide::Buy, 0, 10)}
    );

    EXPECT_THROW(
        handler.apply_execution("AAPL", execution),
        std::invalid_argument
    );

    EXPECT_EQ(stats.trade_count("AAPL"), 0U);
}

TEST(AnalyticsExecutionHandlerTest, RejectsInvalidQuantity) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    auto execution = make_execution(
        OrderSide::Buy,
        {make_trade(OrderSide::Buy, 100, 0)}
    );

    EXPECT_THROW(
        handler.apply_execution("AAPL", execution),
        std::invalid_argument
    );

    EXPECT_EQ(stats.trade_count("AAPL"), 0U);
}

TEST(AnalyticsExecutionHandlerTest, TracksAggressorNotOrderSide) {
    RollingVWAP vwap(10);
    TradeStatistics stats;
    AnalyticsExecutionHandler handler(vwap, stats);

    auto execution = make_execution(
        OrderSide::Buy,
        {make_trade(OrderSide::Sell, 100, 10)}
    );

    handler.apply_execution("AAPL", execution);

    auto snapshot = stats.get_statistics("AAPL");
    ASSERT_TRUE(snapshot.has_value());

    EXPECT_DOUBLE_EQ(snapshot->sell_volume, 10.0L);
    EXPECT_DOUBLE_EQ(snapshot->buy_volume, 0.0L);
}

#include "trade_store.hpp"

#include <gtest/gtest.h>

TEST(TradeStoreTest, StartsEmpty)
{
    TradeStore store;

    EXPECT_TRUE(
        store.trades().empty()
    );
}

TEST(TradeStoreTest, RecordsTrade)
{
    TradeStore store;

    const Trade& trade =
        store.record_trade(
            OrderSide::Buy,
            520'000,
            40,
            10,
            20
        );

    EXPECT_EQ(
        trade.trade_id,
        1U
    );

    EXPECT_EQ(
        trade.execution_sequence,
        1U
    );

    EXPECT_EQ(
        trade.aggressor_side,
        OrderSide::Buy
    );

    EXPECT_EQ(
        trade.price_ticks,
        520'000
    );

    EXPECT_EQ(
        trade.quantity,
        40
    );

    ASSERT_TRUE(
        trade.buy_order_id.has_value()
    );

    ASSERT_TRUE(
        trade.sell_order_id.has_value()
    );

    EXPECT_EQ(
        *trade.buy_order_id,
        10U
    );

    EXPECT_EQ(
        *trade.sell_order_id,
        20U
    );

    ASSERT_EQ(
        store.trades().size(),
        1U
    );
}

TEST(TradeStoreTest, AssignsSequentialTradeIds)
{
    TradeStore store;

    static_cast<void>(
        store.record_trade(
            OrderSide::Buy,
            520'000,
            10
        )
    );

    static_cast<void>(
        store.record_trade(
            OrderSide::Sell,
            525'000,
            20
        )
    );

    static_cast<void>(
        store.record_trade(
            OrderSide::Buy,
            530'000,
            30
        )
    );

    const auto& trades =
        store.trades();

    ASSERT_EQ(
        trades.size(),
        3U
    );

    EXPECT_EQ(
        trades[0].trade_id,
        1U
    );

    EXPECT_EQ(
        trades[1].trade_id,
        2U
    );

    EXPECT_EQ(
        trades[2].trade_id,
        3U
    );
}

TEST(TradeStoreTest, AssignsSequentialExecutionNumbers)
{
    TradeStore store;

    static_cast<void>(
        store.record_trade(
            OrderSide::Buy,
            520'000,
            10
        )
    );

    static_cast<void>(
        store.record_trade(
            OrderSide::Sell,
            525'000,
            20
        )
    );

    const auto& trades =
        store.trades();

    ASSERT_EQ(
        trades.size(),
        2U
    );

    EXPECT_EQ(
        trades[0].execution_sequence,
        1U
    );

    EXPECT_EQ(
        trades[1].execution_sequence,
        2U
    );
}

TEST(TradeStoreTest, SupportsTradesWithoutOrderIds)
{
    TradeStore store;

    const Trade& trade =
        store.record_trade(
            OrderSide::Buy,
            520'000,
            50
        );

    EXPECT_FALSE(
        trade.buy_order_id.has_value()
    );

    EXPECT_FALSE(
        trade.sell_order_id.has_value()
    );
}

TEST(TradeStoreTest, ClearRemovesStoredTrades)
{
    TradeStore store;

    static_cast<void>(
        store.record_trade(
            OrderSide::Buy,
            520'000,
            10
        )
    );

    ASSERT_FALSE(
        store.trades().empty()
    );

    store.clear();

    EXPECT_TRUE(
        store.trades().empty()
    );
}

TEST(TradeStoreTest, ClearDoesNotReuseIds)
{
    TradeStore store;

    static_cast<void>(
        store.record_trade(
            OrderSide::Buy,
            520'000,
            10
        )
    );

    store.clear();

    const Trade& trade =
        store.record_trade(
            OrderSide::Sell,
            525'000,
            20
        );

    EXPECT_EQ(
        trade.trade_id,
        2U
    );

    EXPECT_EQ(
        trade.execution_sequence,
        2U
    );
}
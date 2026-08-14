#include "matching_engine.hpp"

#include <gtest/gtest.h>

TEST(TradePersistenceTest, PersistsSingleLimitTrade)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId sell_order_id =
        engine.place_limit_sell(
            520'000,
            100
        );

    const OrderId buy_order_id =
        engine.place_limit_buy(
            525'000,
            40
        );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 1U);

    const Trade& trade =
        trades[0];

    EXPECT_EQ(trade.trade_id, 1U);
    EXPECT_EQ(trade.execution_sequence, 1U);

    ASSERT_TRUE(
        trade.buy_order_id.has_value()
    );

    ASSERT_TRUE(
        trade.sell_order_id.has_value()
    );

    EXPECT_EQ(
        *trade.buy_order_id,
        buy_order_id
    );

    EXPECT_EQ(
        *trade.sell_order_id,
        sell_order_id
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
}

TEST(TradePersistenceTest, PersistsMultipleTradesInSequence)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            40
        )
    );

    static_cast<void>(
        engine.place_limit_sell(
            525'000,
            60
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            530'000,
            100
        )
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 2U);

    EXPECT_EQ(
        trades[0].trade_id,
        1U
    );

    EXPECT_EQ(
        trades[1].trade_id,
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

    EXPECT_LT(
        trades[0].trade_id,
        trades[1].trade_id
    );

    EXPECT_LT(
        trades[0].execution_sequence,
        trades[1].execution_sequence
    );
}

TEST(TradePersistenceTest, MultiLevelMatchCreatesMultipleTradeRecords)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            510'000,
            20
        )
    );

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            30
        )
    );

    static_cast<void>(
        engine.place_limit_sell(
            530'000,
            40
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            540'000,
            90
        )
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 3U);

    EXPECT_EQ(
        trades[0].price_ticks,
        510'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        20
    );

    EXPECT_EQ(
        trades[1].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[1].quantity,
        30
    );

    EXPECT_EQ(
        trades[2].price_ticks,
        530'000
    );

    EXPECT_EQ(
        trades[2].quantity,
        40
    );

    EXPECT_EQ(trades[0].trade_id, 1U);
    EXPECT_EQ(trades[1].trade_id, 2U);
    EXPECT_EQ(trades[2].trade_id, 3U);

    EXPECT_EQ(
        trades[0].execution_sequence,
        1U
    );

    EXPECT_EQ(
        trades[1].execution_sequence,
        2U
    );

    EXPECT_EQ(
        trades[2].execution_sequence,
        3U
    );
}

TEST(TradePersistenceTest, AssignsUniqueTradeIds)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            20
        )
    );

    static_cast<void>(
        engine.place_limit_sell(
            525'000,
            20
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            530'000,
            40
        )
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 2U);

    EXPECT_NE(
        trades[0].trade_id,
        trades[1].trade_id
    );
}

TEST(TradePersistenceTest, StoresCorrectBuyAndSellOrderIds)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId buy_order_id =
        engine.place_limit_buy(
            520'000,
            100
        );

    const OrderId sell_order_id =
        engine.place_limit_sell(
            515'000,
            40
        );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 1U);

    ASSERT_TRUE(
        trades[0].buy_order_id.has_value()
    );

    ASSERT_TRUE(
        trades[0].sell_order_id.has_value()
    );

    EXPECT_EQ(
        *trades[0].buy_order_id,
        buy_order_id
    );

    EXPECT_EQ(
        *trades[0].sell_order_id,
        sell_order_id
    );

    EXPECT_EQ(
        trades[0].aggressor_side,
        OrderSide::Sell
    );
}

TEST(TradePersistenceTest, StoresCorrectQuantityAndExecutionPrice)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            100
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            525'000,
            35
        )
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 1U);

    EXPECT_EQ(
        trades[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        35
    );
}

TEST(TradePersistenceTest, CancelledOrderCreatesNoTrade)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(
            520'000,
            100
        );

    EXPECT_TRUE(
        engine.cancel_order(order_id)
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

TEST(TradePersistenceTest, NonCrossingOrdersCreateNoTrade)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            530'000,
            100
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            100
        )
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

TEST(TradePersistenceTest, MarketTradesReceiveStableIdsAndSequence)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {},
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 20
            },
            PriceLevel{
                .price_ticks = 525'000,
                .quantity = 30
            }
        }
    );

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_buy(50);

    ASSERT_EQ(result.trades.size(), 2U);

    EXPECT_EQ(
        result.trades[0].trade_id,
        1U
    );

    EXPECT_EQ(
        result.trades[1].trade_id,
        2U
    );

    EXPECT_EQ(
        result.trades[0].execution_sequence,
        1U
    );

    EXPECT_EQ(
        result.trades[1].execution_sequence,
        2U
    );

    EXPECT_FALSE(
        result.trades[0].buy_order_id.has_value()
    );

    EXPECT_FALSE(
        result.trades[0].sell_order_id.has_value()
    );

    const auto& history =
        engine.trade_history();

    ASSERT_EQ(history.size(), 2U);

    EXPECT_EQ(
        history[0].trade_id,
        result.trades[0].trade_id
    );

    EXPECT_EQ(
        history[1].trade_id,
        result.trades[1].trade_id
    );
}

TEST(
    TradePersistenceTest,
    ClearingHistoryDoesNotReuseTradeIdsOrSequenceNumbers
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            20
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            20
        )
    );

    ASSERT_EQ(
        engine.trade_history().size(),
        1U
    );

    EXPECT_EQ(
        engine.trade_history()[0].trade_id,
        1U
    );

    EXPECT_EQ(
        engine.trade_history()[0].execution_sequence,
        1U
    );

    engine.clear_trade_history();

    EXPECT_TRUE(
        engine.trade_history().empty()
    );

    static_cast<void>(
        engine.place_limit_sell(
            525'000,
            30
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            525'000,
            30
        )
    );

    ASSERT_EQ(
        engine.trade_history().size(),
        1U
    );

    EXPECT_EQ(
        engine.trade_history()[0].trade_id,
        2U
    );

    EXPECT_EQ(
        engine.trade_history()[0].execution_sequence,
        2U
    );
}
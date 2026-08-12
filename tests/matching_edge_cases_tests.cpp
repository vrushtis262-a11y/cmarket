#include "matching_engine.hpp"

#include <gtest/gtest.h>

TEST(
    MatchingEdgeCasesTest,
    IncomingBuyPartiallyFillsRestingSell
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId resting_sell =
        engine.place_limit_sell(
            520'000,
            100
        );

    static_cast<void>(
        engine.place_limit_buy(
            525'000,
            40
        )
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        resting_sell
    );

    EXPECT_EQ(
        orders[0].side,
        OrderSide::Sell
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        60
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 1U);

    EXPECT_EQ(
        trades[0].aggressor_side,
        OrderSide::Buy
    );

    EXPECT_EQ(
        trades[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        40
    );

    ASSERT_EQ(
        order_book.asks().size(),
        1U
    );

    EXPECT_EQ(
        order_book.asks()[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        order_book.asks()[0].quantity,
        60
    );

    EXPECT_TRUE(
        order_book.bids().empty()
    );
}

TEST(
    MatchingEdgeCasesTest,
    RestingSellPartiallyFillsIncomingBuy
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            40
        )
    );

    const OrderId incoming_buy =
        engine.place_limit_buy(
            525'000,
            100
        );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        incoming_buy
    );

    EXPECT_EQ(
        orders[0].side,
        OrderSide::Buy
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        60
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
        40
    );

    EXPECT_TRUE(
        order_book.asks().empty()
    );

    ASSERT_EQ(
        order_book.bids().size(),
        1U
    );

    EXPECT_EQ(
        order_book.bids()[0].price_ticks,
        525'000
    );

    EXPECT_EQ(
        order_book.bids()[0].quantity,
        60
    );
}

TEST(
    MatchingEdgeCasesTest,
    MatchesMultipleRestingOrdersAtSamePrice
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            40
        )
    );

    const OrderId second_sell =
        engine.place_limit_sell(
            520'000,
            60
        );

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            70
        )
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        second_sell
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        30
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 2U);

    EXPECT_EQ(
        trades[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        40
    );

    EXPECT_EQ(
        trades[1].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[1].quantity,
        30
    );

    ASSERT_EQ(
        order_book.asks().size(),
        1U
    );

    EXPECT_EQ(
        order_book.asks()[0].quantity,
        30
    );
}

TEST(
    MatchingEdgeCasesTest,
    PreservesFifoAfterPartialFill
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId first_sell =
        engine.place_limit_sell(
            520'000,
            100
        );

    const OrderId second_sell =
        engine.place_limit_sell(
            520'000,
            100
        );

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            60
        )
    );

    {
        const auto& orders =
            engine.active_limit_orders();

        ASSERT_EQ(orders.size(), 2U);

        EXPECT_EQ(
            orders[0].order_id,
            first_sell
        );

        EXPECT_EQ(
            orders[0].remaining_quantity,
            40
        );

        EXPECT_EQ(
            orders[1].order_id,
            second_sell
        );

        EXPECT_EQ(
            orders[1].remaining_quantity,
            100
        );
    }

    engine.clear_trade_history();

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            50
        )
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        second_sell
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        90
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 2U);

    EXPECT_EQ(
        trades[0].quantity,
        40
    );

    EXPECT_EQ(
        trades[1].quantity,
        10
    );

    ASSERT_EQ(
        order_book.asks().size(),
        1U
    );

    EXPECT_EQ(
        order_book.asks()[0].quantity,
        90
    );
}

TEST(
    MatchingEdgeCasesTest,
    EmptyOppositeSideLeavesLimitOrderResting
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(
            520'000,
            100
        );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        order_id
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        100
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );

    ASSERT_EQ(
        order_book.bids().size(),
        1U
    );

    EXPECT_EQ(
        order_book.bids()[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        order_book.bids()[0].quantity,
        100
    );

    EXPECT_TRUE(
        order_book.asks().empty()
    );
}

TEST(
    MatchingEdgeCasesTest,
    NonCrossingLimitOrdersRemainActive
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId sell_id =
        engine.place_limit_sell(
            530'000,
            100
        );

    const OrderId buy_id =
        engine.place_limit_buy(
            520'000,
            100
        );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 2U);

    EXPECT_EQ(
        orders[0].order_id,
        sell_id
    );

    EXPECT_EQ(
        orders[1].order_id,
        buy_id
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );

    ASSERT_EQ(
        order_book.bids().size(),
        1U
    );

    ASSERT_EQ(
        order_book.asks().size(),
        1U
    );

    EXPECT_EQ(
        order_book.bids()[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        order_book.bids()[0].quantity,
        100
    );

    EXPECT_EQ(
        order_book.asks()[0].price_ticks,
        530'000
    );

    EXPECT_EQ(
        order_book.asks()[0].quantity,
        100
    );
}

TEST(
    MatchingEdgeCasesTest,
    LargeIncomingBuyExhaustsBookAndRestsRemainder
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            510'000,
            10
        )
    );

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            20
        )
    );

    static_cast<void>(
        engine.place_limit_sell(
            530'000,
            30
        )
    );

    const OrderId incoming_buy =
        engine.place_limit_buy(
            540'000,
            100
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
        10
    );

    EXPECT_EQ(
        trades[1].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[1].quantity,
        20
    );

    EXPECT_EQ(
        trades[2].price_ticks,
        530'000
    );

    EXPECT_EQ(
        trades[2].quantity,
        30
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        incoming_buy
    );

    EXPECT_EQ(
        orders[0].side,
        OrderSide::Buy
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        40
    );

    EXPECT_TRUE(
        order_book.asks().empty()
    );

    ASSERT_EQ(
        order_book.bids().size(),
        1U
    );

    EXPECT_EQ(
        order_book.bids()[0].price_ticks,
        540'000
    );

    EXPECT_EQ(
        order_book.bids()[0].quantity,
        40
    );
}

TEST(
    MatchingEdgeCasesTest,
    LimitSellMatchingMirrorsLimitBuyMatching
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId resting_buy =
        engine.place_limit_buy(
            520'000,
            100
        );

    static_cast<void>(
        engine.place_limit_sell(
            515'000,
            40
        )
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        resting_buy
    );

    EXPECT_EQ(
        orders[0].side,
        OrderSide::Buy
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        60
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(trades.size(), 1U);

    EXPECT_EQ(
        trades[0].aggressor_side,
        OrderSide::Sell
    );

    EXPECT_EQ(
        trades[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        40
    );

    ASSERT_EQ(
        order_book.bids().size(),
        1U
    );

    EXPECT_EQ(
        order_book.bids()[0].quantity,
        60
    );

    EXPECT_TRUE(
        order_book.asks().empty()
    );
}
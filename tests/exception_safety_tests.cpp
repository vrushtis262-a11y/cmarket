#include "matching_engine.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

TEST(ExceptionSafetyTest, ValidLimitOrderWorksAfterRejectedLimitOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    EXPECT_THROW(
        static_cast<void>(
            engine.place_limit_buy(
                0,
                100
            )
        ),
        std::invalid_argument
    );

    const OrderId order_id =
        engine.place_limit_buy(
            520'000,
            100
        );

    ASSERT_EQ(
        engine.active_limit_orders().size(),
        1U
    );

    const LimitOrder& order =
        engine.active_limit_orders()[0];

    EXPECT_EQ(
        order.order_id,
        order_id
    );

    EXPECT_EQ(
        order.price_ticks,
        520'000
    );

    EXPECT_EQ(
        order.remaining_quantity,
        100
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
        engine.trade_history().empty()
    );
}

TEST(ExceptionSafetyTest, ValidMarketOrderWorksAfterRejectedMarketOrder)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {},
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 100
            }
        }
    );

    MatchingEngine engine(order_book);

    EXPECT_THROW(
        static_cast<void>(
            engine.execute_market_buy(0)
        ),
        std::invalid_argument
    );

    const ExecutionResult result =
        engine.execute_market_buy(40);

    EXPECT_EQ(
        result.requested_quantity,
        40
    );

    EXPECT_EQ(
        result.executed_quantity,
        40
    );

    EXPECT_EQ(
        result.remaining_quantity,
        0
    );

    EXPECT_TRUE(
        result.fully_filled()
    );

    ASSERT_EQ(
        result.trades.size(),
        1U
    );

    EXPECT_EQ(
        result.trades[0].price_ticks,
        540'000
    );

    EXPECT_EQ(
        result.trades[0].quantity,
        40
    );

    ASSERT_EQ(
        order_book.asks().size(),
        1U
    );

    EXPECT_EQ(
        order_book.asks()[0].quantity,
        60
    );

    ASSERT_EQ(
        engine.trade_history().size(),
        1U
    );
}

TEST(ExceptionSafetyTest, ValidModifyWorksAfterRejectedModify)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(
            520'000,
            100
        );

    const SequenceNumber original_sequence =
        engine.active_limit_orders()[0]
            .sequence_number;

    EXPECT_THROW(
        static_cast<void>(
            engine.modify_order(
                order_id,
                0,
                80
            )
        ),
        std::invalid_argument
    );

    ASSERT_EQ(
        engine.active_limit_orders().size(),
        1U
    );

    EXPECT_EQ(
        engine.active_limit_orders()[0]
            .price_ticks,
        520'000
    );

    EXPECT_EQ(
        engine.active_limit_orders()[0]
            .remaining_quantity,
        100
    );

    EXPECT_TRUE(
        engine.modify_order(
            order_id,
            520'000,
            60
        )
    );

    ASSERT_EQ(
        engine.active_limit_orders().size(),
        1U
    );

    const LimitOrder& modified =
        engine.active_limit_orders()[0];

    EXPECT_EQ(
        modified.order_id,
        order_id
    );

    EXPECT_EQ(
        modified.price_ticks,
        520'000
    );

    EXPECT_EQ(
        modified.remaining_quantity,
        60
    );

    EXPECT_EQ(
        modified.sequence_number,
        original_sequence
    );

    ASSERT_EQ(
        order_book.bids().size(),
        1U
    );

    EXPECT_EQ(
        order_book.bids()[0].quantity,
        60
    );
}

TEST(ExceptionSafetyTest, UnknownModifyDoesNotPreventLaterValidModify)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_sell(
            540'000,
            100
        );

    EXPECT_FALSE(
        engine.modify_order(
            999,
            550'000,
            50
        )
    );

    ASSERT_EQ(
        engine.active_limit_orders().size(),
        1U
    );

    EXPECT_TRUE(
        engine.modify_order(
            order_id,
            545'000,
            80
        )
    );

    ASSERT_EQ(
        engine.active_limit_orders().size(),
        1U
    );

    const LimitOrder& order =
        engine.active_limit_orders()[0];

    EXPECT_EQ(
        order.order_id,
        order_id
    );

    EXPECT_EQ(
        order.price_ticks,
        545'000
    );

    EXPECT_EQ(
        order.remaining_quantity,
        80
    );

    ASSERT_EQ(
        order_book.asks().size(),
        1U
    );

    EXPECT_EQ(
        order_book.asks()[0].price_ticks,
        545'000
    );

    EXPECT_EQ(
        order_book.asks()[0].quantity,
        80
    );
}

TEST(ExceptionSafetyTest, RepeatedRejectedOperationsPreserveEngineState)
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
            540'000,
            100
        );

    EXPECT_THROW(
        static_cast<void>(
            engine.execute_market_buy(0)
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            engine.place_limit_sell(
                -1,
                50
            )
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            engine.modify_order(
                buy_order_id,
                520'000,
                0
            )
        ),
        std::invalid_argument
    );

    EXPECT_FALSE(
        engine.cancel_order(999)
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(
        orders.size(),
        2U
    );

    EXPECT_EQ(
        orders[0].order_id,
        buy_order_id
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        100
    );

    EXPECT_EQ(
        orders[1].order_id,
        sell_order_id
    );

    EXPECT_EQ(
        orders[1].remaining_quantity,
        100
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
        order_book.bids()[0].quantity,
        100
    );

    EXPECT_EQ(
        order_book.asks()[0].quantity,
        100
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

TEST(ExceptionSafetyTest, QuantityAggregationOverflowPreservesEngineState)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const std::int64_t maximum =
        std::numeric_limits<std::int64_t>::max();

    const OrderId original_order_id =
        engine.place_limit_buy(
            520'000,
            maximum
        );

    ASSERT_EQ(
        engine.active_limit_orders().size(),
        1U
    );

    ASSERT_EQ(
        order_book.bids().size(),
        1U
    );

    EXPECT_EQ(
        order_book.bids()[0].quantity,
        maximum
    );

    EXPECT_THROW(
        static_cast<void>(
            engine.place_limit_buy(
                520'000,
                1
            )
        ),
        std::overflow_error
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(
        orders.size(),
        1U
    );

    EXPECT_EQ(
        orders[0].order_id,
        original_order_id
    );

    EXPECT_EQ(
        orders[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        maximum
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
        maximum
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}
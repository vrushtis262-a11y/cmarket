#include "matching_engine.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace {

void expect_engine_invariants(
    const OrderBook& order_book,
    const MatchingEngine& engine
)
{
    std::map<
        std::int64_t,
        std::int64_t,
        std::greater<>
    > expected_bids;

    std::map<
        std::int64_t,
        std::int64_t
    > expected_asks;

    for (
        const LimitOrder& order :
        engine.active_limit_orders()
    ) {
        EXPECT_GT(
            order.original_quantity,
            0
        );

        EXPECT_GT(
            order.remaining_quantity,
            0
        );

        EXPECT_LE(
            order.remaining_quantity,
            order.original_quantity
        );

        EXPECT_FALSE(
            order.is_filled()
        );

        if (order.side == OrderSide::Buy) {
            std::int64_t& quantity =
                expected_bids[
                    order.price_ticks
                ];

            ASSERT_LE(
                order.remaining_quantity,
                std::numeric_limits<
                    std::int64_t
                >::max() - quantity
            );

            quantity +=
                order.remaining_quantity;
        }
        else {
            std::int64_t& quantity =
                expected_asks[
                    order.price_ticks
                ];

            ASSERT_LE(
                order.remaining_quantity,
                std::numeric_limits<
                    std::int64_t
                >::max() - quantity
            );

            quantity +=
                order.remaining_quantity;
        }
    }

    ASSERT_EQ(
        order_book.bids().size(),
        expected_bids.size()
    );

    ASSERT_EQ(
        order_book.asks().size(),
        expected_asks.size()
    );

    std::size_t bid_index = 0;

    for (
        const auto& [
            price_ticks,
            quantity
        ] : expected_bids
    ) {
        EXPECT_EQ(
            order_book.bids()[bid_index]
                .price_ticks,
            price_ticks
        );

        EXPECT_EQ(
            order_book.bids()[bid_index]
                .quantity,
            quantity
        );

        ++bid_index;
    }

    std::size_t ask_index = 0;

    for (
        const auto& [
            price_ticks,
            quantity
        ] : expected_asks
    ) {
        EXPECT_EQ(
            order_book.asks()[ask_index]
                .price_ticks,
            price_ticks
        );

        EXPECT_EQ(
            order_book.asks()[ask_index]
                .quantity,
            quantity
        );

        ++ask_index;
    }

    std::set<TradeId> trade_ids;

    TradeSequenceNumber
        previous_execution_sequence = 0;

    for (
        const Trade& trade :
        engine.trade_history()
    ) {
        EXPECT_GT(
            trade.price_ticks,
            0
        );

        EXPECT_GT(
            trade.quantity,
            0
        );

        EXPECT_GT(
            trade.trade_id,
            0U
        );

        EXPECT_TRUE(
            trade_ids.insert(
                trade.trade_id
            ).second
        );

        EXPECT_GT(
            trade.execution_sequence,
            previous_execution_sequence
        );

        previous_execution_sequence =
            trade.execution_sequence;
    }
}

} // namespace

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

    expect_engine_invariants(
        order_book,
        engine
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

    expect_engine_invariants(
        order_book,
        engine
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

    expect_engine_invariants(
        order_book,
        engine
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

    expect_engine_invariants(
        order_book,
        engine
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

    expect_engine_invariants(
        order_book,
        engine
    );
}

TEST(ExceptionSafetyTest, FailedLimitPlacementDoesNotConsumeOrderIdOrSequence)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const std::int64_t maximum =
        std::numeric_limits<std::int64_t>::max();

    const OrderId first_order_id =
        engine.place_limit_buy(
            520'000,
            maximum
        );

    ASSERT_EQ(
        first_order_id,
        1U
    );

    ASSERT_EQ(
        engine.active_limit_orders().size(),
        1U
    );

    EXPECT_EQ(
        engine.active_limit_orders()[0]
            .sequence_number,
        1U
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

    const OrderId second_order_id =
        engine.place_limit_buy(
            519'000,
            1
        );

    EXPECT_EQ(
        second_order_id,
        2U
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(
        orders.size(),
        2U
    );

    EXPECT_EQ(
        orders[0].order_id,
        first_order_id
    );

    EXPECT_EQ(
        orders[0].sequence_number,
        1U
    );

    EXPECT_EQ(
        orders[1].order_id,
        second_order_id
    );

    EXPECT_EQ(
        orders[1].sequence_number,
        2U
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );

    expect_engine_invariants(
        order_book,
        engine
    );
}

TEST(ExceptionSafetyTest, FailedModifyRestoresRemovedOrderAndSequenceCounter)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const std::int64_t maximum =
        std::numeric_limits<std::int64_t>::max();

    const OrderId first_order_id =
        engine.place_limit_buy(
            520'000,
            maximum
        );

    const OrderId second_order_id =
        engine.place_limit_buy(
            521'000,
            1
        );

    ASSERT_EQ(
        engine.active_limit_orders().size(),
        2U
    );

    EXPECT_EQ(
        engine.active_limit_orders()[0]
            .sequence_number,
        1U
    );

    EXPECT_EQ(
        engine.active_limit_orders()[1]
            .sequence_number,
        2U
    );

    EXPECT_THROW(
        static_cast<void>(
            engine.modify_order(
                second_order_id,
                520'000,
                1
            )
        ),
        std::overflow_error
    );

    const auto& after_failure =
        engine.active_limit_orders();

    ASSERT_EQ(
        after_failure.size(),
        2U
    );

    EXPECT_EQ(
        after_failure[0].order_id,
        first_order_id
    );

    EXPECT_EQ(
        after_failure[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        after_failure[0].remaining_quantity,
        maximum
    );

    EXPECT_EQ(
        after_failure[0].sequence_number,
        1U
    );

    EXPECT_EQ(
        after_failure[1].order_id,
        second_order_id
    );

    EXPECT_EQ(
        after_failure[1].price_ticks,
        521'000
    );

    EXPECT_EQ(
        after_failure[1].remaining_quantity,
        1
    );

    EXPECT_EQ(
        after_failure[1].sequence_number,
        2U
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );

    EXPECT_TRUE(
        engine.modify_order(
            second_order_id,
            522'000,
            1
        )
    );

    const auto& after_valid_modify =
        engine.active_limit_orders();

    ASSERT_EQ(
        after_valid_modify.size(),
        2U
    );

    EXPECT_EQ(
        after_valid_modify[1].order_id,
        second_order_id
    );

    EXPECT_EQ(
        after_valid_modify[1].price_ticks,
        522'000
    );

    EXPECT_EQ(
        after_valid_modify[1].sequence_number,
        3U
    );

    expect_engine_invariants(
        order_book,
        engine
    );
}

TEST(ExceptionSafetyTest, PartialFillKeepsBookAndActiveOrdersSynchronized)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId sell_order_id =
        engine.place_limit_sell(
            540'000,
            100
        );

    const OrderId buy_order_id =
        engine.place_limit_buy(
            540'000,
            40
        );

    EXPECT_NE(
        sell_order_id,
        buy_order_id
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(
        orders.size(),
        1U
    );

    EXPECT_EQ(
        orders[0].order_id,
        sell_order_id
    );

    EXPECT_EQ(
        orders[0].side,
        OrderSide::Sell
    );

    EXPECT_EQ(
        orders[0].price_ticks,
        540'000
    );

    EXPECT_EQ(
        orders[0].original_quantity,
        100
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        60
    );

    EXPECT_FALSE(
        orders[0].is_filled()
    );

    EXPECT_TRUE(
        order_book.bids().empty()
    );

    ASSERT_EQ(
        order_book.asks().size(),
        1U
    );

    EXPECT_EQ(
        order_book.asks()[0].price_ticks,
        540'000
    );

    EXPECT_EQ(
        order_book.asks()[0].quantity,
        60
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(
        trades.size(),
        1U
    );

    EXPECT_EQ(
        trades[0].price_ticks,
        540'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        40
    );

    EXPECT_EQ(
        trades[0].buy_order_id,
        buy_order_id
    );

    EXPECT_EQ(
        trades[0].sell_order_id,
        sell_order_id
    );

    expect_engine_invariants(
        order_book,
        engine
    );
}
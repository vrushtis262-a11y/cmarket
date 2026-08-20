#include "matching_engine.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <vector>

namespace {

constexpr std::int64_t minimum_price = 490'000;
constexpr std::int64_t maximum_price = 550'000;
constexpr std::int64_t minimum_quantity = 1;
constexpr std::int64_t maximum_quantity = 200;

enum class RandomOperation {
    PlaceBuy,
    PlaceSell,
    Cancel,
    Modify
};

std::int64_t random_price(
    std::mt19937_64& generator
)
{
    std::uniform_int_distribution<std::int64_t>
        distribution(
            minimum_price,
            maximum_price
        );

    return distribution(generator);
}

std::int64_t random_quantity(
    std::mt19937_64& generator
)
{
    std::uniform_int_distribution<std::int64_t>
        distribution(
            minimum_quantity,
            maximum_quantity
        );

    return distribution(generator);
}

RandomOperation random_operation(
    std::mt19937_64& generator
)
{
    std::uniform_int_distribution<int>
        distribution(0, 3);

    return static_cast<RandomOperation>(
        distribution(generator)
    );
}

OrderId random_active_order_id(
    const MatchingEngine& engine,
    std::mt19937_64& generator
)
{
    const auto& orders =
        engine.active_limit_orders();

    std::uniform_int_distribution<std::size_t>
        distribution(
            0,
            orders.size() - 1
        );

    return orders[
        distribution(generator)
    ].order_id;
}

void expect_active_orders_valid(
    const MatchingEngine& engine
)
{
    const auto& orders =
        engine.active_limit_orders();

    std::set<OrderId> seen_order_ids;

    for (const LimitOrder& order : orders) {
        EXPECT_GT(
            order.order_id,
            0U
        );

        EXPECT_TRUE(
            seen_order_ids.insert(
                order.order_id
            ).second
        );

        EXPECT_GT(
            order.price_ticks,
            0
        );

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
    }
}

void expect_book_sorted(
    const OrderBook& order_book
)
{
    const auto& bids =
        order_book.bids();

    const auto& asks =
        order_book.asks();

    for (
        std::size_t index = 1;
        index < bids.size();
        ++index
    ) {
        EXPECT_GT(
            bids[index - 1].price_ticks,
            bids[index].price_ticks
        );
    }

    for (
        std::size_t index = 1;
        index < asks.size();
        ++index
    ) {
        EXPECT_LT(
            asks[index - 1].price_ticks,
            asks[index].price_ticks
        );
    }

    for (const PriceLevel& level : bids) {
        EXPECT_GT(
            level.price_ticks,
            0
        );

        EXPECT_GT(
            level.quantity,
            0
        );
    }

    for (const PriceLevel& level : asks) {
        EXPECT_GT(
            level.price_ticks,
            0
        );

        EXPECT_GT(
            level.quantity,
            0
        );
    }
}

void expect_book_matches_active_orders(
    const MatchingEngine& engine,
    const OrderBook& order_book
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
        if (order.side == OrderSide::Buy) {
            expected_bids[
                order.price_ticks
            ] += order.remaining_quantity;
        }
        else {
            expected_asks[
                order.price_ticks
            ] += order.remaining_quantity;
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
}

void expect_no_crossed_active_book(
    const OrderBook& order_book
)
{
    if (
        order_book.bids().empty() ||
        order_book.asks().empty()
    ) {
        return;
    }

    EXPECT_LT(
        order_book.bids().front().price_ticks,
        order_book.asks().front().price_ticks
    );
}

void expect_trade_history_valid(
    const MatchingEngine& engine
)
{
    const auto& trades =
        engine.trade_history();

    std::set<TradeId> seen_trade_ids;

    TradeId previous_trade_id = 0;

    TradeSequenceNumber
        previous_execution_sequence = 0;

    for (const Trade& trade : trades) {
        EXPECT_GT(
            trade.trade_id,
            0U
        );

        EXPECT_TRUE(
            seen_trade_ids.insert(
                trade.trade_id
            ).second
        );

        EXPECT_GT(
            trade.trade_id,
            previous_trade_id
        );

        EXPECT_GT(
            trade.execution_sequence,
            previous_execution_sequence
        );

        EXPECT_GT(
            trade.price_ticks,
            0
        );

        EXPECT_GT(
            trade.quantity,
            0
        );

        ASSERT_TRUE(
            trade.buy_order_id.has_value()
        );

        ASSERT_TRUE(
            trade.sell_order_id.has_value()
        );

        EXPECT_NE(
            *trade.buy_order_id,
            *trade.sell_order_id
        );

        previous_trade_id =
            trade.trade_id;

        previous_execution_sequence =
            trade.execution_sequence;
    }
}

void expect_engine_invariants(
    const MatchingEngine& engine,
    const OrderBook& order_book
)
{
    expect_active_orders_valid(
        engine
    );

    expect_book_sorted(
        order_book
    );

    expect_book_matches_active_orders(
        engine,
        order_book
    );

    expect_no_crossed_active_book(
        order_book
    );

    expect_trade_history_valid(
        engine
    );
}

void run_randomized_sequence(
    std::uint64_t seed,
    std::size_t operation_count
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    std::mt19937_64 generator(seed);

    for (
        std::size_t operation_index = 0;
        operation_index < operation_count;
        ++operation_index
    ) {
        const RandomOperation operation =
            random_operation(generator);

        switch (operation) {
        case RandomOperation::PlaceBuy:
            static_cast<void>(
                engine.place_limit_buy(
                    random_price(generator),
                    random_quantity(generator)
                )
            );
            break;

        case RandomOperation::PlaceSell:
            static_cast<void>(
                engine.place_limit_sell(
                    random_price(generator),
                    random_quantity(generator)
                )
            );
            break;

        case RandomOperation::Cancel:
            if (
                engine.active_limit_orders()
                    .empty()
            ) {
                break;
            }

            EXPECT_TRUE(
                engine.cancel_order(
                    random_active_order_id(
                        engine,
                        generator
                    )
                )
            );
            break;

        case RandomOperation::Modify:
            if (
                engine.active_limit_orders()
                    .empty()
            ) {
                break;
            }

            EXPECT_TRUE(
                engine.modify_order(
                    random_active_order_id(
                        engine,
                        generator
                    ),
                    random_price(generator),
                    random_quantity(generator)
                )
            );
            break;
        }

        expect_engine_invariants(
            engine,
            order_book
        );
    }
}

} // namespace

TEST(
    RandomizedMatchingTest,
    MaintainsInvariantsAcrossOneThousandOperations
)
{
    run_randomized_sequence(
        0xC0FFEEULL,
        1'000
    );
}

TEST(
    RandomizedMatchingTest,
    MaintainsInvariantsWithAlternateSeed
)
{
    run_randomized_sequence(
        0xBADC0DEULL,
        1'000
    );
}

TEST(
    RandomizedMatchingTest,
    MaintainsInvariantsWithThirdSeed
)
{
    run_randomized_sequence(
        0x123456789ULL,
        1'000
    );
}

TEST(
    RandomizedMatchingTest,
    SameSeedProducesSameFinalState
)
{
    constexpr std::uint64_t seed =
        0xDEADBEEFULL;

    constexpr std::size_t
        operation_count = 500;

    OrderBook first_order_book;
    MatchingEngine first_engine(
        first_order_book
    );

    OrderBook second_order_book;
    MatchingEngine second_engine(
        second_order_book
    );

    std::mt19937_64 first_generator(
        seed
    );

    std::mt19937_64 second_generator(
        seed
    );

    for (
        std::size_t operation_index = 0;
        operation_index < operation_count;
        ++operation_index
    ) {
        const RandomOperation first_operation =
            random_operation(
                first_generator
            );

        const RandomOperation second_operation =
            random_operation(
                second_generator
            );

        ASSERT_EQ(
            static_cast<int>(first_operation),
            static_cast<int>(second_operation)
        );

        switch (first_operation) {
        case RandomOperation::PlaceBuy:
        {
            const std::int64_t first_price =
                random_price(
                    first_generator
                );

            const std::int64_t second_price =
                random_price(
                    second_generator
                );

            const std::int64_t first_quantity =
                random_quantity(
                    first_generator
                );

            const std::int64_t second_quantity =
                random_quantity(
                    second_generator
                );

            ASSERT_EQ(
                first_price,
                second_price
            );

            ASSERT_EQ(
                first_quantity,
                second_quantity
            );

            EXPECT_EQ(
                first_engine.place_limit_buy(
                    first_price,
                    first_quantity
                ),
                second_engine.place_limit_buy(
                    second_price,
                    second_quantity
                )
            );

            break;
        }

        case RandomOperation::PlaceSell:
        {
            const std::int64_t first_price =
                random_price(
                    first_generator
                );

            const std::int64_t second_price =
                random_price(
                    second_generator
                );

            const std::int64_t first_quantity =
                random_quantity(
                    first_generator
                );

            const std::int64_t second_quantity =
                random_quantity(
                    second_generator
                );

            ASSERT_EQ(
                first_price,
                second_price
            );

            ASSERT_EQ(
                first_quantity,
                second_quantity
            );

            EXPECT_EQ(
                first_engine.place_limit_sell(
                    first_price,
                    first_quantity
                ),
                second_engine.place_limit_sell(
                    second_price,
                    second_quantity
                )
            );

            break;
        }

        case RandomOperation::Cancel:
            if (
                first_engine.active_limit_orders()
                    .empty()
            ) {
                ASSERT_TRUE(
                    second_engine
                        .active_limit_orders()
                        .empty()
                );

                break;
            }

            {
                const OrderId first_order_id =
                    random_active_order_id(
                        first_engine,
                        first_generator
                    );

                const OrderId second_order_id =
                    random_active_order_id(
                        second_engine,
                        second_generator
                    );

                ASSERT_EQ(
                    first_order_id,
                    second_order_id
                );

                EXPECT_EQ(
                    first_engine.cancel_order(
                        first_order_id
                    ),
                    second_engine.cancel_order(
                        second_order_id
                    )
                );
            }

            break;

        case RandomOperation::Modify:
            if (
                first_engine.active_limit_orders()
                    .empty()
            ) {
                ASSERT_TRUE(
                    second_engine
                        .active_limit_orders()
                        .empty()
                );

                break;
            }

            {
                const OrderId first_order_id =
                    random_active_order_id(
                        first_engine,
                        first_generator
                    );

                const OrderId second_order_id =
                    random_active_order_id(
                        second_engine,
                        second_generator
                    );

                const std::int64_t first_price =
                    random_price(
                        first_generator
                    );

                const std::int64_t second_price =
                    random_price(
                        second_generator
                    );

                const std::int64_t first_quantity =
                    random_quantity(
                        first_generator
                    );

                const std::int64_t second_quantity =
                    random_quantity(
                        second_generator
                    );

                ASSERT_EQ(
                    first_order_id,
                    second_order_id
                );

                ASSERT_EQ(
                    first_price,
                    second_price
                );

                ASSERT_EQ(
                    first_quantity,
                    second_quantity
                );

                EXPECT_EQ(
                    first_engine.modify_order(
                        first_order_id,
                        first_price,
                        first_quantity
                    ),
                    second_engine.modify_order(
                        second_order_id,
                        second_price,
                        second_quantity
                    )
                );
            }

            break;
        }

        expect_engine_invariants(
            first_engine,
            first_order_book
        );

        expect_engine_invariants(
            second_engine,
            second_order_book
        );
    }

    const auto& first_orders =
        first_engine.active_limit_orders();

    const auto& second_orders =
        second_engine.active_limit_orders();

    ASSERT_EQ(
        first_orders.size(),
        second_orders.size()
    );

    for (
        std::size_t index = 0;
        index < first_orders.size();
        ++index
    ) {
        EXPECT_EQ(
            first_orders[index].order_id,
            second_orders[index].order_id
        );

        EXPECT_EQ(
            first_orders[index].side,
            second_orders[index].side
        );

        EXPECT_EQ(
            first_orders[index].price_ticks,
            second_orders[index].price_ticks
        );

        EXPECT_EQ(
            first_orders[index].remaining_quantity,
            second_orders[index].remaining_quantity
        );

        EXPECT_EQ(
            first_orders[index].sequence_number,
            second_orders[index].sequence_number
        );
    }

    const auto& first_trades =
        first_engine.trade_history();

    const auto& second_trades =
        second_engine.trade_history();

    ASSERT_EQ(
        first_trades.size(),
        second_trades.size()
    );

    for (
        std::size_t index = 0;
        index < first_trades.size();
        ++index
    ) {
        EXPECT_EQ(
            first_trades[index].trade_id,
            second_trades[index].trade_id
        );

        EXPECT_EQ(
            first_trades[index].execution_sequence,
            second_trades[index].execution_sequence
        );

        EXPECT_EQ(
            first_trades[index].aggressor_side,
            second_trades[index].aggressor_side
        );

        EXPECT_EQ(
            first_trades[index].price_ticks,
            second_trades[index].price_ticks
        );

        EXPECT_EQ(
            first_trades[index].quantity,
            second_trades[index].quantity
        );

        EXPECT_EQ(
            first_trades[index].buy_order_id,
            second_trades[index].buy_order_id
        );

        EXPECT_EQ(
            first_trades[index].sell_order_id,
            second_trades[index].sell_order_id
        );
    }
}
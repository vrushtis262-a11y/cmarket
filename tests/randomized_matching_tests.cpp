#include "matching_engine.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <vector>

namespace {

constexpr std::int64_t minimum_price = 490'000;
constexpr std::int64_t maximum_price = 550'000;
constexpr std::int64_t minimum_quantity = 1;
constexpr std::int64_t maximum_quantity = 200;

enum class RandomOperationType {
    PlaceBuy,
    PlaceSell,
    MarketBuy,
    MarketSell,
    Cancel,
    Modify
};

struct RandomOperation {
    RandomOperationType type;
    std::int64_t price_ticks = 0;
    std::int64_t quantity = 0;
    std::size_t active_order_index = 0;

    [[nodiscard]]
    bool operator==(
        const RandomOperation& other
    ) const noexcept
    {
        return type ==
                   other.type &&
               price_ticks ==
                   other.price_ticks &&
               quantity ==
                   other.quantity &&
               active_order_index ==
                   other.active_order_index;
    }
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

RandomOperationType random_operation_type(
    std::mt19937_64& generator
)
{
    std::uniform_int_distribution<int>
        distribution(0, 5);

    return static_cast<RandomOperationType>(
        distribution(generator)
    );
}

RandomOperation generate_random_operation(
    std::mt19937_64& generator
)
{
    RandomOperation operation{
        .type = random_operation_type(
            generator
        )
    };

    switch (operation.type) {
    case RandomOperationType::PlaceBuy:
    case RandomOperationType::PlaceSell:
    case RandomOperationType::Modify:
        operation.price_ticks =
            random_price(generator);

        operation.quantity =
            random_quantity(generator);
        break;

    case RandomOperationType::MarketBuy:
    case RandomOperationType::MarketSell:
        operation.quantity =
            random_quantity(generator);
        break;

    case RandomOperationType::Cancel:
        break;
    }

    if (
        operation.type ==
            RandomOperationType::Cancel ||
        operation.type ==
            RandomOperationType::Modify
    ) {
        operation.active_order_index =
            static_cast<std::size_t>(
                generator()
            );
    }

    return operation;
}

std::vector<RandomOperation>
generate_random_operation_sequence(
    std::uint64_t seed,
    std::size_t operation_count
)
{
    std::mt19937_64 generator(seed);

    std::vector<RandomOperation> operations;

    operations.reserve(
        operation_count
    );

    for (
        std::size_t index = 0;
        index < operation_count;
        ++index
    ) {
        operations.push_back(
            generate_random_operation(
                generator
            )
        );
    }

    return operations;
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

        previous_trade_id =
            trade.trade_id;

        previous_execution_sequence =
            trade.execution_sequence;
    }
}

void assert_engine_invariants(
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

void execute_random_operation(
    const RandomOperation& operation,
    MatchingEngine& engine
)
{
    switch (operation.type) {
    case RandomOperationType::PlaceBuy:
        static_cast<void>(
            engine.place_limit_buy(
                operation.price_ticks,
                operation.quantity
            )
        );
        break;

    case RandomOperationType::PlaceSell:
        static_cast<void>(
            engine.place_limit_sell(
                operation.price_ticks,
                operation.quantity
            )
        );
        break;

    case RandomOperationType::MarketBuy:
        static_cast<void>(
            engine.execute_market_buy(
                operation.quantity
            )
        );
        break;

    case RandomOperationType::MarketSell:
        static_cast<void>(
            engine.execute_market_sell(
                operation.quantity
            )
        );
        break;

    case RandomOperationType::Cancel:
    {
        const auto& orders =
            engine.active_limit_orders();

        if (orders.empty()) {
            break;
        }

        const std::size_t index =
            operation.active_order_index %
            orders.size();

        const OrderId order_id =
            orders[index].order_id;

        EXPECT_TRUE(
            engine.cancel_order(
                order_id
            )
        );

        break;
    }

    case RandomOperationType::Modify:
    {
        const auto& orders =
            engine.active_limit_orders();

        if (orders.empty()) {
            break;
        }

        const std::size_t index =
            operation.active_order_index %
            orders.size();

        const OrderId order_id =
            orders[index].order_id;

        EXPECT_TRUE(
            engine.modify_order(
                order_id,
                operation.price_ticks,
                operation.quantity
            )
        );

        break;
    }
    }
}

void run_randomized_sequence(
    std::uint64_t seed,
    std::size_t operation_count
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const std::vector<RandomOperation>
        operations =
            generate_random_operation_sequence(
                seed,
                operation_count
            );

    for (
        const RandomOperation& operation :
        operations
    ) {
        execute_random_operation(
            operation,
            engine
        );

        assert_engine_invariants(
            engine,
            order_book
        );
    }
}

void expect_same_engine_state(
    const MatchingEngine& first_engine,
    const OrderBook& first_order_book,
    const MatchingEngine& second_engine,
    const OrderBook& second_order_book
)
{
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
            first_orders[index].original_quantity,
            second_orders[index].original_quantity
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

    const auto& first_bids =
        first_order_book.bids();

    const auto& second_bids =
        second_order_book.bids();

    ASSERT_EQ(
        first_bids.size(),
        second_bids.size()
    );

    for (
        std::size_t index = 0;
        index < first_bids.size();
        ++index
    ) {
        EXPECT_EQ(
            first_bids[index].price_ticks,
            second_bids[index].price_ticks
        );

        EXPECT_EQ(
            first_bids[index].quantity,
            second_bids[index].quantity
        );
    }

    const auto& first_asks =
        first_order_book.asks();

    const auto& second_asks =
        second_order_book.asks();

    ASSERT_EQ(
        first_asks.size(),
        second_asks.size()
    );

    for (
        std::size_t index = 0;
        index < first_asks.size();
        ++index
    ) {
        EXPECT_EQ(
            first_asks[index].price_ticks,
            second_asks[index].price_ticks
        );

        EXPECT_EQ(
            first_asks[index].quantity,
            second_asks[index].quantity
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

} // namespace

TEST(
    RandomizedMatchingTest,
    MaintainsInvariantsAcrossTwoThousandOperations
)
{
    run_randomized_sequence(
        0xC0FFEEULL,
        2'000
    );
}

TEST(
    RandomizedMatchingTest,
    MaintainsInvariantsWithAlternateSeed
)
{
    run_randomized_sequence(
        0xBADC0DEULL,
        2'000
    );
}

TEST(
    RandomizedMatchingTest,
    MaintainsInvariantsWithThirdSeed
)
{
    run_randomized_sequence(
        0x123456789ULL,
        2'000
    );
}

TEST(
    RandomizedMatchingTest,
    SameSeedProducesIdenticalOperationSequence
)
{
    constexpr std::uint64_t seed =
        0xDEADBEEFULL;

    constexpr std::size_t
        operation_count = 1'000;

    const std::vector<RandomOperation>
        first_sequence =
            generate_random_operation_sequence(
                seed,
                operation_count
            );

    const std::vector<RandomOperation>
        second_sequence =
            generate_random_operation_sequence(
                seed,
                operation_count
            );

    ASSERT_EQ(
        first_sequence.size(),
        second_sequence.size()
    );

    for (
        std::size_t index = 0;
        index < first_sequence.size();
        ++index
    ) {
        EXPECT_TRUE(
            first_sequence[index] ==
            second_sequence[index]
        );
    }
}

TEST(
    RandomizedMatchingTest,
    DifferentSeedsProduceDifferentOperationSequences
)
{
    const std::vector<RandomOperation>
        first_sequence =
            generate_random_operation_sequence(
                0x111111ULL,
                250
            );

    const std::vector<RandomOperation>
        second_sequence =
            generate_random_operation_sequence(
                0x222222ULL,
                250
            );

    ASSERT_EQ(
        first_sequence.size(),
        second_sequence.size()
    );

    EXPECT_FALSE(
        first_sequence ==
        second_sequence
    );
}

TEST(
    RandomizedMatchingTest,
    SameOperationSequenceProducesSameFinalState
)
{
    constexpr std::uint64_t seed =
        0xABCDEF123ULL;

    constexpr std::size_t
        operation_count = 1'000;

    const std::vector<RandomOperation>
        operations =
            generate_random_operation_sequence(
                seed,
                operation_count
            );

    OrderBook first_order_book;
    MatchingEngine first_engine(
        first_order_book
    );

    OrderBook second_order_book;
    MatchingEngine second_engine(
        second_order_book
    );

    for (
        const RandomOperation& operation :
        operations
    ) {
        execute_random_operation(
            operation,
            first_engine
        );

        execute_random_operation(
            operation,
            second_engine
        );

        assert_engine_invariants(
            first_engine,
            first_order_book
        );

        assert_engine_invariants(
            second_engine,
            second_order_book
        );
    }

    expect_same_engine_state(
        first_engine,
        first_order_book,
        second_engine,
        second_order_book
    );
}
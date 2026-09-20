#include "blocking_queue.hpp"
#include "concurrent_matching_engine.hpp"
#include "order_book.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <mutex>
#include <numeric>
#include <optional>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

bool active_orders_are_valid(
    const std::vector<LimitOrder>& orders
)
{
    std::unordered_set<OrderId> order_ids;
    order_ids.reserve(orders.size());

    for (const LimitOrder& order : orders) {
        if (order.order_id == 0) {
            return false;
        }

        if (order.price_ticks <= 0) {
            return false;
        }

        if (order.original_quantity <= 0) {
            return false;
        }

        if (order.remaining_quantity <= 0) {
            return false;
        }

        if (order.remaining_quantity >
            order.original_quantity) {
            return false;
        }

        if (order.sequence_number == 0) {
            return false;
        }

        if (!order_ids.insert(order.order_id).second) {
            return false;
        }
    }

    return true;
}

bool trades_are_valid(
    const std::vector<Trade>& trades
)
{
    std::unordered_set<TradeId> trade_ids;
    std::unordered_set<TradeSequenceNumber>
        execution_sequences;

    trade_ids.reserve(trades.size());
    execution_sequences.reserve(trades.size());

    for (const Trade& trade : trades) {
        if (trade.trade_id == 0) {
            return false;
        }

        if (trade.execution_sequence == 0) {
            return false;
        }

        if (trade.price_ticks <= 0) {
            return false;
        }

        if (trade.quantity <= 0) {
            return false;
        }

        if (!trade_ids.insert(trade.trade_id).second) {
            return false;
        }

        if (!execution_sequences
                 .insert(trade.execution_sequence)
                 .second) {
            return false;
        }
    }

    return true;
}

std::int64_t total_trade_quantity(
    const std::vector<Trade>& trades
)
{
    return std::accumulate(
        trades.begin(),
        trades.end(),
        std::int64_t{0},
        [](
            std::int64_t total,
            const Trade& trade
        ) {
            return total + trade.quantity;
        }
    );
}

bool bids_are_valid(
    const std::vector<PriceLevel>& bids
)
{
    for (std::size_t index = 0;
         index < bids.size();
         ++index) {
        if (bids[index].quantity <= 0) {
            return false;
        }

        if (
            index > 0 &&
            bids[index - 1].price_ticks <=
                bids[index].price_ticks
        ) {
            return false;
        }
    }

    return true;
}

bool asks_are_valid(
    const std::vector<PriceLevel>& asks
)
{
    for (std::size_t index = 0;
         index < asks.size();
         ++index) {
        if (asks[index].quantity <= 0) {
            return false;
        }

        if (
            index > 0 &&
            asks[index - 1].price_ticks >=
                asks[index].price_ticks
        ) {
            return false;
        }
    }

    return true;
}

} // namespace

TEST(
    ConcurrentStressTest,
    HighVolumeMultipleProducersCreateUniqueOrders
)
{
    OrderBook book;
    ConcurrentMatchingEngine engine(book);

    constexpr int producer_count = 8;
    constexpr int orders_per_producer = 2'000;

    const std::size_t total_orders =
        static_cast<std::size_t>(
            producer_count * orders_per_producer
        );

    std::vector<std::vector<OrderId>>
        producer_order_ids(
            static_cast<std::size_t>(
                producer_count
            )
        );

    std::atomic<bool> worker_failed{false};

    std::vector<std::thread> producers;
    producers.reserve(
        static_cast<std::size_t>(
            producer_count
        )
    );

    for (int producer = 0;
         producer < producer_count;
         ++producer) {
        producers.emplace_back(
            [
                &engine,
                &producer_order_ids,
                &worker_failed,
                producer
            ] {
                auto& order_ids =
                    producer_order_ids[
                        static_cast<std::size_t>(
                            producer
                        )
                    ];

                order_ids.reserve(
                    static_cast<std::size_t>(
                        orders_per_producer
                    )
                );

                try {
                    for (
                        int order_index = 0;
                        order_index <
                            orders_per_producer;
                        ++order_index
                    ) {
                        const std::int64_t price =
                            100'000 +
                            static_cast<std::int64_t>(
                                producer
                            ) *
                                10'000 +
                            static_cast<std::int64_t>(
                                order_index % 1'000
                            );

                        const OrderId order_id =
                            engine.place_limit_buy(
                                price,
                                10
                            ).get();

                        order_ids.push_back(
                            order_id
                        );
                    }
                }
                catch (...) {
                    worker_failed.store(
                        true,
                        std::memory_order_relaxed
                    );
                }
            }
        );
    }

    for (auto& producer : producers) {
        producer.join();
    }

    ASSERT_FALSE(
        worker_failed.load(
            std::memory_order_relaxed
        )
    );

    std::unordered_set<OrderId> unique_ids;
    unique_ids.reserve(total_orders);

    for (const auto& order_ids :
         producer_order_ids) {
        ASSERT_EQ(
            order_ids.size(),
            static_cast<std::size_t>(
                orders_per_producer
            )
        );

        for (const OrderId order_id :
             order_ids) {
            EXPECT_TRUE(
                unique_ids.insert(
                    order_id
                ).second
            );
        }
    }

    EXPECT_EQ(
        unique_ids.size(),
        total_orders
    );

    const auto active_orders =
        engine.active_limit_orders().get();

    ASSERT_EQ(
        active_orders.size(),
        total_orders
    );

    EXPECT_TRUE(
        active_orders_are_valid(
            active_orders
        )
    );

    EXPECT_TRUE(
        bids_are_valid(
            book.bids()
        )
    );

    EXPECT_TRUE(
        asks_are_valid(
            book.asks()
        )
    );
}

TEST(
    ConcurrentStressTest,
    ConcurrentCancellationRemovesEverySubmittedOrder
)
{
    OrderBook book;
    ConcurrentMatchingEngine engine(book);

    constexpr int order_count = 10'000;
    constexpr int canceller_count = 8;

    std::vector<OrderId> order_ids;
    order_ids.reserve(
        static_cast<std::size_t>(
            order_count
        )
    );

    for (int index = 0;
         index < order_count;
         ++index) {
        order_ids.push_back(
            engine.place_limit_buy(
                400'000 +
                    static_cast<std::int64_t>(
                        index % 100
                    ),
                10
            ).get()
        );
    }

    std::atomic<int> successful_cancels{0};
    std::atomic<bool> worker_failed{false};

    std::vector<std::thread> cancellers;
    cancellers.reserve(
        static_cast<std::size_t>(
            canceller_count
        )
    );

    for (int canceller = 0;
         canceller < canceller_count;
         ++canceller) {
        cancellers.emplace_back(
            [
                &engine,
                &order_ids,
                &successful_cancels,
                &worker_failed,
                canceller
            ] {
                try {
                    for (
                        std::size_t index =
                            static_cast<
                                std::size_t
                            >(canceller);
                        index < order_ids.size();
                        index +=
                            static_cast<
                                std::size_t
                            >(canceller_count)
                    ) {
                        if (
                            engine.cancel_order(
                                order_ids[index]
                            ).get()
                        ) {
                            successful_cancels
                                .fetch_add(
                                    1,
                                    std::memory_order_relaxed
                                );
                        }
                    }
                }
                catch (...) {
                    worker_failed.store(
                        true,
                        std::memory_order_relaxed
                    );
                }
            }
        );
    }

    for (auto& canceller : cancellers) {
        canceller.join();
    }

    ASSERT_FALSE(
        worker_failed.load(
            std::memory_order_relaxed
        )
    );

    EXPECT_EQ(
        successful_cancels.load(
            std::memory_order_relaxed
        ),
        order_count
    );

    EXPECT_TRUE(
        engine.active_limit_orders()
            .get()
            .empty()
    );

    EXPECT_TRUE(book.empty());
}

TEST(
    ConcurrentStressTest,
    ConcurrentModificationPreservesOrderIdentityAndQuantities
)
{
    OrderBook book;
    ConcurrentMatchingEngine engine(book);

    constexpr int order_count = 8'000;
    constexpr int modifier_count = 8;

    std::vector<OrderId> order_ids;
    order_ids.reserve(
        static_cast<std::size_t>(
            order_count
        )
    );

    for (int index = 0;
         index < order_count;
         ++index) {
        order_ids.push_back(
            engine.place_limit_buy(
                300'000 +
                    static_cast<std::int64_t>(
                        index % 100
                    ),
                10
            ).get()
        );
    }

    std::atomic<int> successful_modifies{0};
    std::atomic<bool> worker_failed{false};

    std::vector<std::thread> modifiers;
    modifiers.reserve(
        static_cast<std::size_t>(
            modifier_count
        )
    );

    for (int modifier = 0;
         modifier < modifier_count;
         ++modifier) {
        modifiers.emplace_back(
            [
                &engine,
                &order_ids,
                &successful_modifies,
                &worker_failed,
                modifier
            ] {
                try {
                    for (
                        std::size_t index =
                            static_cast<
                                std::size_t
                            >(modifier);
                        index < order_ids.size();
                        index +=
                            static_cast<
                                std::size_t
                            >(modifier_count)
                    ) {
                        const std::int64_t price =
                            350'000 +
                            static_cast<std::int64_t>(
                                index % 100
                            );

                        const std::int64_t quantity =
                            20 +
                            static_cast<std::int64_t>(
                                index % 50
                            );

                        if (
                            engine.modify_order(
                                order_ids[index],
                                price,
                                quantity
                            ).get()
                        ) {
                            successful_modifies
                                .fetch_add(
                                    1,
                                    std::memory_order_relaxed
                                );
                        }
                    }
                }
                catch (...) {
                    worker_failed.store(
                        true,
                        std::memory_order_relaxed
                    );
                }
            }
        );
    }

    for (auto& modifier : modifiers) {
        modifier.join();
    }

    ASSERT_FALSE(
        worker_failed.load(
            std::memory_order_relaxed
        )
    );

    EXPECT_EQ(
        successful_modifies.load(
            std::memory_order_relaxed
        ),
        order_count
    );

    const auto active_orders =
        engine.active_limit_orders().get();

    ASSERT_EQ(
        active_orders.size(),
        static_cast<std::size_t>(
            order_count
        )
    );

    EXPECT_TRUE(
        active_orders_are_valid(
            active_orders
        )
    );

    std::unordered_set<OrderId>
        active_ids;

    active_ids.reserve(
        active_orders.size()
    );

    for (const LimitOrder& order :
         active_orders) {
        active_ids.insert(
            order.order_id
        );

        EXPECT_GE(
            order.remaining_quantity,
            20
        );

        EXPECT_LE(
            order.remaining_quantity,
            69
        );
    }

    for (const OrderId order_id :
         order_ids) {
        EXPECT_TRUE(
            active_ids.contains(
                order_id
            )
        );
    }
}

TEST(
    ConcurrentStressTest,
    ConcurrentMarketOrdersConserveExecutedQuantity
)
{
    OrderBook book;
    ConcurrentMatchingEngine engine(book);

    constexpr int resting_order_count = 4'000;
    constexpr std::int64_t quantity_per_order = 10;

    constexpr int producer_count = 8;
    constexpr int market_orders_per_producer = 500;

    const std::int64_t total_resting_quantity =
        static_cast<std::int64_t>(
            resting_order_count
        ) *
        quantity_per_order;

    for (int index = 0;
         index < resting_order_count;
         ++index) {
        static_cast<void>(
            engine.place_limit_sell(
                500'000,
                quantity_per_order
            ).get()
        );
    }

    std::vector<std::vector<ExecutionResult>>
        producer_results(
            static_cast<std::size_t>(
                producer_count
            )
        );

    std::atomic<bool> worker_failed{false};

    std::vector<std::thread> producers;
    producers.reserve(
        static_cast<std::size_t>(
            producer_count
        )
    );

    for (int producer = 0;
         producer < producer_count;
         ++producer) {
        producers.emplace_back(
            [
                &engine,
                &producer_results,
                &worker_failed,
                producer
            ] {
                auto& results =
                    producer_results[
                        static_cast<std::size_t>(
                            producer
                        )
                    ];

                results.reserve(
                    static_cast<std::size_t>(
                        market_orders_per_producer
                    )
                );

                try {
                    for (
                        int index = 0;
                        index <
                            market_orders_per_producer;
                        ++index
                    ) {
                        results.push_back(
                            engine.execute_market_buy(
                                quantity_per_order
                            ).get()
                        );
                    }
                }
                catch (...) {
                    worker_failed.store(
                        true,
                        std::memory_order_relaxed
                    );
                }
            }
        );
    }

    for (auto& producer : producers) {
        producer.join();
    }

    ASSERT_FALSE(
        worker_failed.load(
            std::memory_order_relaxed
        )
    );

    std::int64_t total_executed = 0;

    for (const auto& results :
         producer_results) {
        ASSERT_EQ(
            results.size(),
            static_cast<std::size_t>(
                market_orders_per_producer
            )
        );

        for (const ExecutionResult& result :
             results) {
            EXPECT_EQ(
                result.requested_quantity,
                quantity_per_order
            );

            EXPECT_EQ(
                result.executed_quantity +
                    result.remaining_quantity,
                result.requested_quantity
            );

            EXPECT_GE(
                result.executed_quantity,
                0
            );

            EXPECT_GE(
                result.remaining_quantity,
                0
            );

            total_executed +=
                result.executed_quantity;
        }
    }

    EXPECT_EQ(
        total_executed,
        total_resting_quantity
    );

    EXPECT_TRUE(
        engine.active_limit_orders()
            .get()
            .empty()
    );

    const auto trades =
        engine.trade_history().get();

    EXPECT_TRUE(
        trades_are_valid(
            trades
        )
    );

    EXPECT_EQ(
        total_trade_quantity(
            trades
        ),
        total_resting_quantity
    );

    EXPECT_TRUE(book.empty());
}

TEST(
    ConcurrentStressTest,
    SamePriceRestingOrdersRemainFifoUnderConcurrentMarketSubmission
)
{
    OrderBook book;
    ConcurrentMatchingEngine engine(book);

    constexpr int resting_order_count = 2'000;
    constexpr std::int64_t price = 500'000;
    constexpr std::int64_t quantity = 1;
    constexpr int producer_count = 8;

    std::vector<OrderId> resting_ids;
    resting_ids.reserve(
        static_cast<std::size_t>(
            resting_order_count
        )
    );

    for (int index = 0;
         index < resting_order_count;
         ++index) {
        resting_ids.push_back(
            engine.place_limit_sell(
                price,
                quantity
            ).get()
        );
    }

    std::atomic<bool> worker_failed{false};

    std::vector<std::thread> producers;
    producers.reserve(
        static_cast<std::size_t>(
            producer_count
        )
    );

    for (int producer = 0;
         producer < producer_count;
         ++producer) {
        producers.emplace_back(
            [
                &engine,
                &worker_failed,
                producer
            ] {
                try {
                    for (
                        int index = producer;
                        index <
                            resting_order_count;
                        index += producer_count
                    ) {
                        const ExecutionResult result =
                            engine.execute_market_buy(
                                quantity
                            ).get();

                        if (
                            result.executed_quantity !=
                                quantity ||
                            result.remaining_quantity !=
                                0
                        ) {
                            worker_failed.store(
                                true,
                                std::memory_order_relaxed
                            );
                            return;
                        }
                    }
                }
                catch (...) {
                    worker_failed.store(
                        true,
                        std::memory_order_relaxed
                    );
                }
            }
        );
    }

    for (auto& producer : producers) {
        producer.join();
    }

    ASSERT_FALSE(
        worker_failed.load(
            std::memory_order_relaxed
        )
    );

    const auto trades =
        engine.trade_history().get();

    ASSERT_EQ(
        trades.size(),
        static_cast<std::size_t>(
            resting_order_count
        )
    );

    ASSERT_TRUE(
        trades_are_valid(
            trades
        )
    );

    for (std::size_t index = 0;
         index < trades.size();
         ++index) {
        ASSERT_TRUE(
            trades[index]
                .sell_order_id
                .has_value()
        );

        EXPECT_EQ(
            *trades[index].sell_order_id,
            resting_ids[index]
        );

        EXPECT_EQ(
            trades[index].price_ticks,
            price
        );

        EXPECT_EQ(
            trades[index].quantity,
            quantity
        );
    }

    EXPECT_TRUE(
        engine.active_limit_orders()
            .get()
            .empty()
    );
}

TEST(
    ConcurrentStressTest,
    MixedSubmissionCancellationAndModificationPreservesInvariants
)
{
    constexpr int rounds = 5;
    constexpr int producer_count = 6;
    constexpr int orders_per_producer = 1'000;

    for (int round = 0;
         round < rounds;
         ++round) {
        OrderBook book;
        ConcurrentMatchingEngine engine(book);

        std::vector<std::vector<OrderId>>
            producer_ids(
                static_cast<std::size_t>(
                    producer_count
                )
            );

        std::atomic<bool> worker_failed{false};

        std::vector<std::thread> producers;
        producers.reserve(
            static_cast<std::size_t>(
                producer_count
            )
        );

        for (int producer = 0;
             producer < producer_count;
             ++producer) {
            producers.emplace_back(
                [
                    &engine,
                    &producer_ids,
                    &worker_failed,
                    producer,
                    round
                ] {
                    auto& ids =
                        producer_ids[
                            static_cast<
                                std::size_t
                            >(producer)
                        ];

                    ids.reserve(
                        static_cast<std::size_t>(
                            orders_per_producer
                        )
                    );

                    try {
                        for (
                            int index = 0;
                            index <
                                orders_per_producer;
                            ++index
                        ) {
                            const std::int64_t price =
                                200'000 +
                                static_cast<
                                    std::int64_t
                                >(
                                    round * 1'000 +
                                    producer * 100 +
                                    index % 100
                                );

                            ids.push_back(
                                engine.place_limit_buy(
                                    price,
                                    100
                                ).get()
                            );
                        }
                    }
                    catch (...) {
                        worker_failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                    }
                }
            );
        }

        for (auto& producer : producers) {
            producer.join();
        }

        ASSERT_FALSE(
            worker_failed.load(
                std::memory_order_relaxed
            )
        );

        std::vector<OrderId> all_ids;

        all_ids.reserve(
            static_cast<std::size_t>(
                producer_count *
                orders_per_producer
            )
        );

        for (const auto& ids :
             producer_ids) {
            all_ids.insert(
                all_ids.end(),
                ids.begin(),
                ids.end()
            );
        }

        std::vector<std::thread> mutators;
        mutators.reserve(
            static_cast<std::size_t>(
                producer_count
            )
        );

        for (int worker = 0;
             worker < producer_count;
             ++worker) {
            mutators.emplace_back(
                [
                    &engine,
                    &all_ids,
                    &worker_failed,
                    worker,
                    round
                ] {
                    try {
                        for (
                            std::size_t index =
                                static_cast<
                                    std::size_t
                                >(worker);
                            index <
                                all_ids.size();
                            index +=
                                static_cast<
                                    std::size_t
                                >(producer_count)
                        ) {
                            if (index % 3 == 0) {
                                static_cast<void>(
                                    engine.cancel_order(
                                        all_ids[index]
                                    ).get()
                                );
                            }
                            else {
                                const std::int64_t
                                    new_price =
                                        300'000 +
                                        static_cast<
                                            std::int64_t
                                        >(
                                            round * 1'000
                                        ) +
                                        static_cast<
                                            std::int64_t
                                        >(
                                            index % 500
                                        );

                                const std::int64_t
                                    new_quantity =
                                        50 +
                                        static_cast<
                                            std::int64_t
                                        >(
                                            index % 50
                                        );

                                static_cast<void>(
                                    engine.modify_order(
                                        all_ids[index],
                                        new_price,
                                        new_quantity
                                    ).get()
                                );
                            }
                        }
                    }
                    catch (...) {
                        worker_failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                    }
                }
            );
        }

        for (auto& mutator : mutators) {
            mutator.join();
        }

        ASSERT_FALSE(
            worker_failed.load(
                std::memory_order_relaxed
            )
        );

        const auto active_orders =
            engine.active_limit_orders().get();

        EXPECT_TRUE(
            active_orders_are_valid(
                active_orders
            )
        );

        EXPECT_TRUE(
            bids_are_valid(
                book.bids()
            )
        );

        EXPECT_TRUE(
            asks_are_valid(
                book.asks()
            )
        );

        std::unordered_set<OrderId>
            active_ids;

        active_ids.reserve(
            active_orders.size()
        );

        for (const LimitOrder& order :
             active_orders) {
            EXPECT_TRUE(
                active_ids.insert(
                    order.order_id
                ).second
            );
        }
    }
}

TEST(
    ConcurrentStressTest,
    QueueMultipleProducersAndConsumersLoseNoEvents
)
{
    BlockingQueue<std::uint64_t> queue;

    constexpr int producer_count = 8;
    constexpr int consumer_count = 8;
    constexpr int values_per_producer = 10'000;

    const std::size_t total_values =
        static_cast<std::size_t>(
            producer_count *
            values_per_producer
        );

    std::vector<std::atomic<unsigned int>>
        seen(total_values);

    for (auto& count : seen) {
        count.store(
            0,
            std::memory_order_relaxed
        );
    }

    std::atomic<std::size_t>
        consumed_count{0};

    std::atomic<bool> invalid_value{false};

    std::vector<std::thread> consumers;
    consumers.reserve(
        static_cast<std::size_t>(
            consumer_count
        )
    );

    for (int consumer = 0;
         consumer < consumer_count;
         ++consumer) {
        consumers.emplace_back(
            [
                &queue,
                &seen,
                &consumed_count,
                &invalid_value
            ] {
                while (const auto value =
                           queue.pop()) {
                    const std::uint64_t index =
                        *value;

                    if (
                        index >=
                        static_cast<std::uint64_t>(
                            total_values
                        )
                    ) {
                        invalid_value.store(
                            true,
                            std::memory_order_relaxed
                        );
                        continue;
                    }

                    seen[
                        static_cast<std::size_t>(
                            index
                        )
                    ].fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                    consumed_count.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                }
            }
        );
    }

    std::atomic<bool> push_failed{false};

    std::vector<std::thread> producers;
    producers.reserve(
        static_cast<std::size_t>(
            producer_count
        )
    );

    for (int producer = 0;
         producer < producer_count;
         ++producer) {
        producers.emplace_back(
            [
                &queue,
                &push_failed,
                producer
            ] {
                const std::uint64_t base =
                    static_cast<std::uint64_t>(
                        producer
                    ) *
                    static_cast<std::uint64_t>(
                        values_per_producer
                    );

                for (int index = 0;
                     index <
                         values_per_producer;
                     ++index) {
                    if (
                        !queue.push(
                            base +
                            static_cast<
                                std::uint64_t
                            >(index)
                        )
                    ) {
                        push_failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                        return;
                    }
                }
            }
        );
    }

    for (auto& producer : producers) {
        producer.join();
    }

    queue.close();

    for (auto& consumer : consumers) {
        consumer.join();
    }

    EXPECT_FALSE(
        push_failed.load(
            std::memory_order_relaxed
        )
    );

    EXPECT_FALSE(
        invalid_value.load(
            std::memory_order_relaxed
        )
    );

    EXPECT_EQ(
        consumed_count.load(
            std::memory_order_relaxed
        ),
        total_values
    );

    for (const auto& count : seen) {
        EXPECT_EQ(
            count.load(
                std::memory_order_relaxed
            ),
            1U
        );
    }
}
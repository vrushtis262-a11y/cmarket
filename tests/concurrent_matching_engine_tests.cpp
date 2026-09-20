#include "concurrent_matching_engine.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

TEST(
    ConcurrentMatchingEngineTest,
    PlacesLimitOrderThroughWorker
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(
            500'000,
            100
        ).get();

    const auto orders =
        engine.active_limit_orders().get();

    ASSERT_EQ(orders.size(), 1U);
    EXPECT_EQ(orders.front().order_id, order_id);
    EXPECT_EQ(
        orders.front().side,
        OrderSide::Buy
    );
    EXPECT_EQ(
        orders.front().price_ticks,
        500'000
    );
    EXPECT_EQ(
        orders.front().remaining_quantity,
        100
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    ExecutesMarketOrderThroughWorker
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            600'000,
            100
        ).get()
    );

    const ExecutionResult result =
        engine.execute_market_buy(
            40
        ).get();

    EXPECT_EQ(result.requested_quantity, 40);
    EXPECT_EQ(result.executed_quantity, 40);
    EXPECT_EQ(result.remaining_quantity, 0);
    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(
        result.trades.front().price_ticks,
        600'000
    );
    EXPECT_EQ(
        result.trades.front().quantity,
        40
    );

    const auto orders =
        engine.active_limit_orders().get();

    ASSERT_EQ(orders.size(), 1U);
    EXPECT_EQ(
        orders.front().remaining_quantity,
        60
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    CancelsOrderThroughWorker
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(
            400'000,
            75
        ).get();

    EXPECT_TRUE(
        engine.cancel_order(
            order_id
        ).get()
    );

    EXPECT_TRUE(
        engine.active_limit_orders()
            .get()
            .empty()
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    ModifiesOrderThroughWorker
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(
            400'000,
            75
        ).get();

    EXPECT_TRUE(
        engine.modify_order(
            order_id,
            450'000,
            125
        ).get()
    );

    const auto orders =
        engine.active_limit_orders().get();

    ASSERT_EQ(orders.size(), 1U);
    EXPECT_EQ(orders.front().order_id, order_id);
    EXPECT_EQ(
        orders.front().price_ticks,
        450'000
    );
    EXPECT_EQ(
        orders.front().remaining_quantity,
        125
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    ReturnsTradeHistoryCopies
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            550'000,
            100
        ).get()
    );

    static_cast<void>(
        engine.execute_market_buy(
            25
        ).get()
    );

    const auto trades =
        engine.trade_history().get();

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(
        trades.front().price_ticks,
        550'000
    );
    EXPECT_EQ(
        trades.front().quantity,
        25
    );

    engine.clear_trade_history().get();

    EXPECT_TRUE(
        engine.trade_history()
            .get()
            .empty()
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    PropagatesEngineExceptionsThroughFuture
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    auto future =
        engine.place_limit_buy(
            500'000,
            0
        );

    EXPECT_THROW(
        static_cast<void>(future.get()),
        std::invalid_argument
    );

    const OrderId order_id =
        engine.place_limit_buy(
            500'000,
            10
        ).get();

    EXPECT_NE(order_id, 0U);
}

TEST(
    ConcurrentMatchingEngineTest,
    MultipleProducersSubmitOrdersWithoutLoss
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    constexpr int producer_count = 4;
    constexpr int orders_per_producer = 250;
    constexpr int total_orders =
        producer_count * orders_per_producer;

    std::vector<std::vector<OrderId>>
        producer_order_ids(
            static_cast<std::size_t>(
                producer_count
            )
        );

    std::vector<std::thread> producers;
    producers.reserve(
        static_cast<std::size_t>(
            producer_count
        )
    );

    for (
        int producer = 0;
        producer < producer_count;
        ++producer
    ) {
        producers.emplace_back(
            [&, producer] {
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

                for (
                    int order = 0;
                    order < orders_per_producer;
                    ++order
                ) {
                    order_ids.push_back(
                        engine.place_limit_buy(
                            100'000 +
                                producer * 1'000 +
                                order,
                            10
                        ).get()
                    );
                }
            }
        );
    }

    for (auto& producer : producers) {
        producer.join();
    }

    const auto orders =
        engine.active_limit_orders().get();

    ASSERT_EQ(
        orders.size(),
        static_cast<std::size_t>(
            total_orders
        )
    );

    std::vector<OrderId> order_ids;
    order_ids.reserve(
        static_cast<std::size_t>(
            total_orders
        )
    );

    for (const auto& producer_ids :
         producer_order_ids) {
        order_ids.insert(
            order_ids.end(),
            producer_ids.begin(),
            producer_ids.end()
        );
    }

    std::sort(
        order_ids.begin(),
        order_ids.end()
    );

    const auto duplicate =
        std::adjacent_find(
            order_ids.begin(),
            order_ids.end()
        );

    EXPECT_EQ(
        duplicate,
        order_ids.end()
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    ConcurrentProducersCanSubmitAndCancelOrders
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    constexpr int producer_count = 4;
    constexpr int orders_per_producer = 200;

    std::vector<std::thread> producers;
    producers.reserve(
        static_cast<std::size_t>(
            producer_count
        )
    );

    for (
        int producer = 0;
        producer < producer_count;
        ++producer
    ) {
        producers.emplace_back(
            [&, producer] {
                for (
                    int order = 0;
                    order < orders_per_producer;
                    ++order
                ) {
                    const OrderId order_id =
                        engine.place_limit_buy(
                            200'000 +
                                producer * 1'000 +
                                order,
                            20
                        ).get();

                    const bool cancelled =
                        engine.cancel_order(
                            order_id
                        ).get();

                    EXPECT_TRUE(cancelled);
                }
            }
        );
    }

    for (auto& producer : producers) {
        producer.join();
    }

    EXPECT_TRUE(
        engine.active_limit_orders()
            .get()
            .empty()
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    ConcurrentMarketOrdersAreSerialized
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    constexpr int seller_count = 400;
    constexpr std::int64_t quantity_per_order = 10;

    for (
        int seller = 0;
        seller < seller_count;
        ++seller
    ) {
        static_cast<void>(
            engine.place_limit_sell(
                600'000,
                quantity_per_order
            ).get()
        );
    }

    constexpr int buyer_count = 4;
    constexpr int buys_per_thread = 100;

    std::vector<std::thread> buyers;
    buyers.reserve(
        static_cast<std::size_t>(
            buyer_count
        )
    );

    for (
        int buyer = 0;
        buyer < buyer_count;
        ++buyer
    ) {
        buyers.emplace_back(
            [&] {
                for (
                    int order = 0;
                    order < buys_per_thread;
                    ++order
                ) {
                    const ExecutionResult result =
                        engine.execute_market_buy(
                            quantity_per_order
                        ).get();

                    EXPECT_TRUE(
                        result.fully_filled()
                    );

                    EXPECT_EQ(
                        result.executed_quantity,
                        quantity_per_order
                    );
                }
            }
        );
    }

    for (auto& buyer : buyers) {
        buyer.join();
    }

    EXPECT_TRUE(
        engine.active_limit_orders()
            .get()
            .empty()
    );

    const auto trades =
        engine.trade_history().get();

    EXPECT_EQ(
        trades.size(),
        static_cast<std::size_t>(
            seller_count
        )
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    ShutdownDrainsQueuedCommands
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    std::vector<std::future<OrderId>> futures;

    constexpr int order_count = 500;

    futures.reserve(
        static_cast<std::size_t>(
            order_count
        )
    );

    for (
        int order = 0;
        order < order_count;
        ++order
    ) {
        futures.push_back(
            engine.place_limit_buy(
                300'000 + order,
                10
            )
        );
    }

    engine.shutdown();

    for (auto& future : futures) {
        EXPECT_NE(
            future.get(),
            0U
        );
    }

    EXPECT_TRUE(engine.is_shutdown());
}

TEST(
    ConcurrentMatchingEngineTest,
    SubmissionFailsAfterShutdown
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    engine.shutdown();

    EXPECT_THROW(
        static_cast<void>(
            engine.place_limit_buy(
                500'000,
                10
            )
        ),
        std::runtime_error
    );
}

TEST(
    ConcurrentMatchingEngineTest,
    ShutdownIsIdempotent
)
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(order_book);

    engine.shutdown();
    engine.shutdown();

    EXPECT_TRUE(engine.is_shutdown());
}
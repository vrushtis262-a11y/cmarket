
#include "ofi_concurrent_matching_adapter.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

class OFIConcurrentMatchingAdapterTest
    : public ::testing::Test {
protected:
    OrderBook book;
    ConcurrentMatchingEngine engine{book};

    cmarket::OrderFlowImbalance ofi{1000};
    cmarket::OFIOrderBookHandler handler{ofi};

    cmarket::OFIConcurrentMatchingAdapter adapter{
        engine,
        book,
        handler,
        "AAPL"
    };

    void SetUp() override {
        adapter.initialize();
    }
};

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    InitialSnapshotIsBaseline
) {
    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
    EXPECT_FALSE(ofi.latest_ofi("AAPL").has_value());
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    LimitBuyUpdatesOFI
) {
    const auto id = adapter.place_limit_buy(100, 50);

    EXPECT_GT(id, 0U);
    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 50.0L);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    LimitSellUpdatesOFI
) {
    const auto id = adapter.place_limit_sell(110, 40);

    EXPECT_GT(id, 0U);
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -40.0L);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    CancelOrderUpdatesOFI
) {
    const auto id = adapter.place_limit_buy(100, 50);

    ASSERT_TRUE(adapter.cancel_order(id));

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -50.0L);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 0.0L);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    ModifyOrderUpdatesOFI
) {
    const auto id = adapter.place_limit_buy(100, 50);

    ASSERT_TRUE(adapter.modify_order(id, 100, 80));

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 30.0L);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 80.0L);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    MarketBuyUpdatesOFI
) {
    static_cast<void>(
        adapter.place_limit_sell(110, 50)
    );

    const auto result = adapter.execute_market_buy(20);

    EXPECT_EQ(result.executed_quantity, 20);
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 20.0L);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    MarketSellUpdatesOFI
) {
    static_cast<void>(
        adapter.place_limit_buy(100, 50)
    );

    const auto result = adapter.execute_market_sell(20);

    EXPECT_EQ(result.executed_quantity, 20);
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -20.0L);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    InvalidOrderDoesNotUpdateOFI
) {
    EXPECT_THROW(
        static_cast<void>(
            adapter.place_limit_buy(100, 0)
        ),
        std::invalid_argument
    );

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    UnknownCancelDoesNotUpdateOFI
) {
    EXPECT_FALSE(adapter.cancel_order(99999));
    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    ConcurrentBuySubmissions
) {
    constexpr int thread_count = 8;
    constexpr int orders_per_thread = 25;

    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;

    for (int index = 0; index < thread_count; ++index) {
        threads.emplace_back([&] {
            try {
                for (int n = 0; n < orders_per_thread; ++n) {
                    static_cast<void>(
                        adapter.place_limit_buy(100, 1)
                    );
                }
            } catch (...) {
                failed.store(true);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_FALSE(failed.load());

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->quantity, 200);

    EXPECT_EQ(ofi.event_count("AAPL"), 200U);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 200.0L);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    ConcurrentCancellations
) {
    constexpr int order_count = 100;

    std::vector<OrderId> ids;

    for (int i = 0; i < order_count; ++i) {
        ids.push_back(
            adapter.place_limit_buy(100, 1)
        );
    }

    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;

    for (OrderId id : ids) {
        threads.emplace_back([&, id] {
            try {
                if (!adapter.cancel_order(id)) {
                    failed.store(true);
                }
            } catch (...) {
                failed.store(true);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_FALSE(failed.load());

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 0.0L);
    EXPECT_EQ(ofi.event_count("AAPL"), 200U);
}

TEST_F(
    OFIConcurrentMatchingAdapterTest,
    ConcurrentMarketOrdersConserveVolume
) {
    constexpr int thread_count = 5;
    constexpr int orders_per_thread = 10;

    static_cast<void>(
        adapter.place_limit_sell(110, 100)
    );

    std::atomic<std::int64_t> executed{0};
    std::atomic<bool> failed{false};

    std::vector<std::thread> threads;

    for (int index = 0; index < thread_count; ++index) {
        threads.emplace_back([&] {
            try {
                for (int n = 0; n < orders_per_thread; ++n) {
                    const auto result =
                        adapter.execute_market_buy(2);

                    executed.fetch_add(
                        result.executed_quantity
                    );
                }
            } catch (...) {
                failed.store(true);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_FALSE(failed.load());

    EXPECT_EQ(executed.load(), 100);
    EXPECT_FALSE(book.best_ask().has_value());

    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 0.0L);
    EXPECT_EQ(ofi.event_count("AAPL"), 51U);
}

} // namespace

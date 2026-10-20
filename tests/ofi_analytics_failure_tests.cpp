
#include "ofi_matching_engine_adapter.hpp"
#include "ofi_concurrent_matching_adapter.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace {

TEST(
    OFIAnalyticsFailureTest,
    CommittedLimitOrderSurvivesAnalyticsFailure
) {
    OrderBook book;
    MatchingEngine engine(book);

    cmarket::OrderFlowImbalance ofi(10);

    bool inject_failure = false;

    cmarket::OFIOrderBookHandler handler(
        ofi,
        [&inject_failure] {
            if (inject_failure) {
                throw std::runtime_error(
                    "Simulated OFI failure"
                );
            }
        }
    );

    cmarket::OFIMatchingEngineAdapter adapter(
        engine,
        book,
        handler,
        "AAPL"
    );

    adapter.initialize();

    inject_failure = true;

    OrderId order_id = 0;

    EXPECT_NO_THROW(
        order_id = adapter.place_limit_buy(
            100,
            50
        )
    );

    EXPECT_GT(order_id, 0U);
    EXPECT_FALSE(adapter.analytics_healthy());

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->quantity, 50);

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);

    EXPECT_EQ(
        engine.active_limit_orders().size(),
        1U
    );
}

TEST(
    OFIAnalyticsFailureTest,
    CommittedMarketExecutionSurvivesAnalyticsFailure
) {
    OrderBook book;
    MatchingEngine engine(book);

    cmarket::OrderFlowImbalance ofi(10);

    bool inject_failure = false;

    cmarket::OFIOrderBookHandler handler(
        ofi,
        [&inject_failure] {
            if (inject_failure) {
                throw std::runtime_error(
                    "Simulated OFI failure"
                );
            }
        }
    );

    cmarket::OFIMatchingEngineAdapter adapter(
        engine,
        book,
        handler,
        "AAPL"
    );

    adapter.initialize();

    static_cast<void>(
        adapter.place_limit_sell(110, 50)
    );

    inject_failure = true;

    EXPECT_NO_THROW({
        const auto result =
            adapter.execute_market_buy(20);

        EXPECT_EQ(result.executed_quantity, 20);
        EXPECT_EQ(result.remaining_quantity, 0);
    });

    EXPECT_FALSE(adapter.analytics_healthy());

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(book.best_ask()->quantity, 30);

    EXPECT_FALSE(engine.trade_history().empty());
}

TEST(
    OFIAnalyticsFailureTest,
    InvalidOrderStillThrowsOriginalException
) {
    OrderBook book;
    MatchingEngine engine(book);

    cmarket::OrderFlowImbalance ofi(10);

    cmarket::OFIOrderBookHandler handler(
        ofi,
        [] {
            throw std::runtime_error(
                "Simulated OFI failure"
            );
        }
    );

    cmarket::OFIMatchingEngineAdapter adapter(
        engine,
        book,
        handler,
        "AAPL"
    );

    EXPECT_THROW(
        static_cast<void>(
            adapter.place_limit_buy(100, 0)
        ),
        std::invalid_argument
    );

    EXPECT_TRUE(adapter.analytics_healthy());
    EXPECT_TRUE(book.empty());
}

TEST(
    OFIAnalyticsFailureTest,
    ConcurrentCommittedOrderSurvivesAnalyticsFailure
) {
    OrderBook book;
    ConcurrentMatchingEngine engine(book);

    cmarket::OrderFlowImbalance ofi(10);

    bool inject_failure = false;

    cmarket::OFIOrderBookHandler handler(
        ofi,
        [&inject_failure] {
            if (inject_failure) {
                throw std::runtime_error(
                    "Simulated OFI failure"
                );
            }
        }
    );

    cmarket::OFIConcurrentMatchingAdapter adapter(
        engine,
        book,
        handler,
        "AAPL"
    );

    adapter.initialize();

    inject_failure = true;

    OrderId order_id = 0;

    EXPECT_NO_THROW(
        order_id = adapter.place_limit_buy(
            100,
            50
        )
    );

    EXPECT_GT(order_id, 0U);
    EXPECT_FALSE(adapter.analytics_healthy());

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->quantity, 50);

    EXPECT_EQ(
        engine.active_limit_orders().get().size(),
        1U
    );
}

TEST(
    OFIAnalyticsFailureTest,
    ConcurrentMarketExecutionSurvivesAnalyticsFailure
) {
    OrderBook book;
    ConcurrentMatchingEngine engine(book);

    cmarket::OrderFlowImbalance ofi(10);

    bool inject_failure = false;

    cmarket::OFIOrderBookHandler handler(
        ofi,
        [&inject_failure] {
            if (inject_failure) {
                throw std::runtime_error(
                    "Simulated OFI failure"
                );
            }
        }
    );

    cmarket::OFIConcurrentMatchingAdapter adapter(
        engine,
        book,
        handler,
        "AAPL"
    );

    adapter.initialize();

    static_cast<void>(
        adapter.place_limit_sell(110, 50)
    );

    inject_failure = true;

    EXPECT_NO_THROW({
        const auto result =
            adapter.execute_market_buy(20);

        EXPECT_EQ(result.executed_quantity, 20);
        EXPECT_EQ(result.remaining_quantity, 0);
    });

    EXPECT_FALSE(adapter.analytics_healthy());

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(book.best_ask()->quantity, 30);

    EXPECT_FALSE(
        engine.trade_history().get().empty()
    );
}

TEST(
    OFIAnalyticsFailureTest,
    ConcurrentInvalidOrderStillThrowsOriginalException
) {
    OrderBook book;
    ConcurrentMatchingEngine engine(book);

    cmarket::OrderFlowImbalance ofi(10);

    cmarket::OFIOrderBookHandler handler(
        ofi,
        [] {
            throw std::runtime_error(
                "Simulated OFI failure"
            );
        }
    );

    cmarket::OFIConcurrentMatchingAdapter adapter(
        engine,
        book,
        handler,
        "AAPL"
    );

    EXPECT_THROW(
        static_cast<void>(
            adapter.place_limit_buy(100, 0)
        ),
        std::invalid_argument
    );

    EXPECT_TRUE(adapter.analytics_healthy());
    EXPECT_TRUE(book.empty());
}

} // namespace

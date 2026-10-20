
#include "ofi_matching_engine_adapter.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace {

class OFIMatchingEngineAdapterTest
    : public ::testing::Test {
protected:
    OrderBook book;
    MatchingEngine engine{book};

    cmarket::OrderFlowImbalance ofi{10};
    cmarket::OFIOrderBookHandler handler{ofi};

    cmarket::OFIMatchingEngineAdapter adapter{
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
    OFIMatchingEngineAdapterTest,
    InitialStateIsBaseline
) {
    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
    EXPECT_FALSE(ofi.latest_ofi("AAPL").has_value());
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    LimitBuyUpdatesOFI
) {
    const auto id = adapter.place_limit_buy(100, 50);
    EXPECT_GT(id, 0U);

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 50.0L);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->quantity, 50);
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    LimitSellUpdatesOFI
) {
    const auto id = adapter.place_limit_sell(110, 40);
    EXPECT_GT(id, 0U);

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -40.0L);
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    CancelBuyUpdatesOFI
) {
    const auto id = adapter.place_limit_buy(100, 50);

    ASSERT_TRUE(adapter.cancel_order(id));

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -50.0L);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 0.0L);
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    CancelSellUpdatesOFI
) {
    const auto id = adapter.place_limit_sell(110, 40);

    ASSERT_TRUE(adapter.cancel_order(id));

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 40.0L);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 0.0L);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    ModifyQuantityUpdatesOFI
) {
    const auto id = adapter.place_limit_buy(100, 50);

    ASSERT_TRUE(adapter.modify_order(id, 100, 70));

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 20.0L);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 70.0L);
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    ModifyPriceUpdatesOFI
) {
    const auto id = adapter.place_limit_buy(100, 50);

    ASSERT_TRUE(adapter.modify_order(id, 105, 30));

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 30.0L);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->price_ticks, 105);
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    MarketBuyUpdatesOFI
) {
    const auto id = adapter.place_limit_sell(110, 40);
    EXPECT_GT(id, 0U);

    const auto result =
        adapter.execute_market_buy(20);

    EXPECT_EQ(result.executed_quantity, 20);
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 20.0L);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(book.best_ask()->quantity, 20);
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    MarketSellUpdatesOFI
) {
    const auto id = adapter.place_limit_buy(100, 50);
    EXPECT_GT(id, 0U);

    const auto result =
        adapter.execute_market_sell(30);

    EXPECT_EQ(result.executed_quantity, 30);
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -30.0L);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->quantity, 20);
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    PartialFillUsesExecutedQuantity
) {
    const auto id = adapter.place_limit_sell(110, 25);
    EXPECT_GT(id, 0U);

    const auto result =
        adapter.execute_market_buy(60);

    EXPECT_EQ(result.executed_quantity, 25);
    EXPECT_EQ(result.remaining_quantity, 35);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 25.0L);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    UnknownCancelDoesNotRecordEvent
) {
    EXPECT_FALSE(adapter.cancel_order(99999));

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
    EXPECT_FALSE(ofi.latest_ofi("AAPL").has_value());
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    InvalidOrderDoesNotRecordEvent
) {
    EXPECT_THROW(
        static_cast<void>(
            adapter.place_limit_buy(100, 0)
        ),
        std::invalid_argument
    );

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
    EXPECT_TRUE(engine.trade_history().empty());
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    UnfilledMarketOrderDoesNotRecordEvent
) {
    const auto result =
        adapter.execute_market_buy(10);

    EXPECT_TRUE(result.unfilled());
    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
}

TEST_F(
    OFIMatchingEngineAdapterTest,
    MultipleOperationsAccumulateOFI
) {
    const auto buy_id =
        adapter.place_limit_buy(100, 50);

    const auto sell_id =
        adapter.place_limit_sell(110, 40);

    EXPECT_GT(buy_id, 0U);
    EXPECT_GT(sell_id, 0U);

    const auto result =
        adapter.execute_market_buy(10);

    EXPECT_EQ(result.executed_quantity, 10);

    EXPECT_EQ(ofi.event_count("AAPL"), 3U);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 10.0L);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 20.0L);
}

} // namespace

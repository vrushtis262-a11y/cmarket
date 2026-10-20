
#include "ofi_observable_order_book.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class OFIObservableOrderBookTest : public ::testing::Test {
protected:
    OrderBook book;
    cmarket::OrderFlowImbalance ofi{10};
    cmarket::OFIOrderBookHandler handler{ofi};
    cmarket::OFIObservableOrderBook observable{
        book, handler, "AAPL"
    };

    void initialize_book() {
        book.update_bid(100, 50);
        book.update_ask(110, 40);
        observable.initialize();
    }
};

TEST_F(OFIObservableOrderBookTest, InitializationSetsBaseline) {
    initialize_book();

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
    EXPECT_FALSE(ofi.latest_ofi("AAPL").has_value());
}

TEST_F(OFIObservableOrderBookTest, BidUpdateAutomaticallyUpdatesOFI) {
    initialize_book();

    observable.update_bid(100, 70);

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 20.0L);
}

TEST_F(OFIObservableOrderBookTest, AskUpdateAutomaticallyUpdatesOFI) {
    initialize_book();

    observable.update_ask(110, 60);

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -20.0L);
}

TEST_F(OFIObservableOrderBookTest, BidAdjustmentUpdatesOFI) {
    initialize_book();

    observable.adjust_bid(100, -15);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -15.0L);
}

TEST_F(OFIObservableOrderBookTest, AskAdjustmentUpdatesOFI) {
    initialize_book();

    observable.adjust_ask(110, -10);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 10.0L);
}

TEST_F(OFIObservableOrderBookTest, RemovingBidLevelUpdatesOFI) {
    initialize_book();

    observable.update_bid(100, 0);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -50.0L);
}

TEST_F(OFIObservableOrderBookTest, RemovingAskLevelUpdatesOFI) {
    initialize_book();

    observable.update_ask(110, 0);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 40.0L);
}

TEST_F(OFIObservableOrderBookTest, SnapshotReplacementUpdatesOFI) {
    initialize_book();

    observable.replace_snapshot(
        {PriceLevel{100, 70}},
        {PriceLevel{110, 45}}
    );

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 15.0L);
}

TEST_F(OFIObservableOrderBookTest, InvalidBidDoesNotUpdateOFI) {
    initialize_book();

    EXPECT_THROW(
        observable.update_bid(-1, 10),
        std::invalid_argument
    );

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
    EXPECT_FALSE(ofi.latest_ofi("AAPL").has_value());
}

TEST_F(OFIObservableOrderBookTest, InvalidAdjustmentDoesNotUpdateOFI) {
    initialize_book();

    EXPECT_THROW(
        observable.adjust_bid(100, -100),
        std::invalid_argument
    );

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
}

TEST_F(OFIObservableOrderBookTest, RejectsEmptySymbol) {
    EXPECT_THROW(
        cmarket::OFIObservableOrderBook(
            book, handler, ""
        ),
        std::invalid_argument
    );
}

TEST_F(OFIObservableOrderBookTest, MultipleUpdatesAccumulate) {
    initialize_book();

    observable.update_bid(100, 70);
    observable.update_ask(110, 50);
    observable.adjust_bid(100, -5);

    EXPECT_EQ(ofi.event_count("AAPL"), 3U);
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -5.0L);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 5.0L);
}

} // namespace

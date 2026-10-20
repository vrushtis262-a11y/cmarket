
#include "ofi_order_book_handler.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using cmarket::OFIOrderBookHandler;
using cmarket::OrderFlowImbalance;

namespace {

class OFIOrderBookHandlerTest : public ::testing::Test {
protected:
    OrderBook book;
    OrderFlowImbalance ofi{10};
    OFIOrderBookHandler handler{ofi};
};

TEST_F(OFIOrderBookHandlerTest, InitialSnapshotIsBaseline) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
    EXPECT_FALSE(ofi.latest_ofi("AAPL").has_value());
}

TEST_F(OFIOrderBookHandlerTest, IncreasingBidQuantity) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    book.update_bid(100, 70);
    handler.capture("AAPL", book);

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 20.0L);
}

TEST_F(OFIOrderBookHandlerTest, IncreasingAskQuantity) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    book.update_ask(110, 60);
    handler.capture("AAPL", book);

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -20.0L);
}

TEST_F(OFIOrderBookHandlerTest, RemovingBidLiquidity) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    book.adjust_bid(100, -20);
    handler.capture("AAPL", book);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -20.0L);
}

TEST_F(OFIOrderBookHandlerTest, RemovingAskLiquidity) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    book.adjust_ask(110, -15);
    handler.capture("AAPL", book);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 15.0L);
}

TEST_F(OFIOrderBookHandlerTest, BidPriceImprovement) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    book.update_bid(105, 30);
    handler.capture("AAPL", book);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 30.0L);
}

TEST_F(OFIOrderBookHandlerTest, AskPriceImprovement) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    book.update_ask(105, 25);
    handler.capture("AAPL", book);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -25.0L);
}

TEST_F(OFIOrderBookHandlerTest, RemovingEntireBidLevel) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    book.update_bid(100, 0);
    handler.capture("AAPL", book);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -50.0L);
}

TEST_F(OFIOrderBookHandlerTest, RemovingEntireAskLevel) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    book.update_ask(110, 0);
    handler.capture("AAPL", book);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 40.0L);
}

TEST_F(OFIOrderBookHandlerTest, EmptyBookToPopulatedBook) {
    handler.capture("AAPL", book);

    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 10.0L);
}

TEST_F(OFIOrderBookHandlerTest, MultipleSymbols) {
    OrderBook aapl_book;
    OrderBook msft_book;

    aapl_book.update_bid(100, 50);
    aapl_book.update_ask(110, 40);

    msft_book.update_bid(200, 60);
    msft_book.update_ask(220, 50);

    handler.capture("AAPL", aapl_book);
    handler.capture("MSFT", msft_book);

    aapl_book.update_bid(100, 70);
    msft_book.update_ask(220, 70);

    handler.capture("AAPL", aapl_book);
    handler.capture("MSFT", msft_book);

    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 20.0L);
    EXPECT_EQ(*ofi.rolling_ofi("MSFT"), -20.0L);
}

TEST_F(OFIOrderBookHandlerTest, RejectsEmptySymbol) {
    EXPECT_THROW(
        handler.capture("", book),
        std::invalid_argument
    );
}

TEST_F(OFIOrderBookHandlerTest, UnchangedBookProducesZeroOFI) {
    book.update_bid(100, 50);
    book.update_ask(110, 40);

    handler.capture("AAPL", book);
    handler.capture("AAPL", book);

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 0.0L);
}

} // namespace

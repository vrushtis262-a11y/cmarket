#include "order_book.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

PriceLevel level(
    const std::string& price,
    const std::string& quantity
)
{
    return PriceLevel{
        OrderBook::price_to_ticks(price),
        OrderBook::price_to_ticks(quantity)
    };
}

} // namespace

TEST(OrderBookTest, SortsUnsortedBidAndAskInput)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.510", "10"),
            level("0.530", "20"),
            level("0.520", "30")
        },
        {
            level("0.560", "10"),
            level("0.540", "20"),
            level("0.550", "30")
        }
    );

    ASSERT_EQ(book.bids().size(), 3U);
    ASSERT_EQ(book.asks().size(), 3U);

    EXPECT_EQ(
        book.bids()[0].price_ticks,
        OrderBook::price_to_ticks("0.530")
    );

    EXPECT_EQ(
        book.bids()[1].price_ticks,
        OrderBook::price_to_ticks("0.520")
    );

    EXPECT_EQ(
        book.bids()[2].price_ticks,
        OrderBook::price_to_ticks("0.510")
    );

    EXPECT_EQ(
        book.asks()[0].price_ticks,
        OrderBook::price_to_ticks("0.540")
    );

    EXPECT_EQ(
        book.asks()[1].price_ticks,
        OrderBook::price_to_ticks("0.550")
    );

    EXPECT_EQ(
        book.asks()[2].price_ticks,
        OrderBook::price_to_ticks("0.560")
    );
}

TEST(OrderBookTest, CalculatesBestBidAndBestAsk)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.500", "10"),
            level("0.532", "20"),
            level("0.520", "30")
        },
        {
            level("0.550", "10"),
            level("0.538", "20"),
            level("0.545", "30")
        }
    );

    const auto best_bid = book.best_bid();
    const auto best_ask = book.best_ask();

    ASSERT_TRUE(best_bid.has_value());
    ASSERT_TRUE(best_ask.has_value());

    EXPECT_EQ(
        best_bid->price_ticks,
        OrderBook::price_to_ticks("0.532")
    );

    EXPECT_EQ(
        best_ask->price_ticks,
        OrderBook::price_to_ticks("0.538")
    );
}

TEST(OrderBookTest, CalculatesSpread)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.532", "100")
        },
        {
            level("0.538", "100")
        }
    );

    const auto spread = book.spread_ticks();

    ASSERT_TRUE(spread.has_value());

    EXPECT_EQ(
        spread.value(),
        OrderBook::price_to_ticks("0.006")
    );
}

TEST(OrderBookTest, CalculatesMidPrice)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.532", "100")
        },
        {
            level("0.538", "100")
        }
    );

    const auto mid = book.mid_price_ticks();

    ASSERT_TRUE(mid.has_value());

    EXPECT_EQ(
        mid.value(),
        OrderBook::price_to_ticks("0.535")
    );
}

TEST(OrderBookTest, HandlesEmptyBook)
{
    OrderBook book;

    EXPECT_TRUE(book.empty());
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.spread_ticks().has_value());
    EXPECT_FALSE(book.mid_price_ticks().has_value());
    EXPECT_EQ(book.bid_depth(), 0);
    EXPECT_EQ(book.ask_depth(), 0);
    EXPECT_EQ(book.total_depth(), 0);
}

TEST(OrderBookTest, HandlesEmptyBidSide)
{
    OrderBook book;

    book.replace_snapshot(
        {},
        {
            level("0.538", "100")
        }
    );

    EXPECT_FALSE(book.empty());
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_TRUE(book.best_ask().has_value());
    EXPECT_FALSE(book.spread_ticks().has_value());
    EXPECT_FALSE(book.mid_price_ticks().has_value());
}

TEST(OrderBookTest, HandlesEmptyAskSide)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.532", "100")
        },
        {}
    );

    EXPECT_FALSE(book.empty());
    EXPECT_TRUE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.spread_ticks().has_value());
    EXPECT_FALSE(book.mid_price_ticks().has_value());
}

TEST(OrderBookTest, AggregatesDuplicatePriceLevels)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.532", "10"),
            level("0.532", "15"),
            level("0.520", "20")
        },
        {
            level("0.538", "12"),
            level("0.538", "8"),
            level("0.550", "25")
        }
    );

    ASSERT_EQ(book.bids().size(), 2U);
    ASSERT_EQ(book.asks().size(), 2U);

    EXPECT_EQ(
        book.bids()[0].price_ticks,
        OrderBook::price_to_ticks("0.532")
    );

    EXPECT_EQ(
        book.bids()[0].quantity,
        OrderBook::price_to_ticks("25")
    );

    EXPECT_EQ(
        book.asks()[0].price_ticks,
        OrderBook::price_to_ticks("0.538")
    );

    EXPECT_EQ(
        book.asks()[0].quantity,
        OrderBook::price_to_ticks("20")
    );
}

TEST(OrderBookTest, ReplacesPreviousSnapshot)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.500", "10")
        },
        {
            level("0.600", "10")
        }
    );

    book.replace_snapshot(
        {
            level("0.532", "20")
        },
        {
            level("0.538", "30")
        }
    );

    ASSERT_EQ(book.bids().size(), 1U);
    ASSERT_EQ(book.asks().size(), 1U);

    EXPECT_EQ(
        book.best_bid()->price_ticks,
        OrderBook::price_to_ticks("0.532")
    );

    EXPECT_EQ(
        book.best_ask()->price_ticks,
        OrderBook::price_to_ticks("0.538")
    );
}
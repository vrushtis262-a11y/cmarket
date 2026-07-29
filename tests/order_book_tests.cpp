#include "order_book.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

PriceLevel level(
    const std::string& price,
    const std::string& quantity
)
{
    return PriceLevel{
        OrderBook::price_to_ticks(price),
        OrderBook::quantity_to_fixed(quantity)
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
    EXPECT_FALSE(book.order_book_imbalance().has_value());
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

    const auto imbalance = book.order_book_imbalance();

    ASSERT_TRUE(imbalance.has_value());
    EXPECT_DOUBLE_EQ(imbalance.value(), -1.0);
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

    const auto imbalance = book.order_book_imbalance();

    ASSERT_TRUE(imbalance.has_value());
    EXPECT_DOUBLE_EQ(imbalance.value(), 1.0);
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
        OrderBook::quantity_to_fixed("25")
    );

    EXPECT_EQ(
        book.asks()[0].price_ticks,
        OrderBook::price_to_ticks("0.538")
    );

    EXPECT_EQ(
        book.asks()[0].quantity,
        OrderBook::quantity_to_fixed("20")
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

TEST(OrderBookTest, ParsesQuantitySeparatelyFromPrice)
{
    EXPECT_EQ(
        OrderBook::quantity_to_fixed("25"),
        25 * OrderBook::ticks_per_unit
    );

    EXPECT_EQ(
        OrderBook::quantity_to_fixed("12.345678"),
        12'345'678
    );
}

TEST(OrderBookTest, RejectsInvalidQuantity)
{
    EXPECT_THROW(
        static_cast<void>(
            OrderBook::quantity_to_fixed("")
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            OrderBook::quantity_to_fixed("-1")
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            OrderBook::quantity_to_fixed("1.1234567")
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            OrderBook::quantity_to_fixed("abc")
        ),
        std::invalid_argument
    );
}

TEST(OrderBookTest, InsertsIncrementalBidInDescendingOrder)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "10"),
            level("0.510", "20")
        },
        {}
    );

    book.update_bid(
        OrderBook::price_to_ticks("0.520"),
        OrderBook::quantity_to_fixed("15")
    );

    ASSERT_EQ(book.bids().size(), 3U);

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
}

TEST(OrderBookTest, InsertsIncrementalAskInAscendingOrder)
{
    OrderBook book;

    book.replace_snapshot(
        {},
        {
            level("0.540", "10"),
            level("0.560", "20")
        }
    );

    book.update_ask(
        OrderBook::price_to_ticks("0.550"),
        OrderBook::quantity_to_fixed("15")
    );

    ASSERT_EQ(book.asks().size(), 3U);

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

TEST(OrderBookTest, ReplacesExistingIncrementalQuantities)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "10")
        },
        {
            level("0.540", "20")
        }
    );

    book.update_bid(
        OrderBook::price_to_ticks("0.530"),
        OrderBook::quantity_to_fixed("25")
    );

    book.update_ask(
        OrderBook::price_to_ticks("0.540"),
        OrderBook::quantity_to_fixed("35")
    );

    ASSERT_EQ(book.bids().size(), 1U);
    ASSERT_EQ(book.asks().size(), 1U);

    EXPECT_EQ(
        book.bids()[0].quantity,
        OrderBook::quantity_to_fixed("25")
    );

    EXPECT_EQ(
        book.asks()[0].quantity,
        OrderBook::quantity_to_fixed("35")
    );
}

TEST(OrderBookTest, RemovesIncrementalLevelsWithZeroQuantity)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "10"),
            level("0.520", "20")
        },
        {
            level("0.540", "30"),
            level("0.550", "40")
        }
    );

    book.update_bid(
        OrderBook::price_to_ticks("0.530"),
        0
    );

    book.update_ask(
        OrderBook::price_to_ticks("0.540"),
        0
    );

    ASSERT_EQ(book.bids().size(), 1U);
    ASSERT_EQ(book.asks().size(), 1U);

    EXPECT_EQ(
        book.best_bid()->price_ticks,
        OrderBook::price_to_ticks("0.520")
    );

    EXPECT_EQ(
        book.best_ask()->price_ticks,
        OrderBook::price_to_ticks("0.550")
    );
}

TEST(OrderBookTest, IgnoresZeroQuantityForMissingLevels)
{
    OrderBook book;

    book.update_bid(
        OrderBook::price_to_ticks("0.500"),
        0
    );

    book.update_ask(
        OrderBook::price_to_ticks("0.600"),
        0
    );

    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, RejectsNegativeIncrementalUpdates)
{
    OrderBook book;

    EXPECT_THROW(
        book.update_bid(-1, 1),
        std::invalid_argument
    );

    EXPECT_THROW(
        book.update_bid(1, -1),
        std::invalid_argument
    );

    EXPECT_THROW(
        book.update_ask(-1, 1),
        std::invalid_argument
    );

    EXPECT_THROW(
        book.update_ask(1, -1),
        std::invalid_argument
    );
}

TEST(OrderBookTest, CalculatesBalancedOrderBookImbalance)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "40"),
            level("0.520", "60")
        },
        {
            level("0.540", "25"),
            level("0.550", "75")
        }
    );

    const auto imbalance = book.order_book_imbalance();

    ASSERT_TRUE(imbalance.has_value());
    EXPECT_DOUBLE_EQ(imbalance.value(), 0.0);
}

TEST(OrderBookTest, CalculatesPositiveOrderBookImbalance)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "120"),
            level("0.520", "80")
        },
        {
            level("0.540", "25"),
            level("0.550", "75")
        }
    );

    const auto imbalance = book.order_book_imbalance();

    ASSERT_TRUE(imbalance.has_value());
    EXPECT_NEAR(
        imbalance.value(),
        1.0 / 3.0,
        1e-12
    );
}

TEST(OrderBookTest, CalculatesNegativeOrderBookImbalance)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "25"),
            level("0.520", "75")
        },
        {
            level("0.540", "120"),
            level("0.550", "80")
        }
    );

    const auto imbalance = book.order_book_imbalance();

    ASSERT_TRUE(imbalance.has_value());
    EXPECT_NEAR(
        imbalance.value(),
        -1.0 / 3.0,
        1e-12
    );
}

TEST(OrderBookTest, ReturnsNoVwapForEmptySides)
{
    OrderBook book;

    EXPECT_FALSE(book.bid_vwap_ticks().has_value());
    EXPECT_FALSE(book.ask_vwap_ticks().has_value());
}

TEST(OrderBookTest, CalculatesBidVwap)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.500", "100"),
            level("0.600", "300")
        },
        {}
    );

    const auto vwap = book.bid_vwap_ticks();

    ASSERT_TRUE(vwap.has_value());

    EXPECT_EQ(
        vwap.value(),
        OrderBook::price_to_ticks("0.575")
    );
}

TEST(OrderBookTest, CalculatesAskVwap)
{
    OrderBook book;

    book.replace_snapshot(
        {},
        {
            level("0.600", "100"),
            level("0.700", "300")
        }
    );

    const auto vwap = book.ask_vwap_ticks();

    ASSERT_TRUE(vwap.has_value());

    EXPECT_EQ(
        vwap.value(),
        OrderBook::price_to_ticks("0.675")
    );
}

TEST(OrderBookTest, CalculatesVwapAfterIncrementalUpdates)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.500", "100"),
            level("0.600", "100")
        },
        {
            level("0.700", "100"),
            level("0.800", "100")
        }
    );

    book.update_bid(
        OrderBook::price_to_ticks("0.600"),
        OrderBook::quantity_to_fixed("300")
    );

    book.update_ask(
        OrderBook::price_to_ticks("0.700"),
        OrderBook::quantity_to_fixed("300")
    );

    const auto bid_vwap = book.bid_vwap_ticks();
    const auto ask_vwap = book.ask_vwap_ticks();

    ASSERT_TRUE(bid_vwap.has_value());
    ASSERT_TRUE(ask_vwap.has_value());

    EXPECT_EQ(
        bid_vwap.value(),
        OrderBook::price_to_ticks("0.575")
    );

    EXPECT_EQ(
        ask_vwap.value(),
        OrderBook::price_to_ticks("0.725")
    );
}

TEST(OrderBookTest, ReturnsNoMicropriceForIncompleteBook)
{
    OrderBook empty_book;

    EXPECT_FALSE(
        empty_book.microprice_ticks().has_value()
    );

    OrderBook bid_only_book;

    bid_only_book.replace_snapshot(
        {
            level("0.530", "100")
        },
        {}
    );

    EXPECT_FALSE(
        bid_only_book.microprice_ticks().has_value()
    );

    OrderBook ask_only_book;

    ask_only_book.replace_snapshot(
        {},
        {
            level("0.540", "100")
        }
    );

    EXPECT_FALSE(
        ask_only_book.microprice_ticks().has_value()
    );
}

TEST(OrderBookTest, CalculatesBalancedMicroprice)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "100")
        },
        {
            level("0.540", "100")
        }
    );

    const auto microprice = book.microprice_ticks();

    ASSERT_TRUE(microprice.has_value());

    EXPECT_EQ(
        microprice.value(),
        OrderBook::price_to_ticks("0.535")
    );
}

TEST(OrderBookTest, WeightsMicropriceTowardAskWithLargerBidQuantity)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "300")
        },
        {
            level("0.540", "100")
        }
    );

    const auto microprice = book.microprice_ticks();

    ASSERT_TRUE(microprice.has_value());

    EXPECT_EQ(
        microprice.value(),
        OrderBook::price_to_ticks("0.5375")
    );
}

TEST(OrderBookTest, WeightsMicropriceTowardBidWithLargerAskQuantity)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "100")
        },
        {
            level("0.540", "300")
        }
    );

    const auto microprice = book.microprice_ticks();

    ASSERT_TRUE(microprice.has_value());

    EXPECT_EQ(
        microprice.value(),
        OrderBook::price_to_ticks("0.5325")
    );
}

TEST(OrderBookTest, RecalculatesMicropriceAfterIncrementalUpdate)
{
    OrderBook book;

    book.replace_snapshot(
        {
            level("0.530", "100")
        },
        {
            level("0.540", "100")
        }
    );

    book.update_bid(
        OrderBook::price_to_ticks("0.530"),
        OrderBook::quantity_to_fixed("300")
    );

    const auto microprice = book.microprice_ticks();

    ASSERT_TRUE(microprice.has_value());

    EXPECT_EQ(
        microprice.value(),
        OrderBook::price_to_ticks("0.5375")
    );
}

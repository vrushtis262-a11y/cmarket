#include "order_book.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void expect_level(
    const PriceLevel& level,
    std::int64_t expected_price,
    std::int64_t expected_quantity
)
{
    EXPECT_EQ(
        level.price_ticks,
        expected_price
    );

    EXPECT_EQ(
        level.quantity,
        expected_quantity
    );
}

} // namespace

TEST(
    OrderBookExceptionSafetyTest,
    InvalidBidSnapshotPreservesExistingState
)
{
    OrderBook book;

    book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 100
            }
        },
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 200
            }
        }
    );

    EXPECT_THROW(
        book.replace_snapshot(
            {
                PriceLevel{
                    .price_ticks = -1,
                    .quantity = 50
                }
            },
            {
                PriceLevel{
                    .price_ticks = 550'000,
                    .quantity = 300
                }
            }
        ),
        std::invalid_argument
    );

    ASSERT_EQ(
        book.bids().size(),
        1U
    );

    ASSERT_EQ(
        book.asks().size(),
        1U
    );

    expect_level(
        book.bids()[0],
        520'000,
        100
    );

    expect_level(
        book.asks()[0],
        540'000,
        200
    );
}

TEST(
    OrderBookExceptionSafetyTest,
    InvalidAskSnapshotPreservesExistingState
)
{
    OrderBook book;

    book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 100
            }
        },
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 200
            }
        }
    );

    EXPECT_THROW(
        book.replace_snapshot(
            {
                PriceLevel{
                    .price_ticks = 510'000,
                    .quantity = 50
                }
            },
            {
                PriceLevel{
                    .price_ticks = 550'000,
                    .quantity = -1
                }
            }
        ),
        std::invalid_argument
    );

    ASSERT_EQ(
        book.bids().size(),
        1U
    );

    ASSERT_EQ(
        book.asks().size(),
        1U
    );

    expect_level(
        book.bids()[0],
        520'000,
        100
    );

    expect_level(
        book.asks()[0],
        540'000,
        200
    );
}

TEST(
    OrderBookExceptionSafetyTest,
    BidAggregationOverflowPreservesExistingState
)
{
    OrderBook book;

    book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 100
            }
        },
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 200
            }
        }
    );

    const std::int64_t maximum =
        std::numeric_limits<std::int64_t>::max();

    EXPECT_THROW(
        book.replace_snapshot(
            {
                PriceLevel{
                    .price_ticks = 530'000,
                    .quantity = maximum
                },
                PriceLevel{
                    .price_ticks = 530'000,
                    .quantity = 1
                }
            },
            {
                PriceLevel{
                    .price_ticks = 550'000,
                    .quantity = 300
                }
            }
        ),
        std::overflow_error
    );

    ASSERT_EQ(
        book.bids().size(),
        1U
    );

    ASSERT_EQ(
        book.asks().size(),
        1U
    );

    expect_level(
        book.bids()[0],
        520'000,
        100
    );

    expect_level(
        book.asks()[0],
        540'000,
        200
    );
}

TEST(
    OrderBookExceptionSafetyTest,
    AskAggregationOverflowPreservesExistingState
)
{
    OrderBook book;

    book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 100
            }
        },
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 200
            }
        }
    );

    const std::int64_t maximum =
        std::numeric_limits<std::int64_t>::max();

    EXPECT_THROW(
        book.replace_snapshot(
            {
                PriceLevel{
                    .price_ticks = 510'000,
                    .quantity = 300
                }
            },
            {
                PriceLevel{
                    .price_ticks = 550'000,
                    .quantity = maximum
                },
                PriceLevel{
                    .price_ticks = 550'000,
                    .quantity = 1
                }
            }
        ),
        std::overflow_error
    );

    ASSERT_EQ(
        book.bids().size(),
        1U
    );

    ASSERT_EQ(
        book.asks().size(),
        1U
    );

    expect_level(
        book.bids()[0],
        520'000,
        100
    );

    expect_level(
        book.asks()[0],
        540'000,
        200
    );
}

TEST(
    OrderBookExceptionSafetyTest,
    ValidSnapshotStillWorksAfterRepeatedFailures
)
{
    OrderBook book;

    book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 100
            }
        },
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 200
            }
        }
    );

    EXPECT_THROW(
        book.replace_snapshot(
            {
                PriceLevel{
                    .price_ticks = -1,
                    .quantity = 10
                }
            },
            {}
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        book.replace_snapshot(
            {},
            {
                PriceLevel{
                    .price_ticks = 550'000,
                    .quantity = -1
                }
            }
        ),
        std::invalid_argument
    );

    const std::int64_t maximum =
        std::numeric_limits<std::int64_t>::max();

    EXPECT_THROW(
        book.replace_snapshot(
            {
                PriceLevel{
                    .price_ticks = 530'000,
                    .quantity = maximum
                },
                PriceLevel{
                    .price_ticks = 530'000,
                    .quantity = 1
                }
            },
            {}
        ),
        std::overflow_error
    );

    book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 525'000,
                .quantity = 75
            }
        },
        {
            PriceLevel{
                .price_ticks = 535'000,
                .quantity = 125
            }
        }
    );

    ASSERT_EQ(
        book.bids().size(),
        1U
    );

    ASSERT_EQ(
        book.asks().size(),
        1U
    );

    expect_level(
        book.bids()[0],
        525'000,
        75
    );

    expect_level(
        book.asks()[0],
        535'000,
        125
    );
}
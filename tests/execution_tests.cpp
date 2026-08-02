#include "execution.hpp"

#include <gtest/gtest.h>

#include <optional>

TEST(ExecutionResultTest, ReportsFullyFilledExecution)
{
    const ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = 100,
        .executed_quantity = 100,
        .remaining_quantity = 0,
        .average_price_ticks = 525'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 525'000,
                .quantity = 100
            }
        }
    };

    EXPECT_TRUE(result.fully_filled());
    EXPECT_FALSE(result.partially_filled());
    EXPECT_FALSE(result.unfilled());
}

TEST(ExecutionResultTest, ReportsPartiallyFilledExecution)
{
    const ExecutionResult result{
        .side = OrderSide::Sell,
        .requested_quantity = 100,
        .executed_quantity = 60,
        .remaining_quantity = 40,
        .average_price_ticks = 520'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Sell,
                .price_ticks = 520'000,
                .quantity = 60
            }
        }
    };

    EXPECT_FALSE(result.fully_filled());
    EXPECT_TRUE(result.partially_filled());
    EXPECT_FALSE(result.unfilled());
}

TEST(ExecutionResultTest, ReportsUnfilledExecution)
{
    const ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = 100,
        .executed_quantity = 0,
        .remaining_quantity = 100,
        .average_price_ticks = std::nullopt,
        .trades = {}
    };

    EXPECT_FALSE(result.fully_filled());
    EXPECT_FALSE(result.partially_filled());
    EXPECT_TRUE(result.unfilled());
    EXPECT_FALSE(result.average_price_ticks.has_value());
    EXPECT_TRUE(result.trades.empty());
}

TEST(ExecutionResultTest, PreservesMultipleTrades)
{
    const ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = 100,
        .executed_quantity = 100,
        .remaining_quantity = 0,
        .average_price_ticks = 526'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 525'000,
                .quantity = 40
            },
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 526'667,
                .quantity = 60
            }
        }
    };

    ASSERT_EQ(result.trades.size(), 2U);

    EXPECT_EQ(result.trades[0].price_ticks, 525'000);
    EXPECT_EQ(result.trades[0].quantity, 40);
    EXPECT_EQ(result.trades[1].price_ticks, 526'667);
    EXPECT_EQ(result.trades[1].quantity, 60);
}
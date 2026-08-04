#include "limit_order.hpp"

#include <gtest/gtest.h>

TEST(LimitOrderTest, StoresOrderMetadata)
{
    const LimitOrder order{
        .order_id = 1,
        .side = OrderSide::Buy,
        .price_ticks = 525'000,
        .original_quantity = 100,
        .remaining_quantity = 100,
        .sequence_number = 10
    };

    EXPECT_EQ(order.order_id, 1U);
    EXPECT_EQ(order.side, OrderSide::Buy);
    EXPECT_EQ(order.price_ticks, 525'000);
    EXPECT_EQ(order.original_quantity, 100);
    EXPECT_EQ(order.remaining_quantity, 100);
    EXPECT_EQ(order.sequence_number, 10U);
}

TEST(LimitOrderTest, ReportsUnfilledOrder)
{
    const LimitOrder order{
        .order_id = 2,
        .side = OrderSide::Sell,
        .price_ticks = 540'000,
        .original_quantity = 80,
        .remaining_quantity = 80,
        .sequence_number = 11
    };

    EXPECT_EQ(order.filled_quantity(), 0);
    EXPECT_FALSE(order.is_filled());
    EXPECT_FALSE(order.is_partially_filled());
}

TEST(LimitOrderTest, ReportsPartiallyFilledOrder)
{
    const LimitOrder order{
        .order_id = 3,
        .side = OrderSide::Buy,
        .price_ticks = 530'000,
        .original_quantity = 100,
        .remaining_quantity = 40,
        .sequence_number = 12
    };

    EXPECT_EQ(order.filled_quantity(), 60);
    EXPECT_FALSE(order.is_filled());
    EXPECT_TRUE(order.is_partially_filled());
}

TEST(LimitOrderTest, ReportsFullyFilledOrder)
{
    const LimitOrder order{
        .order_id = 4,
        .side = OrderSide::Sell,
        .price_ticks = 515'000,
        .original_quantity = 75,
        .remaining_quantity = 0,
        .sequence_number = 13
    };

    EXPECT_EQ(order.filled_quantity(), 75);
    EXPECT_TRUE(order.is_filled());
    EXPECT_FALSE(order.is_partially_filled());
}
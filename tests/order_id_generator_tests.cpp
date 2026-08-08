#include "order_id_generator.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

TEST(OrderIdGeneratorTest, StartsAtOneByDefault)
{
    OrderIdGenerator generator;

    EXPECT_EQ(generator.peek(), 1U);
    EXPECT_EQ(generator.next(), 1U);
    EXPECT_EQ(generator.peek(), 2U);
}

TEST(OrderIdGeneratorTest, GeneratesSequentialOrderIds)
{
    OrderIdGenerator generator;

    EXPECT_EQ(generator.next(), 1U);
    EXPECT_EQ(generator.next(), 2U);
    EXPECT_EQ(generator.next(), 3U);
    EXPECT_EQ(generator.peek(), 4U);
}

TEST(OrderIdGeneratorTest, SupportsCustomStartingOrderId)
{
    OrderIdGenerator generator(100);

    EXPECT_EQ(generator.peek(), 100U);
    EXPECT_EQ(generator.next(), 100U);
    EXPECT_EQ(generator.next(), 101U);
    EXPECT_EQ(generator.peek(), 102U);
}

TEST(OrderIdGeneratorTest, RejectsZeroStartingOrderId)
{
    EXPECT_THROW(
        OrderIdGenerator generator(0),
        std::invalid_argument
    );
}

TEST(OrderIdGeneratorTest, ThrowsWhenOrderIdsAreExhausted)
{
    const OrderId maximum_order_id =
        std::numeric_limits<OrderId>::max();

    OrderIdGenerator generator(maximum_order_id);

    EXPECT_THROW(
        static_cast<void>(
            generator.next()
        ),
        std::overflow_error
    );
}
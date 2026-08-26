#include "order_manager.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

LimitOrder make_order(
    OrderId order_id,
    OrderSide side,
    std::int64_t price_ticks,
    std::int64_t quantity,
    SequenceNumber sequence_number
)
{
    return LimitOrder{
        .order_id = order_id,
        .side = side,
        .price_ticks = price_ticks,
        .original_quantity = quantity,
        .remaining_quantity = quantity,
        .sequence_number = sequence_number
    };
}

} // namespace

TEST(OrderManagerTest, StartsEmpty)
{
    OrderManager manager;

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, AddsOrder)
{
    OrderManager manager;

    manager.add_order(
        make_order(
            1,
            OrderSide::Buy,
            520'000,
            100,
            1
        )
    );

    const auto& orders =
        manager.orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(orders[0].order_id, 1U);
    EXPECT_EQ(orders[0].side, OrderSide::Buy);
    EXPECT_EQ(orders[0].price_ticks, 520'000);
    EXPECT_EQ(orders[0].remaining_quantity, 100);
}

TEST(OrderManagerTest, FindsExistingOrder)
{
    OrderManager manager;

    manager.add_order(
        make_order(
            10,
            OrderSide::Sell,
            525'000,
            50,
            1
        )
    );

    const LimitOrder* order =
        manager.find_order(10);

    ASSERT_NE(order, nullptr);

    EXPECT_EQ(order->order_id, 10U);
    EXPECT_EQ(order->price_ticks, 525'000);
}

TEST(OrderManagerTest, ReturnsNullForMissingOrder)
{
    OrderManager manager;

    EXPECT_EQ(
        manager.find_order(999),
        nullptr
    );
}

TEST(OrderManagerTest, CancelsExistingOrder)
{
    OrderManager manager;

    manager.add_order(
        make_order(
            1,
            OrderSide::Buy,
            520'000,
            100,
            1
        )
    );

    manager.add_order(
        make_order(
            2,
            OrderSide::Sell,
            530'000,
            100,
            2
        )
    );

    EXPECT_TRUE(
        manager.cancel_order(1)
    );

    const auto& orders =
        manager.orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        2U
    );
}

TEST(OrderManagerTest, CancelMissingOrderReturnsFalse)
{
    OrderManager manager;

    EXPECT_FALSE(
        manager.cancel_order(999)
    );
}

TEST(OrderManagerTest, MutableLookupUpdatesOwnedOrder)
{
    OrderManager manager;

    manager.add_order(
        make_order(
            1,
            OrderSide::Buy,
            520'000,
            100,
            1
        )
    );

    LimitOrder* order =
        manager.find_order(1);

    ASSERT_NE(order, nullptr);

    order->remaining_quantity = 40;

    const OrderManager& const_manager =
        manager;

    const LimitOrder* stored_order =
        const_manager.find_order(1);

    ASSERT_NE(stored_order, nullptr);

    EXPECT_EQ(
        stored_order->remaining_quantity,
        40
    );
}

TEST(OrderManagerTest, ConstLookupProvidesReadOnlyAccess)
{
    OrderManager manager;

    manager.add_order(
        make_order(
            1,
            OrderSide::Sell,
            530'000,
            75,
            1
        )
    );

    const OrderManager& const_manager =
        manager;

    const LimitOrder* order =
        const_manager.find_order(1);

    ASSERT_NE(order, nullptr);

    EXPECT_EQ(order->order_id, 1U);
    EXPECT_EQ(order->remaining_quantity, 75);
}

TEST(OrderManagerTest, RejectsZeroOrderId)
{
    OrderManager manager;

    EXPECT_THROW(
        manager.add_order(
            make_order(
                0,
                OrderSide::Buy,
                520'000,
                100,
                1
            )
        ),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsZeroPrice)
{
    OrderManager manager;

    EXPECT_THROW(
        manager.add_order(
            make_order(
                1,
                OrderSide::Buy,
                0,
                100,
                1
            )
        ),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsNegativePrice)
{
    OrderManager manager;

    EXPECT_THROW(
        manager.add_order(
            make_order(
                1,
                OrderSide::Buy,
                -1,
                100,
                1
            )
        ),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsZeroOriginalQuantity)
{
    OrderManager manager;

    LimitOrder order{
        .order_id = 1,
        .side = OrderSide::Buy,
        .price_ticks = 520'000,
        .original_quantity = 0,
        .remaining_quantity = 1,
        .sequence_number = 1
    };

    EXPECT_THROW(
        manager.add_order(order),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsNegativeOriginalQuantity)
{
    OrderManager manager;

    LimitOrder order{
        .order_id = 1,
        .side = OrderSide::Buy,
        .price_ticks = 520'000,
        .original_quantity = -1,
        .remaining_quantity = 1,
        .sequence_number = 1
    };

    EXPECT_THROW(
        manager.add_order(order),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsZeroRemainingQuantity)
{
    OrderManager manager;

    LimitOrder order{
        .order_id = 1,
        .side = OrderSide::Buy,
        .price_ticks = 520'000,
        .original_quantity = 100,
        .remaining_quantity = 0,
        .sequence_number = 1
    };

    EXPECT_THROW(
        manager.add_order(order),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsNegativeRemainingQuantity)
{
    OrderManager manager;

    LimitOrder order{
        .order_id = 1,
        .side = OrderSide::Buy,
        .price_ticks = 520'000,
        .original_quantity = 100,
        .remaining_quantity = -1,
        .sequence_number = 1
    };

    EXPECT_THROW(
        manager.add_order(order),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsRemainingQuantityGreaterThanOriginal)
{
    OrderManager manager;

    LimitOrder order{
        .order_id = 1,
        .side = OrderSide::Buy,
        .price_ticks = 520'000,
        .original_quantity = 100,
        .remaining_quantity = 101,
        .sequence_number = 1
    };

    EXPECT_THROW(
        manager.add_order(order),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsZeroSequenceNumber)
{
    OrderManager manager;

    EXPECT_THROW(
        manager.add_order(
            make_order(
                1,
                OrderSide::Buy,
                520'000,
                100,
                0
            )
        ),
        std::invalid_argument
    );

    EXPECT_TRUE(
        manager.orders().empty()
    );
}

TEST(OrderManagerTest, RejectsDuplicateOrderId)
{
    OrderManager manager;

    manager.add_order(
        make_order(
            1,
            OrderSide::Buy,
            520'000,
            100,
            1
        )
    );

    EXPECT_THROW(
        manager.add_order(
            make_order(
                1,
                OrderSide::Sell,
                530'000,
                50,
                2
            )
        ),
        std::invalid_argument
    );

    const auto& orders =
        manager.orders();

    ASSERT_EQ(
        orders.size(),
        1U
    );

    EXPECT_EQ(
        orders[0].order_id,
        1U
    );

    EXPECT_EQ(
        orders[0].side,
        OrderSide::Buy
    );

    EXPECT_EQ(
        orders[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        100
    );
}

TEST(OrderManagerTest, RejectedOrderDoesNotMutateExistingState)
{
    OrderManager manager;

    manager.add_order(
        make_order(
            1,
            OrderSide::Buy,
            520'000,
            100,
            1
        )
    );

    manager.add_order(
        make_order(
            2,
            OrderSide::Sell,
            530'000,
            50,
            2
        )
    );

    const std::vector<LimitOrder> before =
        manager.orders();

    EXPECT_THROW(
        manager.add_order(
            make_order(
                3,
                OrderSide::Buy,
                -1,
                25,
                3
            )
        ),
        std::invalid_argument
    );

    const auto& after =
        manager.orders();

    ASSERT_EQ(
        after.size(),
        before.size()
    );

    for (
        std::size_t index = 0;
        index < before.size();
        ++index
    ) {
        EXPECT_EQ(
            after[index].order_id,
            before[index].order_id
        );

        EXPECT_EQ(
            after[index].side,
            before[index].side
        );

        EXPECT_EQ(
            after[index].price_ticks,
            before[index].price_ticks
        );

        EXPECT_EQ(
            after[index].original_quantity,
            before[index].original_quantity
        );

        EXPECT_EQ(
            after[index].remaining_quantity,
            before[index].remaining_quantity
        );

        EXPECT_EQ(
            after[index].sequence_number,
            before[index].sequence_number
        );
    }
}
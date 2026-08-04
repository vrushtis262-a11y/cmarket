#ifndef CMARKET_ORDER_ID_GENERATOR_HPP
#define CMARKET_ORDER_ID_GENERATOR_HPP

#include "limit_order.hpp"

#include <limits>
#include <stdexcept>

class OrderIdGenerator {
public:
    explicit OrderIdGenerator(
        OrderId first_order_id = 1
    )
        : next_order_id_(first_order_id)
    {
        if (first_order_id == 0) {
            throw std::invalid_argument(
                "First order ID must be greater than zero."
            );
        }
    }

    [[nodiscard]]
    OrderId next()
    {
        if (
            next_order_id_ ==
            std::numeric_limits<OrderId>::max()
        ) {
            throw std::overflow_error(
                "Order ID generator exhausted."
            );
        }

        const OrderId generated_id =
            next_order_id_;

        ++next_order_id_;

        return generated_id;
    }

    [[nodiscard]]
    OrderId peek() const noexcept
    {
        return next_order_id_;
    }

private:
    OrderId next_order_id_;
};

#endif
#include "order_manager.hpp"
#include "validation.hpp"

#include <algorithm>
#include <stdexcept>

void OrderManager::validate_order(
    const LimitOrder& order
)
{
    if (order.order_id == 0) {
        throw std::invalid_argument(
            "Order ID must be non-zero."
        );
    }

    validation::require_positive(
        order.price_ticks,
        "Order price"
    );

    validation::require_positive(
        order.original_quantity,
        "Order original quantity"
    );

    validation::require_positive(
        order.remaining_quantity,
        "Active order remaining quantity"
    );

    if (
        order.remaining_quantity >
        order.original_quantity
    ) {
        throw std::invalid_argument(
            "Order remaining quantity cannot exceed "
            "original quantity."
        );
    }

    if (order.sequence_number == 0) {
        throw std::invalid_argument(
            "Order sequence number must be non-zero."
        );
    }
}

void OrderManager::add_order(
    const LimitOrder& order
)
{
    validate_order(order);

    if (find_order(order.order_id) != nullptr) {
        throw std::invalid_argument(
            "Order ID already exists."
        );
    }

    orders_.push_back(order);
}

bool OrderManager::cancel_order(
    OrderId order_id
)
{
    const auto iterator =
        std::find_if(
            orders_.begin(),
            orders_.end(),
            [order_id](
                const LimitOrder& order
            )
            {
                return order.order_id ==
                    order_id;
            }
        );

    if (iterator == orders_.end()) {
        return false;
    }

    orders_.erase(iterator);

    return true;
}

LimitOrder* OrderManager::find_order(
    OrderId order_id
) noexcept
{
    const auto iterator =
        std::find_if(
            orders_.begin(),
            orders_.end(),
            [order_id](
                const LimitOrder& order
            )
            {
                return order.order_id ==
                    order_id;
            }
        );

    if (iterator == orders_.end()) {
        return nullptr;
    }

    return &(*iterator);
}

const LimitOrder* OrderManager::find_order(
    OrderId order_id
) const noexcept
{
    const auto iterator =
        std::find_if(
            orders_.begin(),
            orders_.end(),
            [order_id](
                const LimitOrder& order
            )
            {
                return order.order_id ==
                    order_id;
            }
        );

    if (iterator == orders_.end()) {
        return nullptr;
    }

    return &(*iterator);
}

const std::vector<LimitOrder>&
OrderManager::orders() const noexcept
{
    return orders_;
}
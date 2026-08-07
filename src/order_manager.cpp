#include "order_manager.hpp"

#include <algorithm>

void OrderManager::add_order(
    const LimitOrder& order
)
{
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

std::vector<LimitOrder>&
OrderManager::orders() noexcept
{
    return orders_;
}

const std::vector<LimitOrder>&
OrderManager::orders() const noexcept
{
    return orders_;
}
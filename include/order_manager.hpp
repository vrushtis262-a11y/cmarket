#ifndef CMARKET_ORDER_MANAGER_HPP
#define CMARKET_ORDER_MANAGER_HPP

#include "limit_order.hpp"

#include <vector>

class OrderManager {
public:
    void add_order(
        const LimitOrder& order
    );

    [[nodiscard]]
    bool cancel_order(
        OrderId order_id
    );

    [[nodiscard]]
    LimitOrder* find_order(
        OrderId order_id
    ) noexcept;

    [[nodiscard]]
    const LimitOrder* find_order(
        OrderId order_id
    ) const noexcept;

    [[nodiscard]]
    const std::vector<LimitOrder>&
    orders() const noexcept;

private:
    std::vector<LimitOrder> orders_;
};

#endif
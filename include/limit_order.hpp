#ifndef CMARKET_LIMIT_ORDER_HPP
#define CMARKET_LIMIT_ORDER_HPP

#include "execution.hpp"

#include <cstdint>

using OrderId = std::uint64_t;
using SequenceNumber = std::uint64_t;

struct LimitOrder {
    OrderId order_id;
    OrderSide side;
    std::int64_t price_ticks;
    std::int64_t original_quantity;
    std::int64_t remaining_quantity;
    SequenceNumber sequence_number;

    [[nodiscard]]
    std::int64_t filled_quantity() const noexcept
    {
        return original_quantity - remaining_quantity;
    }

    [[nodiscard]]
    bool is_filled() const noexcept
    {
        return remaining_quantity == 0;
    }

    [[nodiscard]]
    bool is_partially_filled() const noexcept
    {
        return remaining_quantity > 0 &&
               remaining_quantity < original_quantity;
    }
};

#endif
#ifndef CMARKET_EXECUTION_HPP
#define CMARKET_EXECUTION_HPP

#include <cstdint>
#include <optional>
#include <vector>

enum class OrderSide {
    Buy,
    Sell
};

struct Trade {
    OrderSide aggressor_side;
    std::int64_t price_ticks;
    std::int64_t quantity;
};

struct ExecutionResult {
    OrderSide side;
    std::int64_t requested_quantity;
    std::int64_t executed_quantity;
    std::int64_t remaining_quantity;
    std::optional<std::int64_t> average_price_ticks;
    std::vector<Trade> trades;

    [[nodiscard]]
    bool fully_filled() const noexcept
    {
        return remaining_quantity == 0;
    }

    [[nodiscard]]
    bool partially_filled() const noexcept
    {
        return executed_quantity > 0 && remaining_quantity > 0;
    }

    [[nodiscard]]
    bool unfilled() const noexcept
    {
        return executed_quantity == 0;
    }
};

#endif
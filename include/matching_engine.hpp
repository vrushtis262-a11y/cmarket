#ifndef CMARKET_MATCHING_ENGINE_HPP
#define CMARKET_MATCHING_ENGINE_HPP

#include "execution.hpp"
#include "order_book.hpp"

#include <cstdint>
#include <vector>

class MatchingEngine {
public:
    explicit MatchingEngine(OrderBook& order_book) noexcept;

    [[nodiscard]]
    ExecutionResult execute_market_buy(
        std::int64_t quantity
    );

    [[nodiscard]]
    ExecutionResult execute_market_sell(
        std::int64_t quantity
    );

    [[nodiscard]]
    const std::vector<Trade>&
    trade_history() const noexcept;

    void clear_trade_history() noexcept;

private:
    [[nodiscard]]
    ExecutionResult execute_market_order(
        OrderSide side,
        std::int64_t quantity
    );

    OrderBook& order_book_;
    std::vector<Trade> trade_history_;
};

#endif
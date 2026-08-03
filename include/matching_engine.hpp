#ifndef CMARKET_MATCHING_ENGINE_HPP
#define CMARKET_MATCHING_ENGINE_HPP

#include "execution.hpp"
#include "order_book.hpp"

#include <cstdint>

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

private:
    OrderBook& order_book_;
};

#endif
#ifndef CMARKET_TRADE_STORE_HPP
#define CMARKET_TRADE_STORE_HPP

#include "execution.hpp"
#include "validation.hpp"

#include <cstdint>
#include <optional>
#include <vector>

class TradeStore {
public:
    [[nodiscard]]
    const Trade& record_trade(
        OrderSide aggressor_side,
        std::int64_t price_ticks,
        std::int64_t quantity,
        std::optional<std::uint64_t> buy_order_id = std::nullopt,
        std::optional<std::uint64_t> sell_order_id = std::nullopt
    )
    {
        validation::validate_trade(
            price_ticks,
            quantity
        );

        const Trade trade{
            .aggressor_side = aggressor_side,
            .price_ticks = price_ticks,
            .quantity = quantity,
            .trade_id = next_trade_id_++,
            .buy_order_id = buy_order_id,
            .sell_order_id = sell_order_id,
            .execution_sequence = next_execution_sequence_++
        };

        trades_.push_back(trade);

        return trades_.back();
    }

    [[nodiscard]]
    const std::vector<Trade>&
    trades() const noexcept
    {
        return trades_;
    }

    void clear() noexcept
    {
        trades_.clear();
    }

private:
    TradeId next_trade_id_ = 1;

    TradeSequenceNumber
        next_execution_sequence_ = 1;

    std::vector<Trade> trades_;
};

#endif
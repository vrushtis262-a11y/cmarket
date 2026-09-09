#ifndef CMARKET_TRADE_STORE_HPP
#define CMARKET_TRADE_STORE_HPP

#include "execution.hpp"
#include "validation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

class TradeStore {
public:
    struct Checkpoint {
        std::size_t trade_count;
        TradeId next_trade_id;
        TradeSequenceNumber
            next_execution_sequence;
    };

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

        try {
            trades_.push_back(
                trade
            );
        }
        catch (...) {
            --next_trade_id_;
            --next_execution_sequence_;

            throw;
        }

        return trades_.back();
    }

    [[nodiscard]]
    Checkpoint checkpoint() const noexcept
    {
        return Checkpoint{
            .trade_count = trades_.size(),
            .next_trade_id = next_trade_id_,
            .next_execution_sequence =
                next_execution_sequence_
        };
    }

    void rollback(
        const Checkpoint& checkpoint
    )
    {
        if (
            checkpoint.trade_count >
            trades_.size()
        ) {
            throw std::invalid_argument(
                "TradeStore checkpoint is newer "
                "than the current trade state."
            );
        }

        trades_.resize(
            checkpoint.trade_count
        );

        next_trade_id_ =
            checkpoint.next_trade_id;

        next_execution_sequence_ =
            checkpoint.next_execution_sequence;
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
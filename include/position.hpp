#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace cmarket {

class Position {
public:
    using Quantity = std::int64_t;
    using Price = std::int64_t;

    explicit Position(std::string symbol)
        : symbol_(std::move(symbol)),
          quantity_(0),
          average_entry_price_ticks_(0) {}

    Position(
        std::string symbol,
        Quantity quantity,
        Price average_entry_price_ticks
    )
        : symbol_(std::move(symbol)),
          quantity_(quantity),
          average_entry_price_ticks_(
              average_entry_price_ticks
          ) {}

    [[nodiscard]]
    const std::string& symbol() const noexcept {
        return symbol_;
    }

    [[nodiscard]]
    Quantity quantity() const noexcept {
        return quantity_;
    }

    [[nodiscard]]
    Price average_entry_price_ticks() const noexcept {
        return average_entry_price_ticks_;
    }

    [[nodiscard]]
    bool is_flat() const noexcept {
        return quantity_ == 0;
    }

    [[nodiscard]]
    bool is_long() const noexcept {
        return quantity_ > 0;
    }

    [[nodiscard]]
    bool is_short() const noexcept {
        return quantity_ < 0;
    }

    void apply_trade(
        Quantity quantity,
        Price price_ticks
    ) {
        if (quantity == 0) {
            throw std::invalid_argument(
                "Trade quantity must not be zero"
            );
        }

        if (price_ticks <= 0) {
            throw std::invalid_argument(
                "Trade price must be positive"
            );
        }

        // Opening a position from flat.
        if (quantity_ == 0) {
            quantity_ = quantity;
            average_entry_price_ticks_ =
                price_ticks;
            return;
        }

        const bool current_is_long =
            quantity_ > 0;

        const bool trade_is_buy =
            quantity > 0;

        // Same direction: increase the existing position and
        // calculate a new weighted average entry price.
        if (current_is_long == trade_is_buy) {
            const Quantity old_absolute_quantity =
                absolute_quantity(quantity_);

            const Quantity trade_absolute_quantity =
                absolute_quantity(quantity);

            const Quantity new_absolute_quantity =
                old_absolute_quantity +
                trade_absolute_quantity;

            const __int128 weighted_price_total =
                static_cast<__int128>(
                    average_entry_price_ticks_
                ) *
                    static_cast<__int128>(
                        old_absolute_quantity
                    ) +
                static_cast<__int128>(
                    price_ticks
                ) *
                    static_cast<__int128>(
                        trade_absolute_quantity
                    );

            average_entry_price_ticks_ =
                static_cast<Price>(
                    weighted_price_total /
                    static_cast<__int128>(
                        new_absolute_quantity
                    )
                );

            quantity_ += quantity;
            return;
        }

        const Quantity old_absolute_quantity =
            absolute_quantity(quantity_);

        const Quantity trade_absolute_quantity =
            absolute_quantity(quantity);

        // Opposite direction but smaller than the current
        // position: reduce it without changing the average
        // entry price.
        if (
            trade_absolute_quantity <
            old_absolute_quantity
        ) {
            quantity_ += quantity;
            return;
        }

        // Exactly offsets the current position.
        if (
            trade_absolute_quantity ==
            old_absolute_quantity
        ) {
            quantity_ = 0;
            average_entry_price_ticks_ = 0;
            return;
        }

        // The trade is larger than the current position,
        // so the position flips direction. The remaining
        // quantity opens at the new trade price.
        quantity_ += quantity;
        average_entry_price_ticks_ =
            price_ticks;
    }

private:
    static Quantity absolute_quantity(
        Quantity quantity
    ) noexcept {
        return quantity < 0
            ? -quantity
            : quantity;
    }

    std::string symbol_;
    Quantity quantity_;
    Price average_entry_price_ticks_;
};

}  // namespace cmarket
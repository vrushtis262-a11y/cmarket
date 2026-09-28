#pragma once

#include "position.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace cmarket {

class PositionManager {
public:
    using Quantity = Position::Quantity;
    using Price = Position::Price;

    void apply_trade(
        const std::string& symbol,
        Quantity quantity,
        Price price_ticks
    ) {
        if (symbol.empty()) {
            throw std::invalid_argument(
                "Position symbol must not be empty"
            );
        }

        auto [iterator, inserted] =
            positions_.try_emplace(
                symbol,
                symbol
            );

        try {
            iterator->second.apply_trade(
                quantity,
                price_ticks
            );
        }
        catch (...) {
            // If this was a newly created position and the
            // trade failed validation, do not leave an empty
            // position behind.
            if (inserted) {
                positions_.erase(iterator);
            }

            throw;
        }
    }

    [[nodiscard]]
    bool has_position(
        const std::string& symbol
    ) const noexcept {
        return
            positions_.find(symbol) !=
            positions_.end();
    }

    [[nodiscard]]
    const Position& position(
        const std::string& symbol
    ) const {
        const auto iterator =
            positions_.find(symbol);

        if (iterator == positions_.end()) {
            throw std::out_of_range(
                "Position not found for symbol: " +
                symbol
            );
        }

        return iterator->second;
    }

    [[nodiscard]]
    std::size_t size() const noexcept {
        return positions_.size();
    }

private:
    std::unordered_map<
        std::string,
        Position
    > positions_;
};

}  // namespace cmarket
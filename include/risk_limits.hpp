#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace cmarket {

class RiskLimits {
public:
    using Quantity = std::int64_t;

    explicit RiskLimits(
        Quantity max_absolute_position
    )
        : max_absolute_position_(
              max_absolute_position
          )
    {
        if (max_absolute_position <= 0) {
            throw std::invalid_argument(
                "Maximum absolute position must be positive"
            );
        }
    }

    [[nodiscard]]
    Quantity max_absolute_position() const noexcept {
        return max_absolute_position_;
    }

    [[nodiscard]]
    bool is_position_allowed(
        Quantity position
    ) const noexcept {
        if (position == std::numeric_limits<Quantity>::min()) {
            return false;
        }

        const Quantity absolute_position =
            position < 0
                ? -position
                : position;

        return
            absolute_position <=
            max_absolute_position_;
    }

    [[nodiscard]]
    bool is_trade_allowed(
        Quantity current_position,
        Quantity trade_quantity
    ) const noexcept {
        const __int128 resulting_position =
            static_cast<__int128>(
                current_position
            ) +
            static_cast<__int128>(
                trade_quantity
            );

        return
            resulting_position <=
                static_cast<__int128>(
                    max_absolute_position_
                ) &&
            resulting_position >=
                -static_cast<__int128>(
                    max_absolute_position_
                );
    }

private:
    Quantity max_absolute_position_;
};

}  // namespace cmarket
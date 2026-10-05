#pragma once

#include "position_manager.hpp"
#include "risk_limits.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace cmarket {

class RiskValidator {
public:
    using Quantity = std::int64_t;

    RiskValidator(
        const PositionManager& position_manager,
        const RiskLimits& risk_limits
    ) noexcept
        : position_manager_(position_manager),
          risk_limits_(risk_limits)
    {
    }

    [[nodiscard]]
    bool is_trade_allowed(
        const std::string& symbol,
        Quantity trade_quantity
    ) const {
        if (symbol.empty()) {
            throw std::invalid_argument(
                "Risk validation symbol must not be empty"
            );
        }

        if (trade_quantity == 0) {
            throw std::invalid_argument(
                "Risk validation trade quantity must not be zero"
            );
        }

        Quantity current_position = 0;

        if (position_manager_.has_position(symbol)) {
            current_position =
                position_manager_
                    .position(symbol)
                    .quantity();
        }

        return risk_limits_.is_trade_allowed(
            current_position,
            trade_quantity
        );
    }

private:
    const PositionManager& position_manager_;
    const RiskLimits& risk_limits_;
};

}  // namespace cmarket
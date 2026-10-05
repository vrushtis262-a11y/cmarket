#pragma once

#include "matching_engine.hpp"
#include "position_execution_handler.hpp"
#include "risk_validator.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace cmarket {

class RiskManagedExecution {
public:
    using Quantity = std::int64_t;

    RiskManagedExecution(
        MatchingEngine& matching_engine,
        const RiskValidator& risk_validator,
        PositionExecutionHandler& position_handler
    ) noexcept
        : matching_engine_(matching_engine),
          risk_validator_(risk_validator),
          position_handler_(position_handler)
    {
    }

    [[nodiscard]]
    ExecutionResult execute_market_buy(
        const std::string& symbol,
        Quantity quantity
    ) {
        validate_quantity(quantity);

        if (
            !risk_validator_.is_trade_allowed(
                symbol,
                quantity
            )
        ) {
            throw std::runtime_error(
                "Market buy rejected by risk limits"
            );
        }

        ExecutionResult result =
            matching_engine_.execute_market_buy(
                quantity
            );

        position_handler_.apply_execution(
            symbol,
            result
        );

        return result;
    }

    [[nodiscard]]
    ExecutionResult execute_market_sell(
        const std::string& symbol,
        Quantity quantity
    ) {
        validate_quantity(quantity);

        if (
            !risk_validator_.is_trade_allowed(
                symbol,
                -quantity
            )
        ) {
            throw std::runtime_error(
                "Market sell rejected by risk limits"
            );
        }

        ExecutionResult result =
            matching_engine_.execute_market_sell(
                quantity
            );

        position_handler_.apply_execution(
            symbol,
            result
        );

        return result;
    }

private:
    static void validate_quantity(
        Quantity quantity
    ) {
        if (quantity <= 0) {
            throw std::invalid_argument(
                "Market order quantity must be positive"
            );
        }
    }

    MatchingEngine& matching_engine_;
    const RiskValidator& risk_validator_;
    PositionExecutionHandler& position_handler_;
};

}  // namespace cmarket
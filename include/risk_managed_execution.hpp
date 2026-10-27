
#pragma once

#include "analytics_execution_handler.hpp"
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
        PositionExecutionHandler& position_handler,
        AnalyticsExecutionHandler* analytics_handler = nullptr
    ) noexcept
        : matching_engine_(matching_engine),
          risk_validator_(risk_validator),
          position_handler_(position_handler),
          analytics_handler_(analytics_handler)
    {
    }

    [[nodiscard]]
    ExecutionResult execute_market_buy(
        const std::string& symbol,
        Quantity quantity
    ) {
        validate_quantity(quantity);

        if (!risk_validator_.is_trade_allowed(
                symbol,
                quantity
            )) {
            throw std::runtime_error(
                "Market buy rejected by risk limits"
            );
        }

        ExecutionResult result =
            matching_engine_.execute_market_buy(quantity);

        position_handler_.apply_execution(
            symbol,
            result
        );

        apply_analytics(symbol, result);

        return result;
    }

    [[nodiscard]]
    ExecutionResult execute_market_sell(
        const std::string& symbol,
        Quantity quantity
    ) {
        validate_quantity(quantity);

        if (!risk_validator_.is_trade_allowed(
                symbol,
                -quantity
            )) {
            throw std::runtime_error(
                "Market sell rejected by risk limits"
            );
        }

        ExecutionResult result =
            matching_engine_.execute_market_sell(quantity);

        position_handler_.apply_execution(
            symbol,
            result
        );

        apply_analytics(symbol, result);

        return result;
    }

    [[nodiscard]]
    bool analytics_healthy() const noexcept {
        return analytics_healthy_;
    }

private:
    static void validate_quantity(Quantity quantity)
    {
        if (quantity <= 0) {
            throw std::invalid_argument(
                "Market order quantity must be positive"
            );
        }
    }

    void apply_analytics(
        const std::string& symbol,
        const ExecutionResult& result
    ) noexcept {
        if (analytics_handler_ == nullptr ||
            result.executed_quantity == 0) {
            return;
        }

        try {
            analytics_handler_->apply_execution(
                symbol,
                result
            );
        } catch (...) {
            // Analytics failures must not change committed trades.
            analytics_healthy_ = false;
        }
    }

    MatchingEngine& matching_engine_;
    const RiskValidator& risk_validator_;
    PositionExecutionHandler& position_handler_;
    AnalyticsExecutionHandler* analytics_handler_;

    bool analytics_healthy_ = true;
};

} // namespace cmarket

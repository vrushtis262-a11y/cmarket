
#pragma once

#include "analytics_engine.hpp"
#include "market_impact.hpp"
#include "risk_managed_execution.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace cmarket {

class AnalyticsEngineExecutionAdapter {
public:
    AnalyticsEngineExecutionAdapter(
        RiskManagedExecution& execution,
        AnalyticsEngine& analytics,
        OrderBook& order_book
    ) noexcept
        : execution_(execution),
          analytics_(analytics),
          order_book_(order_book)
    {
    }

    [[nodiscard]]
    ExecutionResult execute_market_buy(
        const std::string& symbol,
        std::int64_t quantity
    ) {
        return execute(
            symbol,
            quantity,
            OrderSide::Buy
        );
    }

    [[nodiscard]]
    ExecutionResult execute_market_sell(
        const std::string& symbol,
        std::int64_t quantity
    ) {
        return execute(
            symbol,
            quantity,
            OrderSide::Sell
        );
    }

    // Call after other committed order book changes.
    void on_order_book_update(
        const std::string& symbol
    ) noexcept {
        try {
            analytics_.on_order_book_update(
                symbol,
                order_book_
            );
        } catch (...) {
            analytics_healthy_ = false;
        }
    }

    [[nodiscard]]
    AnalyticsSnapshot snapshot(
        const std::string& symbol
    ) const {
        return analytics_.snapshot(symbol);
    }

    [[nodiscard]]
    bool analytics_healthy() const noexcept {
        return analytics_healthy_;
    }

private:
    [[nodiscard]]
    ExecutionResult execute(
        const std::string& symbol,
        std::int64_t quantity,
        OrderSide side
    ) {
        std::optional<MarketQuote> before;

        try {
            before = MarketImpact::capture_quote(
                order_book_
            );
        } catch (...) {
            analytics_healthy_ = false;
        }

        // RiskManagedExecution owns risk checks, matching,
        // positions, and VWAP/trade-statistics updates.
        ExecutionResult result =
            side == OrderSide::Buy
                ? execution_.execute_market_buy(
                    symbol,
                    quantity
                )
                : execution_.execute_market_sell(
                    symbol,
                    quantity
                );

        if (result.executed_quantity == 0) {
            return result;
        }

        std::optional<MarketQuote> after;

        try {
            after = MarketImpact::capture_quote(
                order_book_
            );
        } catch (...) {
            analytics_healthy_ = false;
        }

        // Record committed order book changes.
        on_order_book_update(symbol);

        // Record market impact without counting trades twice.
        if (before && after) {
            try {
                analytics_.record_market_impact(
                    symbol,
                    result,
                    *before,
                    *after
                );
            } catch (...) {
                analytics_healthy_ = false;
            }
        }

        return result;
    }

    RiskManagedExecution& execution_;
    AnalyticsEngine& analytics_;
    OrderBook& order_book_;

    bool analytics_healthy_ = true;
};

} // namespace cmarket

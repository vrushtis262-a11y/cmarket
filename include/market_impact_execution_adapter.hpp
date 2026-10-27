
#pragma once

#include "market_impact.hpp"
#include "risk_managed_execution.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace cmarket {

class MarketImpactExecutionAdapter {
public:
    MarketImpactExecutionAdapter(
        RiskManagedExecution& execution,
        OrderBook& order_book
    ) noexcept
        : execution_(execution),
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

    [[nodiscard]]
    std::optional<MarketImpactSummary> latest_impact(
        const std::string& symbol
    ) const {
        const auto it = impacts_.find(symbol);

        if (it == impacts_.end()) {
            return std::nullopt;
        }

        return it->second;
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

        try {
            const MarketQuote after =
                MarketImpact::capture_quote(
                    order_book_
                );

            if (before.has_value()) {
                MarketImpactSummary summary =
                    MarketImpact::analyze(
                        result,
                        *before,
                        after
                    );

                impacts_.insert_or_assign(
                    symbol,
                    std::move(summary)
                );
            }
        } catch (...) {
            // Analytics failure must not undo execution.
            analytics_healthy_ = false;
        }

        return result;
    }

    RiskManagedExecution& execution_;
    OrderBook& order_book_;

    std::unordered_map<
        std::string,
        MarketImpactSummary
    > impacts_;

    bool analytics_healthy_ = true;
};

} // namespace cmarket

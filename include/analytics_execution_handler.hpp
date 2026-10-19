
#pragma once

#include "execution.hpp"
#include "rolling_vwap.hpp"
#include "trade_statistics.hpp"

#include <stdexcept>
#include <string>

namespace cmarket {

class AnalyticsExecutionHandler {
public:
    AnalyticsExecutionHandler(
        RollingVWAP& rolling_vwap,
        TradeStatistics& trade_statistics
    ) noexcept
        : rolling_vwap_(rolling_vwap),
          trade_statistics_(trade_statistics)
    {
    }

    void apply_execution(
        const std::string& symbol,
        const ExecutionResult& result
    ) {
        if (symbol.empty()) {
            throw std::invalid_argument(
                "Execution symbol must not be empty"
            );
        }

        for (const Trade& trade : result.trades) {
            if (trade.price_ticks <= 0 ||
                trade.quantity <= 0) {
                throw std::invalid_argument(
                    "Execution trade must have positive "
                    "price and quantity"
                );
            }

            const double price =
                static_cast<double>(trade.price_ticks);

            const double quantity =
                static_cast<double>(trade.quantity);

            TradeAggressorSide side =
                TradeAggressorSide::Unknown;

            if (trade.aggressor_side == OrderSide::Buy) {
                side = TradeAggressorSide::Buy;
            } else if (
                trade.aggressor_side == OrderSide::Sell
            ) {
                side = TradeAggressorSide::Sell;
            } else {
                throw std::invalid_argument(
                    "Invalid execution aggressor side"
                );
            }

            rolling_vwap_.add_trade(
                symbol,
                price,
                quantity
            );

            trade_statistics_.add_trade(
                symbol,
                price,
                quantity,
                side
            );
        }
    }

private:
    RollingVWAP& rolling_vwap_;
    TradeStatistics& trade_statistics_;
};

} // namespace cmarket

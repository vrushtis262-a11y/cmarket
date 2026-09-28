#pragma once

#include "execution.hpp"
#include "position_manager.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace cmarket {

class PositionExecutionHandler {
public:
    explicit PositionExecutionHandler(
        PositionManager& position_manager
    ) noexcept
        : position_manager_(position_manager)
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
            const std::int64_t signed_quantity =
                result.side == OrderSide::Buy
                    ? trade.quantity
                    : -trade.quantity;

            position_manager_.apply_trade(
                symbol,
                signed_quantity,
                trade.price_ticks
            );
        }
    }

private:
    PositionManager& position_manager_;
};

}  // namespace cmarket
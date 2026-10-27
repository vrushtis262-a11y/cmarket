
#pragma once

#include "execution.hpp"
#include "order_book.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>

namespace cmarket {

struct MarketQuote {
    std::optional<std::int64_t> best_bid_ticks;
    std::optional<std::int64_t> best_ask_ticks;
    std::int64_t bid_depth = 0;
    std::int64_t ask_depth = 0;

    [[nodiscard]]
    std::optional<long double> mid_price_ticks() const {
        if (!best_bid_ticks || !best_ask_ticks) {
            return std::nullopt;
        }

        return (
            static_cast<long double>(*best_bid_ticks) +
            static_cast<long double>(*best_ask_ticks)
        ) / 2.0L;
    }
};

struct MarketImpactSummary {
    OrderSide side;
    std::int64_t requested_quantity;
    std::int64_t executed_quantity;
    std::int64_t remaining_quantity;
    std::int64_t liquidity_consumed;
    std::size_t fill_count;

    std::optional<long double> average_execution_price_ticks;
    std::optional<long double> reference_price_ticks;
    std::optional<long double> slippage_ticks;
    std::optional<long double> effective_spread_ticks;
    std::optional<long double> pre_trade_mid_ticks;
    std::optional<long double> post_trade_mid_ticks;
    std::optional<long double> mid_price_change_ticks;
    std::optional<long double> signed_mid_impact_ticks;
    std::optional<long double> liquidity_consumed_ratio;
};

class MarketImpact {
public:
    // Capture a consistent quote snapshot before or after execution.
    [[nodiscard]]
    static MarketQuote capture_quote(const OrderBook& book) {
        const OrderBook snapshot(book);

        const auto bid = snapshot.best_bid();
        const auto ask = snapshot.best_ask();

        return MarketQuote{
            .best_bid_ticks = bid
                ? std::optional<std::int64_t>(bid->price_ticks)
                : std::nullopt,
            .best_ask_ticks = ask
                ? std::optional<std::int64_t>(ask->price_ticks)
                : std::nullopt,
            .bid_depth = snapshot.bid_depth(),
            .ask_depth = snapshot.ask_depth()
        };
    }

    [[nodiscard]]
    static MarketImpactSummary analyze(
        const ExecutionResult& execution,
        const MarketQuote& before,
        const MarketQuote& after
    ) {
        if (execution.requested_quantity < 0 ||
            execution.executed_quantity < 0 ||
            execution.remaining_quantity < 0 ||
            execution.executed_quantity >
                execution.requested_quantity ||
            execution.remaining_quantity !=
                execution.requested_quantity -
                    execution.executed_quantity) {
            throw std::invalid_argument(
                "Invalid execution quantities"
            );
        }

        long double notional = 0.0L;
        std::int64_t filled_quantity = 0;

        for (const Trade& trade : execution.trades) {
            if (trade.price_ticks <= 0 ||
                trade.quantity <= 0 ||
                trade.quantity >
                    execution.executed_quantity - filled_quantity) {
                throw std::invalid_argument(
                    "Invalid execution trade"
                );
            }

            filled_quantity += trade.quantity;

            notional +=
                static_cast<long double>(trade.price_ticks) *
                static_cast<long double>(trade.quantity);
        }

        if (filled_quantity != execution.executed_quantity) {
            throw std::invalid_argument(
                "Executed quantity does not match trades"
            );
        }

        MarketImpactSummary result{
            .side = execution.side,
            .requested_quantity = execution.requested_quantity,
            .executed_quantity = execution.executed_quantity,
            .remaining_quantity = execution.remaining_quantity,
            .liquidity_consumed = filled_quantity,
            .fill_count = execution.trades.size()
        };

        result.pre_trade_mid_ticks = before.mid_price_ticks();
        result.post_trade_mid_ticks = after.mid_price_ticks();

        if (filled_quantity == 0) {
            return result;
        }

        const long double average_price =
            notional / static_cast<long double>(filled_quantity);

        result.average_execution_price_ticks = average_price;

        const long double direction =
            execution.side == OrderSide::Buy ? 1.0L : -1.0L;

        const auto reference =
            execution.side == OrderSide::Buy
                ? before.best_ask_ticks
                : before.best_bid_ticks;

        if (reference) {
            result.reference_price_ticks =
                static_cast<long double>(*reference);

            result.slippage_ticks =
                direction * (
                    average_price -
                    static_cast<long double>(*reference)
                );
        }

        if (result.pre_trade_mid_ticks) {
            result.effective_spread_ticks =
                2.0L * direction * (
                    average_price - *result.pre_trade_mid_ticks
                );
        }

        if (result.pre_trade_mid_ticks &&
            result.post_trade_mid_ticks) {
            const long double change =
                *result.post_trade_mid_ticks -
                *result.pre_trade_mid_ticks;

            result.mid_price_change_ticks = change;
            result.signed_mid_impact_ticks =
                direction * change;
        }

        const std::int64_t available_liquidity =
            execution.side == OrderSide::Buy
                ? before.ask_depth
                : before.bid_depth;

        if (available_liquidity > 0) {
            result.liquidity_consumed_ratio =
                static_cast<long double>(filled_quantity) /
                static_cast<long double>(available_liquidity);
        }

        return result;
    }
};

} // namespace cmarket

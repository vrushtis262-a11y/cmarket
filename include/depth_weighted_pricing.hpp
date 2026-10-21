
#pragma once

#include "depth_analytics.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace cmarket {

struct DepthWeightedPriceSummary {
    std::size_t requested_levels;
    std::size_t bid_levels;
    std::size_t ask_levels;

    long double bid_depth;
    long double ask_depth;

    std::optional<long double> weighted_bid_price_ticks;
    std::optional<long double> weighted_ask_price_ticks;
    std::optional<long double> weighted_mid_price_ticks;
};

class DepthWeightedPricing {
public:
    // Calculate quantity-weighted prices across the best N levels.
    [[nodiscard]]
    static DepthWeightedPriceSummary analyze(
        const OrderBook& order_book,
        std::size_t max_levels
    ) {
        const DepthSummary depth =
            DepthAnalytics::analyze(
                order_book,
                max_levels
            );

        const std::optional<long double> weighted_bid =
            calculate_weighted_price(
                depth.bids,
                depth.bid_depth
            );

        const std::optional<long double> weighted_ask =
            calculate_weighted_price(
                depth.asks,
                depth.ask_depth
            );

        std::optional<long double> weighted_mid =
            std::nullopt;

        if (
            weighted_bid.has_value() &&
            weighted_ask.has_value()
        ) {
            weighted_mid =
                (*weighted_bid + *weighted_ask) / 2.0L;
        }

        return DepthWeightedPriceSummary{
            .requested_levels = depth.requested_levels,
            .bid_levels = depth.bid_levels,
            .ask_levels = depth.ask_levels,
            .bid_depth = depth.bid_depth,
            .ask_depth = depth.ask_depth,
            .weighted_bid_price_ticks = weighted_bid,
            .weighted_ask_price_ticks = weighted_ask,
            .weighted_mid_price_ticks = weighted_mid
        };
    }

private:
    [[nodiscard]]
    static std::optional<long double>
    calculate_weighted_price(
        const std::vector<DepthLevel>& levels,
        long double total_depth
    ) {
        if (
            levels.empty() ||
            total_depth <= 0.0L
        ) {
            return std::nullopt;
        }

        long double weighted_price = 0.0L;

        for (const DepthLevel& level : levels) {
            const long double quantity_share =
                static_cast<long double>(
                    level.quantity
                ) / total_depth;

            weighted_price +=
                static_cast<long double>(
                    level.price_ticks
                ) * quantity_share;
        }

        return weighted_price;
    }
};

} // namespace cmarket

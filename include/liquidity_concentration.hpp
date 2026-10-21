
#pragma once

#include "depth_analytics.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace cmarket {

struct LiquidityConcentrationSummary {
    std::size_t requested_levels;
    std::size_t bid_levels;
    std::size_t ask_levels;

    std::optional<long double> bid_best_level_share;
    std::optional<long double> ask_best_level_share;

    std::optional<long double> bid_hhi;
    std::optional<long double> ask_hhi;
};

class LiquidityConcentration {
public:
    // Measure liquidity concentration across the best N levels.
    [[nodiscard]]
    static LiquidityConcentrationSummary analyze(
        const OrderBook& order_book,
        std::size_t max_levels
    ) {
        const DepthSummary depth =
            DepthAnalytics::analyze(
                order_book,
                max_levels
            );

        return LiquidityConcentrationSummary{
            .requested_levels = depth.requested_levels,
            .bid_levels = depth.bid_levels,
            .ask_levels = depth.ask_levels,

            .bid_best_level_share = best_level_share(
                depth.bids,
                depth.bid_depth
            ),

            .ask_best_level_share = best_level_share(
                depth.asks,
                depth.ask_depth
            ),

            .bid_hhi = calculate_hhi(
                depth.bids,
                depth.bid_depth
            ),

            .ask_hhi = calculate_hhi(
                depth.asks,
                depth.ask_depth
            )
        };
    }

private:
    [[nodiscard]]
    static std::optional<long double> best_level_share(
        const std::vector<DepthLevel>& levels,
        long double total_depth
    ) {
        if (levels.empty() || total_depth <= 0.0L) {
            return std::nullopt;
        }

        return
            static_cast<long double>(
                levels.front().quantity
            ) / total_depth;
    }

    [[nodiscard]]
    static std::optional<long double> calculate_hhi(
        const std::vector<DepthLevel>& levels,
        long double total_depth
    ) {
        if (levels.empty() || total_depth <= 0.0L) {
            return std::nullopt;
        }

        long double hhi = 0.0L;

        for (const DepthLevel& level : levels) {
            const long double share =
                static_cast<long double>(
                    level.quantity
                ) / total_depth;

            hhi += share * share;
        }

        return hhi;
    }
};

} // namespace cmarket

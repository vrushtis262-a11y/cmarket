
#pragma once

#include "order_book.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace cmarket {

struct DepthLevel {
    std::int64_t price_ticks;
    std::int64_t quantity;
    long double cumulative_quantity;
};

struct DepthSummary {
    std::size_t requested_levels;
    std::size_t bid_levels;
    std::size_t ask_levels;

    long double bid_depth;
    long double ask_depth;
    long double total_depth;

    std::optional<long double> imbalance;

    std::vector<DepthLevel> bids;
    std::vector<DepthLevel> asks;
};

class DepthAnalytics {
public:
    // Analyze the best N levels on each side.
    [[nodiscard]]
    static DepthSummary analyze(
        const OrderBook& order_book,
        std::size_t max_levels
    ) {
        if (max_levels == 0) {
            throw std::invalid_argument(
                "Depth level count must be positive"
            );
        }

        // Copy once to keep both sides consistent.
        const OrderBook snapshot(order_book);

        const std::vector<PriceLevel> bids =
            snapshot.bids();

        const std::vector<PriceLevel> asks =
            snapshot.asks();

        DepthSummary result{
            .requested_levels = max_levels,
            .bid_levels = 0,
            .ask_levels = 0,
            .bid_depth = 0.0L,
            .ask_depth = 0.0L,
            .total_depth = 0.0L,
            .imbalance = std::nullopt,
            .bids = {},
            .asks = {}
        };

        result.bids = build_levels(
            bids,
            max_levels
        );

        result.asks = build_levels(
            asks,
            max_levels
        );

        result.bid_levels = result.bids.size();
        result.ask_levels = result.asks.size();

        if (!result.bids.empty()) {
            result.bid_depth =
                result.bids.back().cumulative_quantity;
        }

        if (!result.asks.empty()) {
            result.ask_depth =
                result.asks.back().cumulative_quantity;
        }

        result.total_depth =
            result.bid_depth + result.ask_depth;

        if (result.total_depth > 0.0L) {
            result.imbalance =
                (result.bid_depth - result.ask_depth) /
                result.total_depth;
        }

        return result;
    }

private:
    [[nodiscard]]
    static std::vector<DepthLevel> build_levels(
        const std::vector<PriceLevel>& levels,
        std::size_t max_levels
    ) {
        const std::size_t count =
            std::min(
                levels.size(),
                max_levels
            );

        std::vector<DepthLevel> result;
        result.reserve(count);

        long double cumulative_quantity = 0.0L;

        for (std::size_t index = 0;
             index < count;
             ++index) {
            const PriceLevel& level = levels[index];

            cumulative_quantity +=
                static_cast<long double>(
                    level.quantity
                );

            result.push_back(
                DepthLevel{
                    .price_ticks = level.price_ticks,
                    .quantity = level.quantity,
                    .cumulative_quantity =
                        cumulative_quantity
                }
            );
        }

        return result;
    }
};

} // namespace cmarket

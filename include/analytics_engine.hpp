
#pragma once

#include "analytics_execution_handler.hpp"
#include "depth_analytics.hpp"
#include "market_impact.hpp"
#include "order_flow_imbalance.hpp"
#include "rolling_vwap.hpp"
#include "trade_statistics.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace cmarket {

struct AnalyticsSnapshot {
    std::optional<double> rolling_vwap;
    std::optional<TradeStatisticsSnapshot> trade_statistics;
    std::optional<long double> latest_ofi;
    std::optional<long double> rolling_ofi;
    std::optional<DepthSummary> depth;
    std::optional<MarketImpactSummary> market_impact;
};

class AnalyticsEngine {
public:
    AnalyticsEngine(
        RollingVWAP& rolling_vwap,
        TradeStatistics& trade_statistics,
        OrderFlowImbalance& order_flow_imbalance,
        std::size_t depth_levels = 10
    )
        : rolling_vwap_(rolling_vwap),
          trade_statistics_(trade_statistics),
          order_flow_imbalance_(order_flow_imbalance),
          execution_handler_(
              rolling_vwap,
              trade_statistics
          ),
          depth_levels_(depth_levels)
    {
        if (depth_levels_ == 0) {
            throw std::invalid_argument(
                "Analytics depth levels must be positive"
            );
        }
    }

    // Supply this handler to RiskManagedExecution.
    [[nodiscard]]
    AnalyticsExecutionHandler& execution_handler() noexcept {
        return execution_handler_;
    }

    // For executions not already sent through execution_handler().
    void record_execution(
        const std::string& symbol,
        const ExecutionResult& execution
    ) {
        validate_symbol(symbol);

        execution_handler_.apply_execution(
            symbol,
            execution
        );
    }

    // Call after an order book update has committed.
    void on_order_book_update(
        const std::string& symbol,
        const OrderBook& order_book
    ) {
        validate_symbol(symbol);

        const OrderBook snapshot(order_book);

        const DepthSummary depth =
            DepthAnalytics::analyze(
                snapshot,
                depth_levels_
            );

        OFISnapshot ofi_snapshot;

        const auto best_bid = snapshot.best_bid();
        const auto best_ask = snapshot.best_ask();

        if (best_bid) {
            ofi_snapshot.best_bid = OFILevel{
                best_bid->price_ticks,
                best_bid->quantity
            };
        }

        if (best_ask) {
            ofi_snapshot.best_ask = OFILevel{
                best_ask->price_ticks,
                best_ask->quantity
            };
        }

        // Prepare depth storage before updating OFI.
        auto& saved_depth = depth_by_symbol_[symbol];

        order_flow_imbalance_.update(
            symbol,
            ofi_snapshot
        );

        saved_depth = depth;
    }

    // Call after execution using its associated pre/post quotes.
    void record_market_impact(
        const std::string& symbol,
        const ExecutionResult& execution,
        const MarketQuote& before,
        const MarketQuote& after
    ) {
        validate_symbol(symbol);

        if (execution.executed_quantity == 0) {
            return;
        }

        MarketImpactSummary impact =
            MarketImpact::analyze(
                execution,
                before,
                after
            );

        impact_by_symbol_.insert_or_assign(
            symbol,
            std::move(impact)
        );
    }

    [[nodiscard]]
    std::optional<DepthSummary> latest_depth(
        const std::string& symbol
    ) const {
        const auto it = depth_by_symbol_.find(symbol);

        if (it == depth_by_symbol_.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    [[nodiscard]]
    std::optional<MarketImpactSummary> latest_impact(
        const std::string& symbol
    ) const {
        const auto it = impact_by_symbol_.find(symbol);

        if (it == impact_by_symbol_.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    [[nodiscard]]
    AnalyticsSnapshot snapshot(
        const std::string& symbol
    ) const {
        validate_symbol(symbol);

        return AnalyticsSnapshot{
            .rolling_vwap = rolling_vwap_.get_vwap(symbol),
            .trade_statistics =
                trade_statistics_.get_statistics(symbol),
            .latest_ofi =
                order_flow_imbalance_.latest_ofi(symbol),
            .rolling_ofi =
                order_flow_imbalance_.rolling_ofi(symbol),
            .depth = latest_depth(symbol),
            .market_impact = latest_impact(symbol)
        };
    }

    [[nodiscard]]
    std::size_t depth_levels() const noexcept {
        return depth_levels_;
    }

private:
    static void validate_symbol(
        const std::string& symbol
    ) {
        if (symbol.empty()) {
            throw std::invalid_argument(
                "Analytics symbol must not be empty"
            );
        }
    }

    RollingVWAP& rolling_vwap_;
    TradeStatistics& trade_statistics_;
    OrderFlowImbalance& order_flow_imbalance_;

    AnalyticsExecutionHandler execution_handler_;

    std::size_t depth_levels_;

    std::unordered_map<
        std::string,
        DepthSummary
    > depth_by_symbol_;

    std::unordered_map<
        std::string,
        MarketImpactSummary
    > impact_by_symbol_;
};

} // namespace cmarket


#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace cmarket {

enum class TradeAggressorSide {
    Buy,
    Sell,
    Unknown
};

struct TradeStatisticsSnapshot {
    std::size_t trade_count = 0;

    long double total_volume = 0.0L;
    long double average_trade_price = 0.0L;
    long double minimum_trade_price = 0.0L;
    long double maximum_trade_price = 0.0L;
    long double average_trade_size = 0.0L;

    long double buy_volume = 0.0L;
    long double sell_volume = 0.0L;
    long double unknown_volume = 0.0L;
};

class TradeStatistics {
public:
    void add_trade(
        const std::string& symbol,
        double price,
        double quantity,
        TradeAggressorSide side = TradeAggressorSide::Unknown
    ) {
        if (symbol.empty()) {
            throw std::invalid_argument(
                "Trade symbol cannot be empty"
            );
        }

        if (!std::isfinite(price) || price <= 0.0) {
            throw std::invalid_argument(
                "Trade price must be finite and positive"
            );
        }

        if (!std::isfinite(quantity) || quantity <= 0.0) {
            throw std::invalid_argument(
                "Trade quantity must be finite and positive"
            );
        }

        if (side != TradeAggressorSide::Buy &&
            side != TradeAggressorSide::Sell &&
            side != TradeAggressorSide::Unknown) {
            throw std::invalid_argument(
                "Invalid trade aggressor side"
            );
        }

        const long double trade_price = price;
        const long double trade_quantity = quantity;

        auto& state = symbols_[symbol];

        if (state.trade_count ==
            std::numeric_limits<std::size_t>::max()) {
            throw std::overflow_error(
                "Trade count overflow"
            );
        }

        state.trade_count++;

        state.total_volume += trade_quantity;
        state.total_price += trade_price;

        if (state.trade_count == 1) {
            state.minimum_price = trade_price;
            state.maximum_price = trade_price;
        } else {
            if (trade_price < state.minimum_price) {
                state.minimum_price = trade_price;
            }

            if (trade_price > state.maximum_price) {
                state.maximum_price = trade_price;
            }
        }

        switch (side) {
            case TradeAggressorSide::Buy:
                state.buy_volume += trade_quantity;
                break;

            case TradeAggressorSide::Sell:
                state.sell_volume += trade_quantity;
                break;

            case TradeAggressorSide::Unknown:
                state.unknown_volume += trade_quantity;
                break;
        }
    }

    std::optional<TradeStatisticsSnapshot> get_statistics(
        const std::string& symbol
    ) const {
        const auto it = symbols_.find(symbol);

        if (it == symbols_.end()) {
            return std::nullopt;
        }

        const auto& state = it->second;

        if (state.trade_count == 0) {
            return std::nullopt;
        }

        TradeStatisticsSnapshot result;

        result.trade_count = state.trade_count;
        result.total_volume = state.total_volume;

        result.average_trade_price =
            state.total_price /
            static_cast<long double>(state.trade_count);

        result.minimum_trade_price = state.minimum_price;
        result.maximum_trade_price = state.maximum_price;

        result.average_trade_size =
            state.total_volume /
            static_cast<long double>(state.trade_count);

        result.buy_volume = state.buy_volume;
        result.sell_volume = state.sell_volume;
        result.unknown_volume = state.unknown_volume;

        return result;
    }

    std::size_t trade_count(
        const std::string& symbol
    ) const {
        const auto it = symbols_.find(symbol);

        if (it == symbols_.end()) {
            return 0;
        }

        return it->second.trade_count;
    }

    void clear_symbol(const std::string& symbol) {
        symbols_.erase(symbol);
    }

    void clear() noexcept {
        symbols_.clear();
    }

private:
    struct SymbolState {
        std::size_t trade_count = 0;

        long double total_volume = 0.0L;
        long double total_price = 0.0L;

        long double minimum_price = 0.0L;
        long double maximum_price = 0.0L;

        long double buy_volume = 0.0L;
        long double sell_volume = 0.0L;
        long double unknown_volume = 0.0L;
    };

    std::unordered_map<std::string, SymbolState> symbols_;
};

} // namespace cmarket

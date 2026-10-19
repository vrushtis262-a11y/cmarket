
#pragma once

#include <cmath>
#include <cstddef>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace cmarket {

class RollingVWAP {
public:
    explicit RollingVWAP(std::size_t window_size)
        : window_size_(window_size) {
        if (window_size_ == 0) {
            throw std::invalid_argument(
                "VWAP window size must be positive"
            );
        }
    }

    void add_trade(
        const std::string& symbol,
        double price,
        double quantity
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

        auto& state = symbols_[symbol];

        if (state.trades.size() == window_size_) {
            const auto& oldest = state.trades.front();

            state.total_notional -=
                static_cast<long double>(oldest.first) *
                oldest.second;

            state.total_volume -= oldest.second;

            state.trades.pop_front();
        }

        state.trades.emplace_back(price, quantity);

        state.total_notional +=
            static_cast<long double>(price) * quantity;

        state.total_volume += quantity;
    }

    std::optional<double> get_vwap(
        const std::string& symbol
    ) const {
        const auto it = symbols_.find(symbol);

        if (it == symbols_.end()) {
            return std::nullopt;
        }

        const auto& state = it->second;

        if (state.trades.empty() ||
            state.total_volume <= 0.0L) {
            return std::nullopt;
        }

        return static_cast<double>(
            state.total_notional / state.total_volume
        );
    }

    std::size_t trade_count(
        const std::string& symbol
    ) const {
        const auto it = symbols_.find(symbol);

        if (it == symbols_.end()) {
            return 0;
        }

        return it->second.trades.size();
    }

    std::size_t window_size() const noexcept {
        return window_size_;
    }

    void clear_symbol(const std::string& symbol) {
        symbols_.erase(symbol);
    }

    void clear() noexcept {
        symbols_.clear();
    }

private:
    struct SymbolState {
        std::deque<std::pair<double, double>> trades;
        long double total_notional = 0.0L;
        long double total_volume = 0.0L;
    };

    std::size_t window_size_;

    std::unordered_map<std::string, SymbolState> symbols_;
};

} // namespace cmarket

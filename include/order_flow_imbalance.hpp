
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace cmarket {

struct OFILevel {
    std::int64_t price_ticks;
    std::int64_t quantity;
};

struct OFISnapshot {
    std::optional<OFILevel> best_bid;
    std::optional<OFILevel> best_ask;
};

class OrderFlowImbalance {
public:
    explicit OrderFlowImbalance(std::size_t window_size)
        : window_size_(window_size)
    {
        if (window_size == 0) {
            throw std::invalid_argument(
                "OFI window size must be positive"
            );
        }
    }

    // Calculate top-of-book OFI from consecutive snapshots.
    void update(
        const std::string& symbol,
        const OFISnapshot& snapshot
    ) {
        if (symbol.empty()) {
            throw std::invalid_argument(
                "OFI symbol cannot be empty"
            );
        }

        validate_level(snapshot.best_bid);
        validate_level(snapshot.best_ask);

        if (snapshot.best_bid && snapshot.best_ask &&
            snapshot.best_bid->price_ticks >=
                snapshot.best_ask->price_ticks) {
            throw std::invalid_argument(
                "OFI snapshot has a crossed or locked book"
            );
        }

        auto& state = symbols_[symbol];

        if (!state.initialized) {
            state.previous = snapshot;
            state.initialized = true;
            return;
        }

        const long double bid_change =
            bid_contribution(
                state.previous.best_bid,
                snapshot.best_bid
            );

        const long double ask_change =
            ask_contribution(
                state.previous.best_ask,
                snapshot.best_ask
            );

        const long double event_ofi =
            bid_change - ask_change;

        if (state.events.size() == window_size_) {
            state.rolling_total -= state.events.front();
            state.events.pop_front();
        }

        state.events.push_back(event_ofi);
        state.rolling_total += event_ofi;
        state.latest_event = event_ofi;
        state.previous = snapshot;
    }

    std::optional<long double> latest_ofi(
        const std::string& symbol
    ) const {
        const auto it = symbols_.find(symbol);

        if (it == symbols_.end() ||
            it->second.events.empty()) {
            return std::nullopt;
        }

        return it->second.latest_event;
    }

    std::optional<long double> rolling_ofi(
        const std::string& symbol
    ) const {
        const auto it = symbols_.find(symbol);

        if (it == symbols_.end() ||
            it->second.events.empty()) {
            return std::nullopt;
        }

        return it->second.rolling_total;
    }

    std::size_t event_count(
        const std::string& symbol
    ) const {
        const auto it = symbols_.find(symbol);

        if (it == symbols_.end()) {
            return 0;
        }

        return it->second.events.size();
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
        bool initialized = false;
        OFISnapshot previous;

        std::deque<long double> events;

        long double rolling_total = 0.0L;
        long double latest_event = 0.0L;
    };

    static void validate_level(
        const std::optional<OFILevel>& level
    ) {
        if (!level) {
            return;
        }

        if (level->price_ticks <= 0 ||
            level->quantity <= 0) {
            throw std::invalid_argument(
                "OFI price and quantity must be positive"
            );
        }
    }

    static long double bid_contribution(
        const std::optional<OFILevel>& previous,
        const std::optional<OFILevel>& current
    ) {
        if (!previous && !current) {
            return 0.0L;
        }

        if (!previous) {
            return static_cast<long double>(
                current->quantity
            );
        }

        if (!current) {
            return -static_cast<long double>(
                previous->quantity
            );
        }

        if (current->price_ticks >
            previous->price_ticks) {
            return static_cast<long double>(
                current->quantity
            );
        }

        if (current->price_ticks <
            previous->price_ticks) {
            return -static_cast<long double>(
                previous->quantity
            );
        }

        return
            static_cast<long double>(current->quantity) -
            static_cast<long double>(previous->quantity);
    }

    static long double ask_contribution(
        const std::optional<OFILevel>& previous,
        const std::optional<OFILevel>& current
    ) {
        if (!previous && !current) {
            return 0.0L;
        }

        if (!previous) {
            return static_cast<long double>(
                current->quantity
            );
        }

        if (!current) {
            return -static_cast<long double>(
                previous->quantity
            );
        }

        if (current->price_ticks <
            previous->price_ticks) {
            return static_cast<long double>(
                current->quantity
            );
        }

        if (current->price_ticks >
            previous->price_ticks) {
            return -static_cast<long double>(
                previous->quantity
            );
        }

        return
            static_cast<long double>(current->quantity) -
            static_cast<long double>(previous->quantity);
    }

    std::size_t window_size_;

    std::unordered_map<std::string, SymbolState> symbols_;
};

} // namespace cmarket

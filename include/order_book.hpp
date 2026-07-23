#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct PriceLevel {
    std::int64_t price_ticks;
    std::int64_t quantity;
};

class OrderBook {
public:
    static constexpr std::int64_t ticks_per_unit = 1'000'000;

    void replace_snapshot(
        std::vector<PriceLevel> bids,
        std::vector<PriceLevel> asks
    );

    [[nodiscard]]
    const std::vector<PriceLevel>& bids() const noexcept;

    [[nodiscard]]
    const std::vector<PriceLevel>& asks() const noexcept;

    [[nodiscard]]
    std::optional<PriceLevel> best_bid() const noexcept;

    [[nodiscard]]
    std::optional<PriceLevel> best_ask() const noexcept;

    [[nodiscard]]
    std::optional<std::int64_t> spread_ticks() const noexcept;

    [[nodiscard]]
    std::optional<std::int64_t> mid_price_ticks() const noexcept;

    [[nodiscard]]
    std::int64_t bid_depth() const noexcept;

    [[nodiscard]]
    std::int64_t ask_depth() const noexcept;

    [[nodiscard]]
    std::int64_t total_depth() const noexcept;

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    static std::int64_t price_to_ticks(const std::string& price);

    [[nodiscard]]
    static std::string format_price(
        std::int64_t price_ticks,
        std::size_t decimal_places = 3
    );

private:
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;
};
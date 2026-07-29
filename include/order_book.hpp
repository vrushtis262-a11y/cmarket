#ifndef CMARKET_ORDER_BOOK_HPP
#define CMARKET_ORDER_BOOK_HPP

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

    void update_bid(
        std::int64_t price_ticks,
        std::int64_t quantity
    );

    void update_ask(
        std::int64_t price_ticks,
        std::int64_t quantity
    );

    [[nodiscard]]
    const std::vector<PriceLevel>&
    bids() const noexcept;

    [[nodiscard]]
    const std::vector<PriceLevel>&
    asks() const noexcept;

    [[nodiscard]]
    std::optional<PriceLevel>
    best_bid() const noexcept;

    [[nodiscard]]
    std::optional<PriceLevel>
    best_ask() const noexcept;

    [[nodiscard]]
    std::optional<std::int64_t>
    spread_ticks() const noexcept;

    [[nodiscard]]
    std::optional<std::int64_t>
    mid_price_ticks() const noexcept;

    [[nodiscard]]
    std::int64_t
    bid_depth() const noexcept;

    [[nodiscard]]
    std::int64_t
    ask_depth() const noexcept;

    [[nodiscard]]
    std::int64_t
    total_depth() const noexcept;

    [[nodiscard]]
    std::optional<double>
    order_book_imbalance() const noexcept;

    [[nodiscard]]
    std::optional<std::int64_t>
    bid_vwap_ticks() const noexcept;

    [[nodiscard]]
    std::optional<std::int64_t>
    ask_vwap_ticks() const noexcept;

    [[nodiscard]]
    std::optional<std::int64_t>
    microprice_ticks() const noexcept;

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    static std::int64_t price_to_ticks(
        const std::string& price
    );

    [[nodiscard]]
    static std::int64_t quantity_to_fixed(
        const std::string& quantity
    );

    [[nodiscard]]
    static std::string format_price(
        std::int64_t price_ticks,
        std::size_t decimal_places = 6
    );

private:
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;
};

#endif
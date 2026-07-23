#include "order_book.hpp"

#include <cctype>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

std::vector<PriceLevel> normalize_bids(
    const std::vector<PriceLevel>& levels
)
{
    std::map<std::int64_t, std::int64_t, std::greater<>> aggregated;

    for (const PriceLevel& level : levels) {
        if (level.price_ticks < 0) {
            throw std::invalid_argument(
                "Bid price cannot be negative."
            );
        }

        if (level.quantity < 0) {
            throw std::invalid_argument(
                "Bid quantity cannot be negative."
            );
        }

        if (level.quantity == 0) {
            continue;
        }

        std::int64_t& quantity = aggregated[level.price_ticks];

        if (level.quantity >
            std::numeric_limits<std::int64_t>::max() - quantity) {
            throw std::overflow_error(
                "Duplicate bid quantity overflow."
            );
        }

        quantity += level.quantity;
    }

    std::vector<PriceLevel> normalized;
    normalized.reserve(aggregated.size());

    for (const auto& [price_ticks, quantity] : aggregated) {
        normalized.push_back({price_ticks, quantity});
    }

    return normalized;
}

std::vector<PriceLevel> normalize_asks(
    const std::vector<PriceLevel>& levels
)
{
    std::map<std::int64_t, std::int64_t> aggregated;

    for (const PriceLevel& level : levels) {
        if (level.price_ticks < 0) {
            throw std::invalid_argument(
                "Ask price cannot be negative."
            );
        }

        if (level.quantity < 0) {
            throw std::invalid_argument(
                "Ask quantity cannot be negative."
            );
        }

        if (level.quantity == 0) {
            continue;
        }

        std::int64_t& quantity = aggregated[level.price_ticks];

        if (level.quantity >
            std::numeric_limits<std::int64_t>::max() - quantity) {
            throw std::overflow_error(
                "Duplicate ask quantity overflow."
            );
        }

        quantity += level.quantity;
    }

    std::vector<PriceLevel> normalized;
    normalized.reserve(aggregated.size());

    for (const auto& [price_ticks, quantity] : aggregated) {
        normalized.push_back({price_ticks, quantity});
    }

    return normalized;
}

std::int64_t calculate_depth(
    const std::vector<PriceLevel>& levels
)
{
    std::int64_t depth = 0;

    for (const PriceLevel& level : levels) {
        if (level.quantity >
            std::numeric_limits<std::int64_t>::max() - depth) {
            throw std::overflow_error("Order-book depth overflow.");
        }

        depth += level.quantity;
    }

    return depth;
}

} // namespace

void OrderBook::replace_snapshot(
    std::vector<PriceLevel> bids,
    std::vector<PriceLevel> asks
)
{
    std::vector<PriceLevel> normalized_bids =
        normalize_bids(bids);

    std::vector<PriceLevel> normalized_asks =
        normalize_asks(asks);

    bids_ = std::move(normalized_bids);
    asks_ = std::move(normalized_asks);
}

const std::vector<PriceLevel>&
OrderBook::bids() const noexcept
{
    return bids_;
}

const std::vector<PriceLevel>&
OrderBook::asks() const noexcept
{
    return asks_;
}

std::optional<PriceLevel>
OrderBook::best_bid() const noexcept
{
    if (bids_.empty()) {
        return std::nullopt;
    }

    return bids_.front();
}

std::optional<PriceLevel>
OrderBook::best_ask() const noexcept
{
    if (asks_.empty()) {
        return std::nullopt;
    }

    return asks_.front();
}

std::optional<std::int64_t>
OrderBook::spread_ticks() const noexcept
{
    if (bids_.empty() || asks_.empty()) {
        return std::nullopt;
    }

    return asks_.front().price_ticks -
           bids_.front().price_ticks;
}

std::optional<std::int64_t>
OrderBook::mid_price_ticks() const noexcept
{
    if (bids_.empty() || asks_.empty()) {
        return std::nullopt;
    }

    const std::int64_t bid =
        bids_.front().price_ticks;

    const std::int64_t ask =
        asks_.front().price_ticks;

    return bid + ((ask - bid) / 2);
}

std::int64_t
OrderBook::bid_depth() const noexcept
{
    try {
        return calculate_depth(bids_);
    }
    catch (...) {
        return std::numeric_limits<std::int64_t>::max();
    }
}

std::int64_t
OrderBook::ask_depth() const noexcept
{
    try {
        return calculate_depth(asks_);
    }
    catch (...) {
        return std::numeric_limits<std::int64_t>::max();
    }
}

std::int64_t
OrderBook::total_depth() const noexcept
{
    const std::int64_t bid_total = bid_depth();
    const std::int64_t ask_total = ask_depth();

    if (ask_total >
        std::numeric_limits<std::int64_t>::max() - bid_total) {
        return std::numeric_limits<std::int64_t>::max();
    }

    return bid_total + ask_total;
}

bool OrderBook::empty() const noexcept
{
    return bids_.empty() && asks_.empty();
}

std::int64_t OrderBook::price_to_ticks(
    const std::string& price
)
{
    if (price.empty()) {
        throw std::invalid_argument(
            "Price cannot be empty."
        );
    }

    std::size_t position = 0;

    if (price[position] == '+') {
        ++position;
    }
    else if (price[position] == '-') {
        throw std::invalid_argument(
            "Price cannot be negative."
        );
    }

    if (position == price.size()) {
        throw std::invalid_argument("Invalid price.");
    }

    std::int64_t whole_part = 0;
    bool has_whole_digits = false;

    while (
        position < price.size() &&
        std::isdigit(
            static_cast<unsigned char>(price[position])
        )
    ) {
        has_whole_digits = true;

        const int digit = price[position] - '0';

        if (
            whole_part >
            (
                std::numeric_limits<std::int64_t>::max() -
                digit
            ) / 10
        ) {
            throw std::overflow_error(
                "Price is too large."
            );
        }

        whole_part = whole_part * 10 + digit;
        ++position;
    }

    std::int64_t fractional_part = 0;
    std::size_t fractional_digits = 0;

    if (
        position < price.size() &&
        price[position] == '.'
    ) {
        ++position;

        while (
            position < price.size() &&
            std::isdigit(
                static_cast<unsigned char>(
                    price[position]
                )
            )
        ) {
            if (fractional_digits >= 6) {
                throw std::invalid_argument(
                    "Price supports at most 6 decimal places."
                );
            }

            fractional_part =
                fractional_part * 10 +
                (price[position] - '0');

            ++fractional_digits;
            ++position;
        }
    }

    if (!has_whole_digits && fractional_digits == 0) {
        throw std::invalid_argument("Invalid price.");
    }

    if (position != price.size()) {
        throw std::invalid_argument(
            "Price contains invalid characters."
        );
    }

    while (fractional_digits < 6) {
        fractional_part *= 10;
        ++fractional_digits;
    }

    if (
        whole_part >
        (
            std::numeric_limits<std::int64_t>::max() -
            fractional_part
        ) / ticks_per_unit
    ) {
        throw std::overflow_error(
            "Price is too large."
        );
    }

    return whole_part * ticks_per_unit +
           fractional_part;
}

std::string OrderBook::format_price(
    std::int64_t price_ticks,
    std::size_t decimal_places
)
{
    if (price_ticks < 0) {
        throw std::invalid_argument(
            "Price ticks cannot be negative."
        );
    }

    if (decimal_places > 6) {
        throw std::invalid_argument(
            "Price formatting supports at most 6 decimal places."
        );
    }

    const std::int64_t whole_part =
        price_ticks / ticks_per_unit;

    const std::int64_t fractional_part =
        price_ticks % ticks_per_unit;

    if (decimal_places == 0) {
        return std::to_string(whole_part);
    }

    std::ostringstream output;

    output
        << whole_part
        << '.'
        << std::setw(6)
        << std::setfill('0')
        << fractional_part;

    std::string formatted = output.str();

    formatted.resize(
        formatted.size() -
        (6 - decimal_places)
    );

    return formatted;
}
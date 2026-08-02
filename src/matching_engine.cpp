#include "matching_engine.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

MatchingEngine::MatchingEngine(
    OrderBook& order_book
) noexcept
    : order_book_(order_book)
{
}

ExecutionResult MatchingEngine::execute_market_buy(
    std::int64_t quantity
)
{
    if (quantity <= 0) {
        throw std::invalid_argument(
            "Market buy quantity must be positive."
        );
    }

    ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = quantity,
        .executed_quantity = 0,
        .remaining_quantity = quantity,
        .average_price_ticks = std::nullopt,
        .trades = {}
    };

    long double weighted_price_total = 0.0L;

    while (
        result.remaining_quantity > 0 &&
        !order_book_.asks().empty()
    ) {
        const PriceLevel best_ask =
            order_book_.asks().front();

        const std::int64_t executed_at_level =
            std::min(
                result.remaining_quantity,
                best_ask.quantity
            );

        result.trades.push_back(
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = best_ask.price_ticks,
                .quantity = executed_at_level
            }
        );

        result.executed_quantity += executed_at_level;
        result.remaining_quantity -= executed_at_level;

        weighted_price_total +=
            static_cast<long double>(
                best_ask.price_ticks
            ) *
            static_cast<long double>(
                executed_at_level
            );

        const std::int64_t remaining_at_level =
            best_ask.quantity - executed_at_level;

        order_book_.update_ask(
            best_ask.price_ticks,
            remaining_at_level
        );
    }

    if (result.executed_quantity > 0) {
        const long double average_price =
            weighted_price_total /
            static_cast<long double>(
                result.executed_quantity
            );

        if (
            average_price >
            static_cast<long double>(
                std::numeric_limits<std::int64_t>::max()
            )
        ) {
            throw std::overflow_error(
                "Average execution price overflow."
            );
        }

        result.average_price_ticks =
            static_cast<std::int64_t>(
                average_price
            );
    }

    return result;
}
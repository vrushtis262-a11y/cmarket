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

ExecutionResult MatchingEngine::execute_market_order(
    OrderSide side,
    std::int64_t quantity
)
{
    if (quantity <= 0) {
        throw std::invalid_argument(
            "Market order quantity must be positive."
        );
    }

    ExecutionResult result{
        .side = side,
        .requested_quantity = quantity,
        .executed_quantity = 0,
        .remaining_quantity = quantity,
        .average_price_ticks = std::nullopt,
        .trades = {}
    };

    long double weighted_price_total = 0.0L;

    while (result.remaining_quantity > 0) {
        const std::vector<PriceLevel>& levels =
            side == OrderSide::Buy
                ? order_book_.asks()
                : order_book_.bids();

        if (levels.empty()) {
            break;
        }

        const PriceLevel best_level = levels.front();

        const std::int64_t executed_at_level =
            std::min(
                result.remaining_quantity,
                best_level.quantity
            );

        const Trade trade{
            .aggressor_side = side,
            .price_ticks = best_level.price_ticks,
            .quantity = executed_at_level
        };

        result.trades.push_back(trade);
        trade_history_.push_back(trade);

        result.executed_quantity += executed_at_level;
        result.remaining_quantity -= executed_at_level;

        weighted_price_total +=
            static_cast<long double>(best_level.price_ticks) *
            static_cast<long double>(executed_at_level);

        const std::int64_t remaining_at_level =
            best_level.quantity - executed_at_level;

        if (side == OrderSide::Buy) {
            order_book_.update_ask(
                best_level.price_ticks,
                remaining_at_level
            );
        } else {
            order_book_.update_bid(
                best_level.price_ticks,
                remaining_at_level
            );
        }
    }

    if (result.executed_quantity > 0) {
        const long double average_price =
            weighted_price_total /
            static_cast<long double>(
                result.executed_quantity
            );

        result.average_price_ticks =
            static_cast<std::int64_t>(
                average_price
            );
    }

    return result;
}

ExecutionResult MatchingEngine::execute_market_buy(
    std::int64_t quantity
)
{
    return execute_market_order(
        OrderSide::Buy,
        quantity
    );
}

ExecutionResult MatchingEngine::execute_market_sell(
    std::int64_t quantity
)
{
    return execute_market_order(
        OrderSide::Sell,
        quantity
    );
}

OrderId MatchingEngine::place_limit_order(
    OrderSide side,
    std::int64_t price_ticks,
    std::int64_t quantity
)
{
    if (price_ticks <= 0) {
        throw std::invalid_argument(
            "Limit price must be positive."
        );
    }

    if (quantity <= 0) {
        throw std::invalid_argument(
            "Limit quantity must be positive."
        );
    }

    const OrderId order_id =
        order_id_generator_.next();

    active_limit_orders_.push_back(
        LimitOrder{
            .order_id = order_id,
            .side = side,
            .price_ticks = price_ticks,
            .original_quantity = quantity,
            .remaining_quantity = quantity,
            .sequence_number =
                next_sequence_number_++
        }
    );

    return order_id;
}

OrderId MatchingEngine::place_limit_buy(
    std::int64_t price_ticks,
    std::int64_t quantity
)
{
    return place_limit_order(
        OrderSide::Buy,
        price_ticks,
        quantity
    );
}

OrderId MatchingEngine::place_limit_sell(
    std::int64_t price_ticks,
    std::int64_t quantity
)
{
    return place_limit_order(
        OrderSide::Sell,
        price_ticks,
        quantity
    );
}

const std::vector<LimitOrder>&
MatchingEngine::active_limit_orders() const noexcept
{
    return active_limit_orders_;
}

const std::vector<Trade>&
MatchingEngine::trade_history() const noexcept
{
    return trade_history_;
}

void MatchingEngine::clear_trade_history() noexcept
{
    trade_history_.clear();
}
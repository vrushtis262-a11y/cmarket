#include "matching_engine.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>
#include <vector>

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

        const PriceLevel best_level =
            levels.front();

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

        result.executed_quantity +=
            executed_at_level;

        result.remaining_quantity -=
            executed_at_level;

        weighted_price_total +=
            static_cast<long double>(
                best_level.price_ticks
            ) *
            static_cast<long double>(
                executed_at_level
            );

        const std::int64_t remaining_at_level =
            best_level.quantity -
            executed_at_level;

        if (side == OrderSide::Buy) {
            order_book_.update_ask(
                best_level.price_ticks,
                remaining_at_level
            );
        }
        else {
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

    LimitOrder incoming_order{
        .order_id = order_id,
        .side = side,
        .price_ticks = price_ticks,
        .original_quantity = quantity,
        .remaining_quantity = quantity,
        .sequence_number =
            next_sequence_number_++
    };

    match_limit_order(incoming_order);

    if (!incoming_order.is_filled()) {
        order_manager_.add_order(
            incoming_order
        );
    }

    rebuild_order_book();

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

MatchingEngine::OrderIterator
MatchingEngine::find_best_match(
    const LimitOrder& incoming_order,
    std::vector<LimitOrder>& orders
)
{
    OrderIterator best_match =
        orders.end();

    for (
        auto iterator = orders.begin();
        iterator != orders.end();
        ++iterator
    ) {
        if (
            iterator->side ==
            incoming_order.side
        ) {
            continue;
        }

        const bool prices_cross =
            incoming_order.side ==
                OrderSide::Buy
            ? iterator->price_ticks <=
                incoming_order.price_ticks
            : iterator->price_ticks >=
                incoming_order.price_ticks;

        if (!prices_cross) {
            continue;
        }

        if (best_match == orders.end()) {
            best_match = iterator;
            continue;
        }

        const bool has_better_price =
            incoming_order.side ==
                OrderSide::Buy
            ? iterator->price_ticks <
                best_match->price_ticks
            : iterator->price_ticks >
                best_match->price_ticks;

        const bool has_same_price =
            iterator->price_ticks ==
            best_match->price_ticks;

        const bool has_earlier_time =
            iterator->sequence_number <
            best_match->sequence_number;

        if (
            has_better_price ||
            (
                has_same_price &&
                has_earlier_time
            )
        ) {
            best_match = iterator;
        }
    }

    return best_match;
}

void MatchingEngine::execute_limit_trade(
    LimitOrder& incoming_order,
    LimitOrder& resting_order
)
{
    const std::int64_t executed_quantity =
        std::min(
            incoming_order.remaining_quantity,
            resting_order.remaining_quantity
        );

    const Trade trade{
        .aggressor_side =
            incoming_order.side,
        .price_ticks =
            resting_order.price_ticks,
        .quantity =
            executed_quantity
    };

    trade_history_.push_back(trade);

    incoming_order.remaining_quantity -=
        executed_quantity;

    resting_order.remaining_quantity -=
        executed_quantity;
}

void MatchingEngine::match_limit_order(
    LimitOrder& incoming_order
)
{
    std::vector<LimitOrder>& orders =
        order_manager_.orders();

    while (incoming_order.remaining_quantity > 0) {
        OrderIterator best_match =
            find_best_match(
                incoming_order,
                orders
            );

        if (best_match == orders.end()) {
            break;
        }

        execute_limit_trade(
            incoming_order,
            *best_match
        );

        if (best_match->is_filled()) {
            orders.erase(best_match);
        }
    }
}

bool MatchingEngine::cancel_order(
    OrderId order_id
)
{
    const bool cancelled =
        order_manager_.cancel_order(
            order_id
        );

    if (!cancelled) {
        return false;
    }

    rebuild_order_book();

    return true;
}

bool MatchingEngine::modify_order(
    OrderId order_id,
    std::int64_t new_price_ticks,
    std::int64_t new_quantity
)
{
    if (new_price_ticks <= 0) {
        throw std::invalid_argument(
            "Modified limit price must be positive."
        );
    }

    if (new_quantity <= 0) {
        throw std::invalid_argument(
            "Modified limit quantity must be positive."
        );
    }

    const LimitOrder* existing_order =
        order_manager_.find_order(
            order_id
        );

    if (existing_order == nullptr) {
        return false;
    }

    LimitOrder modified_order =
        *existing_order;

    static_cast<void>(
        order_manager_.cancel_order(
            order_id
        )
    );

    const bool price_changed =
        new_price_ticks !=
        modified_order.price_ticks;

    const bool quantity_increased =
        new_quantity >
        modified_order.remaining_quantity;

    if (
        price_changed ||
        quantity_increased
    ) {
        modified_order.sequence_number =
            next_sequence_number_++;
    }

    modified_order.price_ticks =
        new_price_ticks;

    modified_order.original_quantity =
        new_quantity;

    modified_order.remaining_quantity =
        new_quantity;

    match_limit_order(
        modified_order
    );

    if (!modified_order.is_filled()) {
        order_manager_.add_order(
            modified_order
        );
    }

    rebuild_order_book();

    return true;
}

void MatchingEngine::rebuild_order_book()
{
    std::map<
        std::int64_t,
        std::int64_t,
        std::greater<>
    > bid_quantities;

    std::map<
        std::int64_t,
        std::int64_t
    > ask_quantities;

    for (
        const LimitOrder& order :
        order_manager_.orders()
    ) {
        if (order.is_filled()) {
            continue;
        }

        if (order.side == OrderSide::Buy) {
            bid_quantities[
                order.price_ticks
            ] += order.remaining_quantity;
        }
        else {
            ask_quantities[
                order.price_ticks
            ] += order.remaining_quantity;
        }
    }

    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;

    bids.reserve(
        bid_quantities.size()
    );

    asks.reserve(
        ask_quantities.size()
    );

    for (
        const auto& [
            price_ticks,
            quantity
        ] : bid_quantities
    ) {
        bids.push_back(
            PriceLevel{
                .price_ticks = price_ticks,
                .quantity = quantity
            }
        );
    }

    for (
        const auto& [
            price_ticks,
            quantity
        ] : ask_quantities
    ) {
        asks.push_back(
            PriceLevel{
                .price_ticks = price_ticks,
                .quantity = quantity
            }
        );
    }

    order_book_.replace_snapshot(
        bids,
        asks
    );
}

const std::vector<LimitOrder>&
MatchingEngine::active_limit_orders() const noexcept
{
    return order_manager_.orders();
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
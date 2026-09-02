#include "matching_engine.hpp"
#include "validation.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

MatchingEngine::MatchingEngine(
    OrderBook& order_book
) noexcept
    : order_book_(order_book)
{
}

void MatchingEngine::adjust_order_book(
    OrderSide side,
    std::int64_t price_ticks,
    std::int64_t quantity_delta
)
{
    if (side == OrderSide::Buy) {
        order_book_.adjust_bid(
            price_ticks,
            quantity_delta
        );

        return;
    }

    order_book_.adjust_ask(
        price_ticks,
        quantity_delta
    );
}

bool MatchingEngine::crosses_order_book(
    OrderSide side,
    std::int64_t price_ticks
) const noexcept
{
    if (side == OrderSide::Buy) {
        const std::optional<PriceLevel> best_ask =
            order_book_.best_ask();

        return
            best_ask.has_value() &&
            best_ask->price_ticks <=
                price_ticks;
    }

    const std::optional<PriceLevel> best_bid =
        order_book_.best_bid();

    return
        best_bid.has_value() &&
        best_bid->price_ticks >=
            price_ticks;
}

ExecutionResult MatchingEngine::execute_market_order(
    OrderSide side,
    std::int64_t quantity
)
{
    validation::validate_market_order(
        quantity
    );

    OrderBook order_book_backup =
        order_book_;

    OrderManager order_manager_backup =
        order_manager_;

    TradeStore trade_store_backup =
        trade_store_;

    try {
        ExecutionResult result{
            .side = side,
            .requested_quantity = quantity,
            .executed_quantity = 0,
            .remaining_quantity = quantity,
            .average_price_ticks = std::nullopt,
            .trades = {}
        };

        long double weighted_price_total = 0.0L;

        const bool uses_active_orders =
            !order_manager_.orders().empty();

        if (uses_active_orders) {
            while (
                result.remaining_quantity > 0
            ) {
                const LimitOrder* best_match =
                    nullptr;

                for (
                    const LimitOrder& order :
                    order_manager_.orders()
                ) {
                    const bool opposite_side =
                        side == OrderSide::Buy
                            ? order.side ==
                                OrderSide::Sell
                            : order.side ==
                                OrderSide::Buy;

                    if (!opposite_side) {
                        continue;
                    }

                    if (best_match == nullptr) {
                        best_match = &order;
                        continue;
                    }

                    const bool better_price =
                        side == OrderSide::Buy
                            ? order.price_ticks <
                                best_match->price_ticks
                            : order.price_ticks >
                                best_match->price_ticks;

                    const bool same_price =
                        order.price_ticks ==
                        best_match->price_ticks;

                    const bool earlier_sequence =
                        order.sequence_number <
                        best_match->sequence_number;

                    if (
                        better_price ||
                        (
                            same_price &&
                            earlier_sequence
                        )
                    ) {
                        best_match = &order;
                    }
                }

                if (best_match == nullptr) {
                    break;
                }

                const OrderId resting_order_id =
                    best_match->order_id;

                LimitOrder* resting_order =
                    order_manager_.find_order(
                        resting_order_id
                    );

                if (resting_order == nullptr) {
                    throw std::logic_error(
                        "Selected resting order disappeared."
                    );
                }

                const std::int64_t executed_quantity =
                    std::min(
                        result.remaining_quantity,
                        resting_order->remaining_quantity
                    );

                const std::optional<std::uint64_t>
                    buy_order_id =
                        side == OrderSide::Buy
                            ? std::nullopt
                            : std::optional<std::uint64_t>{
                                  resting_order->order_id
                              };

                const std::optional<std::uint64_t>
                    sell_order_id =
                        side == OrderSide::Sell
                            ? std::nullopt
                            : std::optional<std::uint64_t>{
                                  resting_order->order_id
                              };

                const Trade& recorded_trade =
                    trade_store_.record_trade(
                        side,
                        resting_order->price_ticks,
                        executed_quantity,
                        buy_order_id,
                        sell_order_id
                    );

                result.trades.push_back(
                    recorded_trade
                );

                result.executed_quantity +=
                    executed_quantity;

                result.remaining_quantity -=
                    executed_quantity;

                weighted_price_total +=
                    static_cast<long double>(
                        resting_order->price_ticks
                    ) *
                    static_cast<long double>(
                        executed_quantity
                    );

                adjust_order_book(
                    resting_order->side,
                    resting_order->price_ticks,
                    -executed_quantity
                );

                resting_order->remaining_quantity -=
                    executed_quantity;

                if (resting_order->is_filled()) {
                    const bool removed =
                        order_manager_.cancel_order(
                            resting_order_id
                        );

                    if (!removed) {
                        throw std::logic_error(
                            "Filled resting order could not "
                            "be removed."
                        );
                    }
                }
            }
        }
        else {
            while (
                result.remaining_quantity > 0
            ) {
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

                const Trade& recorded_trade =
                    trade_store_.record_trade(
                        side,
                        best_level.price_ticks,
                        executed_at_level
                    );

                result.trades.push_back(
                    recorded_trade
                );

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
    catch (...) {
        order_book_ =
            std::move(
                order_book_backup
            );

        order_manager_ =
            std::move(
                order_manager_backup
            );

        trade_store_ =
            std::move(
                trade_store_backup
            );

        throw;
    }
}

ExecutionResult
MatchingEngine::execute_market_buy(
    std::int64_t quantity
)
{
    return execute_market_order(
        OrderSide::Buy,
        quantity
    );
}

ExecutionResult
MatchingEngine::execute_market_sell(
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
    validation::validate_limit_order(
        price_ticks,
        quantity
    );

    OrderBook order_book_backup =
        order_book_;

    OrderIdGenerator order_id_generator_backup =
        order_id_generator_;

    const SequenceNumber sequence_number_backup =
        next_sequence_number_;

    const bool first_local_order =
        order_manager_.orders().empty();

    try {
        if (first_local_order) {
            order_book_.replace_normalized_snapshot(
                {},
                {}
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

        const bool crosses_existing_order =
            crosses_order_book(
                incoming_order.side,
                incoming_order.price_ticks
            );

        if (!crosses_existing_order) {
            adjust_order_book(
                incoming_order.side,
                incoming_order.price_ticks,
                incoming_order.remaining_quantity
            );

            try {
                order_manager_.add_order(
                    incoming_order
                );
            }
            catch (...) {
                order_book_ =
                    std::move(
                        order_book_backup
                    );

                order_id_generator_ =
                    std::move(
                        order_id_generator_backup
                    );

                next_sequence_number_ =
                    sequence_number_backup;

                throw;
            }

            return order_id;
        }

        OrderManager order_manager_backup =
            order_manager_;

        TradeStore trade_store_backup =
            trade_store_;

        try {
            match_limit_order(
                incoming_order
            );

            if (!incoming_order.is_filled()) {
                order_manager_.add_order(
                    incoming_order
                );

                adjust_order_book(
                    incoming_order.side,
                    incoming_order.price_ticks,
                    incoming_order.remaining_quantity
                );
            }

            return order_id;
        }
        catch (...) {
            order_book_ =
                std::move(
                    order_book_backup
                );

            order_id_generator_ =
                std::move(
                    order_id_generator_backup
                );

            next_sequence_number_ =
                sequence_number_backup;

            order_manager_ =
                std::move(
                    order_manager_backup
                );

            trade_store_ =
                std::move(
                    trade_store_backup
                );

            throw;
        }
    }
    catch (...) {
        order_book_ =
            std::move(
                order_book_backup
            );

        order_id_generator_ =
            std::move(
                order_id_generator_backup
            );

        next_sequence_number_ =
            sequence_number_backup;

        throw;
    }
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

std::optional<OrderId>
MatchingEngine::find_best_match(
    const LimitOrder& incoming_order
) const noexcept
{
    const std::vector<LimitOrder>& orders =
        order_manager_.orders();

    const LimitOrder* best_match =
        nullptr;

    for (
        const LimitOrder& order :
        orders
    ) {
        if (
            order.side ==
            incoming_order.side
        ) {
            continue;
        }

        const bool prices_cross =
            incoming_order.side ==
                OrderSide::Buy
            ? order.price_ticks <=
                incoming_order.price_ticks
            : order.price_ticks >=
                incoming_order.price_ticks;

        if (!prices_cross) {
            continue;
        }

        if (best_match == nullptr) {
            best_match = &order;
            continue;
        }

        const bool has_better_price =
            incoming_order.side ==
                OrderSide::Buy
            ? order.price_ticks <
                best_match->price_ticks
            : order.price_ticks >
                best_match->price_ticks;

        const bool has_same_price =
            order.price_ticks ==
            best_match->price_ticks;

        const bool has_earlier_time =
            order.sequence_number <
            best_match->sequence_number;

        if (
            has_better_price ||
            (
                has_same_price &&
                has_earlier_time
            )
        ) {
            best_match = &order;
        }
    }

    if (best_match == nullptr) {
        return std::nullopt;
    }

    return best_match->order_id;
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

    const OrderId buy_order_id =
        incoming_order.side ==
            OrderSide::Buy
        ? incoming_order.order_id
        : resting_order.order_id;

    const OrderId sell_order_id =
        incoming_order.side ==
            OrderSide::Sell
        ? incoming_order.order_id
        : resting_order.order_id;

    static_cast<void>(
        trade_store_.record_trade(
            incoming_order.side,
            resting_order.price_ticks,
            executed_quantity,
            buy_order_id,
            sell_order_id
        )
    );

    adjust_order_book(
        resting_order.side,
        resting_order.price_ticks,
        -executed_quantity
    );

    incoming_order.remaining_quantity -=
        executed_quantity;

    resting_order.remaining_quantity -=
        executed_quantity;
}

void MatchingEngine::process_limit_match(
    LimitOrder& incoming_order,
    OrderId resting_order_id
)
{
    LimitOrder* resting_order =
        order_manager_.find_order(
            resting_order_id
        );

    if (resting_order == nullptr) {
        return;
    }

    execute_limit_trade(
        incoming_order,
        *resting_order
    );

    if (resting_order->is_filled()) {
        static_cast<void>(
            order_manager_.cancel_order(
                resting_order_id
            )
        );
    }
}

void MatchingEngine::match_limit_order(
    LimitOrder& incoming_order
)
{
    while (
        incoming_order.remaining_quantity > 0
    ) {
        const std::optional<OrderId>
            best_match_id =
                find_best_match(
                    incoming_order
                );

        if (!best_match_id.has_value()) {
            break;
        }

        process_limit_match(
            incoming_order,
            *best_match_id
        );
    }
}

bool MatchingEngine::cancel_order(
    OrderId order_id
)
{
    const LimitOrder* existing_order =
        order_manager_.find_order(
            order_id
        );

    if (existing_order == nullptr) {
        return false;
    }

    const LimitOrder order_to_cancel =
        *existing_order;

    OrderBook order_book_backup =
        order_book_;

    OrderManager order_manager_backup =
        order_manager_;

    try {
        const bool cancelled =
            order_manager_.cancel_order(
                order_id
            );

        if (!cancelled) {
            return false;
        }

        adjust_order_book(
            order_to_cancel.side,
            order_to_cancel.price_ticks,
            -order_to_cancel.remaining_quantity
        );

        return true;
    }
    catch (...) {
        order_book_ =
            std::move(
                order_book_backup
            );

        order_manager_ =
            std::move(
                order_manager_backup
            );

        throw;
    }
}

bool MatchingEngine::modify_order(
    OrderId order_id,
    std::int64_t new_price_ticks,
    std::int64_t new_quantity
)
{
    validation::validate_modify_order(
        new_price_ticks,
        new_quantity
    );

    const LimitOrder* existing_order =
        order_manager_.find_order(
            order_id
        );

    if (existing_order == nullptr) {
        return false;
    }

    const LimitOrder original_order =
        *existing_order;

    LimitOrder modified_order =
        original_order;

    const bool price_changed =
        new_price_ticks !=
        modified_order.price_ticks;

    const bool quantity_increased =
        new_quantity >
        modified_order.remaining_quantity;

    const bool loses_priority =
        price_changed ||
        quantity_increased;

    modified_order.price_ticks =
        new_price_ticks;

    modified_order.original_quantity =
        new_quantity;

    modified_order.remaining_quantity =
        new_quantity;

    OrderBook order_book_backup =
        order_book_;

    const SequenceNumber
        sequence_number_backup =
            next_sequence_number_;

    OrderManager order_manager_backup =
        order_manager_;

    TradeStore trade_store_backup =
        trade_store_;

    try {
        if (loses_priority) {
            modified_order.sequence_number =
                next_sequence_number_++;
        }

        const bool removed =
            order_manager_.cancel_order(
                order_id
            );

        if (!removed) {
            return false;
        }

        adjust_order_book(
            original_order.side,
            original_order.price_ticks,
            -original_order.remaining_quantity
        );

        match_limit_order(
            modified_order
        );

        if (!modified_order.is_filled()) {
            order_manager_.add_order(
                modified_order
            );

            adjust_order_book(
                modified_order.side,
                modified_order.price_ticks,
                modified_order.remaining_quantity
            );
        }

        return true;
    }
    catch (...) {
        order_book_ =
            std::move(
                order_book_backup
            );

        next_sequence_number_ =
            sequence_number_backup;

        order_manager_ =
            std::move(
                order_manager_backup
            );

        trade_store_ =
            std::move(
                trade_store_backup
            );

        throw;
    }
}

const std::vector<LimitOrder>&
MatchingEngine::active_limit_orders() const noexcept
{
    return order_manager_.orders();
}

const std::vector<Trade>&
MatchingEngine::trade_history() const noexcept
{
    return trade_store_.trades();
}

void MatchingEngine::clear_trade_history() noexcept
{
    trade_store_.clear();
}
#ifndef CMARKET_MATCHING_ENGINE_HPP
#define CMARKET_MATCHING_ENGINE_HPP

#include "execution.hpp"
#include "limit_order.hpp"
#include "order_book.hpp"
#include "order_id_generator.hpp"
#include "order_manager.hpp"
#include "trade_store.hpp"

#include <cstdint>
#include <optional>
#include <vector>

class MatchingEngine {
public:
    explicit MatchingEngine(OrderBook& order_book) noexcept;

    [[nodiscard]]
    ExecutionResult execute_market_buy(
        std::int64_t quantity
    );

    [[nodiscard]]
    ExecutionResult execute_market_sell(
        std::int64_t quantity
    );

    [[nodiscard]]
    OrderId place_limit_buy(
        std::int64_t price_ticks,
        std::int64_t quantity
    );

    [[nodiscard]]
    OrderId place_limit_sell(
        std::int64_t price_ticks,
        std::int64_t quantity
    );

    [[nodiscard]]
    bool cancel_order(OrderId order_id);

    [[nodiscard]]
    bool modify_order(
        OrderId order_id,
        std::int64_t new_price_ticks,
        std::int64_t new_quantity
    );

    [[nodiscard]]
    const std::vector<LimitOrder>&
    active_limit_orders() const noexcept;

    [[nodiscard]]
    const std::vector<Trade>&
    trade_history() const noexcept;

    void clear_trade_history() noexcept;

private:
    [[nodiscard]]
    ExecutionResult execute_market_order(
        OrderSide side,
        std::int64_t quantity
    );

    [[nodiscard]]
    OrderId place_limit_order(
        OrderSide side,
        std::int64_t price_ticks,
        std::int64_t quantity
    );

    void match_limit_order(
        LimitOrder& incoming_order
    );

    [[nodiscard]]
    std::optional<OrderId> find_best_match(
        const LimitOrder& incoming_order
    ) const noexcept;

    void execute_limit_trade(
        LimitOrder& incoming_order,
        LimitOrder& resting_order
    );

    void process_limit_match(
        LimitOrder& incoming_order,
        OrderId resting_order_id
    );

    void rebuild_order_book();

    OrderBook& order_book_;
    OrderIdGenerator order_id_generator_;
    SequenceNumber next_sequence_number_ = 1;
    OrderManager order_manager_;
    TradeStore trade_store_;
};

#endif
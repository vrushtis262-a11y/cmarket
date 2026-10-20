
#pragma once

#include "matching_engine.hpp"
#include "ofi_order_book_handler.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace cmarket {

class OFIMatchingEngineAdapter {
public:
    using Quantity = std::int64_t;
    using Price = std::int64_t;

    OFIMatchingEngineAdapter(
        MatchingEngine& engine,
        OrderBook& order_book,
        OFIOrderBookHandler& handler,
        std::string symbol
    )
        : engine_(engine),
          order_book_(order_book),
          handler_(handler),
          symbol_(std::move(symbol))
    {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "OFI symbol must not be empty"
            );
        }
    }

    void initialize() {
        capture();
    }

    [[nodiscard]]
    OrderId place_limit_buy(
        Price price_ticks,
        Quantity quantity
    ) {
        const OrderId id =
            engine_.place_limit_buy(
                price_ticks,
                quantity
            );

        capture_after_commit();
        return id;
    }

    [[nodiscard]]
    OrderId place_limit_sell(
        Price price_ticks,
        Quantity quantity
    ) {
        const OrderId id =
            engine_.place_limit_sell(
                price_ticks,
                quantity
            );

        capture_after_commit();
        return id;
    }

    [[nodiscard]]
    bool cancel_order(OrderId order_id) {
        const bool cancelled =
            engine_.cancel_order(order_id);

        if (cancelled) {
            capture_after_commit();
        }

        return cancelled;
    }

    [[nodiscard]]
    bool modify_order(
        OrderId order_id,
        Price new_price_ticks,
        Quantity new_quantity
    ) {
        const bool modified =
            engine_.modify_order(
                order_id,
                new_price_ticks,
                new_quantity
            );

        if (modified) {
            capture_after_commit();
        }

        return modified;
    }

    [[nodiscard]]
    ExecutionResult execute_market_buy(
        Quantity quantity
    ) {
        ExecutionResult result =
            engine_.execute_market_buy(quantity);

        if (result.executed_quantity > 0) {
            capture_after_commit();
        }

        return result;
    }

    [[nodiscard]]
    ExecutionResult execute_market_sell(
        Quantity quantity
    ) {
        ExecutionResult result =
            engine_.execute_market_sell(quantity);

        if (result.executed_quantity > 0) {
            capture_after_commit();
        }

        return result;
    }

    [[nodiscard]]
    bool analytics_healthy() const noexcept {
        return analytics_healthy_;
    }

    [[nodiscard]]
    const std::string& symbol() const noexcept {
        return symbol_;
    }

private:
    void capture() {
        handler_.capture(
            symbol_,
            order_book_
        );
    }

    void capture_after_commit() noexcept {
        try {
            capture();
        }
        catch (...) {
            // Preserve committed trading results if OFI fails.
            analytics_healthy_ = false;
        }
    }

    MatchingEngine& engine_;
    OrderBook& order_book_;
    OFIOrderBookHandler& handler_;
    std::string symbol_;

    bool analytics_healthy_ = true;
};

} // namespace cmarket


#pragma once

#include "concurrent_matching_engine.hpp"
#include "ofi_order_book_handler.hpp"

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace cmarket {

class OFIConcurrentMatchingAdapter {
public:
    using Quantity = std::int64_t;
    using Price = std::int64_t;

    OFIConcurrentMatchingAdapter(
        ConcurrentMatchingEngine& engine,
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
        std::lock_guard<std::mutex> lock(mutex_);
        capture();
    }

    [[nodiscard]]
    OrderId place_limit_buy(
        Price price_ticks,
        Quantity quantity
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        const OrderId id = engine_
            .place_limit_buy(price_ticks, quantity)
            .get();

        capture_after_commit();
        return id;
    }

    [[nodiscard]]
    OrderId place_limit_sell(
        Price price_ticks,
        Quantity quantity
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        const OrderId id = engine_
            .place_limit_sell(price_ticks, quantity)
            .get();

        capture_after_commit();
        return id;
    }

    [[nodiscard]]
    bool cancel_order(OrderId order_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        const bool cancelled = engine_
            .cancel_order(order_id)
            .get();

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
        std::lock_guard<std::mutex> lock(mutex_);

        const bool modified = engine_
            .modify_order(
                order_id,
                new_price_ticks,
                new_quantity
            )
            .get();

        if (modified) {
            capture_after_commit();
        }

        return modified;
    }

    [[nodiscard]]
    ExecutionResult execute_market_buy(
        Quantity quantity
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        ExecutionResult result = engine_
            .execute_market_buy(quantity)
            .get();

        if (result.executed_quantity > 0) {
            capture_after_commit();
        }

        return result;
    }

    [[nodiscard]]
    ExecutionResult execute_market_sell(
        Quantity quantity
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        ExecutionResult result = engine_
            .execute_market_sell(quantity)
            .get();

        if (result.executed_quantity > 0) {
            capture_after_commit();
        }

        return result;
    }

    [[nodiscard]]
    bool analytics_healthy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return analytics_healthy_;
    }

    [[nodiscard]]
    const std::string& symbol() const noexcept {
        return symbol_;
    }

private:
    void capture() {
        handler_.capture(symbol_, order_book_);
    }

    void capture_after_commit() noexcept {
        try {
            capture();
        }
        catch (...) {
            // Keep committed trading results independent of OFI failures.
            analytics_healthy_ = false;
        }
    }

    ConcurrentMatchingEngine& engine_;
    OrderBook& order_book_;
    OFIOrderBookHandler& handler_;
    std::string symbol_;

    mutable std::mutex mutex_;
    bool analytics_healthy_ = true;
};

} // namespace cmarket

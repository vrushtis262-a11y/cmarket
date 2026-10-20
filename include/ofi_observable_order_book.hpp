
#pragma once

#include "ofi_order_book_handler.hpp"
#include "order_book.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cmarket {

class OFIObservableOrderBook {
public:
    OFIObservableOrderBook(
        OrderBook& order_book,
        OFIOrderBookHandler& handler,
        std::string symbol
    )
        : order_book_(order_book),
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
        handler_.capture(symbol_, order_book_);
    }

    void update_bid(
        std::int64_t price_ticks,
        std::int64_t quantity
    ) {
        order_book_.update_bid(price_ticks, quantity);
        handler_.capture(symbol_, order_book_);
    }

    void update_ask(
        std::int64_t price_ticks,
        std::int64_t quantity
    ) {
        order_book_.update_ask(price_ticks, quantity);
        handler_.capture(symbol_, order_book_);
    }

    void adjust_bid(
        std::int64_t price_ticks,
        std::int64_t quantity_delta
    ) {
        order_book_.adjust_bid(
            price_ticks,
            quantity_delta
        );

        handler_.capture(symbol_, order_book_);
    }

    void adjust_ask(
        std::int64_t price_ticks,
        std::int64_t quantity_delta
    ) {
        order_book_.adjust_ask(
            price_ticks,
            quantity_delta
        );

        handler_.capture(symbol_, order_book_);
    }

    void replace_snapshot(
        std::vector<PriceLevel> bids,
        std::vector<PriceLevel> asks
    ) {
        order_book_.replace_snapshot(
            std::move(bids),
            std::move(asks)
        );

        handler_.capture(symbol_, order_book_);
    }

    [[nodiscard]]
    const std::string& symbol() const noexcept {
        return symbol_;
    }

private:
    OrderBook& order_book_;
    OFIOrderBookHandler& handler_;
    std::string symbol_;
};

} // namespace cmarket

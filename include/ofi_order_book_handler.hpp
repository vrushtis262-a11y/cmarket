
#pragma once

#include "order_book.hpp"
#include "order_flow_imbalance.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace cmarket {

class OFIOrderBookHandler {
public:
    explicit OFIOrderBookHandler(
        OrderFlowImbalance& ofi,
        std::function<void()> before_capture = {}
    )
        : ofi_(ofi),
          before_capture_(
              std::move(before_capture)
          )
    {
    }

    // Capture a consistent book snapshot and update OFI.
    void capture(
        const std::string& symbol,
        const OrderBook& order_book
    ) {
        if (symbol.empty()) {
            throw std::invalid_argument(
                "OFI symbol must not be empty"
            );
        }

        if (before_capture_) {
            before_capture_();
        }

        const OrderBook snapshot_book(order_book);

        OFISnapshot snapshot;

        const auto bid = snapshot_book.best_bid();

        if (bid.has_value()) {
            snapshot.best_bid = OFILevel{
                bid->price_ticks,
                bid->quantity
            };
        }

        const auto ask = snapshot_book.best_ask();

        if (ask.has_value()) {
            snapshot.best_ask = OFILevel{
                ask->price_ticks,
                ask->quantity
            };
        }

        ofi_.update(symbol, snapshot);
    }

private:
    OrderFlowImbalance& ofi_;
    std::function<void()> before_capture_;
};

} // namespace cmarket

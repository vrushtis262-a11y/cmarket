#include "matching_engine.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

class ByteReader {
public:
    ByteReader(
        const std::uint8_t* data,
        std::size_t size
    ) noexcept
        : data_(data),
          size_(size)
    {
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return position_ >= size_;
    }

    [[nodiscard]]
    std::uint8_t next_byte() noexcept
    {
        if (empty()) {
            return 0;
        }

        return data_[position_++];
    }

    [[nodiscard]]
    std::uint64_t next_u64() noexcept
    {
        std::uint64_t value = 0;

        for (
            int shift = 0;
            shift < 64;
            shift += 8
        ) {
            value |=
                static_cast<std::uint64_t>(
                    next_byte()
                ) << shift;
        }

        return value;
    }

    [[nodiscard]]
    std::int64_t next_i64() noexcept
    {
        const std::uint64_t raw =
            next_u64();

        std::int64_t value = 0;

        static_assert(
            sizeof(value) == sizeof(raw)
        );

        std::memcpy(
            &value,
            &raw,
            sizeof(value)
        );

        return value;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t position_ = 0;
};

void check_engine_invariants(
    const MatchingEngine& engine,
    const OrderBook& order_book
)
{
    const auto& orders =
        engine.active_limit_orders();

    for (const LimitOrder& order : orders) {
        if (
            order.order_id == 0 ||
            order.price_ticks <= 0 ||
            order.original_quantity <= 0 ||
            order.remaining_quantity <= 0 ||
            order.remaining_quantity >
                order.original_quantity
        ) {
            std::terminate();
        }
    }

    const auto& bids =
        order_book.bids();

    const auto& asks =
        order_book.asks();

    for (
        std::size_t index = 0;
        index < bids.size();
        ++index
    ) {
        if (
            bids[index].price_ticks <= 0 ||
            bids[index].quantity <= 0
        ) {
            std::terminate();
        }

        if (
            index > 0 &&
            bids[index - 1].price_ticks <=
                bids[index].price_ticks
        ) {
            std::terminate();
        }
    }

    for (
        std::size_t index = 0;
        index < asks.size();
        ++index
    ) {
        if (
            asks[index].price_ticks <= 0 ||
            asks[index].quantity <= 0
        ) {
            std::terminate();
        }

        if (
            index > 0 &&
            asks[index - 1].price_ticks >=
                asks[index].price_ticks
        ) {
            std::terminate();
        }
    }

    if (
        !bids.empty() &&
        !asks.empty() &&
        bids.front().price_ticks >=
            asks.front().price_ticks
    ) {
        std::terminate();
    }

    const auto& trades =
        engine.trade_history();

    TradeId previous_trade_id = 0;

    TradeSequenceNumber
        previous_execution_sequence = 0;

    for (const Trade& trade : trades) {
        if (
            trade.trade_id == 0 ||
            trade.execution_sequence == 0 ||
            trade.price_ticks <= 0 ||
            trade.quantity <= 0
        ) {
            std::terminate();
        }

        if (
            trade.trade_id <=
                previous_trade_id ||
            trade.execution_sequence <=
                previous_execution_sequence
        ) {
            std::terminate();
        }

        previous_trade_id =
            trade.trade_id;

        previous_execution_sequence =
            trade.execution_sequence;
    }
}

void fuzz_one_input(
    const std::uint8_t* data,
    std::size_t size
)
{
    if (
        data == nullptr ||
        size == 0
    ) {
        return;
    }

    ByteReader reader(
        data,
        size
    );

    OrderBook order_book;

    MatchingEngine engine(
        order_book
    );

    while (!reader.empty()) {
        const std::uint8_t operation =
            reader.next_byte() % 4U;

        const OrderSide side =
            reader.next_byte() % 2U == 0U
                ? OrderSide::Buy
                : OrderSide::Sell;

        const std::int64_t price_ticks =
            reader.next_i64();

        const std::int64_t quantity =
            reader.next_i64();

        const OrderId order_id =
            static_cast<OrderId>(
                reader.next_u64()
            );

        try {
            switch (operation) {
            case 0:
                if (side == OrderSide::Buy) {
                    static_cast<void>(
                        engine.place_limit_buy(
                            price_ticks,
                            quantity
                        )
                    );
                }
                else {
                    static_cast<void>(
                        engine.place_limit_sell(
                            price_ticks,
                            quantity
                        )
                    );
                }
                break;

            case 1:
                if (side == OrderSide::Buy) {
                    static_cast<void>(
                        engine.execute_market_buy(
                            quantity
                        )
                    );
                }
                else {
                    static_cast<void>(
                        engine.execute_market_sell(
                            quantity
                        )
                    );
                }
                break;

            case 2:
                static_cast<void>(
                    engine.cancel_order(
                        order_id
                    )
                );
                break;

            case 3:
                static_cast<void>(
                    engine.modify_order(
                        order_id,
                        price_ticks,
                        quantity
                    )
                );
                break;
            }
        }
        catch (
            const std::invalid_argument&
        ) {
        }
        catch (
            const std::overflow_error&
        ) {
        }

        check_engine_invariants(
            engine,
            order_book
        );
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size
)
{
    fuzz_one_input(
        data,
        size
    );

    return 0;
}

#ifndef CMARKET_LIBFUZZER
int main()
{
    std::vector<std::uint8_t> input;

    std::uint8_t byte = 0;

    while (
        std::cin.read(
            reinterpret_cast<char*>(
                &byte
            ),
            1
        )
    ) {
        input.push_back(
            byte
        );
    }

    if (!input.empty()) {
        fuzz_one_input(
            input.data(),
            input.size()
        );
    }

    return 0;
}
#endif
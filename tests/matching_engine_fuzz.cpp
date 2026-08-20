#include "matching_engine.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
    std::int64_t next_positive_i64(
        std::int64_t maximum
    ) noexcept
    {
        if (maximum <= 0) {
            return 1;
        }

        const std::uint64_t value =
            next_u64();

        return static_cast<std::int64_t>(
            value %
            static_cast<std::uint64_t>(
                maximum
            )
        ) + 1;
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

    constexpr std::int64_t
        maximum_price = 1'000'000;

    constexpr std::int64_t
        maximum_quantity = 10'000;

    while (!reader.empty()) {
        const std::uint8_t operation =
            reader.next_byte() % 6;

        try {
            switch (operation) {
            case 0:
                static_cast<void>(
                    engine.place_limit_buy(
                        reader.next_positive_i64(
                            maximum_price
                        ),
                        reader.next_positive_i64(
                            maximum_quantity
                        )
                    )
                );
                break;

            case 1:
                static_cast<void>(
                    engine.place_limit_sell(
                        reader.next_positive_i64(
                            maximum_price
                        ),
                        reader.next_positive_i64(
                            maximum_quantity
                        )
                    )
                );
                break;

            case 2:
                static_cast<void>(
                    engine.execute_market_buy(
                        reader.next_positive_i64(
                            maximum_quantity
                        )
                    )
                );
                break;

            case 3:
                static_cast<void>(
                    engine.execute_market_sell(
                        reader.next_positive_i64(
                            maximum_quantity
                        )
                    )
                );
                break;

            case 4:
                static_cast<void>(
                    engine.cancel_order(
                        static_cast<OrderId>(
                            reader.next_u64()
                        )
                    )
                );
                break;

            case 5:
                static_cast<void>(
                    engine.modify_order(
                        static_cast<OrderId>(
                            reader.next_u64()
                        ),
                        reader.next_positive_i64(
                            maximum_price
                        ),
                        reader.next_positive_i64(
                            maximum_quantity
                        )
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
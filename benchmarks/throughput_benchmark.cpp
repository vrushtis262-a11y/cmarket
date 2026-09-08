#include "matching_engine.hpp"
#include "order_book.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::int64_t kQuantity = 10;
constexpr std::int64_t kBaseBidPrice = 1'000'000;

constexpr std::size_t kOperations = 100'000;
constexpr std::size_t kInitialBookSize = 100'000;

struct ThroughputResult {
    double elapsed_seconds;
    double operations_per_second;
};

template <typename Function>
ThroughputResult measure_throughput(
    std::size_t operations,
    Function&& function
)
{
    const auto start = Clock::now();

    function();

    const auto end = Clock::now();

    const double elapsed_seconds =
        std::chrono::duration<double>(
            end - start
        ).count();

    return {
        elapsed_seconds,
        static_cast<double>(operations) /
            elapsed_seconds
    };
}

void print_result(
    std::string_view workload,
    std::size_t operations,
    const ThroughputResult& result
)
{
    std::cout
        << std::left
        << std::setw(28)
        << workload
        << std::right
        << std::setw(14)
        << operations
        << std::setw(16)
        << std::fixed
        << std::setprecision(3)
        << result.elapsed_seconds
        << std::setw(18)
        << std::fixed
        << std::setprecision(0)
        << result.operations_per_second
        << '\n';
}

void populate_buy_orders(
    MatchingEngine& engine,
    std::size_t count
)
{
    for (
        std::size_t i = 0;
        i < count;
        ++i
    ) {
        const auto order_id =
            engine.place_limit_buy(
                kBaseBidPrice,
                kQuantity
            );

        (void)order_id;
    }
}

ThroughputResult benchmark_order_placement()
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    return measure_throughput(
        kOperations,
        [&]() {
            for (
                std::size_t i = 0;
                i < kOperations;
                ++i
            ) {
                const auto order_id =
                    engine.place_limit_buy(
                        kBaseBidPrice,
                        kQuantity
                    );

                (void)order_id;
            }
        }
    );
}

ThroughputResult benchmark_heavy_matching()
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_buy_orders(
        engine,
        kInitialBookSize
    );

    return measure_throughput(
        kOperations,
        [&]() {
            for (
                std::size_t i = 0;
                i < kOperations;
                ++i
            ) {
                const auto order_id =
                    engine.place_limit_sell(
                        kBaseBidPrice,
                        kQuantity
                    );

                (void)order_id;
            }
        }
    );
}

ThroughputResult benchmark_place_cancel_mix()
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_buy_orders(
        engine,
        kInitialBookSize
    );

    return measure_throughput(
        kOperations,
        [&]() {
            for (
                std::size_t i = 0;
                i < kOperations / 2;
                ++i
            ) {
                const auto order_id =
                    engine.place_limit_buy(
                        kBaseBidPrice,
                        kQuantity
                    );

                (void)order_id;

                const auto& orders =
                    engine.active_limit_orders();

                const OrderId target_order_id =
                    orders[
                        orders.size() / 2
                    ].order_id;

                const bool cancelled =
                    engine.cancel_order(
                        target_order_id
                    );

                (void)cancelled;
            }
        }
    );
}

ThroughputResult benchmark_place_modify_mix()
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_buy_orders(
        engine,
        kInitialBookSize
    );

    return measure_throughput(
        kOperations,
        [&]() {
            for (
                std::size_t i = 0;
                i < kOperations / 2;
                ++i
            ) {
                const auto order_id =
                    engine.place_limit_buy(
                        kBaseBidPrice,
                        kQuantity
                    );

                (void)order_id;

                const auto& orders =
                    engine.active_limit_orders();

                const auto& target_order =
                    orders[
                        orders.size() / 2
                    ];

                const std::int64_t new_quantity =
                    target_order.original_quantity ==
                            kQuantity
                        ? kQuantity + 1
                        : kQuantity;

                const bool modified =
                    engine.modify_order(
                        target_order.order_id,
                        target_order.price_ticks,
                        new_quantity
                    );

                (void)modified;
            }
        }
    );
}

ThroughputResult benchmark_mixed_workload()
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_buy_orders(
        engine,
        kInitialBookSize
    );

    return measure_throughput(
        kOperations,
        [&]() {
            constexpr std::size_t operations_per_cycle = 10;
            constexpr std::size_t cycles =
                kOperations /
                operations_per_cycle;

            for (
                std::size_t cycle = 0;
                cycle < cycles;
                ++cycle
            ) {
                for (
                    std::size_t i = 0;
                    i < 4;
                    ++i
                ) {
                    const auto order_id =
                        engine.place_limit_buy(
                            kBaseBidPrice,
                            kQuantity
                        );

                    (void)order_id;
                }

                for (
                    std::size_t i = 0;
                    i < 2;
                    ++i
                ) {
                    const auto& orders =
                        engine.active_limit_orders();

                    const OrderId target_order_id =
                        orders[
                            orders.size() / 2
                        ].order_id;

                    const bool cancelled =
                        engine.cancel_order(
                            target_order_id
                        );

                    (void)cancelled;
                }

                for (
                    std::size_t i = 0;
                    i < 2;
                    ++i
                ) {
                    const auto& orders =
                        engine.active_limit_orders();

                    const auto& target_order =
                        orders[
                            orders.size() / 2
                        ];

                    const std::int64_t new_quantity =
                        target_order.original_quantity ==
                                kQuantity
                            ? kQuantity + 1
                            : kQuantity;

                    const bool modified =
                        engine.modify_order(
                            target_order.order_id,
                            target_order.price_ticks,
                            new_quantity
                        );

                    (void)modified;
                }

                for (
                    std::size_t i = 0;
                    i < 2;
                    ++i
                ) {
                    const auto order_id =
                        engine.place_limit_sell(
                            kBaseBidPrice,
                            kQuantity
                        );

                    (void)order_id;
                }
            }
        }
    );
}

}  // namespace

int main()
{
    std::cout
        << "\ncmarket Throughput Benchmark\n"
        << "================================================"
        << "==============================\n"
        << "Operations per workload: "
        << kOperations
        << '\n'
        << "Initial active orders: "
        << kInitialBookSize
        << "\n\n";

    std::cout
        << std::left
        << std::setw(28)
        << "Workload"
        << std::right
        << std::setw(14)
        << "Operations"
        << std::setw(16)
        << "Seconds"
        << std::setw(18)
        << "Ops/sec"
        << '\n';

    std::cout
        << "------------------------------------------------"
        << "----------------------------\n";

    print_result(
        "Order placement",
        kOperations,
        benchmark_order_placement()
    );

    print_result(
        "Heavy matching",
        kOperations,
        benchmark_heavy_matching()
    );

    print_result(
        "Place/cancel mix",
        kOperations,
        benchmark_place_cancel_mix()
    );

    print_result(
        "Place/modify mix",
        kOperations,
        benchmark_place_modify_mix()
    );

    print_result(
        "Mixed workload",
        kOperations,
        benchmark_mixed_workload()
    );

    std::cout << '\n';

    return 0;
}
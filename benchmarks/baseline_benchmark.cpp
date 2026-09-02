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
constexpr std::int64_t kBaseAskPrice = 2'000'000;

template <typename Function>
void run_benchmark(
    std::string_view name,
    std::size_t operations,
    Function&& function
)
{
    const auto start = Clock::now();

    function();

    const auto end = Clock::now();

    const auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start
        );

    const double total_ns =
        static_cast<double>(elapsed.count());

    const double ns_per_operation =
        total_ns / static_cast<double>(operations);

    const double seconds =
        std::chrono::duration<double>(end - start).count();

    const double operations_per_second =
        static_cast<double>(operations) / seconds;

    std::cout
        << std::left
        << std::setw(28)
        << name
        << std::right
        << std::setw(14)
        << std::fixed
        << std::setprecision(2)
        << ns_per_operation
        << " ns/op"
        << std::setw(18)
        << std::fixed
        << std::setprecision(0)
        << operations_per_second
        << " ops/sec\n";
}

void benchmark_limit_buy_placement(
    std::size_t operations
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    run_benchmark(
        "Limit buy placement",
        operations,
        [&]() {
            for (std::size_t i = 0; i < operations; ++i) {
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

void benchmark_limit_sell_placement(
    std::size_t operations
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    run_benchmark(
        "Limit sell placement",
        operations,
        [&]() {
            for (std::size_t i = 0; i < operations; ++i) {
                const auto order_id =
                    engine.place_limit_sell(
                        kBaseAskPrice,
                        kQuantity
                    );

                (void)order_id;
            }
        }
    );
}

void benchmark_cancellation(
    std::size_t operations
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    for (std::size_t i = 0; i < operations; ++i) {
        const auto order_id =
            engine.place_limit_buy(
                kBaseBidPrice,
                kQuantity
            );

        (void)order_id;
    }

    const auto orders = engine.active_limit_orders();

    run_benchmark(
        "Order cancellation",
        operations,
        [&]() {
            for (const auto& order : orders) {
                const bool cancelled =
                    engine.cancel_order(order.order_id);

                (void)cancelled;
            }
        }
    );
}

void benchmark_modification(
    std::size_t operations
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    for (std::size_t i = 0; i < operations; ++i) {
        const auto order_id =
            engine.place_limit_buy(
                kBaseBidPrice,
                kQuantity
            );

        (void)order_id;
    }

    const auto orders = engine.active_limit_orders();

    run_benchmark(
        "Order modification",
        operations,
        [&]() {
            for (const auto& order : orders) {
                const bool modified =
                    engine.modify_order(
                        order.order_id,
                        kBaseBidPrice,
                        kQuantity + 1
                    );

                (void)modified;
            }
        }
    );
}

void benchmark_matching(
    std::size_t operations
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    for (std::size_t i = 0; i < operations; ++i) {
        const auto order_id =
            engine.place_limit_sell(
                kBaseAskPrice,
                kQuantity
            );

        (void)order_id;
    }

    run_benchmark(
        "Market buy matching",
        operations,
        [&]() {
            for (std::size_t i = 0; i < operations; ++i) {
                const auto result =
                    engine.execute_market_buy(kQuantity);

                (void)result;
            }
        }
    );
}

}  // namespace

int main()
{
    constexpr std::size_t operations = 100'000;

    std::cout << "\ncmarket Baseline Benchmark\n";
    std::cout << "========================================\n";
    std::cout << "Operations per benchmark: "
              << operations
              << "\n\n";

    std::cout
        << std::left
        << std::setw(28)
        << "Benchmark"
        << std::right
        << std::setw(20)
        << "Latency"
        << std::setw(18)
        << "Throughput"
        << '\n';

    std::cout
        << "--------------------------------------------------------------\n";

    benchmark_limit_buy_placement(operations);
    benchmark_limit_sell_placement(operations);
    benchmark_cancellation(operations);
    benchmark_modification(operations);
    benchmark_matching(operations);

    std::cout << '\n';

    return 0;
}
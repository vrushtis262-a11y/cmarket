#include "matching_engine.hpp"
#include "order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::int64_t kQuantity = 10;
constexpr std::int64_t kBaseBidPrice = 1'000'000;
constexpr std::int64_t kAlternateBidPrice = 999'999;
constexpr std::int64_t kBaseAskPrice = 2'000'000;

constexpr std::size_t kMeasuredIterations = 1'000;
constexpr std::size_t kWarmupIterations = 100;
constexpr std::size_t kMultiLevelCount = 10;

struct LatencyStatistics {
    double p50_ns;
    double p95_ns;
    double p99_ns;
    double worst_ns;
};

template <typename Function>
std::int64_t measure_latency_ns(
    Function&& function
)
{
    const auto start = Clock::now();

    function();

    const auto end = Clock::now();

    return std::chrono::duration_cast<
        std::chrono::nanoseconds
    >(
        end - start
    ).count();
}

double percentile_nearest_rank(
    const std::vector<std::int64_t>& samples,
    std::size_t percentile
)
{
    const std::size_t rank =
        (
            percentile * samples.size() +
            99
        ) / 100;

    const std::size_t index =
        rank == 0
            ? 0
            : rank - 1;

    return static_cast<double>(
        samples[index]
    );
}

LatencyStatistics calculate_statistics(
    std::vector<std::int64_t> samples
)
{
    std::sort(
        samples.begin(),
        samples.end()
    );

    return {
        percentile_nearest_rank(samples, 50),
        percentile_nearest_rank(samples, 95),
        percentile_nearest_rank(samples, 99),
        static_cast<double>(samples.back())
    };
}

void print_statistics(
    std::string_view operation,
    std::size_t book_size,
    const LatencyStatistics& statistics
)
{
    std::cout
        << std::left
        << std::setw(22)
        << operation
        << std::right
        << std::setw(12)
        << book_size
        << std::setw(16)
        << std::fixed
        << std::setprecision(0)
        << statistics.p50_ns
        << std::setw(16)
        << statistics.p95_ns
        << std::setw(16)
        << statistics.p99_ns
        << std::setw(16)
        << statistics.worst_ns
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

void populate_sell_orders(
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
            engine.place_limit_sell(
                kBaseAskPrice,
                kQuantity
            );

        (void)order_id;
    }
}

void populate_multi_level_sell_book(
    MatchingEngine& engine,
    std::size_t book_size
)
{
    for (
        std::size_t level = 0;
        level < kMultiLevelCount;
        ++level
    ) {
        const auto order_id =
            engine.place_limit_sell(
                kBaseAskPrice +
                    static_cast<std::int64_t>(
                        level
                    ),
                kQuantity
            );

        (void)order_id;
    }

    const std::int64_t far_price =
        kBaseAskPrice + 1'000;

    for (
        std::size_t i = kMultiLevelCount;
        i < book_size;
        ++i
    ) {
        const auto order_id =
            engine.place_limit_sell(
                far_price,
                kQuantity
            );

        (void)order_id;
    }
}

void replenish_multi_level_orders(
    MatchingEngine& engine
)
{
    for (
        std::size_t level = 0;
        level < kMultiLevelCount;
        ++level
    ) {
        const auto order_id =
            engine.place_limit_sell(
                kBaseAskPrice +
                    static_cast<std::int64_t>(
                        level
                    ),
                kQuantity
            );

        (void)order_id;
    }
}

LatencyStatistics benchmark_place_order(
    std::size_t book_size
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_buy_orders(
        engine,
        book_size
    );

    for (
        std::size_t i = 0;
        i < kWarmupIterations;
        ++i
    ) {
        const OrderId order_id =
            engine.place_limit_buy(
                kBaseBidPrice,
                kQuantity
            );

        const bool cancelled =
            engine.cancel_order(order_id);

        (void)cancelled;
    }

    std::vector<std::int64_t> samples;
    samples.reserve(kMeasuredIterations);

    for (
        std::size_t i = 0;
        i < kMeasuredIterations;
        ++i
    ) {
        OrderId order_id = 0;

        const auto latency =
            measure_latency_ns(
                [&]() {
                    order_id =
                        engine.place_limit_buy(
                            kBaseBidPrice,
                            kQuantity
                        );
                }
            );

        samples.push_back(latency);

        const bool cancelled =
            engine.cancel_order(order_id);

        (void)cancelled;
    }

    return calculate_statistics(
        std::move(samples)
    );
}

LatencyStatistics benchmark_match_order(
    std::size_t book_size
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_sell_orders(
        engine,
        book_size
    );

    for (
        std::size_t i = 0;
        i < kWarmupIterations;
        ++i
    ) {
        const auto order_id =
            engine.place_limit_buy(
                kBaseAskPrice,
                kQuantity
            );

        (void)order_id;

        const auto replacement_id =
            engine.place_limit_sell(
                kBaseAskPrice,
                kQuantity
            );

        (void)replacement_id;
    }

    std::vector<std::int64_t> samples;
    samples.reserve(kMeasuredIterations);

    for (
        std::size_t i = 0;
        i < kMeasuredIterations;
        ++i
    ) {
        const auto latency =
            measure_latency_ns(
                [&]() {
                    const auto order_id =
                        engine.place_limit_buy(
                            kBaseAskPrice,
                            kQuantity
                        );

                    (void)order_id;
                }
            );

        samples.push_back(latency);

        const auto replacement_id =
            engine.place_limit_sell(
                kBaseAskPrice,
                kQuantity
            );

        (void)replacement_id;
    }

    return calculate_statistics(
        std::move(samples)
    );
}

LatencyStatistics benchmark_cancel_order(
    std::size_t book_size
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_buy_orders(
        engine,
        book_size
    );

    for (
        std::size_t i = 0;
        i < kWarmupIterations;
        ++i
    ) {
        const auto& orders =
            engine.active_limit_orders();

        const OrderId target_order_id =
            orders[orders.size() / 2].order_id;

        const bool cancelled =
            engine.cancel_order(
                target_order_id
            );

        (void)cancelled;

        const auto replacement_id =
            engine.place_limit_buy(
                kBaseBidPrice,
                kQuantity
            );

        (void)replacement_id;
    }

    std::vector<std::int64_t> samples;
    samples.reserve(kMeasuredIterations);

    for (
        std::size_t i = 0;
        i < kMeasuredIterations;
        ++i
    ) {
        const auto& orders =
            engine.active_limit_orders();

        const OrderId target_order_id =
            orders[orders.size() / 2].order_id;

        const auto latency =
            measure_latency_ns(
                [&]() {
                    const bool cancelled =
                        engine.cancel_order(
                            target_order_id
                        );

                    (void)cancelled;
                }
            );

        samples.push_back(latency);

        const auto replacement_id =
            engine.place_limit_buy(
                kBaseBidPrice,
                kQuantity
            );

        (void)replacement_id;
    }

    return calculate_statistics(
        std::move(samples)
    );
}

LatencyStatistics benchmark_modify_order(
    std::size_t book_size
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_buy_orders(
        engine,
        book_size
    );

    for (
        std::size_t i = 0;
        i < kWarmupIterations;
        ++i
    ) {
        const auto& orders =
            engine.active_limit_orders();

        const OrderId target_order_id =
            orders[orders.size() / 2].order_id;

        const bool modified =
            engine.modify_order(
                target_order_id,
                kBaseBidPrice,
                kQuantity + 1
            );

        (void)modified;
    }

    std::vector<std::int64_t> samples;
    samples.reserve(kMeasuredIterations);

    for (
        std::size_t i = 0;
        i < kMeasuredIterations;
        ++i
    ) {
        const auto& orders =
            engine.active_limit_orders();

        const OrderId target_order_id =
            orders[orders.size() / 2].order_id;

        const auto latency =
            measure_latency_ns(
                [&]() {
                    const bool modified =
                        engine.modify_order(
                            target_order_id,
                            kBaseBidPrice,
                            kQuantity + 1
                        );

                    (void)modified;
                }
            );

        samples.push_back(latency);
    }

    return calculate_statistics(
        std::move(samples)
    );
}

LatencyStatistics benchmark_modify_price(
    std::size_t book_size
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_buy_orders(
        engine,
        book_size
    );

    const auto& initial_orders =
        engine.active_limit_orders();

    const OrderId target_order_id =
        initial_orders[
            initial_orders.size() / 2
        ].order_id;

    std::int64_t current_price =
        kBaseBidPrice;

    for (
        std::size_t i = 0;
        i < kWarmupIterations;
        ++i
    ) {
        const std::int64_t new_price =
            current_price == kBaseBidPrice
                ? kAlternateBidPrice
                : kBaseBidPrice;

        const bool modified =
            engine.modify_order(
                target_order_id,
                new_price,
                kQuantity
            );

        (void)modified;

        current_price =
            new_price;
    }

    std::vector<std::int64_t> samples;
    samples.reserve(kMeasuredIterations);

    for (
        std::size_t i = 0;
        i < kMeasuredIterations;
        ++i
    ) {
        const std::int64_t new_price =
            current_price == kBaseBidPrice
                ? kAlternateBidPrice
                : kBaseBidPrice;

        const auto latency =
            measure_latency_ns(
                [&]() {
                    const bool modified =
                        engine.modify_order(
                            target_order_id,
                            new_price,
                            kQuantity
                        );

                    (void)modified;
                }
            );

        samples.push_back(latency);

        current_price =
            new_price;
    }

    return calculate_statistics(
        std::move(samples)
    );
}

LatencyStatistics benchmark_multi_level_match(
    std::size_t book_size
)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    populate_multi_level_sell_book(
        engine,
        book_size
    );

    const std::int64_t match_quantity =
        static_cast<std::int64_t>(
            kMultiLevelCount
        ) *
        kQuantity;

    for (
        std::size_t i = 0;
        i < kWarmupIterations;
        ++i
    ) {
        const auto result =
            engine.execute_market_buy(
                match_quantity
            );

        (void)result;

        replenish_multi_level_orders(
            engine
        );
    }

    std::vector<std::int64_t> samples;
    samples.reserve(kMeasuredIterations);

    for (
        std::size_t i = 0;
        i < kMeasuredIterations;
        ++i
    ) {
        const auto latency =
            measure_latency_ns(
                [&]() {
                    const auto result =
                        engine.execute_market_buy(
                            match_quantity
                        );

                    (void)result;
                }
            );

        samples.push_back(latency);

        replenish_multi_level_orders(
            engine
        );
    }

    return calculate_statistics(
        std::move(samples)
    );
}

void run_book_size(
    std::size_t book_size
)
{
    print_statistics(
        "Place order",
        book_size,
        benchmark_place_order(book_size)
    );

    print_statistics(
        "Match order",
        book_size,
        benchmark_match_order(book_size)
    );

    print_statistics(
        "Cancel order",
        book_size,
        benchmark_cancel_order(book_size)
    );

    print_statistics(
        "Modify order",
        book_size,
        benchmark_modify_order(book_size)
    );

    print_statistics(
        "Modify price",
        book_size,
        benchmark_modify_price(book_size)
    );

    print_statistics(
        "Multi-level match",
        book_size,
        benchmark_multi_level_match(book_size)
    );
}

}  // namespace

int main()
{
    constexpr std::size_t book_sizes[] = {
        100,
        1'000,
        10'000,
        100'000
    };

    std::cout
        << "\ncmarket Latency Benchmark\n"
        << "================================================"
        << "================================\n"
        << "Measured iterations per operation: "
        << kMeasuredIterations
        << '\n'
        << "Warmup iterations per operation: "
        << kWarmupIterations
        << "\n\n";

    std::cout
        << std::left
        << std::setw(22)
        << "Operation"
        << std::right
        << std::setw(12)
        << "Book size"
        << std::setw(16)
        << "p50 (ns)"
        << std::setw(16)
        << "p95 (ns)"
        << std::setw(16)
        << "p99 (ns)"
        << std::setw(16)
        << "Worst (ns)"
        << '\n';

    std::cout
        << "------------------------------------------------"
        << "----------------------------------------------\n";

    for (
        const std::size_t book_size :
        book_sizes
    ) {
        run_book_size(book_size);
    }

    std::cout << '\n';

    return 0;
}
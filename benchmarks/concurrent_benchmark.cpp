#include "concurrent_matching_engine.hpp"
#include "order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::int64_t kQuantity = 10;
constexpr std::int64_t kBaseBidPrice = 1'000'000;

constexpr std::size_t kProducerCount = 4;
constexpr std::size_t kOperationsPerProducer = 25'000;
constexpr std::size_t kTotalOperations =
    kProducerCount * kOperationsPerProducer;

constexpr std::size_t kLatencySamples = 20'000;

struct ThroughputResult {
    double elapsed_seconds;
    double operations_per_second;
};

struct LatencyResult {
    double mean_nanoseconds;
    std::int64_t p50_nanoseconds;
    std::int64_t p95_nanoseconds;
    std::int64_t p99_nanoseconds;
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

std::int64_t percentile(
    const std::vector<std::int64_t>& sorted_values,
    double fraction
)
{
    if (sorted_values.empty()) {
        return 0;
    }

    const double scaled_index =
        fraction *
        static_cast<double>(
            sorted_values.size() - 1
        );

    const std::size_t index =
        static_cast<std::size_t>(
            scaled_index
        );

    return sorted_values[index];
}

LatencyResult summarize_latency(
    std::vector<std::int64_t> samples
)
{
    std::sort(
        samples.begin(),
        samples.end()
    );

    const std::int64_t total =
        std::accumulate(
            samples.begin(),
            samples.end(),
            std::int64_t{0}
        );

    const double mean =
        samples.empty()
            ? 0.0
            : static_cast<double>(total) /
                  static_cast<double>(
                      samples.size()
                  );

    return {
        mean,
        percentile(samples, 0.50),
        percentile(samples, 0.95),
        percentile(samples, 0.99)
    };
}

void print_throughput_result(
    std::string_view workload,
    std::size_t operations,
    const ThroughputResult& result
)
{
    std::cout
        << std::left
        << std::setw(34)
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

void print_latency_result(
    std::string_view workload,
    const LatencyResult& result
)
{
    std::cout
        << std::left
        << std::setw(34)
        << workload
        << std::right
        << std::setw(16)
        << std::fixed
        << std::setprecision(0)
        << result.mean_nanoseconds
        << std::setw(14)
        << result.p50_nanoseconds
        << std::setw(14)
        << result.p95_nanoseconds
        << std::setw(14)
        << result.p99_nanoseconds
        << '\n';
}

ThroughputResult benchmark_concurrent_placement()
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(
        order_book
    );

    return measure_throughput(
        kTotalOperations,
        [&]() {
            std::vector<std::thread> producers;
            producers.reserve(kProducerCount);

            for (
                std::size_t producer = 0;
                producer < kProducerCount;
                ++producer
            ) {
                producers.emplace_back(
                    [
                        &engine,
                        producer
                    ] {
                        for (
                            std::size_t index = 0;
                            index <
                                kOperationsPerProducer;
                            ++index
                        ) {
                            const std::int64_t price =
                                kBaseBidPrice +
                                static_cast<std::int64_t>(
                                    producer
                                ) *
                                    10'000 +
                                static_cast<std::int64_t>(
                                    index % 1'000
                                );

                            const OrderId order_id =
                                engine.place_limit_buy(
                                    price,
                                    kQuantity
                                ).get();

                            (void)order_id;
                        }
                    }
                );
            }

            for (auto& producer : producers) {
                producer.join();
            }
        }
    );
}

ThroughputResult benchmark_batched_submission()
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(
        order_book
    );

    return measure_throughput(
        kTotalOperations,
        [&]() {
            std::vector<std::thread> producers;
            producers.reserve(kProducerCount);

            for (
                std::size_t producer = 0;
                producer < kProducerCount;
                ++producer
            ) {
                producers.emplace_back(
                    [
                        &engine,
                        producer
                    ] {
                        std::vector<
                            std::future<OrderId>
                        > futures;

                        futures.reserve(
                            kOperationsPerProducer
                        );

                        for (
                            std::size_t index = 0;
                            index <
                                kOperationsPerProducer;
                            ++index
                        ) {
                            const std::int64_t price =
                                kBaseBidPrice +
                                static_cast<std::int64_t>(
                                    producer
                                ) *
                                    10'000 +
                                static_cast<std::int64_t>(
                                    index % 1'000
                                );

                            futures.push_back(
                                engine.place_limit_buy(
                                    price,
                                    kQuantity
                                )
                            );
                        }

                        for (auto& future : futures) {
                            const OrderId order_id =
                                future.get();

                            (void)order_id;
                        }
                    }
                );
            }

            for (auto& producer : producers) {
                producer.join();
            }
        }
    );
}

ThroughputResult benchmark_concurrent_market_execution()
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(
        order_book
    );

    for (
        std::size_t index = 0;
        index < kTotalOperations;
        ++index
    ) {
        const OrderId order_id =
            engine.place_limit_sell(
                kBaseBidPrice,
                kQuantity
            ).get();

        (void)order_id;
    }

    return measure_throughput(
        kTotalOperations,
        [&]() {
            std::vector<std::thread> producers;
            producers.reserve(kProducerCount);

            for (
                std::size_t producer = 0;
                producer < kProducerCount;
                ++producer
            ) {
                producers.emplace_back(
                    [&engine] {
                        for (
                            std::size_t index = 0;
                            index <
                                kOperationsPerProducer;
                            ++index
                        ) {
                            const ExecutionResult result =
                                engine.execute_market_buy(
                                    kQuantity
                                ).get();

                            (void)result;
                        }
                    }
                );
            }

            for (auto& producer : producers) {
                producer.join();
            }
        }
    );
}

LatencyResult benchmark_end_to_end_latency()
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(
        order_book
    );

    std::vector<std::int64_t> samples;
    samples.reserve(kLatencySamples);

    for (
        std::size_t index = 0;
        index < kLatencySamples;
        ++index
    ) {
        const std::int64_t price =
            kBaseBidPrice +
            static_cast<std::int64_t>(
                index % 1'000
            );

        const auto start = Clock::now();

        const OrderId order_id =
            engine.place_limit_buy(
                price,
                kQuantity
            ).get();

        const auto end = Clock::now();

        (void)order_id;

        samples.push_back(
            std::chrono::duration_cast<
                std::chrono::nanoseconds
            >(
                end - start
            ).count()
        );
    }

    return summarize_latency(
        std::move(samples)
    );
}

LatencyResult benchmark_queued_latency()
{
    OrderBook order_book;
    ConcurrentMatchingEngine engine(
        order_book
    );

    std::vector<
        std::future<OrderId>
    > futures;

    futures.reserve(kLatencySamples);

    std::vector<Clock::time_point>
        start_times;

    start_times.reserve(kLatencySamples);

    for (
        std::size_t index = 0;
        index < kLatencySamples;
        ++index
    ) {
        const std::int64_t price =
            kBaseBidPrice +
            static_cast<std::int64_t>(
                index % 1'000
            );

        start_times.push_back(
            Clock::now()
        );

        futures.push_back(
            engine.place_limit_buy(
                price,
                kQuantity
            )
        );
    }

    std::vector<std::int64_t> samples;
    samples.reserve(kLatencySamples);

    for (
        std::size_t index = 0;
        index < futures.size();
        ++index
    ) {
        const OrderId order_id =
            futures[index].get();

        const auto end = Clock::now();

        (void)order_id;

        samples.push_back(
            std::chrono::duration_cast<
                std::chrono::nanoseconds
            >(
                end - start_times[index]
            ).count()
        );
    }

    return summarize_latency(
        std::move(samples)
    );
}

}  // namespace

int main()
{
    std::cout
        << "\ncmarket Concurrent Benchmark\n"
        << "================================================"
        << "==================================\n"
        << "Producer threads: "
        << kProducerCount
        << '\n'
        << "Operations per producer: "
        << kOperationsPerProducer
        << '\n'
        << "Total operations: "
        << kTotalOperations
        << '\n'
        << "Latency samples: "
        << kLatencySamples
        << "\n\n";

    std::cout
        << "Throughput\n"
        << "------------------------------------------------"
        << "----------------------------------\n";

    std::cout
        << std::left
        << std::setw(34)
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
        << "----------------------------------\n";

    print_throughput_result(
        "Concurrent placement",
        kTotalOperations,
        benchmark_concurrent_placement()
    );

    print_throughput_result(
        "Batched async submission",
        kTotalOperations,
        benchmark_batched_submission()
    );

    print_throughput_result(
        "Concurrent market execution",
        kTotalOperations,
        benchmark_concurrent_market_execution()
    );

    std::cout
        << "\nLatency\n"
        << "------------------------------------------------"
        << "----------------------------------\n";

    std::cout
        << std::left
        << std::setw(34)
        << "Workload"
        << std::right
        << std::setw(16)
        << "Mean ns"
        << std::setw(14)
        << "p50 ns"
        << std::setw(14)
        << "p95 ns"
        << std::setw(14)
        << "p99 ns"
        << '\n';

    std::cout
        << "------------------------------------------------"
        << "----------------------------------\n";

    print_latency_result(
        "Synchronous round-trip",
        benchmark_end_to_end_latency()
    );

    print_latency_result(
        "Queued async completion",
        benchmark_queued_latency()
    );

    std::cout << '\n';

    return 0;
}
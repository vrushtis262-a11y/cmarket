#include "order_book.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

bool bids_are_valid(
    const std::vector<PriceLevel>& bids
)
{
    for (std::size_t index = 0;
         index < bids.size();
         ++index) {
        if (bids[index].quantity <= 0) {
            return false;
        }

        if (index > 0 &&
            bids[index - 1].price_ticks <=
                bids[index].price_ticks) {
            return false;
        }
    }

    return true;
}

bool asks_are_valid(
    const std::vector<PriceLevel>& asks
)
{
    for (std::size_t index = 0;
         index < asks.size();
         ++index) {
        if (asks[index].quantity <= 0) {
            return false;
        }

        if (index > 0 &&
            asks[index - 1].price_ticks >=
                asks[index].price_ticks) {
            return false;
        }
    }

    return true;
}

} // namespace

TEST(
    OrderBookConcurrencyTest,
    ConcurrentReadersObserveValidSnapshots
)
{
    OrderBook book;

    book.replace_normalized_snapshot(
        {
            {500'000, 100},
            {490'000, 200},
            {480'000, 300}
        },
        {
            {510'000, 100},
            {520'000, 200},
            {530'000, 300}
        }
    );

    constexpr int reader_count = 8;
    constexpr int iterations = 5'000;

    std::atomic<bool> failed{false};

    std::vector<std::thread> readers;
    readers.reserve(
        static_cast<std::size_t>(
            reader_count
        )
    );

    for (int reader = 0;
         reader < reader_count;
         ++reader) {
        readers.emplace_back(
            [&book, &failed] {
                for (int iteration = 0;
                     iteration < iterations;
                     ++iteration) {
                    const auto bids =
                        book.bids();

                    const auto asks =
                        book.asks();

                    if (!bids_are_valid(bids) ||
                        !asks_are_valid(asks)) {
                        failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                        return;
                    }

                    static_cast<void>(
                        book.best_bid()
                    );

                    static_cast<void>(
                        book.best_ask()
                    );

                    static_cast<void>(
                        book.spread_ticks()
                    );

                    static_cast<void>(
                        book.mid_price_ticks()
                    );

                    static_cast<void>(
                        book.bid_depth()
                    );

                    static_cast<void>(
                        book.ask_depth()
                    );

                    static_cast<void>(
                        book.total_depth()
                    );

                    static_cast<void>(
                        book.order_book_imbalance()
                    );

                    static_cast<void>(
                        book.bid_vwap_ticks()
                    );

                    static_cast<void>(
                        book.ask_vwap_ticks()
                    );

                    static_cast<void>(
                        book.microprice_ticks()
                    );

                    static_cast<void>(
                        book.empty()
                    );
                }
            }
        );
    }

    for (auto& reader : readers) {
        reader.join();
    }

    EXPECT_FALSE(
        failed.load(
            std::memory_order_relaxed
        )
    );
}

TEST(
    OrderBookConcurrencyTest,
    ConcurrentReadersAndWritersPreserveBookInvariants
)
{
    OrderBook book;

    book.replace_normalized_snapshot(
        {
            {500'000, 100},
            {490'000, 100},
            {480'000, 100}
        },
        {
            {510'000, 100},
            {520'000, 100},
            {530'000, 100}
        }
    );

    constexpr int reader_count = 6;
    constexpr int writer_count = 4;
    constexpr int iterations = 3'000;

    std::atomic<bool> failed{false};

    std::vector<std::thread> readers;
    readers.reserve(
        static_cast<std::size_t>(
            reader_count
        )
    );

    for (int reader = 0;
         reader < reader_count;
         ++reader) {
        readers.emplace_back(
            [&book, &failed] {
                for (int iteration = 0;
                     iteration < iterations;
                     ++iteration) {
                    const auto bids =
                        book.bids();

                    const auto asks =
                        book.asks();

                    if (!bids_are_valid(bids) ||
                        !asks_are_valid(asks)) {
                        failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                        return;
                    }

                    const auto best_bid =
                        book.best_bid();

                    const auto best_ask =
                        book.best_ask();

                    if (best_bid &&
                        best_bid->quantity <= 0) {
                        failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                        return;
                    }

                    if (best_ask &&
                        best_ask->quantity <= 0) {
                        failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                        return;
                    }

                    static_cast<void>(
                        book.spread_ticks()
                    );

                    static_cast<void>(
                        book.mid_price_ticks()
                    );

                    static_cast<void>(
                        book.bid_depth()
                    );

                    static_cast<void>(
                        book.ask_depth()
                    );

                    static_cast<void>(
                        book.total_depth()
                    );

                    static_cast<void>(
                        book.order_book_imbalance()
                    );

                    static_cast<void>(
                        book.bid_vwap_ticks()
                    );

                    static_cast<void>(
                        book.ask_vwap_ticks()
                    );

                    static_cast<void>(
                        book.microprice_ticks()
                    );
                }
            }
        );
    }

    std::vector<std::thread> writers;
    writers.reserve(
        static_cast<std::size_t>(
            writer_count
        )
    );

    for (int writer = 0;
         writer < writer_count;
         ++writer) {
        writers.emplace_back(
            [&book, writer] {
                const std::int64_t bid_price =
                    400'000 +
                    static_cast<std::int64_t>(
                        writer
                    ) *
                        1'000;

                const std::int64_t ask_price =
                    600'000 +
                    static_cast<std::int64_t>(
                        writer
                    ) *
                        1'000;

                for (int iteration = 0;
                     iteration < iterations;
                     ++iteration) {
                    const std::int64_t quantity =
                        1 +
                        static_cast<std::int64_t>(
                            iteration % 100
                        );

                    book.update_bid(
                        bid_price,
                        quantity
                    );

                    book.update_ask(
                        ask_price,
                        quantity
                    );
                }
            }
        );
    }

    for (auto& writer : writers) {
        writer.join();
    }

    for (auto& reader : readers) {
        reader.join();
    }

    EXPECT_FALSE(
        failed.load(
            std::memory_order_relaxed
        )
    );

    EXPECT_TRUE(
        bids_are_valid(
            book.bids()
        )
    );

    EXPECT_TRUE(
        asks_are_valid(
            book.asks()
        )
    );
}

TEST(
    OrderBookConcurrencyTest,
    ConcurrentSnapshotReplacementAndReadsRemainValid
)
{
    OrderBook book;

    book.replace_normalized_snapshot(
        {
            {500'000, 100}
        },
        {
            {510'000, 100}
        }
    );

    constexpr int reader_count = 6;
    constexpr int iterations = 2'000;

    std::atomic<bool> failed{false};

    std::thread writer(
        [&book] {
            for (int iteration = 0;
                 iteration < iterations;
                 ++iteration) {
                const std::int64_t offset =
                    static_cast<std::int64_t>(
                        iteration % 100
                    );

                book.replace_normalized_snapshot(
                    {
                        {
                            500'000 + offset,
                            100
                        },
                        {
                            490'000 + offset,
                            200
                        }
                    },
                    {
                        {
                            510'000 + offset,
                            100
                        },
                        {
                            520'000 + offset,
                            200
                        }
                    }
                );
            }
        }
    );

    std::vector<std::thread> readers;
    readers.reserve(
        static_cast<std::size_t>(
            reader_count
        )
    );

    for (int reader = 0;
         reader < reader_count;
         ++reader) {
        readers.emplace_back(
            [&book, &failed] {
                for (int iteration = 0;
                     iteration < iterations;
                     ++iteration) {
                    const auto bids =
                        book.bids();

                    const auto asks =
                        book.asks();

                    if (!bids_are_valid(bids) ||
                        !asks_are_valid(asks)) {
                        failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                        return;
                    }

                    static_cast<void>(
                        book.best_bid()
                    );

                    static_cast<void>(
                        book.best_ask()
                    );

                    static_cast<void>(
                        book.spread_ticks()
                    );

                    static_cast<void>(
                        book.mid_price_ticks()
                    );

                    static_cast<void>(
                        book.bid_vwap_ticks()
                    );

                    static_cast<void>(
                        book.ask_vwap_ticks()
                    );

                    static_cast<void>(
                        book.microprice_ticks()
                    );
                }
            }
        );
    }

    writer.join();

    for (auto& reader : readers) {
        reader.join();
    }

    EXPECT_FALSE(
        failed.load(
            std::memory_order_relaxed
        )
    );
}

TEST(
    OrderBookConcurrencyTest,
    ConcurrentBidAndAskUpdatesDoNotLoseLevels
)
{
    OrderBook book;

    constexpr int thread_count = 8;
    constexpr int levels_per_thread = 100;

    std::vector<std::thread> writers;
    writers.reserve(
        static_cast<std::size_t>(
            thread_count
        )
    );

    for (int thread_index = 0;
         thread_index < thread_count;
         ++thread_index) {
        writers.emplace_back(
            [&book, thread_index] {
                for (int level = 0;
                     level < levels_per_thread;
                     ++level) {
                    const std::int64_t offset =
                        static_cast<std::int64_t>(
                            thread_index *
                                levels_per_thread +
                            level
                        );

                    book.update_bid(
                        100'000 + offset,
                        10
                    );

                    book.update_ask(
                        700'000 + offset,
                        20
                    );
                }
            }
        );
    }

    for (auto& writer : writers) {
        writer.join();
    }

    const auto bids =
        book.bids();

    const auto asks =
        book.asks();

    const std::size_t expected_levels =
        static_cast<std::size_t>(
            thread_count *
            levels_per_thread
        );

    ASSERT_EQ(
        bids.size(),
        expected_levels
    );

    ASSERT_EQ(
        asks.size(),
        expected_levels
    );

    EXPECT_TRUE(
        bids_are_valid(bids)
    );

    EXPECT_TRUE(
        asks_are_valid(asks)
    );

    EXPECT_EQ(
        book.bid_depth(),
        static_cast<std::int64_t>(
            expected_levels
        ) *
            10
    );

    EXPECT_EQ(
        book.ask_depth(),
        static_cast<std::int64_t>(
            expected_levels
        ) *
            20
    );
}

TEST(
    OrderBookConcurrencyTest,
    ConcurrentAdjustmentsPreserveExactQuantities
)
{
    OrderBook book;

    book.replace_normalized_snapshot(
        {
            {500'000, 10'000}
        },
        {
            {510'000, 10'000}
        }
    );

    constexpr int thread_count = 8;
    constexpr int adjustments_per_thread = 1'000;

    std::vector<std::thread> writers;
    writers.reserve(
        static_cast<std::size_t>(
            thread_count
        )
    );

    for (int thread_index = 0;
         thread_index < thread_count;
         ++thread_index) {
        writers.emplace_back(
            [&book] {
                for (int adjustment = 0;
                     adjustment <
                         adjustments_per_thread;
                     ++adjustment) {
                    book.adjust_bid(
                        500'000,
                        1
                    );

                    book.adjust_ask(
                        510'000,
                        1
                    );
                }
            }
        );
    }

    for (auto& writer : writers) {
        writer.join();
    }

    const std::int64_t expected =
        10'000 +
        static_cast<std::int64_t>(
            thread_count *
            adjustments_per_thread
        );

    const auto best_bid =
        book.best_bid();

    const auto best_ask =
        book.best_ask();

    ASSERT_TRUE(best_bid.has_value());
    ASSERT_TRUE(best_ask.has_value());

    EXPECT_EQ(
        best_bid->quantity,
        expected
    );

    EXPECT_EQ(
        best_ask->quantity,
        expected
    );

    EXPECT_EQ(
        book.bid_depth(),
        expected
    );

    EXPECT_EQ(
        book.ask_depth(),
        expected
    );
}

TEST(
    OrderBookConcurrencyTest,
    CopyConstructionIsSafeDuringConcurrentWrites
)
{
    OrderBook book;

    book.replace_normalized_snapshot(
        {
            {500'000, 100},
            {490'000, 200}
        },
        {
            {510'000, 100},
            {520'000, 200}
        }
    );

    constexpr int iterations = 2'000;

    std::atomic<bool> failed{false};

    std::thread writer(
        [&book] {
            for (int iteration = 0;
                 iteration < iterations;
                 ++iteration) {
                const std::int64_t quantity =
                    1 +
                    static_cast<std::int64_t>(
                        iteration % 100
                    );

                book.update_bid(
                    480'000,
                    quantity
                );

                book.update_ask(
                    530'000,
                    quantity
                );
            }
        }
    );

    std::thread copier(
        [&book, &failed] {
            for (int iteration = 0;
                 iteration < iterations;
                 ++iteration) {
                const OrderBook copy(book);

                if (!bids_are_valid(
                        copy.bids()
                    ) ||
                    !asks_are_valid(
                        copy.asks()
                    )) {
                    failed.store(
                        true,
                        std::memory_order_relaxed
                    );
                    return;
                }
            }
        }
    );

    writer.join();
    copier.join();

    EXPECT_FALSE(
        failed.load(
            std::memory_order_relaxed
        )
    );
}

TEST(
    OrderBookConcurrencyTest,
    CopyAssignmentIsSafeDuringConcurrentWrites
)
{
    OrderBook source;
    OrderBook destination;

    source.replace_normalized_snapshot(
        {
            {500'000, 100},
            {490'000, 200}
        },
        {
            {510'000, 100},
            {520'000, 200}
        }
    );

    constexpr int iterations = 2'000;

    std::atomic<bool> failed{false};

    std::thread writer(
        [&source] {
            for (int iteration = 0;
                 iteration < iterations;
                 ++iteration) {
                const std::int64_t quantity =
                    1 +
                    static_cast<std::int64_t>(
                        iteration % 100
                    );

                source.update_bid(
                    480'000,
                    quantity
                );

                source.update_ask(
                    530'000,
                    quantity
                );
            }
        }
    );

    std::thread copier(
        [&source, &destination, &failed] {
            for (int iteration = 0;
                 iteration < iterations;
                 ++iteration) {
                destination = source;

                if (!bids_are_valid(
                        destination.bids()
                    ) ||
                    !asks_are_valid(
                        destination.asks()
                    )) {
                    failed.store(
                        true,
                        std::memory_order_relaxed
                    );
                    return;
                }
            }
        }
    );

    writer.join();
    copier.join();

    EXPECT_FALSE(
        failed.load(
            std::memory_order_relaxed
        )
    );
}
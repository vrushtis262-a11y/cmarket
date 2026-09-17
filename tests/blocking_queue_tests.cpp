#include "blocking_queue.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <numeric>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(BlockingQueueTest, StartsEmptyAndOpen) {
    BlockingQueue<int> queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0U);
    EXPECT_FALSE(queue.is_closed());
}

TEST(BlockingQueueTest, PushAndTryPopPreserveFifoOrder) {
    BlockingQueue<int> queue;

    EXPECT_TRUE(queue.push(10));
    EXPECT_TRUE(queue.push(20));
    EXPECT_TRUE(queue.push(30));

    EXPECT_EQ(queue.size(), 3U);

    const auto first = queue.try_pop();
    const auto second = queue.try_pop();
    const auto third = queue.try_pop();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());

    EXPECT_EQ(*first, 10);
    EXPECT_EQ(*second, 20);
    EXPECT_EQ(*third, 30);

    EXPECT_TRUE(queue.empty());
}

TEST(BlockingQueueTest, TryPopReturnsEmptyWhenQueueIsEmpty) {
    BlockingQueue<int> queue;

    const auto value = queue.try_pop();

    EXPECT_FALSE(value.has_value());
}

TEST(BlockingQueueTest, PopBlocksUntilProducerPushesValue) {
    BlockingQueue<int> queue;
    std::atomic<bool> consumer_started{false};
    std::atomic<bool> consumer_finished{false};
    int received = 0;

    std::thread consumer([&] {
        consumer_started.store(true);
        const auto value = queue.pop();

        if (value.has_value()) {
            received = *value;
        }

        consumer_finished.store(true);
    });

    while (!consumer_started.load()) {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(consumer_finished.load());

    EXPECT_TRUE(queue.push(42));

    consumer.join();

    EXPECT_TRUE(consumer_finished.load());
    EXPECT_EQ(received, 42);
}

TEST(BlockingQueueTest, CloseWakesBlockedConsumer) {
    BlockingQueue<int> queue;
    std::atomic<bool> consumer_started{false};
    std::atomic<bool> consumer_finished{false};
    std::atomic<bool> received_value{false};

    std::thread consumer([&] {
        consumer_started.store(true);

        const auto value = queue.pop();
        received_value.store(value.has_value());

        consumer_finished.store(true);
    });

    while (!consumer_started.load()) {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(consumer_finished.load());

    queue.close();
    consumer.join();

    EXPECT_TRUE(consumer_finished.load());
    EXPECT_FALSE(received_value.load());
    EXPECT_TRUE(queue.is_closed());
}

TEST(BlockingQueueTest, CloseIsIdempotent) {
    BlockingQueue<int> queue;

    queue.close();
    queue.close();

    EXPECT_TRUE(queue.is_closed());
}

TEST(BlockingQueueTest, PushFailsAfterClose) {
    BlockingQueue<int> queue;

    queue.close();

    EXPECT_FALSE(queue.push(123));
    EXPECT_TRUE(queue.empty());
}

TEST(BlockingQueueTest, ExistingItemsCanBeDrainedAfterClose) {
    BlockingQueue<int> queue;

    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));

    queue.close();

    const auto first = queue.pop();
    const auto second = queue.pop();
    const auto third = queue.pop();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());

    EXPECT_EQ(*first, 1);
    EXPECT_EQ(*second, 2);
    EXPECT_FALSE(third.has_value());
    EXPECT_TRUE(queue.empty());
}

TEST(BlockingQueueTest, SingleProducerSingleConsumerTransfersAllValues) {
    BlockingQueue<int> queue;

    constexpr int item_count = 10'000;

    std::vector<int> received;
    received.reserve(item_count);

    std::thread producer([&] {
        for (int value = 0; value < item_count; ++value) {
            ASSERT_TRUE(queue.push(value));
        }

        queue.close();
    });

    std::thread consumer([&] {
        while (const auto value = queue.pop()) {
            received.push_back(*value);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), static_cast<std::size_t>(item_count));

    for (int value = 0; value < item_count; ++value) {
        EXPECT_EQ(
            received[static_cast<std::size_t>(value)],
            value
        );
    }

    EXPECT_TRUE(queue.empty());
}

TEST(BlockingQueueTest, MultipleProducersDoNotLoseValues) {
    BlockingQueue<int> queue;

    constexpr int producer_count = 4;
    constexpr int items_per_producer = 5'000;
    constexpr int total_items =
        producer_count * items_per_producer;

    std::vector<std::thread> producers;

    for (int producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer] {
            const int base = producer * items_per_producer;

            for (int offset = 0;
                 offset < items_per_producer;
                 ++offset) {
                ASSERT_TRUE(queue.push(base + offset));
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    queue.close();

    std::vector<int> received;
    received.reserve(total_items);

    while (const auto value = queue.pop()) {
        received.push_back(*value);
    }

    ASSERT_EQ(
        received.size(),
        static_cast<std::size_t>(total_items)
    );

    std::sort(received.begin(), received.end());

    for (int value = 0; value < total_items; ++value) {
        EXPECT_EQ(
            received[static_cast<std::size_t>(value)],
            value
        );
    }
}

TEST(BlockingQueueTest, MultipleConsumersProcessEachValueExactlyOnce) {
    BlockingQueue<int> queue;

    constexpr int consumer_count = 4;
    constexpr int item_count = 20'000;

    std::vector<std::atomic<int>> counts(
        static_cast<std::size_t>(item_count)
    );

    for (auto& count : counts) {
        count.store(0);
    }

    std::vector<std::thread> consumers;

    for (int consumer = 0; consumer < consumer_count; ++consumer) {
        consumers.emplace_back([&] {
            while (const auto value = queue.pop()) {
                counts[static_cast<std::size_t>(*value)]
                    .fetch_add(1);
            }
        });
    }

    for (int value = 0; value < item_count; ++value) {
        ASSERT_TRUE(queue.push(value));
    }

    queue.close();

    for (auto& consumer : consumers) {
        consumer.join();
    }

    for (const auto& count : counts) {
        EXPECT_EQ(count.load(), 1);
    }
}

TEST(BlockingQueueTest, MultipleProducersAndConsumersHandleHighVolume) {
    BlockingQueue<int> queue;

    constexpr int producer_count = 4;
    constexpr int consumer_count = 4;
    constexpr int items_per_producer = 25'000;
    constexpr int total_items =
        producer_count * items_per_producer;

    std::atomic<int> consumed_count{0};
    std::atomic<long long> consumed_sum{0};

    std::vector<std::thread> consumers;

    for (int consumer = 0; consumer < consumer_count; ++consumer) {
        consumers.emplace_back([&] {
            while (const auto value = queue.pop()) {
                consumed_count.fetch_add(1);
                consumed_sum.fetch_add(*value);
            }
        });
    }

    std::vector<std::thread> producers;

    for (int producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer] {
            const int base = producer * items_per_producer;

            for (int offset = 0;
                 offset < items_per_producer;
                 ++offset) {
                ASSERT_TRUE(queue.push(base + offset));
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    queue.close();

    for (auto& consumer : consumers) {
        consumer.join();
    }

    const long long expected_sum =
        (static_cast<long long>(total_items) *
         static_cast<long long>(total_items - 1)) /
        2;

    EXPECT_EQ(consumed_count.load(), total_items);
    EXPECT_EQ(consumed_sum.load(), expected_sum);
    EXPECT_TRUE(queue.empty());
}

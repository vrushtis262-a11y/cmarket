
#include "depth_analytics.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

class DepthAnalyticsTest : public ::testing::Test {
protected:
    OrderBook book;
};

TEST_F(DepthAnalyticsTest, EmptyBookHasZeroDepth) {
    const auto result =
        cmarket::DepthAnalytics::analyze(book, 5);

    EXPECT_EQ(result.requested_levels, 5U);
    EXPECT_EQ(result.bid_levels, 0U);
    EXPECT_EQ(result.ask_levels, 0U);
    EXPECT_EQ(result.bid_depth, 0.0L);
    EXPECT_EQ(result.ask_depth, 0.0L);
    EXPECT_EQ(result.total_depth, 0.0L);
    EXPECT_FALSE(result.imbalance.has_value());
}

TEST_F(DepthAnalyticsTest, RejectsZeroLevelCount) {
    EXPECT_THROW(
        static_cast<void>(
            cmarket::DepthAnalytics::analyze(book, 0)
        ),
        std::invalid_argument
    );
}

TEST_F(DepthAnalyticsTest, CalculatesTopOneDepth) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {{110, 40}, {111, 20}, {112, 10}}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 1);

    EXPECT_EQ(result.bid_levels, 1U);
    EXPECT_EQ(result.ask_levels, 1U);
    EXPECT_EQ(result.bid_depth, 50.0L);
    EXPECT_EQ(result.ask_depth, 40.0L);
    EXPECT_EQ(result.total_depth, 90.0L);

    ASSERT_TRUE(result.imbalance.has_value());
    EXPECT_NEAR(
        static_cast<double>(*result.imbalance),
        10.0 / 90.0,
        1e-12
    );
}

TEST_F(DepthAnalyticsTest, CalculatesTopTwoDepth) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {{110, 40}, {111, 20}, {112, 10}}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 2);

    EXPECT_EQ(result.bid_depth, 80.0L);
    EXPECT_EQ(result.ask_depth, 60.0L);
    EXPECT_EQ(result.total_depth, 140.0L);

    ASSERT_TRUE(result.imbalance.has_value());
    EXPECT_NEAR(
        static_cast<double>(*result.imbalance),
        20.0 / 140.0,
        1e-12
    );
}

TEST_F(DepthAnalyticsTest, TracksCumulativeBidDepth) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 3);

    ASSERT_EQ(result.bids.size(), 3U);

    EXPECT_EQ(result.bids[0].price_ticks, 100);
    EXPECT_EQ(result.bids[0].quantity, 50);
    EXPECT_EQ(result.bids[0].cumulative_quantity, 50.0L);

    EXPECT_EQ(result.bids[1].price_ticks, 99);
    EXPECT_EQ(result.bids[1].quantity, 30);
    EXPECT_EQ(result.bids[1].cumulative_quantity, 80.0L);

    EXPECT_EQ(result.bids[2].price_ticks, 98);
    EXPECT_EQ(result.bids[2].quantity, 20);
    EXPECT_EQ(result.bids[2].cumulative_quantity, 100.0L);
}

TEST_F(DepthAnalyticsTest, TracksCumulativeAskDepth) {
    book.replace_snapshot(
        {},
        {{110, 40}, {111, 20}, {112, 10}}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 3);

    ASSERT_EQ(result.asks.size(), 3U);
    EXPECT_EQ(result.asks[0].cumulative_quantity, 40.0L);
    EXPECT_EQ(result.asks[1].cumulative_quantity, 60.0L);
    EXPECT_EQ(result.asks[2].cumulative_quantity, 70.0L);
}

TEST_F(
    DepthAnalyticsTest,
    HandlesMoreRequestedLevelsThanAvailable
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}},
        {{110, 40}}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 10);

    EXPECT_EQ(result.requested_levels, 10U);
    EXPECT_EQ(result.bid_levels, 2U);
    EXPECT_EQ(result.ask_levels, 1U);
    EXPECT_EQ(result.bid_depth, 80.0L);
    EXPECT_EQ(result.ask_depth, 40.0L);
}

TEST_F(
    DepthAnalyticsTest,
    BidOnlyBookHasMaximumPositiveImbalance
) {
    book.replace_snapshot(
        {{100, 50}},
        {}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 5);

    ASSERT_TRUE(result.imbalance.has_value());
    EXPECT_EQ(*result.imbalance, 1.0L);
}

TEST_F(
    DepthAnalyticsTest,
    AskOnlyBookHasMaximumNegativeImbalance
) {
    book.replace_snapshot(
        {},
        {{110, 40}}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 5);

    ASSERT_TRUE(result.imbalance.has_value());
    EXPECT_EQ(*result.imbalance, -1.0L);
}

TEST_F(DepthAnalyticsTest, BalancedDepthHasZeroImbalance) {
    book.replace_snapshot(
        {{100, 50}},
        {{110, 50}}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 5);

    ASSERT_TRUE(result.imbalance.has_value());
    EXPECT_EQ(*result.imbalance, 0.0L);
}

TEST_F(DepthAnalyticsTest, RespectsSortedPriceLevels) {
    book.replace_snapshot(
        {{98, 20}, {100, 50}, {99, 30}},
        {{112, 10}, {110, 40}, {111, 20}}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 3);

    ASSERT_EQ(result.bids.size(), 3U);
    ASSERT_EQ(result.asks.size(), 3U);

    EXPECT_EQ(result.bids[0].price_ticks, 100);
    EXPECT_EQ(result.bids[1].price_ticks, 99);
    EXPECT_EQ(result.bids[2].price_ticks, 98);

    EXPECT_EQ(result.asks[0].price_ticks, 110);
    EXPECT_EQ(result.asks[1].price_ticks, 111);
    EXPECT_EQ(result.asks[2].price_ticks, 112);
}

TEST_F(DepthAnalyticsTest, AnalysisDoesNotModifyOrderBook) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}},
        {{110, 40}}
    );

    static_cast<void>(
        cmarket::DepthAnalytics::analyze(book, 1)
    );

    EXPECT_EQ(book.bids().size(), 2U);
    EXPECT_EQ(book.asks().size(), 1U);
    EXPECT_EQ(book.bid_depth(), 80);
    EXPECT_EQ(book.ask_depth(), 40);
}

TEST_F(DepthAnalyticsTest, RecalculationReflectsBookUpdates) {
    book.update_bid(100, 50);

    const auto first =
        cmarket::DepthAnalytics::analyze(book, 1);

    EXPECT_EQ(first.bid_depth, 50.0L);

    book.update_bid(100, 80);

    const auto second =
        cmarket::DepthAnalytics::analyze(book, 1);

    EXPECT_EQ(second.bid_depth, 80.0L);
}

TEST_F(
    DepthAnalyticsTest,
    HandlesLargeDepthWithoutIntegerOverflow
) {
    constexpr std::int64_t quantity =
        INT64_C(9000000000000000000);

    book.replace_snapshot(
        {{100, quantity}, {99, quantity}},
        {}
    );

    const auto result =
        cmarket::DepthAnalytics::analyze(book, 2);

    EXPECT_GT(result.bid_depth, 9.0e18L);
    EXPECT_EQ(result.bid_levels, 2U);
}

} // namespace

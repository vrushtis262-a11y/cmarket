
#include "depth_weighted_pricing.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace {

class DepthWeightedPricingTest
    : public ::testing::Test {
protected:
    OrderBook book;
};

TEST_F(
    DepthWeightedPricingTest,
    EmptyBookHasNoWeightedPrices
) {
    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 5);

    EXPECT_EQ(result.requested_levels, 5U);
    EXPECT_EQ(result.bid_levels, 0U);
    EXPECT_EQ(result.ask_levels, 0U);
    EXPECT_EQ(result.bid_depth, 0.0L);
    EXPECT_EQ(result.ask_depth, 0.0L);

    EXPECT_FALSE(
        result.weighted_bid_price_ticks.has_value()
    );
    EXPECT_FALSE(
        result.weighted_ask_price_ticks.has_value()
    );
    EXPECT_FALSE(
        result.weighted_mid_price_ticks.has_value()
    );
}

TEST_F(
    DepthWeightedPricingTest,
    RejectsZeroLevelCount
) {
    EXPECT_THROW(
        static_cast<void>(
            cmarket::DepthWeightedPricing::analyze(
                book,
                0
            )
        ),
        std::invalid_argument
    );
}

TEST_F(
    DepthWeightedPricingTest,
    CalculatesWeightedBidPrice
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 3);

    ASSERT_TRUE(
        result.weighted_bid_price_ticks.has_value()
    );

    EXPECT_NEAR(
        static_cast<double>(
            *result.weighted_bid_price_ticks
        ),
        99.3,
        1e-12
    );

    EXPECT_EQ(result.bid_depth, 100.0L);
}

TEST_F(
    DepthWeightedPricingTest,
    CalculatesWeightedAskPrice
) {
    book.replace_snapshot(
        {},
        {{110, 40}, {111, 20}, {112, 40}}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 3);

    ASSERT_TRUE(
        result.weighted_ask_price_ticks.has_value()
    );

    EXPECT_NEAR(
        static_cast<double>(
            *result.weighted_ask_price_ticks
        ),
        111.0,
        1e-12
    );

    EXPECT_EQ(result.ask_depth, 100.0L);
}

TEST_F(
    DepthWeightedPricingTest,
    CalculatesWeightedMidpoint
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {{110, 40}, {111, 20}, {112, 40}}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 3);

    ASSERT_TRUE(
        result.weighted_mid_price_ticks.has_value()
    );

    EXPECT_NEAR(
        static_cast<double>(
            *result.weighted_mid_price_ticks
        ),
        105.15,
        1e-12
    );
}

TEST_F(
    DepthWeightedPricingTest,
    TopOneUsesBestPrices
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}},
        {{110, 40}, {111, 20}}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 1);

    ASSERT_TRUE(
        result.weighted_bid_price_ticks.has_value()
    );
    ASSERT_TRUE(
        result.weighted_ask_price_ticks.has_value()
    );
    ASSERT_TRUE(
        result.weighted_mid_price_ticks.has_value()
    );

    EXPECT_EQ(
        *result.weighted_bid_price_ticks,
        100.0L
    );
    EXPECT_EQ(
        *result.weighted_ask_price_ticks,
        110.0L
    );
    EXPECT_EQ(
        *result.weighted_mid_price_ticks,
        105.0L
    );
}

TEST_F(
    DepthWeightedPricingTest,
    TopTwoExcludesDeeperLevels
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 2);

    ASSERT_TRUE(
        result.weighted_bid_price_ticks.has_value()
    );

    EXPECT_EQ(result.bid_levels, 2U);
    EXPECT_EQ(result.bid_depth, 80.0L);

    EXPECT_NEAR(
        static_cast<double>(
            *result.weighted_bid_price_ticks
        ),
        99.625,
        1e-12
    );
}

TEST_F(
    DepthWeightedPricingTest,
    BidOnlyBookHasNoWeightedMidpoint
) {
    book.replace_snapshot(
        {{100, 50}, {99, 50}},
        {}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 5);

    EXPECT_TRUE(
        result.weighted_bid_price_ticks.has_value()
    );
    EXPECT_FALSE(
        result.weighted_ask_price_ticks.has_value()
    );
    EXPECT_FALSE(
        result.weighted_mid_price_ticks.has_value()
    );
}

TEST_F(
    DepthWeightedPricingTest,
    AskOnlyBookHasNoWeightedMidpoint
) {
    book.replace_snapshot(
        {},
        {{110, 40}, {111, 60}}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 5);

    EXPECT_FALSE(
        result.weighted_bid_price_ticks.has_value()
    );
    EXPECT_TRUE(
        result.weighted_ask_price_ticks.has_value()
    );
    EXPECT_FALSE(
        result.weighted_mid_price_ticks.has_value()
    );
}

TEST_F(
    DepthWeightedPricingTest,
    HandlesMoreRequestedLevelsThanAvailable
) {
    book.replace_snapshot(
        {{100, 50}, {99, 50}},
        {{110, 40}}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 10);

    EXPECT_EQ(result.requested_levels, 10U);
    EXPECT_EQ(result.bid_levels, 2U);
    EXPECT_EQ(result.ask_levels, 1U);

    ASSERT_TRUE(
        result.weighted_bid_price_ticks.has_value()
    );
    ASSERT_TRUE(
        result.weighted_ask_price_ticks.has_value()
    );

    EXPECT_NEAR(
        static_cast<double>(
            *result.weighted_bid_price_ticks
        ),
        99.5,
        1e-12
    );

    EXPECT_EQ(
        *result.weighted_ask_price_ticks,
        110.0L
    );
}

TEST_F(
    DepthWeightedPricingTest,
    RespectsSortedPriceLevels
) {
    book.replace_snapshot(
        {{98, 20}, {100, 50}, {99, 30}},
        {{112, 40}, {110, 40}, {111, 20}}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 1);

    ASSERT_TRUE(
        result.weighted_bid_price_ticks.has_value()
    );
    ASSERT_TRUE(
        result.weighted_ask_price_ticks.has_value()
    );

    EXPECT_EQ(
        *result.weighted_bid_price_ticks,
        100.0L
    );
    EXPECT_EQ(
        *result.weighted_ask_price_ticks,
        110.0L
    );
}

TEST_F(
    DepthWeightedPricingTest,
    RecalculationReflectsOrderBookUpdates
) {
    book.update_bid(100, 50);

    const auto first =
        cmarket::DepthWeightedPricing::analyze(book, 2);

    ASSERT_TRUE(
        first.weighted_bid_price_ticks.has_value()
    );

    EXPECT_EQ(
        *first.weighted_bid_price_ticks,
        100.0L
    );

    book.update_bid(99, 50);

    const auto second =
        cmarket::DepthWeightedPricing::analyze(book, 2);

    ASSERT_TRUE(
        second.weighted_bid_price_ticks.has_value()
    );

    EXPECT_NEAR(
        static_cast<double>(
            *second.weighted_bid_price_ticks
        ),
        99.5,
        1e-12
    );
}

TEST_F(
    DepthWeightedPricingTest,
    LargeQuantitiesDoNotOverflow
) {
    constexpr std::int64_t quantity =
        INT64_C(9000000000000000000);

    book.replace_snapshot(
        {{100, quantity}, {99, quantity}},
        {}
    );

    const auto result =
        cmarket::DepthWeightedPricing::analyze(book, 2);

    ASSERT_TRUE(
        result.weighted_bid_price_ticks.has_value()
    );

    EXPECT_NEAR(
        static_cast<double>(
            *result.weighted_bid_price_ticks
        ),
        99.5,
        1e-12
    );
}

TEST_F(
    DepthWeightedPricingTest,
    DoesNotModifyOriginalOrderBook
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}},
        {{110, 40}}
    );

    static_cast<void>(
        cmarket::DepthWeightedPricing::analyze(book, 2)
    );

    EXPECT_EQ(book.bids().size(), 2U);
    EXPECT_EQ(book.asks().size(), 1U);
    EXPECT_EQ(book.bid_depth(), 80);
    EXPECT_EQ(book.ask_depth(), 40);
}

} // namespace

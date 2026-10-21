
#include "liquidity_concentration.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace {

class LiquidityConcentrationTest
    : public ::testing::Test {
protected:
    OrderBook book;
};

TEST_F(
    LiquidityConcentrationTest,
    EmptyBookHasNoConcentration
) {
    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 5);

    EXPECT_EQ(result.requested_levels, 5U);
    EXPECT_EQ(result.bid_levels, 0U);
    EXPECT_EQ(result.ask_levels, 0U);

    EXPECT_FALSE(result.bid_best_level_share.has_value());
    EXPECT_FALSE(result.ask_best_level_share.has_value());
    EXPECT_FALSE(result.bid_hhi.has_value());
    EXPECT_FALSE(result.ask_hhi.has_value());
}

TEST_F(
    LiquidityConcentrationTest,
    RejectsZeroLevelCount
) {
    EXPECT_THROW(
        static_cast<void>(
            cmarket::LiquidityConcentration::analyze(
                book,
                0
            )
        ),
        std::invalid_argument
    );
}

TEST_F(
    LiquidityConcentrationTest,
    CalculatesBestBidLevelShare
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 3);

    ASSERT_TRUE(result.bid_best_level_share.has_value());

    EXPECT_NEAR(
        static_cast<double>(*result.bid_best_level_share),
        0.5,
        1e-12
    );
}

TEST_F(
    LiquidityConcentrationTest,
    CalculatesBestAskLevelShare
) {
    book.replace_snapshot(
        {},
        {{110, 40}, {111, 20}, {112, 40}}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 3);

    ASSERT_TRUE(result.ask_best_level_share.has_value());

    EXPECT_NEAR(
        static_cast<double>(*result.ask_best_level_share),
        0.4,
        1e-12
    );
}

TEST_F(
    LiquidityConcentrationTest,
    CalculatesBidHHI
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 3);

    ASSERT_TRUE(result.bid_hhi.has_value());

    EXPECT_NEAR(
        static_cast<double>(*result.bid_hhi),
        0.38,
        1e-12
    );
}

TEST_F(
    LiquidityConcentrationTest,
    CalculatesAskHHI
) {
    book.replace_snapshot(
        {},
        {{110, 40}, {111, 20}, {112, 40}}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 3);

    ASSERT_TRUE(result.ask_hhi.has_value());

    EXPECT_NEAR(
        static_cast<double>(*result.ask_hhi),
        0.36,
        1e-12
    );
}

TEST_F(
    LiquidityConcentrationTest,
    SingleLevelHasMaximumConcentration
) {
    book.replace_snapshot(
        {{100, 50}},
        {{110, 40}}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 5);

    ASSERT_TRUE(result.bid_best_level_share.has_value());
    ASSERT_TRUE(result.ask_best_level_share.has_value());
    ASSERT_TRUE(result.bid_hhi.has_value());
    ASSERT_TRUE(result.ask_hhi.has_value());

    EXPECT_EQ(*result.bid_best_level_share, 1.0L);
    EXPECT_EQ(*result.ask_best_level_share, 1.0L);
    EXPECT_EQ(*result.bid_hhi, 1.0L);
    EXPECT_EQ(*result.ask_hhi, 1.0L);
}

TEST_F(
    LiquidityConcentrationTest,
    EqualLiquidityHasMinimumHHIForThreeLevels
) {
    book.replace_snapshot(
        {{100, 30}, {99, 30}, {98, 30}},
        {}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 3);

    ASSERT_TRUE(result.bid_hhi.has_value());

    EXPECT_NEAR(
        static_cast<double>(*result.bid_hhi),
        1.0 / 3.0,
        1e-12
    );
}

TEST_F(
    LiquidityConcentrationTest,
    TopOneAlwaysHasFullConcentration
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}},
        {{110, 40}, {111, 20}}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 1);

    ASSERT_TRUE(result.bid_hhi.has_value());
    ASSERT_TRUE(result.ask_hhi.has_value());

    EXPECT_EQ(*result.bid_hhi, 1.0L);
    EXPECT_EQ(*result.ask_hhi, 1.0L);
}

TEST_F(
    LiquidityConcentrationTest,
    TopNUsesOnlySelectedLevels
) {
    book.replace_snapshot(
        {{100, 50}, {99, 30}, {98, 20}},
        {}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 2);

    ASSERT_TRUE(result.bid_best_level_share.has_value());
    ASSERT_TRUE(result.bid_hhi.has_value());

    EXPECT_NEAR(
        static_cast<double>(*result.bid_best_level_share),
        50.0 / 80.0,
        1e-12
    );

    EXPECT_NEAR(
        static_cast<double>(*result.bid_hhi),
        (50.0 * 50.0 + 30.0 * 30.0) / (80.0 * 80.0),
        1e-12
    );
}

TEST_F(
    LiquidityConcentrationTest,
    HandlesOneSidedBook
) {
    book.replace_snapshot(
        {{100, 60}, {99, 40}},
        {}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 5);

    EXPECT_TRUE(result.bid_best_level_share.has_value());
    EXPECT_TRUE(result.bid_hhi.has_value());

    EXPECT_FALSE(result.ask_best_level_share.has_value());
    EXPECT_FALSE(result.ask_hhi.has_value());
}

TEST_F(
    LiquidityConcentrationTest,
    MoreRequestedLevelsThanAvailable
) {
    book.replace_snapshot(
        {{100, 50}, {99, 50}},
        {{110, 40}}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 10);

    EXPECT_EQ(result.bid_levels, 2U);
    EXPECT_EQ(result.ask_levels, 1U);

    ASSERT_TRUE(result.bid_hhi.has_value());
    EXPECT_EQ(*result.bid_hhi, 0.5L);
}

TEST_F(
    LiquidityConcentrationTest,
    RecalculationReflectsOrderBookChanges
) {
    book.replace_snapshot(
        {{100, 50}, {99, 50}},
        {}
    );

    const auto first =
        cmarket::LiquidityConcentration::analyze(book, 2);

    ASSERT_TRUE(first.bid_hhi.has_value());
    EXPECT_EQ(*first.bid_hhi, 0.5L);

    book.update_bid(100, 90);

    const auto second =
        cmarket::LiquidityConcentration::analyze(book, 2);

    ASSERT_TRUE(second.bid_best_level_share.has_value());

    EXPECT_NEAR(
        static_cast<double>(*second.bid_best_level_share),
        90.0 / 140.0,
        1e-12
    );
}

TEST_F(
    LiquidityConcentrationTest,
    LargeQuantitiesDoNotOverflow
) {
    constexpr std::int64_t quantity =
        INT64_C(9000000000000000000);

    book.replace_snapshot(
        {{100, quantity}, {99, quantity}},
        {}
    );

    const auto result =
        cmarket::LiquidityConcentration::analyze(book, 2);

    ASSERT_TRUE(result.bid_hhi.has_value());

    EXPECT_NEAR(
        static_cast<double>(*result.bid_hhi),
        0.5,
        1e-12
    );
}

} // namespace

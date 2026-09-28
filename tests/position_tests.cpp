#include "position.hpp"

#include <gtest/gtest.h>

namespace {

TEST(PositionTest, NewPositionStartsFlat) {
    cmarket::Position position("AAPL");

    EXPECT_EQ(position.symbol(), "AAPL");
    EXPECT_EQ(position.quantity(), 0);
    EXPECT_EQ(
        position.average_entry_price_ticks(),
        0
    );
    EXPECT_TRUE(position.is_flat());
    EXPECT_FALSE(position.is_long());
    EXPECT_FALSE(position.is_short());
}

TEST(PositionTest, OpensLongPosition) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        100,
        150'000'000
    );

    EXPECT_EQ(position.quantity(), 100);
    EXPECT_EQ(
        position.average_entry_price_ticks(),
        150'000'000
    );
    EXPECT_TRUE(position.is_long());
}

TEST(PositionTest, OpensShortPosition) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        -100,
        150'000'000
    );

    EXPECT_EQ(position.quantity(), -100);
    EXPECT_EQ(
        position.average_entry_price_ticks(),
        150'000'000
    );
    EXPECT_TRUE(position.is_short());
}

TEST(PositionTest, IncreasesLongPosition) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        100,
        100'000'000
    );

    position.apply_trade(
        50,
        110'000'000
    );

    EXPECT_EQ(position.quantity(), 150);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        103'333'333
    );
}

TEST(PositionTest, IncreasesShortPosition) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        -100,
        100'000'000
    );

    position.apply_trade(
        -50,
        110'000'000
    );

    EXPECT_EQ(position.quantity(), -150);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        103'333'333
    );
}

TEST(PositionTest, PartiallyClosesLongPosition) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        100,
        100'000'000
    );

    position.apply_trade(
        -40,
        120'000'000
    );

    EXPECT_EQ(position.quantity(), 60);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        100'000'000
    );

    EXPECT_TRUE(position.is_long());
}

TEST(PositionTest, PartiallyClosesShortPosition) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        -100,
        100'000'000
    );

    position.apply_trade(
        40,
        80'000'000
    );

    EXPECT_EQ(position.quantity(), -60);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        100'000'000
    );

    EXPECT_TRUE(position.is_short());
}

TEST(PositionTest, FullyClosesLongPosition) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        100,
        100'000'000
    );

    position.apply_trade(
        -100,
        120'000'000
    );

    EXPECT_EQ(position.quantity(), 0);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        0
    );

    EXPECT_TRUE(position.is_flat());
}

TEST(PositionTest, FullyClosesShortPosition) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        -100,
        100'000'000
    );

    position.apply_trade(
        100,
        80'000'000
    );

    EXPECT_EQ(position.quantity(), 0);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        0
    );

    EXPECT_TRUE(position.is_flat());
}

TEST(PositionTest, FlipsLongPositionToShort) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        100,
        100'000'000
    );

    position.apply_trade(
        -150,
        120'000'000
    );

    EXPECT_EQ(position.quantity(), -50);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        120'000'000
    );

    EXPECT_TRUE(position.is_short());
}

TEST(PositionTest, FlipsShortPositionToLong) {
    cmarket::Position position("AAPL");

    position.apply_trade(
        -100,
        100'000'000
    );

    position.apply_trade(
        150,
        80'000'000
    );

    EXPECT_EQ(position.quantity(), 50);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        80'000'000
    );

    EXPECT_TRUE(position.is_long());
}

TEST(PositionTest, RejectsZeroTradeQuantity) {
    cmarket::Position position("AAPL");

    EXPECT_THROW(
        position.apply_trade(
            0,
            100'000'000
        ),
        std::invalid_argument
    );
}

TEST(PositionTest, RejectsNonPositiveTradePrice) {
    cmarket::Position position("AAPL");

    EXPECT_THROW(
        position.apply_trade(
            100,
            0
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        position.apply_trade(
            100,
            -10'000'000
        ),
        std::invalid_argument
    );
}

}  // namespace
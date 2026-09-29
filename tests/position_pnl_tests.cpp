#include "position.hpp"

#include <gtest/gtest.h>

namespace {

TEST(
    PositionPnlTest,
    StartsWithZeroRealizedPnl
) {
    const cmarket::Position position{"AAPL"};

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        0
    );
}

TEST(
    PositionPnlTest,
    OpeningLongDoesNotRealizePnl
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        100,
        100'000'000
    );

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        0
    );
}

TEST(
    PositionPnlTest,
    OpeningShortDoesNotRealizePnl
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        -100,
        100'000'000
    );

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        0
    );
}

TEST(
    PositionPnlTest,
    PartialLongCloseRealizesProfit
) {
    cmarket::Position position{"AAPL"};

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

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        800'000'000
    );
}

TEST(
    PositionPnlTest,
    PartialLongCloseRealizesLoss
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        100,
        100'000'000
    );

    position.apply_trade(
        -25,
        90'000'000
    );

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        -250'000'000
    );
}

TEST(
    PositionPnlTest,
    PartialShortCloseRealizesProfit
) {
    cmarket::Position position{"AAPL"};

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

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        800'000'000
    );
}

TEST(
    PositionPnlTest,
    PartialShortCloseRealizesLoss
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        -100,
        100'000'000
    );

    position.apply_trade(
        25,
        110'000'000
    );

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        -250'000'000
    );
}

TEST(
    PositionPnlTest,
    FullCloseRealizesPnlAndLeavesPositionFlat
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        100,
        100'000'000
    );

    position.apply_trade(
        -100,
        125'000'000
    );

    EXPECT_TRUE(position.is_flat());

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        0
    );

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        2'500'000'000
    );
}

TEST(
    PositionPnlTest,
    PositionFlipRealizesOnlyClosedQuantity
) {
    cmarket::Position position{"AAPL"};

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

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        2'000'000'000
    );
}

TEST(
    PositionPnlTest,
    RealizedPnlAccumulatesAcrossCloses
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        100,
        100'000'000
    );

    position.apply_trade(
        -40,
        120'000'000
    );

    position.apply_trade(
        -60,
        90'000'000
    );

    EXPECT_TRUE(position.is_flat());

    EXPECT_EQ(
        position.realized_pnl_ticks(),
        200'000'000
    );
}

TEST(
    PositionPnlTest,
    FlatPositionHasZeroUnrealizedPnl
) {
    const cmarket::Position position{"AAPL"};

    EXPECT_EQ(
        position.unrealized_pnl_ticks(
            150'000'000
        ),
        0
    );
}

TEST(
    PositionPnlTest,
    LongPositionCalculatesUnrealizedProfit
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        100,
        100'000'000
    );

    EXPECT_EQ(
        position.unrealized_pnl_ticks(
            120'000'000
        ),
        2'000'000'000
    );
}

TEST(
    PositionPnlTest,
    LongPositionCalculatesUnrealizedLoss
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        100,
        100'000'000
    );

    EXPECT_EQ(
        position.unrealized_pnl_ticks(
            90'000'000
        ),
        -1'000'000'000
    );
}

TEST(
    PositionPnlTest,
    ShortPositionCalculatesUnrealizedProfit
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        -100,
        100'000'000
    );

    EXPECT_EQ(
        position.unrealized_pnl_ticks(
            80'000'000
        ),
        2'000'000'000
    );
}

TEST(
    PositionPnlTest,
    ShortPositionCalculatesUnrealizedLoss
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        -100,
        100'000'000
    );

    EXPECT_EQ(
        position.unrealized_pnl_ticks(
            110'000'000
        ),
        -1'000'000'000
    );
}

TEST(
    PositionPnlTest,
    UnrealizedPnlUsesRemainingPositionAfterPartialClose
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        100,
        100'000'000
    );

    position.apply_trade(
        -40,
        120'000'000
    );

    EXPECT_EQ(
        position.unrealized_pnl_ticks(
            110'000'000
        ),
        600'000'000
    );
}

TEST(
    PositionPnlTest,
    RejectsNonPositiveMarketPriceForUnrealizedPnl
) {
    cmarket::Position position{"AAPL"};

    position.apply_trade(
        100,
        100'000'000
    );

    EXPECT_THROW(
        position.unrealized_pnl_ticks(0),
        std::invalid_argument
    );

    EXPECT_THROW(
        position.unrealized_pnl_ticks(-1),
        std::invalid_argument
    );
}

}  // namespace
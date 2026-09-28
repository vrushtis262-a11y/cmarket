#include "position_manager.hpp"

#include <cstddef>
#include <gtest/gtest.h>

namespace {

TEST(PositionManagerTest, StartsEmpty) {
    cmarket::PositionManager manager;

    EXPECT_EQ(manager.size(), std::size_t{0});
    EXPECT_FALSE(manager.has_position("AAPL"));
}

TEST(PositionManagerTest, CreatesPositionForTrade) {
    cmarket::PositionManager manager;

    manager.apply_trade(
        "AAPL",
        100,
        150'000'000
    );

    ASSERT_TRUE(manager.has_position("AAPL"));
    EXPECT_EQ(manager.size(), std::size_t{1});

    const auto& position =
        manager.position("AAPL");

    EXPECT_EQ(position.symbol(), "AAPL");
    EXPECT_EQ(position.quantity(), 100);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        150'000'000
    );
}

TEST(PositionManagerTest, UpdatesExistingPosition) {
    cmarket::PositionManager manager;

    manager.apply_trade(
        "AAPL",
        100,
        100'000'000
    );

    manager.apply_trade(
        "AAPL",
        50,
        110'000'000
    );

    EXPECT_EQ(manager.size(), std::size_t{1});

    const auto& position =
        manager.position("AAPL");

    EXPECT_EQ(position.quantity(), 150);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        103'333'333
    );
}

TEST(
    PositionManagerTest,
    TracksMultipleSymbolsIndependently
) {
    cmarket::PositionManager manager;

    manager.apply_trade(
        "AAPL",
        100,
        150'000'000
    );

    manager.apply_trade(
        "MSFT",
        -50,
        400'000'000
    );

    manager.apply_trade(
        "GOOG",
        25,
        175'000'000
    );

    EXPECT_EQ(manager.size(), std::size_t{3});

    EXPECT_EQ(
        manager.position("AAPL").quantity(),
        100
    );

    EXPECT_EQ(
        manager.position("AAPL")
            .average_entry_price_ticks(),
        150'000'000
    );

    EXPECT_EQ(
        manager.position("MSFT").quantity(),
        -50
    );

    EXPECT_EQ(
        manager.position("MSFT")
            .average_entry_price_ticks(),
        400'000'000
    );

    EXPECT_EQ(
        manager.position("GOOG").quantity(),
        25
    );

    EXPECT_EQ(
        manager.position("GOOG")
            .average_entry_price_ticks(),
        175'000'000
    );
}

TEST(
    PositionManagerTest,
    SupportsIndependentPositionChanges
) {
    cmarket::PositionManager manager;

    manager.apply_trade(
        "AAPL",
        100,
        100'000'000
    );

    manager.apply_trade(
        "MSFT",
        -100,
        200'000'000
    );

    manager.apply_trade(
        "AAPL",
        -40,
        120'000'000
    );

    manager.apply_trade(
        "MSFT",
        150,
        180'000'000
    );

    const auto& aapl =
        manager.position("AAPL");

    const auto& msft =
        manager.position("MSFT");

    EXPECT_EQ(aapl.quantity(), 60);

    EXPECT_EQ(
        aapl.average_entry_price_ticks(),
        100'000'000
    );

    EXPECT_TRUE(aapl.is_long());

    EXPECT_EQ(msft.quantity(), 50);

    EXPECT_EQ(
        msft.average_entry_price_ticks(),
        180'000'000
    );

    EXPECT_TRUE(msft.is_long());
}

TEST(PositionManagerTest, KeepsClosedPositionAsFlat) {
    cmarket::PositionManager manager;

    manager.apply_trade(
        "AAPL",
        100,
        100'000'000
    );

    manager.apply_trade(
        "AAPL",
        -100,
        120'000'000
    );

    ASSERT_TRUE(manager.has_position("AAPL"));
    EXPECT_EQ(manager.size(), std::size_t{1});

    const auto& position =
        manager.position("AAPL");

    EXPECT_TRUE(position.is_flat());
    EXPECT_EQ(position.quantity(), 0);

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        0
    );
}

TEST(PositionManagerTest, ThrowsForUnknownPosition) {
    cmarket::PositionManager manager;

    EXPECT_THROW(
        static_cast<void>(
            manager.position("AAPL")
        ),
        std::out_of_range
    );
}

TEST(PositionManagerTest, RejectsEmptySymbol) {
    cmarket::PositionManager manager;

    EXPECT_THROW(
        manager.apply_trade(
            "",
            100,
            150'000'000
        ),
        std::invalid_argument
    );

    EXPECT_EQ(manager.size(), std::size_t{0});
}

TEST(
    PositionManagerTest,
    DoesNotCreatePositionForInvalidTrade
) {
    cmarket::PositionManager manager;

    EXPECT_THROW(
        manager.apply_trade(
            "AAPL",
            0,
            150'000'000
        ),
        std::invalid_argument
    );

    EXPECT_FALSE(manager.has_position("AAPL"));
    EXPECT_EQ(manager.size(), std::size_t{0});
}

}  // namespace
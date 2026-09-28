#include "position_execution_handler.hpp"

#include <gtest/gtest.h>

namespace {

TEST(
    PositionExecutionHandlerTest,
    AppliesBuyExecutionToPosition
) {
    cmarket::PositionManager manager;

    cmarket::PositionExecutionHandler handler(
        manager
    );

    const ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = 100,
        .executed_quantity = 100,
        .remaining_quantity = 0,
        .average_price_ticks = 150'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 150'000'000,
                .quantity = 100
            }
        }
    };

    handler.apply_execution(
        "AAPL",
        result
    );

    ASSERT_TRUE(
        manager.has_position("AAPL")
    );

    const auto& position =
        manager.position("AAPL");

    EXPECT_EQ(
        position.quantity(),
        100
    );

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        150'000'000
    );

    EXPECT_TRUE(position.is_long());
}

TEST(
    PositionExecutionHandlerTest,
    AppliesSellExecutionToPosition
) {
    cmarket::PositionManager manager;

    cmarket::PositionExecutionHandler handler(
        manager
    );

    const ExecutionResult result{
        .side = OrderSide::Sell,
        .requested_quantity = 75,
        .executed_quantity = 75,
        .remaining_quantity = 0,
        .average_price_ticks = 200'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Sell,
                .price_ticks = 200'000'000,
                .quantity = 75
            }
        }
    };

    handler.apply_execution(
        "MSFT",
        result
    );

    const auto& position =
        manager.position("MSFT");

    EXPECT_EQ(
        position.quantity(),
        -75
    );

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        200'000'000
    );

    EXPECT_TRUE(position.is_short());
}

TEST(
    PositionExecutionHandlerTest,
    AppliesEveryFillFromMultiLevelExecution
) {
    cmarket::PositionManager manager;

    cmarket::PositionExecutionHandler handler(
        manager
    );

    const ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = 100,
        .executed_quantity = 100,
        .remaining_quantity = 0,
        .average_price_ticks = 106'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 100'000'000,
                .quantity = 40
            },
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 110'000'000,
                .quantity = 60
            }
        }
    };

    handler.apply_execution(
        "AAPL",
        result
    );

    const auto& position =
        manager.position("AAPL");

    EXPECT_EQ(
        position.quantity(),
        100
    );

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        106'000'000
    );
}

TEST(
    PositionExecutionHandlerTest,
    AppliesOnlyExecutedQuantityForPartialFill
) {
    cmarket::PositionManager manager;

    cmarket::PositionExecutionHandler handler(
        manager
    );

    const ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = 100,
        .executed_quantity = 40,
        .remaining_quantity = 60,
        .average_price_ticks = 125'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 125'000'000,
                .quantity = 40
            }
        }
    };

    handler.apply_execution(
        "AAPL",
        result
    );

    const auto& position =
        manager.position("AAPL");

    EXPECT_EQ(
        position.quantity(),
        40
    );

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        125'000'000
    );
}

TEST(
    PositionExecutionHandlerTest,
    DoesNotCreatePositionForUnfilledExecution
) {
    cmarket::PositionManager manager;

    cmarket::PositionExecutionHandler handler(
        manager
    );

    const ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = 100,
        .executed_quantity = 0,
        .remaining_quantity = 100,
        .average_price_ticks = std::nullopt,
        .trades = {}
    };

    handler.apply_execution(
        "AAPL",
        result
    );

    EXPECT_FALSE(
        manager.has_position("AAPL")
    );

    EXPECT_EQ(
        manager.size(),
        std::size_t{0}
    );
}

TEST(
    PositionExecutionHandlerTest,
    UpdatesExistingPositionAcrossExecutions
) {
    cmarket::PositionManager manager;

    cmarket::PositionExecutionHandler handler(
        manager
    );

    const ExecutionResult buy_result{
        .side = OrderSide::Buy,
        .requested_quantity = 100,
        .executed_quantity = 100,
        .remaining_quantity = 0,
        .average_price_ticks = 100'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 100'000'000,
                .quantity = 100
            }
        }
    };

    const ExecutionResult sell_result{
        .side = OrderSide::Sell,
        .requested_quantity = 40,
        .executed_quantity = 40,
        .remaining_quantity = 0,
        .average_price_ticks = 120'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Sell,
                .price_ticks = 120'000'000,
                .quantity = 40
            }
        }
    };

    handler.apply_execution(
        "AAPL",
        buy_result
    );

    handler.apply_execution(
        "AAPL",
        sell_result
    );

    const auto& position =
        manager.position("AAPL");

    EXPECT_EQ(
        position.quantity(),
        60
    );

    EXPECT_EQ(
        position.average_entry_price_ticks(),
        100'000'000
    );

    EXPECT_TRUE(position.is_long());
}

TEST(
    PositionExecutionHandlerTest,
    TracksSymbolsIndependently
) {
    cmarket::PositionManager manager;

    cmarket::PositionExecutionHandler handler(
        manager
    );

    const ExecutionResult buy_result{
        .side = OrderSide::Buy,
        .requested_quantity = 50,
        .executed_quantity = 50,
        .remaining_quantity = 0,
        .average_price_ticks = 150'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 150'000'000,
                .quantity = 50
            }
        }
    };

    const ExecutionResult sell_result{
        .side = OrderSide::Sell,
        .requested_quantity = 25,
        .executed_quantity = 25,
        .remaining_quantity = 0,
        .average_price_ticks = 400'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Sell,
                .price_ticks = 400'000'000,
                .quantity = 25
            }
        }
    };

    handler.apply_execution(
        "AAPL",
        buy_result
    );

    handler.apply_execution(
        "MSFT",
        sell_result
    );

    EXPECT_EQ(
        manager.size(),
        std::size_t{2}
    );

    EXPECT_EQ(
        manager.position("AAPL").quantity(),
        50
    );

    EXPECT_EQ(
        manager.position("MSFT").quantity(),
        -25
    );
}

TEST(
    PositionExecutionHandlerTest,
    RejectsEmptySymbol
) {
    cmarket::PositionManager manager;

    cmarket::PositionExecutionHandler handler(
        manager
    );

    const ExecutionResult result{
        .side = OrderSide::Buy,
        .requested_quantity = 10,
        .executed_quantity = 10,
        .remaining_quantity = 0,
        .average_price_ticks = 100'000'000,
        .trades = {
            Trade{
                .aggressor_side = OrderSide::Buy,
                .price_ticks = 100'000'000,
                .quantity = 10
            }
        }
    };

    EXPECT_THROW(
        handler.apply_execution(
            "",
            result
        ),
        std::invalid_argument
    );

    EXPECT_EQ(
        manager.size(),
        std::size_t{0}
    );
}

}  // namespace
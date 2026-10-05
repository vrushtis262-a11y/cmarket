#include "matching_engine.hpp"
#include "position_execution_handler.hpp"
#include "position_manager.hpp"
#include "risk_limits.hpp"
#include "risk_managed_execution.hpp"
#include "risk_validator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

TEST(
    RiskIntegrationTest,
    AllowsMarketBuyWithinRiskLimitAndUpdatesPosition
) {
    OrderBook order_book;

    order_book.update_ask(
        100'000'000,
        100
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    const ExecutionResult result =
        execution.execute_market_buy(
            "AAPL",
            60
        );

    EXPECT_EQ(
        result.executed_quantity,
        60
    );

    ASSERT_TRUE(
        position_manager.has_position(
            "AAPL"
        )
    );

    EXPECT_EQ(
        position_manager
            .position("AAPL")
            .quantity(),
        60
    );
}

TEST(
    RiskIntegrationTest,
    RejectsMarketBuyBeyondRiskLimitBeforeExecution
) {
    OrderBook order_book;

    order_book.update_ask(
        100'000'000,
        200
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_buy(
                "AAPL",
                101
            )
        ),
        std::runtime_error
    );

    EXPECT_FALSE(
        position_manager.has_position(
            "AAPL"
        )
    );

    ASSERT_TRUE(
        order_book.best_ask().has_value()
    );

    EXPECT_EQ(
        order_book.best_ask()->quantity,
        200
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

TEST(
    RiskIntegrationTest,
    AllowsMarketSellWithinRiskLimitAndUpdatesPosition
) {
    OrderBook order_book;

    order_book.update_bid(
        100'000'000,
        100
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    const ExecutionResult result =
        execution.execute_market_sell(
            "AAPL",
            75
        );

    EXPECT_EQ(
        result.executed_quantity,
        75
    );

    ASSERT_TRUE(
        position_manager.has_position(
            "AAPL"
        )
    );

    EXPECT_EQ(
        position_manager
            .position("AAPL")
            .quantity(),
        -75
    );
}

TEST(
    RiskIntegrationTest,
    RejectsMarketSellBeyondRiskLimitBeforeExecution
) {
    OrderBook order_book;

    order_book.update_bid(
        100'000'000,
        200
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_sell(
                "AAPL",
                101
            )
        ),
        std::runtime_error
    );

    EXPECT_FALSE(
        position_manager.has_position(
            "AAPL"
        )
    );

    ASSERT_TRUE(
        order_book.best_bid().has_value()
    );

    EXPECT_EQ(
        order_book.best_bid()->quantity,
        200
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

TEST(
    RiskIntegrationTest,
    ExistingPositionAffectsNextExecution
) {
    OrderBook order_book;

    order_book.update_ask(
        100'000'000,
        200
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    const ExecutionResult first_result =
        execution.execute_market_buy(
            "AAPL",
            60
        );

    ASSERT_EQ(
        first_result.executed_quantity,
        60
    );

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_buy(
                "AAPL",
                41
            )
        ),
        std::runtime_error
    );

    EXPECT_EQ(
        position_manager
            .position("AAPL")
            .quantity(),
        60
    );

    ASSERT_TRUE(
        order_book.best_ask().has_value()
    );

    EXPECT_EQ(
        order_book.best_ask()->quantity,
        140
    );
}

TEST(
    RiskIntegrationTest,
    AllowsExecutionThatReachesPositionLimit
) {
    OrderBook order_book;

    order_book.update_ask(
        100'000'000,
        100
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        60,
        90'000'000
    );

    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    const ExecutionResult result =
        execution.execute_market_buy(
            "AAPL",
            40
        );

    EXPECT_EQ(
        result.executed_quantity,
        40
    );

    EXPECT_EQ(
        position_manager
            .position("AAPL")
            .quantity(),
        100
    );
}

TEST(
    RiskIntegrationTest,
    PartialFillUpdatesOnlyExecutedExposure
) {
    OrderBook order_book;

    order_book.update_ask(
        100'000'000,
        40
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    const ExecutionResult result =
        execution.execute_market_buy(
            "AAPL",
            100
        );

    EXPECT_EQ(
        result.executed_quantity,
        40
    );

    EXPECT_EQ(
        result.remaining_quantity,
        60
    );

    EXPECT_EQ(
        position_manager
            .position("AAPL")
            .quantity(),
        40
    );

    EXPECT_TRUE(
        risk_validator.is_trade_allowed(
            "AAPL",
            60
        )
    );

    EXPECT_FALSE(
        risk_validator.is_trade_allowed(
            "AAPL",
            61
        )
    );
}

TEST(
    RiskIntegrationTest,
    ReducingExistingLongExposureIsAllowed
) {
    OrderBook order_book;

    order_book.update_bid(
        100'000'000,
        100
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        100,
        90'000'000
    );

    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    const ExecutionResult result =
        execution.execute_market_sell(
            "AAPL",
            40
        );

    EXPECT_EQ(
        result.executed_quantity,
        40
    );

    EXPECT_EQ(
        position_manager
            .position("AAPL")
            .quantity(),
        60
    );
}

TEST(
    RiskIntegrationTest,
    PositionFlipWithinLimitIsAllowed
) {
    OrderBook order_book;

    order_book.update_bid(
        100'000'000,
        200
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        60,
        90'000'000
    );

    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    const ExecutionResult result =
        execution.execute_market_sell(
            "AAPL",
            100
        );

    EXPECT_EQ(
        result.executed_quantity,
        100
    );

    EXPECT_EQ(
        position_manager
            .position("AAPL")
            .quantity(),
        -40
    );
}

TEST(
    RiskIntegrationTest,
    PositionFlipBeyondLimitIsRejectedBeforeExecution
) {
    OrderBook order_book;

    order_book.update_bid(
        100'000'000,
        200
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        60,
        90'000'000
    );

    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_sell(
                "AAPL",
                161
            )
        ),
        std::runtime_error
    );

    EXPECT_EQ(
        position_manager
            .position("AAPL")
            .quantity(),
        60
    );

    ASSERT_TRUE(
        order_book.best_bid().has_value()
    );

    EXPECT_EQ(
        order_book.best_bid()->quantity,
        200
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

TEST(
    RiskIntegrationTest,
    UnfilledExecutionDoesNotCreateExposure
) {
    OrderBook order_book;
    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    const ExecutionResult result =
        execution.execute_market_buy(
            "AAPL",
            100
        );

    EXPECT_TRUE(result.unfilled());

    EXPECT_FALSE(
        position_manager.has_position(
            "AAPL"
        )
    );
}

TEST(
    RiskIntegrationTest,
    RejectsEmptySymbolBeforeExecution
) {
    OrderBook order_book;

    order_book.update_ask(
        100'000'000,
        100
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_buy(
                "",
                50
            )
        ),
        std::invalid_argument
    );

    ASSERT_TRUE(
        order_book.best_ask().has_value()
    );

    EXPECT_EQ(
        order_book.best_ask()->quantity,
        100
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

TEST(
    RiskIntegrationTest,
    RejectsNonPositiveBuyQuantityBeforeExecution
) {
    OrderBook order_book;

    order_book.update_ask(
        100'000'000,
        100
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_buy(
                "AAPL",
                0
            )
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_buy(
                "AAPL",
                -1
            )
        ),
        std::invalid_argument
    );

    ASSERT_TRUE(
        order_book.best_ask().has_value()
    );

    EXPECT_EQ(
        order_book.best_ask()->quantity,
        100
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

TEST(
    RiskIntegrationTest,
    RejectsNonPositiveSellQuantityBeforeExecution
) {
    OrderBook order_book;

    order_book.update_bid(
        100'000'000,
        100
    );

    MatchingEngine engine(order_book);

    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits risk_limits{100};

    const cmarket::RiskValidator risk_validator{
        position_manager,
        risk_limits
    };

    cmarket::PositionExecutionHandler position_handler{
        position_manager
    };

    cmarket::RiskManagedExecution execution{
        engine,
        risk_validator,
        position_handler
    };

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_sell(
                "AAPL",
                0
            )
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_sell(
                "AAPL",
                -1
            )
        ),
        std::invalid_argument
    );

    ASSERT_TRUE(
        order_book.best_bid().has_value()
    );

    EXPECT_EQ(
        order_book.best_bid()->quantity,
        100
    );

    EXPECT_TRUE(
        engine.trade_history().empty()
    );
}

}  // namespace

#include "analytics_execution_handler.hpp"
#include "matching_engine.hpp"
#include "position_execution_handler.hpp"
#include "position_manager.hpp"
#include "risk_limits.hpp"
#include "risk_managed_execution.hpp"
#include "risk_validator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {

class AnalyticsFailureIsolationTest
    : public ::testing::Test {
protected:
    OrderBook book;
    MatchingEngine matching_engine{book};

    cmarket::PositionManager positions;
    const cmarket::RiskLimits limits{100};

    const cmarket::RiskValidator validator{
        positions, limits
    };

    cmarket::PositionExecutionHandler position_handler{
        positions
    };

    cmarket::RollingVWAP vwap{10};
    cmarket::TradeStatistics statistics;

    cmarket::AnalyticsExecutionHandler failing_handler{
        vwap,
        statistics,
        []() {
            throw std::runtime_error(
                "Simulated analytics failure"
            );
        }
    };

    cmarket::RiskManagedExecution execution{
        matching_engine,
        validator,
        position_handler,
        &failing_handler
    };
};

TEST_F(
    AnalyticsFailureIsolationTest,
    BuyExecutionSurvivesAnalyticsFailure
) {
    book.update_ask(100'000'000, 50);

    const auto result =
        execution.execute_market_buy("AAPL", 20);

    EXPECT_EQ(result.executed_quantity, 20);
    EXPECT_EQ(result.remaining_quantity, 0);

    EXPECT_EQ(positions.position("AAPL").quantity(), 20);
    EXPECT_EQ(matching_engine.trade_history().size(), 1U);

    ASSERT_TRUE(book.best_ask());
    EXPECT_EQ(book.best_ask()->quantity, 30);

    EXPECT_FALSE(execution.analytics_healthy());
    EXPECT_FALSE(vwap.get_vwap("AAPL"));
    EXPECT_FALSE(statistics.get_statistics("AAPL"));
}

TEST_F(
    AnalyticsFailureIsolationTest,
    SellExecutionSurvivesAnalyticsFailure
) {
    book.update_bid(100'000'000, 50);

    const auto result =
        execution.execute_market_sell("AAPL", 15);

    EXPECT_EQ(result.executed_quantity, 15);
    EXPECT_EQ(positions.position("AAPL").quantity(), -15);

    EXPECT_EQ(matching_engine.trade_history().size(), 1U);

    ASSERT_TRUE(book.best_bid());
    EXPECT_EQ(book.best_bid()->quantity, 35);

    EXPECT_FALSE(execution.analytics_healthy());
}

TEST_F(
    AnalyticsFailureIsolationTest,
    RiskRejectionStillPreventsExecution
) {
    book.update_ask(100'000'000, 200);

    EXPECT_THROW(
        static_cast<void>(
            execution.execute_market_buy("AAPL", 101)
        ),
        std::runtime_error
    );

    EXPECT_TRUE(matching_engine.trade_history().empty());
    EXPECT_FALSE(positions.has_position("AAPL"));

    EXPECT_TRUE(execution.analytics_healthy());
}

TEST_F(
    AnalyticsFailureIsolationTest,
    UnfilledExecutionDoesNotInvokeAnalytics
) {
    const auto result =
        execution.execute_market_buy("AAPL", 10);

    EXPECT_TRUE(result.unfilled());
    EXPECT_TRUE(execution.analytics_healthy());
    EXPECT_FALSE(positions.has_position("AAPL"));
}

TEST_F(
    AnalyticsFailureIsolationTest,
    MultipleExecutionsRemainCommitted
) {
    book.update_ask(100'000'000, 50);

    const auto first =
        execution.execute_market_buy("AAPL", 10);

    const auto second =
        execution.execute_market_buy("AAPL", 15);

    EXPECT_EQ(first.executed_quantity, 10);
    EXPECT_EQ(second.executed_quantity, 15);

    EXPECT_EQ(positions.position("AAPL").quantity(), 25);
    EXPECT_EQ(matching_engine.trade_history().size(), 2U);

    ASSERT_TRUE(book.best_ask());
    EXPECT_EQ(book.best_ask()->quantity, 25);

    EXPECT_FALSE(execution.analytics_healthy());
}

} // namespace

#include "position_manager.hpp"
#include "risk_limits.hpp"
#include "risk_validator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {

TEST(
    RiskValidatorTest,
    AllowsOpeningLongPositionWithinLimit
) {
    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            50
        )
    );
}

TEST(
    RiskValidatorTest,
    AllowsOpeningShortPositionWithinLimit
) {
    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            -50
        )
    );
}

TEST(
    RiskValidatorTest,
    AllowsOpeningPositionAtLimit
) {
    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            100
        )
    );
}

TEST(
    RiskValidatorTest,
    RejectsOpeningLongPositionBeyondLimit
) {
    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_FALSE(
        validator.is_trade_allowed(
            "AAPL",
            101
        )
    );
}

TEST(
    RiskValidatorTest,
    RejectsOpeningShortPositionBeyondLimit
) {
    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_FALSE(
        validator.is_trade_allowed(
            "AAPL",
            -101
        )
    );
}

TEST(
    RiskValidatorTest,
    UsesExistingLongPositionWhenValidatingTrade
) {
    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        60,
        100'000'000
    );

    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            40
        )
    );

    EXPECT_FALSE(
        validator.is_trade_allowed(
            "AAPL",
            41
        )
    );
}

TEST(
    RiskValidatorTest,
    UsesExistingShortPositionWhenValidatingTrade
) {
    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        -60,
        100'000'000
    );

    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            -40
        )
    );

    EXPECT_FALSE(
        validator.is_trade_allowed(
            "AAPL",
            -41
        )
    );
}

TEST(
    RiskValidatorTest,
    AllowsTradeThatReducesLongExposure
) {
    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        100,
        100'000'000
    );

    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            -40
        )
    );
}

TEST(
    RiskValidatorTest,
    AllowsTradeThatReducesShortExposure
) {
    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        -100,
        100'000'000
    );

    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            40
        )
    );
}

TEST(
    RiskValidatorTest,
    AllowsPositionFlipWithinLimit
) {
    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        60,
        100'000'000
    );

    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            -100
        )
    );
}

TEST(
    RiskValidatorTest,
    RejectsPositionFlipBeyondLimit
) {
    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        60,
        100'000'000
    );

    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_FALSE(
        validator.is_trade_allowed(
            "AAPL",
            -161
        )
    );
}

TEST(
    RiskValidatorTest,
    KeepsSymbolsIndependent
) {
    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        100,
        100'000'000
    );

    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_FALSE(
        validator.is_trade_allowed(
            "AAPL",
            1
        )
    );

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "MSFT",
            100
        )
    );
}

TEST(
    RiskValidatorTest,
    ValidationDoesNotModifyPosition
) {
    cmarket::PositionManager position_manager;

    position_manager.apply_trade(
        "AAPL",
        60,
        100'000'000
    );

    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_TRUE(
        validator.is_trade_allowed(
            "AAPL",
            40
        )
    );

    EXPECT_EQ(
        position_manager.position("AAPL").quantity(),
        60
    );
}

TEST(
    RiskValidatorTest,
    RejectsEmptySymbol
) {
    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_THROW(
        static_cast<void>(
            validator.is_trade_allowed(
                "",
                50
            )
        ),
        std::invalid_argument
    );
}

TEST(
    RiskValidatorTest,
    RejectsZeroTradeQuantity
) {
    cmarket::PositionManager position_manager;
    const cmarket::RiskLimits limits{100};
    const cmarket::RiskValidator validator{
        position_manager,
        limits
    };

    EXPECT_THROW(
        static_cast<void>(
            validator.is_trade_allowed(
                "AAPL",
                0
            )
        ),
        std::invalid_argument
    );
}

}  // namespace
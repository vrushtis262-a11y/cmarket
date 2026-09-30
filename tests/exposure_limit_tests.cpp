#include "risk_limits.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace {

TEST(
    ExposureLimitTest,
    StoresMaximumAbsolutePosition
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_EQ(
        limits.max_absolute_position(),
        100
    );
}

TEST(
    ExposureLimitTest,
    RejectsZeroMaximumAbsolutePosition
) {
    EXPECT_THROW(
        cmarket::RiskLimits(0),
        std::invalid_argument
    );
}

TEST(
    ExposureLimitTest,
    RejectsNegativeMaximumAbsolutePosition
) {
    EXPECT_THROW(
        cmarket::RiskLimits(-1),
        std::invalid_argument
    );
}

TEST(
    ExposureLimitTest,
    AllowsPositionBelowPositiveLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_position_allowed(99)
    );
}

TEST(
    ExposureLimitTest,
    AllowsPositionAtPositiveLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_position_allowed(100)
    );
}

TEST(
    ExposureLimitTest,
    RejectsPositionAbovePositiveLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_FALSE(
        limits.is_position_allowed(101)
    );
}

TEST(
    ExposureLimitTest,
    AllowsPositionBelowNegativeLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_position_allowed(-99)
    );
}

TEST(
    ExposureLimitTest,
    AllowsPositionAtNegativeLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_position_allowed(-100)
    );
}

TEST(
    ExposureLimitTest,
    RejectsPositionBeyondNegativeLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_FALSE(
        limits.is_position_allowed(-101)
    );
}

TEST(
    ExposureLimitTest,
    AllowsFlatPosition
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_position_allowed(0)
    );
}

TEST(
    ExposureLimitTest,
    AllowsTradeThatKeepsPositionWithinLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_trade_allowed(
            60,
            40
        )
    );
}

TEST(
    ExposureLimitTest,
    AllowsTradeThatReachesPositionLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_trade_allowed(
            60,
            40
        )
    );
}

TEST(
    ExposureLimitTest,
    RejectsTradeThatExceedsLongPositionLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_FALSE(
        limits.is_trade_allowed(
            60,
            41
        )
    );
}

TEST(
    ExposureLimitTest,
    RejectsTradeThatExceedsShortPositionLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_FALSE(
        limits.is_trade_allowed(
            -60,
            -41
        )
    );
}

TEST(
    ExposureLimitTest,
    AllowsTradeThatReducesLongExposure
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_trade_allowed(
            100,
            -40
        )
    );
}

TEST(
    ExposureLimitTest,
    AllowsTradeThatReducesShortExposure
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_trade_allowed(
            -100,
            40
        )
    );
}

TEST(
    ExposureLimitTest,
    AllowsTradeThatFlipsPositionWithinLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_TRUE(
        limits.is_trade_allowed(
            60,
            -100
        )
    );
}

TEST(
    ExposureLimitTest,
    RejectsTradeThatFlipsPositionBeyondLimit
) {
    const cmarket::RiskLimits limits{100};

    EXPECT_FALSE(
        limits.is_trade_allowed(
            60,
            -161
        )
    );
}

}  // namespace
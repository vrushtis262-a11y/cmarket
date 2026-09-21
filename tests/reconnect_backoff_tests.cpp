#include "reconnect_backoff.hpp"

#include <gtest/gtest.h>

TEST(
    ReconnectBackoffTest,
    StartsAtInitialDelay
)
{
    ReconnectBackoff backoff;

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        1
    );
}

TEST(
    ReconnectBackoffTest,
    GrowsExponentiallyAndCapsAtMaximum
)
{
    ReconnectBackoff backoff;

    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        2
    );

    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        4
    );

    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        8
    );

    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        16
    );

    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        30
    );

    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        30
    );

    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        30
    );
}

TEST(
    ReconnectBackoffTest,
    ResetRestoresInitialDelay
)
{
    ReconnectBackoff backoff;

    backoff.advance();
    backoff.advance();
    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        8
    );

    backoff.reset();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        1
    );

    backoff.advance();

    EXPECT_EQ(
        backoff.current_delay_seconds(),
        2
    );
}

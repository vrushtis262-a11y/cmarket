
#include "order_flow_imbalance.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

using cmarket::OFILevel;
using cmarket::OFISnapshot;
using cmarket::OrderFlowImbalance;

namespace {

OFISnapshot make_snapshot(
    std::int64_t bid_price,
    std::int64_t bid_quantity,
    std::int64_t ask_price,
    std::int64_t ask_quantity
) {
    return OFISnapshot{
        OFILevel{bid_price, bid_quantity},
        OFILevel{ask_price, ask_quantity}
    };
}

} // namespace

TEST(OrderFlowImbalanceTest, RejectsZeroWindowSize) {
    EXPECT_THROW(
        OrderFlowImbalance(0),
        std::invalid_argument
    );
}

TEST(OrderFlowImbalanceTest, FirstSnapshotIsBaseline) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    EXPECT_EQ(ofi.event_count("AAPL"), 0U);
    EXPECT_FALSE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_FALSE(ofi.rolling_ofi("AAPL").has_value());
}

TEST(OrderFlowImbalanceTest, IncreasingBidQuantity) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 70, 110, 40)
    );

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 20.0L);
}

TEST(OrderFlowImbalanceTest, IncreasingAskQuantity) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 60)
    );

    ASSERT_TRUE(ofi.latest_ofi("AAPL").has_value());
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -20.0L);
}

TEST(OrderFlowImbalanceTest, RemovingBidLiquidity) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 30, 110, 40)
    );

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -20.0L);
}

TEST(OrderFlowImbalanceTest, RemovingAskLiquidity) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 25)
    );

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 15.0L);
}

TEST(OrderFlowImbalanceTest, BidPriceImprovement) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(105, 30, 110, 40)
    );

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 30.0L);
}

TEST(OrderFlowImbalanceTest, AskPriceImprovement) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 105, 25)
    );

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), -25.0L);
}

TEST(OrderFlowImbalanceTest, MixedBookChanges) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 70, 110, 45)
    );

    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 15.0L);
}

TEST(OrderFlowImbalanceTest, RollingWindowExpires) {
    OrderFlowImbalance ofi(2);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 60, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 80, 110, 40)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 85, 110, 40)
    );

    EXPECT_EQ(ofi.event_count("AAPL"), 2U);
    EXPECT_EQ(*ofi.latest_ofi("AAPL"), 5.0L);
    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 25.0L);
}

TEST(OrderFlowImbalanceTest, MultipleSymbols) {
    OrderFlowImbalance ofi(10);

    ofi.update(
        "AAPL",
        make_snapshot(100, 50, 110, 40)
    );

    ofi.update(
        "MSFT",
        make_snapshot(200, 60, 220, 50)
    );

    ofi.update(
        "AAPL",
        make_snapshot(100, 70, 110, 40)
    );

    ofi.update(
        "MSFT",
        make_snapshot(200, 60, 220, 70)
    );

    EXPECT_EQ(*ofi.rolling_ofi("AAPL"), 20.0L);
    EXPECT_EQ(*ofi.rolling_ofi("MSFT"), -20.0L);
}

TEST(OrderFlowImbalanceTest, RejectsInvalidSnapshots) {
    OrderFlowImbalance ofi(10);

    EXPECT_THROW(
        ofi.update(
            "",
            make_snapshot(100, 50, 110, 40)
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        ofi.update(
            "AAPL",
            make_snapshot(0, 50, 110, 40)
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        ofi.update(
            "AAPL",
            make_snapshot(100, 0, 110, 40)
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        ofi.update(
            "AAPL",
            make_snapshot(110, 50, 100, 40)
        ),
        std::invalid_argument
    );

    EXPECT_FALSE(ofi.rolling_ofi("AAPL").has_value());
}

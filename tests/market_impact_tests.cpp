
#include "market_impact.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace cmarket;

namespace {

bool approximately_equal(
    long double actual,
    long double expected
) {
    return std::fabs(actual - expected) < 0.000001L;
}

void check_value(
    const std::optional<long double>& actual,
    long double expected
) {
    assert(actual.has_value());
    assert(approximately_equal(*actual, expected));
}

ExecutionResult make_execution(
    OrderSide side,
    std::int64_t requested,
    std::vector<Trade> trades
) {
    std::int64_t executed = 0;

    for (const Trade& trade : trades) {
        executed += trade.quantity;
    }

    return ExecutionResult{
        .side = side,
        .requested_quantity = requested,
        .executed_quantity = executed,
        .remaining_quantity = requested - executed,
        .average_price_ticks = std::nullopt,
        .trades = std::move(trades)
    };
}

void test_single_buy() {
    const auto execution = make_execution(
        OrderSide::Buy,
        10,
        {{OrderSide::Buy, 102, 10}}
    );

    const MarketQuote before{98, 102, 100, 50};
    const MarketQuote after{99, 103, 100, 40};

    const auto result =
        MarketImpact::analyze(execution, before, after);

    assert(result.fill_count == 1);
    assert(result.executed_quantity == 10);
    assert(result.liquidity_consumed == 10);

    check_value(result.average_execution_price_ticks, 102);
    check_value(result.slippage_ticks, 0);
    check_value(result.effective_spread_ticks, 4);
    check_value(result.mid_price_change_ticks, 1);
    check_value(result.signed_mid_impact_ticks, 1);
    check_value(result.liquidity_consumed_ratio, 0.2L);
}

void test_multiple_buy_fills() {
    const auto execution = make_execution(
        OrderSide::Buy,
        5,
        {
            {OrderSide::Buy, 102, 2},
            {OrderSide::Buy, 104, 3}
        }
    );

    const MarketQuote before{98, 102, 100, 20};
    const MarketQuote after{99, 105, 100, 15};

    const auto result =
        MarketImpact::analyze(execution, before, after);

    assert(result.fill_count == 2);
    assert(result.executed_quantity == 5);

    check_value(result.average_execution_price_ticks, 103.2L);
    check_value(result.slippage_ticks, 1.2L);
    check_value(result.effective_spread_ticks, 6.4L);
    check_value(result.mid_price_change_ticks, 2);
    check_value(result.liquidity_consumed_ratio, 0.25L);
}

void test_sell_impact() {
    const auto execution = make_execution(
        OrderSide::Sell,
        5,
        {
            {OrderSide::Sell, 98, 4},
            {OrderSide::Sell, 96, 1}
        }
    );

    const MarketQuote before{98, 102, 20, 100};
    const MarketQuote after{97, 101, 15, 100};

    const auto result =
        MarketImpact::analyze(execution, before, after);

    check_value(result.average_execution_price_ticks, 97.6L);
    check_value(result.slippage_ticks, 0.4L);
    check_value(result.effective_spread_ticks, 4.8L);
    check_value(result.mid_price_change_ticks, -1);
    check_value(result.signed_mid_impact_ticks, 1);
    check_value(result.liquidity_consumed_ratio, 0.25L);
}

void test_partial_fill() {
    const auto execution = make_execution(
        OrderSide::Buy,
        10,
        {{OrderSide::Buy, 102, 4}}
    );

    const MarketQuote before{98, 102, 100, 10};
    const MarketQuote after{98, 104, 100, 6};

    const auto result =
        MarketImpact::analyze(execution, before, after);

    assert(result.requested_quantity == 10);
    assert(result.executed_quantity == 4);
    assert(result.remaining_quantity == 6);
    assert(result.liquidity_consumed == 4);

    check_value(result.liquidity_consumed_ratio, 0.4L);
}

void test_empty_execution() {
    const auto execution =
        make_execution(OrderSide::Buy, 10, {});

    const MarketQuote before{98, 102, 100, 100};
    const MarketQuote after{98, 102, 100, 100};

    const auto result =
        MarketImpact::analyze(execution, before, after);

    assert(result.fill_count == 0);
    assert(result.executed_quantity == 0);
    assert(!result.average_execution_price_ticks);
    assert(!result.slippage_ticks);
    assert(!result.effective_spread_ticks);
}

void test_missing_quotes() {
    const auto execution = make_execution(
        OrderSide::Buy,
        2,
        {{OrderSide::Buy, 105, 2}}
    );

    const MarketQuote before{
        std::nullopt,
        std::nullopt,
        0,
        0
    };

    const MarketQuote after{
        std::nullopt,
        std::nullopt,
        0,
        0
    };

    const auto result =
        MarketImpact::analyze(execution, before, after);

    check_value(result.average_execution_price_ticks, 105);

    assert(!result.reference_price_ticks);
    assert(!result.slippage_ticks);
    assert(!result.effective_spread_ticks);
    assert(!result.mid_price_change_ticks);
    assert(!result.liquidity_consumed_ratio);
}

void test_invalid_execution() {
    auto execution = make_execution(
        OrderSide::Buy,
        5,
        {{OrderSide::Buy, 102, 5}}
    );

    execution.executed_quantity = 4;

    const MarketQuote quote{98, 102, 100, 100};

    bool rejected = false;

    try {
        (void)MarketImpact::analyze(
            execution,
            quote,
            quote
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
}

} // namespace

int main() {
    test_single_buy();
    test_multiple_buy_fills();
    test_sell_impact();
    test_partial_fill();
    test_empty_execution();
    test_missing_quotes();
    test_invalid_execution();

    std::cout
        << "All 7 market impact tests passed.\n";

    return 0;
}

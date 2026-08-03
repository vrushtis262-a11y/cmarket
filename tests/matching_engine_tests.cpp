#include "matching_engine.hpp"

#include <gtest/gtest.h>

TEST(MatchingEngineTest, ExecutesSingleLevelMarketSell)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 530'000,
                .quantity = 100
            }
        },
        {}
    );

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_sell(40);

    EXPECT_EQ(result.side, OrderSide::Sell);
    EXPECT_EQ(result.requested_quantity, 40);
    EXPECT_EQ(result.executed_quantity, 40);
    EXPECT_EQ(result.remaining_quantity, 0);
    EXPECT_TRUE(result.fully_filled());

    ASSERT_TRUE(result.average_price_ticks.has_value());
    EXPECT_EQ(*result.average_price_ticks, 530'000);

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(
        result.trades[0].aggressor_side,
        OrderSide::Sell
    );
    EXPECT_EQ(result.trades[0].price_ticks, 530'000);
    EXPECT_EQ(result.trades[0].quantity, 40);

    ASSERT_EQ(order_book.bids().size(), 1U);
    EXPECT_EQ(order_book.bids()[0].price_ticks, 530'000);
    EXPECT_EQ(order_book.bids()[0].quantity, 60);
}

TEST(MatchingEngineTest, ExecutesMarketSellAcrossMultipleBidLevels)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 50
            },
            PriceLevel{
                .price_ticks = 530'000,
                .quantity = 40
            },
            PriceLevel{
                .price_ticks = 510'000,
                .quantity = 30
            }
        },
        {}
    );

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_sell(60);

    EXPECT_EQ(result.executed_quantity, 60);
    EXPECT_EQ(result.remaining_quantity, 0);
    EXPECT_TRUE(result.fully_filled());

    ASSERT_EQ(result.trades.size(), 2U);

    EXPECT_EQ(result.trades[0].price_ticks, 530'000);
    EXPECT_EQ(result.trades[0].quantity, 40);

    EXPECT_EQ(result.trades[1].price_ticks, 520'000);
    EXPECT_EQ(result.trades[1].quantity, 20);

    ASSERT_TRUE(result.average_price_ticks.has_value());
    EXPECT_EQ(*result.average_price_ticks, 526'666);

    ASSERT_EQ(order_book.bids().size(), 2U);
    EXPECT_EQ(order_book.bids()[0].price_ticks, 520'000);
    EXPECT_EQ(order_book.bids()[0].quantity, 30);
    EXPECT_EQ(order_book.bids()[1].price_ticks, 510'000);
    EXPECT_EQ(order_book.bids()[1].quantity, 30);
}

TEST(MatchingEngineTest, ExactlyFillsMarketSell)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 530'000,
                .quantity = 40
            },
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 60
            }
        },
        {}
    );

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_sell(100);

    EXPECT_EQ(result.executed_quantity, 100);
    EXPECT_EQ(result.remaining_quantity, 0);
    EXPECT_TRUE(result.fully_filled());
    EXPECT_TRUE(order_book.bids().empty());

    ASSERT_TRUE(result.average_price_ticks.has_value());
    EXPECT_EQ(*result.average_price_ticks, 524'000);
}

TEST(MatchingEngineTest, PartiallyFillsMarketSell)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 530'000,
                .quantity = 25
            }
        },
        {}
    );

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_sell(40);

    EXPECT_EQ(result.requested_quantity, 40);
    EXPECT_EQ(result.executed_quantity, 25);
    EXPECT_EQ(result.remaining_quantity, 15);
    EXPECT_TRUE(result.partially_filled());
    EXPECT_FALSE(result.fully_filled());
    EXPECT_FALSE(result.unfilled());

    ASSERT_TRUE(result.average_price_ticks.has_value());
    EXPECT_EQ(*result.average_price_ticks, 530'000);

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].quantity, 25);

    EXPECT_TRUE(order_book.bids().empty());
}

TEST(MatchingEngineTest, LeavesMarketSellUnfilledForEmptyBidBook)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {},
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 100
            }
        }
    );

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_sell(50);

    EXPECT_EQ(result.executed_quantity, 0);
    EXPECT_EQ(result.remaining_quantity, 50);
    EXPECT_TRUE(result.unfilled());
    EXPECT_FALSE(result.average_price_ticks.has_value());
    EXPECT_TRUE(result.trades.empty());

    ASSERT_EQ(order_book.asks().size(), 1U);
    EXPECT_EQ(order_book.asks()[0].quantity, 100);
}

TEST(MatchingEngineTest, ConsumesAllAvailableBidsWhenLiquidityIsInsufficient)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 530'000,
                .quantity = 20
            },
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 30
            }
        },
        {}
    );

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_sell(100);

    EXPECT_EQ(result.executed_quantity, 50);
    EXPECT_EQ(result.remaining_quantity, 50);
    EXPECT_TRUE(result.partially_filled());

    ASSERT_EQ(result.trades.size(), 2U);
    EXPECT_EQ(result.trades[0].price_ticks, 530'000);
    EXPECT_EQ(result.trades[0].quantity, 20);
    EXPECT_EQ(result.trades[1].price_ticks, 520'000);
    EXPECT_EQ(result.trades[1].quantity, 30);

    ASSERT_TRUE(result.average_price_ticks.has_value());
    EXPECT_EQ(*result.average_price_ticks, 524'000);

    EXPECT_TRUE(order_book.bids().empty());
}

TEST(MatchingEngineTest, RecordsTradesAcrossMultipleExecutions)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 40
            }
        },
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 30
            }
        }
    );

    MatchingEngine engine(order_book);

    const ExecutionResult buy_result =
        engine.execute_market_buy(20);

    const ExecutionResult sell_result =
        engine.execute_market_sell(25);

    ASSERT_EQ(buy_result.trades.size(), 1U);
    ASSERT_EQ(sell_result.trades.size(), 1U);

    const std::vector<Trade>& history =
        engine.trade_history();

    ASSERT_EQ(history.size(), 2U);

    EXPECT_EQ(history[0].aggressor_side, OrderSide::Buy);
    EXPECT_EQ(history[0].price_ticks, 540'000);
    EXPECT_EQ(history[0].quantity, 20);

    EXPECT_EQ(history[1].aggressor_side, OrderSide::Sell);
    EXPECT_EQ(history[1].price_ticks, 520'000);
    EXPECT_EQ(history[1].quantity, 25);
}

TEST(MatchingEngineTest, RecordsEveryTradeFromMultiLevelExecution)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {},
        {
            PriceLevel{
                .price_ticks = 540'000,
                .quantity = 20
            },
            PriceLevel{
                .price_ticks = 550'000,
                .quantity = 30
            }
        }
    );

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_buy(40);

    ASSERT_EQ(result.trades.size(), 2U);

    const std::vector<Trade>& history =
        engine.trade_history();

    ASSERT_EQ(history.size(), 2U);

    EXPECT_EQ(history[0].aggressor_side, OrderSide::Buy);
    EXPECT_EQ(history[0].price_ticks, 540'000);
    EXPECT_EQ(history[0].quantity, 20);

    EXPECT_EQ(history[1].aggressor_side, OrderSide::Buy);
    EXPECT_EQ(history[1].price_ticks, 550'000);
    EXPECT_EQ(history[1].quantity, 20);
}

TEST(MatchingEngineTest, ClearsTradeHistory)
{
    OrderBook order_book;

    order_book.replace_snapshot(
        {
            PriceLevel{
                .price_ticks = 520'000,
                .quantity = 50
            }
        },
        {}
    );

    MatchingEngine engine(order_book);

    engine.execute_market_sell(20);

    ASSERT_EQ(engine.trade_history().size(), 1U);

    engine.clear_trade_history();

    EXPECT_TRUE(engine.trade_history().empty());
}

TEST(MatchingEngineTest, DoesNotRecordTradeForUnfilledOrder)
{
    OrderBook order_book;

    MatchingEngine engine(order_book);

    const ExecutionResult result =
        engine.execute_market_buy(50);

    EXPECT_TRUE(result.unfilled());
    EXPECT_TRUE(result.trades.empty());
    EXPECT_TRUE(engine.trade_history().empty());
}
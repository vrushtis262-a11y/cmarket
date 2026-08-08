#include "matching_engine.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

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

    const ExecutionResult result =
        engine.execute_market_sell(20);

    EXPECT_EQ(result.executed_quantity, 20);
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

TEST(MatchingEngineTest, PlacesLimitBuyOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(525'000, 100);

    EXPECT_EQ(order_id, 1U);

    const std::vector<LimitOrder>& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(orders[0].order_id, 1U);
    EXPECT_EQ(orders[0].side, OrderSide::Buy);
    EXPECT_EQ(orders[0].price_ticks, 525'000);
    EXPECT_EQ(orders[0].original_quantity, 100);
    EXPECT_EQ(orders[0].remaining_quantity, 100);
    EXPECT_EQ(orders[0].sequence_number, 1U);
}

TEST(MatchingEngineTest, PlacesLimitSellOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_sell(540'000, 80);

    EXPECT_EQ(order_id, 1U);

    const std::vector<LimitOrder>& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(orders[0].order_id, 1U);
    EXPECT_EQ(orders[0].side, OrderSide::Sell);
    EXPECT_EQ(orders[0].price_ticks, 540'000);
    EXPECT_EQ(orders[0].original_quantity, 80);
    EXPECT_EQ(orders[0].remaining_quantity, 80);
    EXPECT_EQ(orders[0].sequence_number, 1U);
}

TEST(MatchingEngineTest, AssignsSequentialOrderIds)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId first =
        engine.place_limit_buy(520'000, 50);

    const OrderId second =
        engine.place_limit_sell(540'000, 60);

    const OrderId third =
        engine.place_limit_buy(515'000, 70);

    EXPECT_EQ(first, 1U);
    EXPECT_EQ(second, 2U);
    EXPECT_EQ(third, 3U);
}

TEST(MatchingEngineTest, AssignsFifoSequenceNumbers)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_buy(520'000, 50)
    );

    static_cast<void>(
        engine.place_limit_buy(520'000, 60)
    );

    static_cast<void>(
        engine.place_limit_sell(540'000, 70)
    );

    const std::vector<LimitOrder>& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 3U);

    EXPECT_EQ(orders[0].sequence_number, 1U);
    EXPECT_EQ(orders[1].sequence_number, 2U);
    EXPECT_EQ(orders[2].sequence_number, 3U);
}

TEST(MatchingEngineTest, StoresMultipleActiveLimitOrders)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_buy(520'000, 50)
    );

    static_cast<void>(
        engine.place_limit_sell(540'000, 60)
    );

    static_cast<void>(
        engine.place_limit_buy(515'000, 70)
    );

    const std::vector<LimitOrder>& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 3U);

    EXPECT_EQ(orders[0].side, OrderSide::Buy);
    EXPECT_EQ(orders[1].side, OrderSide::Sell);
    EXPECT_EQ(orders[2].side, OrderSide::Buy);
}

TEST(MatchingEngineTest, RejectsNonPositiveLimitPrice)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    EXPECT_THROW(
        static_cast<void>(
            engine.place_limit_buy(0, 100)
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            engine.place_limit_sell(-1, 100)
        ),
        std::invalid_argument
    );

    EXPECT_TRUE(
        engine.active_limit_orders().empty()
    );
}

TEST(MatchingEngineTest, RejectsNonPositiveLimitQuantity)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    EXPECT_THROW(
        static_cast<void>(
            engine.place_limit_buy(525'000, 0)
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            engine.place_limit_sell(540'000, -1)
        ),
        std::invalid_argument
    );

    EXPECT_TRUE(
        engine.active_limit_orders().empty()
    );
}

TEST(MatchingEngineTest, CancelsExistingOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(520'000, 100);

    EXPECT_TRUE(
        engine.cancel_order(order_id)
    );

    EXPECT_TRUE(
        engine.active_limit_orders().empty()
    );
}

TEST(MatchingEngineTest, CancelMissingOrderReturnsFalse)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            100
        )
    );

    EXPECT_FALSE(
        engine.cancel_order(999)
    );

    EXPECT_EQ(
        engine.active_limit_orders().size(),
        1U
    );
}

TEST(MatchingEngineTest, CancelsFirstOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId first =
        engine.place_limit_buy(520'000, 10);

    static_cast<void>(
        engine.place_limit_buy(
            521'000,
            20
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            522'000,
            30
        )
    );

    EXPECT_TRUE(
        engine.cancel_order(first)
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 2U);

    EXPECT_EQ(orders[0].order_id, 2U);
    EXPECT_EQ(orders[1].order_id, 3U);
}

TEST(MatchingEngineTest, CancelsMiddleOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            10
        )
    );

    const OrderId middle =
        engine.place_limit_buy(521'000, 20);

    static_cast<void>(
        engine.place_limit_buy(
            522'000,
            30
        )
    );

    EXPECT_TRUE(
        engine.cancel_order(middle)
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 2U);

    EXPECT_EQ(orders[0].order_id, 1U);
    EXPECT_EQ(orders[1].order_id, 3U);
}

TEST(MatchingEngineTest, CancelsLastOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            10
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            521'000,
            20
        )
    );

    const OrderId last =
        engine.place_limit_buy(522'000, 30);

    EXPECT_TRUE(
        engine.cancel_order(last)
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 2U);

    EXPECT_EQ(orders[0].order_id, 1U);
    EXPECT_EQ(orders[1].order_id, 2U);
}

TEST(MatchingEngineTest, ModifiesOrderPrice)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(520'000, 100);

    const SequenceNumber original_sequence =
        engine.active_limit_orders()[0].sequence_number;

    EXPECT_TRUE(
        engine.modify_order(
            order_id,
            525'000,
            100
        )
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(orders[0].order_id, order_id);
    EXPECT_EQ(orders[0].price_ticks, 525'000);
    EXPECT_EQ(orders[0].original_quantity, 100);
    EXPECT_EQ(orders[0].remaining_quantity, 100);

    EXPECT_GT(
        orders[0].sequence_number,
        original_sequence
    );
}

TEST(MatchingEngineTest, ModifiesOrderQuantity)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(520'000, 100);

    const SequenceNumber original_sequence =
        engine.active_limit_orders()[0].sequence_number;

    EXPECT_TRUE(
        engine.modify_order(
            order_id,
            520'000,
            60
        )
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(orders[0].order_id, order_id);
    EXPECT_EQ(orders[0].price_ticks, 520'000);
    EXPECT_EQ(orders[0].original_quantity, 60);
    EXPECT_EQ(orders[0].remaining_quantity, 60);

    EXPECT_EQ(
        orders[0].sequence_number,
        original_sequence
    );
}

TEST(MatchingEngineTest, RejectsInvalidModifiedPrice)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(520'000, 100);

    EXPECT_THROW(
        static_cast<void>(
            engine.modify_order(
                order_id,
                0,
                100
            )
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            engine.modify_order(
                order_id,
                -1,
                100
            )
        ),
        std::invalid_argument
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);
    EXPECT_EQ(orders[0].price_ticks, 520'000);
    EXPECT_EQ(orders[0].remaining_quantity, 100);
}

TEST(MatchingEngineTest, RejectsInvalidModifiedQuantity)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId order_id =
        engine.place_limit_buy(520'000, 100);

    EXPECT_THROW(
        static_cast<void>(
            engine.modify_order(
                order_id,
                520'000,
                0
            )
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        static_cast<void>(
            engine.modify_order(
                order_id,
                520'000,
                -1
            )
        ),
        std::invalid_argument
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);
    EXPECT_EQ(orders[0].price_ticks, 520'000);
    EXPECT_EQ(orders[0].remaining_quantity, 100);
}

TEST(MatchingEngineTest, ModifyUnknownOrderReturnsFalse)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId existing_order_id =
        engine.place_limit_buy(520'000, 100);

    EXPECT_FALSE(
        engine.modify_order(
            999,
            525'000,
            80
        )
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        existing_order_id
    );

    EXPECT_EQ(
        orders[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        100
    );
}

TEST(MatchingEngineTest, MatchesCrossingLimitBuyOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            100
        )
    );

    static_cast<void>(
        engine.place_limit_buy(
            525'000,
            100
        )
    );

    EXPECT_TRUE(
        engine.active_limit_orders().empty()
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(
        trades.size(),
        1U
    );

    EXPECT_EQ(
        trades[0].aggressor_side,
        OrderSide::Buy
    );

    EXPECT_EQ(
        trades[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        100
    );

    EXPECT_TRUE(
        order_book.empty()
    );
}

TEST(MatchingEngineTest, MatchesCrossingLimitSellOrder)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            100
        )
    );

    static_cast<void>(
        engine.place_limit_sell(
            515'000,
            100
        )
    );

    EXPECT_TRUE(
        engine.active_limit_orders().empty()
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(
        trades.size(),
        1U
    );

    EXPECT_EQ(
        trades[0].aggressor_side,
        OrderSide::Sell
    );

    EXPECT_EQ(
        trades[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        100
    );

    EXPECT_TRUE(
        order_book.empty()
    );
}

TEST(MatchingEngineTest, MatchesOldestOrderFirstAtSamePrice)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    static_cast<void>(
        engine.place_limit_sell(
            520'000,
            100
        )
    );

    const OrderId second =
        engine.place_limit_sell(
            520'000,
            100
        );

    static_cast<void>(
        engine.place_limit_buy(
            520'000,
            100
        )
    );

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 1U);

    EXPECT_EQ(
        orders[0].order_id,
        second
    );

    EXPECT_EQ(
        orders[0].remaining_quantity,
        100
    );

    const auto& trades =
        engine.trade_history();

    ASSERT_EQ(
        trades.size(),
        1U
    );

    EXPECT_EQ(
        trades[0].price_ticks,
        520'000
    );

    EXPECT_EQ(
        trades[0].quantity,
        100
    );
}

TEST(MatchingEngineTest, AssignsUniqueIdsToActiveOrders)
{
    OrderBook order_book;
    MatchingEngine engine(order_book);

    const OrderId first =
        engine.place_limit_buy(
            500'000,
            100
        );

    const OrderId second =
        engine.place_limit_buy(
            510'000,
            100
        );

    const OrderId third =
        engine.place_limit_sell(
            550'000,
            100
        );

    EXPECT_NE(first, second);
    EXPECT_NE(first, third);
    EXPECT_NE(second, third);

    const auto& orders =
        engine.active_limit_orders();

    ASSERT_EQ(orders.size(), 3U);

    EXPECT_EQ(orders[0].order_id, first);
    EXPECT_EQ(orders[1].order_id, second);
    EXPECT_EQ(orders[2].order_id, third);
}
#ifndef CMARKET_CONCURRENT_MATCHING_ENGINE_HPP
#define CMARKET_CONCURRENT_MATCHING_ENGINE_HPP

#include "blocking_queue.hpp"
#include "engine_command.hpp"
#include "matching_engine.hpp"

#include <cstdint>
#include <future>
#include <thread>
#include <vector>

class ConcurrentMatchingEngine {
public:
    explicit ConcurrentMatchingEngine(
        OrderBook& order_book
    );

    ~ConcurrentMatchingEngine();

    ConcurrentMatchingEngine(
        const ConcurrentMatchingEngine&
    ) = delete;

    ConcurrentMatchingEngine& operator=(
        const ConcurrentMatchingEngine&
    ) = delete;

    ConcurrentMatchingEngine(
        ConcurrentMatchingEngine&&
    ) = delete;

    ConcurrentMatchingEngine& operator=(
        ConcurrentMatchingEngine&&
    ) = delete;

    [[nodiscard]]
    std::future<OrderId> place_limit_buy(
        std::int64_t price_ticks,
        std::int64_t quantity
    );

    [[nodiscard]]
    std::future<OrderId> place_limit_sell(
        std::int64_t price_ticks,
        std::int64_t quantity
    );

    [[nodiscard]]
    std::future<ExecutionResult> execute_market_buy(
        std::int64_t quantity
    );

    [[nodiscard]]
    std::future<ExecutionResult> execute_market_sell(
        std::int64_t quantity
    );

    [[nodiscard]]
    std::future<bool> cancel_order(
        OrderId order_id
    );

    [[nodiscard]]
    std::future<bool> modify_order(
        OrderId order_id,
        std::int64_t new_price_ticks,
        std::int64_t new_quantity
    );

    [[nodiscard]]
    std::future<std::vector<LimitOrder>>
    active_limit_orders();

    [[nodiscard]]
    std::future<std::vector<Trade>>
    trade_history();

    [[nodiscard]]
    std::future<void> clear_trade_history();

    void shutdown();

    [[nodiscard]]
    bool is_shutdown() const;

private:
    void worker_loop();

    void process_command(
        EngineCommand& command
    );

    MatchingEngine engine_;
    BlockingQueue<EngineCommandPtr> command_queue_;
    std::thread worker_;
};

#endif
#ifndef CMARKET_ENGINE_COMMAND_HPP
#define CMARKET_ENGINE_COMMAND_HPP

#include "execution.hpp"
#include "limit_order.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <utility>
#include <vector>

struct PlaceLimitBuyCommand {
    std::int64_t price_ticks;
    std::int64_t quantity;
    std::promise<OrderId> result;
};

struct PlaceLimitSellCommand {
    std::int64_t price_ticks;
    std::int64_t quantity;
    std::promise<OrderId> result;
};

struct ExecuteMarketBuyCommand {
    std::int64_t quantity;
    std::promise<ExecutionResult> result;
};

struct ExecuteMarketSellCommand {
    std::int64_t quantity;
    std::promise<ExecutionResult> result;
};

struct CancelOrderCommand {
    OrderId order_id;
    std::promise<bool> result;
};

struct ModifyOrderCommand {
    OrderId order_id;
    std::int64_t new_price_ticks;
    std::int64_t new_quantity;
    std::promise<bool> result;
};

struct ActiveOrdersCommand {
    std::promise<std::vector<LimitOrder>> result;
};

struct TradeHistoryCommand {
    std::promise<std::vector<Trade>> result;
};

struct ClearTradeHistoryCommand {
    std::promise<void> result;
};

class EngineCommand {
public:
    virtual ~EngineCommand() = default;

    EngineCommand(
        const EngineCommand&
    ) = delete;

    EngineCommand& operator=(
        const EngineCommand&
    ) = delete;

    EngineCommand(
        EngineCommand&&
    ) = delete;

    EngineCommand& operator=(
        EngineCommand&&
    ) = delete;

protected:
    EngineCommand() = default;
};

template <typename Command>
class EngineCommandHolder final
    : public EngineCommand {
public:
    explicit EngineCommandHolder(
        Command command
    )
        : command_(std::move(command))
    {
    }

    Command& command() noexcept
    {
        return command_;
    }

private:
    Command command_;
};

using EngineCommandPtr =
    std::unique_ptr<EngineCommand>;

#endif
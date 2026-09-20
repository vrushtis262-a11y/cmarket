#include "concurrent_matching_engine.hpp"

#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

template <typename Result, typename Function>
void execute_command(
    std::promise<Result>& promise,
    Function&& function
)
{
    try {
        promise.set_value(
            std::forward<Function>(function)()
        );
    }
    catch (...) {
        promise.set_exception(
            std::current_exception()
        );
    }
}

template <typename Function>
void execute_void_command(
    std::promise<void>& promise,
    Function&& function
)
{
    try {
        std::forward<Function>(function)();
        promise.set_value();
    }
    catch (...) {
        promise.set_exception(
            std::current_exception()
        );
    }
}

} // namespace

ConcurrentMatchingEngine::ConcurrentMatchingEngine(
    OrderBook& order_book
)
    : engine_(order_book),
      worker_(
          &ConcurrentMatchingEngine::worker_loop,
          this
      )
{
}

ConcurrentMatchingEngine::~ConcurrentMatchingEngine()
{
    shutdown();
}

std::future<OrderId>
ConcurrentMatchingEngine::place_limit_buy(
    std::int64_t price_ticks,
    std::int64_t quantity
)
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                PlaceLimitBuyCommand
            >
        >(
            PlaceLimitBuyCommand{
                .price_ticks = price_ticks,
                .quantity = quantity,
                .result = {}
            }
        );

    std::future<OrderId> future =
        command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

std::future<OrderId>
ConcurrentMatchingEngine::place_limit_sell(
    std::int64_t price_ticks,
    std::int64_t quantity
)
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                PlaceLimitSellCommand
            >
        >(
            PlaceLimitSellCommand{
                .price_ticks = price_ticks,
                .quantity = quantity,
                .result = {}
            }
        );

    std::future<OrderId> future =
        command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

std::future<ExecutionResult>
ConcurrentMatchingEngine::execute_market_buy(
    std::int64_t quantity
)
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                ExecuteMarketBuyCommand
            >
        >(
            ExecuteMarketBuyCommand{
                .quantity = quantity,
                .result = {}
            }
        );

    std::future<ExecutionResult> future =
        command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

std::future<ExecutionResult>
ConcurrentMatchingEngine::execute_market_sell(
    std::int64_t quantity
)
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                ExecuteMarketSellCommand
            >
        >(
            ExecuteMarketSellCommand{
                .quantity = quantity,
                .result = {}
            }
        );

    std::future<ExecutionResult> future =
        command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

std::future<bool>
ConcurrentMatchingEngine::cancel_order(
    OrderId order_id
)
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                CancelOrderCommand
            >
        >(
            CancelOrderCommand{
                .order_id = order_id,
                .result = {}
            }
        );

    std::future<bool> future =
        command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

std::future<bool>
ConcurrentMatchingEngine::modify_order(
    OrderId order_id,
    std::int64_t new_price_ticks,
    std::int64_t new_quantity
)
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                ModifyOrderCommand
            >
        >(
            ModifyOrderCommand{
                .order_id = order_id,
                .new_price_ticks =
                    new_price_ticks,
                .new_quantity =
                    new_quantity,
                .result = {}
            }
        );

    std::future<bool> future =
        command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

std::future<std::vector<LimitOrder>>
ConcurrentMatchingEngine::active_limit_orders()
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                ActiveOrdersCommand
            >
        >(
            ActiveOrdersCommand{
                .result = {}
            }
        );

    std::future<std::vector<LimitOrder>>
        future =
            command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

std::future<std::vector<Trade>>
ConcurrentMatchingEngine::trade_history()
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                TradeHistoryCommand
            >
        >(
            TradeHistoryCommand{
                .result = {}
            }
        );

    std::future<std::vector<Trade>>
        future =
            command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

std::future<void>
ConcurrentMatchingEngine::clear_trade_history()
{
    auto command =
        std::make_unique<
            EngineCommandHolder<
                ClearTradeHistoryCommand
            >
        >(
            ClearTradeHistoryCommand{
                .result = {}
            }
        );

    std::future<void> future =
        command->command().result.get_future();

    if (!command_queue_.push(
            std::move(command)
        )) {
        throw std::runtime_error(
            "ConcurrentMatchingEngine is shut down."
        );
    }

    return future;
}

void ConcurrentMatchingEngine::shutdown()
{
    command_queue_.close();

    if (worker_.joinable()) {
        worker_.join();
    }
}

bool ConcurrentMatchingEngine::is_shutdown() const
{
    return command_queue_.is_closed();
}

void ConcurrentMatchingEngine::worker_loop()
{
    while (auto command =
               command_queue_.pop()) {
        process_command(
            **command
        );
    }
}

void ConcurrentMatchingEngine::process_command(
    EngineCommand& command
)
{
    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    PlaceLimitBuyCommand
                >*
            >(&command)) {
        PlaceLimitBuyCommand& value =
            holder->command();

        execute_command(
            value.result,
            [this, &value] {
                return engine_.place_limit_buy(
                    value.price_ticks,
                    value.quantity
                );
            }
        );

        return;
    }

    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    PlaceLimitSellCommand
                >*
            >(&command)) {
        PlaceLimitSellCommand& value =
            holder->command();

        execute_command(
            value.result,
            [this, &value] {
                return engine_.place_limit_sell(
                    value.price_ticks,
                    value.quantity
                );
            }
        );

        return;
    }

    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    ExecuteMarketBuyCommand
                >*
            >(&command)) {
        ExecuteMarketBuyCommand& value =
            holder->command();

        execute_command(
            value.result,
            [this, &value] {
                return engine_.execute_market_buy(
                    value.quantity
                );
            }
        );

        return;
    }

    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    ExecuteMarketSellCommand
                >*
            >(&command)) {
        ExecuteMarketSellCommand& value =
            holder->command();

        execute_command(
            value.result,
            [this, &value] {
                return engine_.execute_market_sell(
                    value.quantity
                );
            }
        );

        return;
    }

    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    CancelOrderCommand
                >*
            >(&command)) {
        CancelOrderCommand& value =
            holder->command();

        execute_command(
            value.result,
            [this, &value] {
                return engine_.cancel_order(
                    value.order_id
                );
            }
        );

        return;
    }

    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    ModifyOrderCommand
                >*
            >(&command)) {
        ModifyOrderCommand& value =
            holder->command();

        execute_command(
            value.result,
            [this, &value] {
                return engine_.modify_order(
                    value.order_id,
                    value.new_price_ticks,
                    value.new_quantity
                );
            }
        );

        return;
    }

    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    ActiveOrdersCommand
                >*
            >(&command)) {
        ActiveOrdersCommand& value =
            holder->command();

        execute_command(
            value.result,
            [this] {
                const auto& orders =
                    engine_.active_limit_orders();

                return std::vector<LimitOrder>(
                    orders.begin(),
                    orders.end()
                );
            }
        );

        return;
    }

    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    TradeHistoryCommand
                >*
            >(&command)) {
        TradeHistoryCommand& value =
            holder->command();

        execute_command(
            value.result,
            [this] {
                const auto& trades =
                    engine_.trade_history();

                return std::vector<Trade>(
                    trades.begin(),
                    trades.end()
                );
            }
        );

        return;
    }

    if (auto* holder =
            dynamic_cast<
                EngineCommandHolder<
                    ClearTradeHistoryCommand
                >*
            >(&command)) {
        ClearTradeHistoryCommand& value =
            holder->command();

        execute_void_command(
            value.result,
            [this] {
                engine_.clear_trade_history();
            }
        );

        return;
    }

    throw std::logic_error(
        "Unknown engine command."
    );
}
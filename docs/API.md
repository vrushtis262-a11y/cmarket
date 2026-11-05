
# cmarket — C++ API Documentation

## 1. Overview

cmarket is a C++20 exchange-style order matching system.

This document describes its core C++ interfaces for
submitting orders, managing resting orders, querying
order-book state, and recording executed trades.

The API documentation is based on the public declarations
in the following headers:

- `include/matching_engine.hpp`
- `include/order_manager.hpp`
- `include/order_book.hpp`
- `include/trade_store.hpp`
- `include/limit_order.hpp`
- `include/execution.hpp`
- `include/validation.hpp`

This is a C++ library interface, not an HTTP REST API.

## 2. Core Data Types

### OrderSide

Defined in `execution.hpp`.

```cpp
enum class OrderSide {
    Buy,
    Sell
};
```

Identifies the direction of an order.

### Identifiers

```cpp
using OrderId = std::uint64_t;
using SequenceNumber = std::uint64_t;
using TradeId = std::uint64_t;
using TradeSequenceNumber = std::uint64_t;
```

Order IDs identify orders, while sequence numbers
support ordering and execution history.

### LimitOrder

Defined in `limit_order.hpp`.

```cpp
struct LimitOrder {
    OrderId order_id;
    OrderSide side;
    std::int64_t price_ticks;
    std::int64_t original_quantity;
    std::int64_t remaining_quantity;
    SequenceNumber sequence_number;

    std::int64_t filled_quantity() const noexcept;
    bool is_filled() const noexcept;
    bool is_partially_filled() const noexcept;
};
```

The helper methods report filled quantity, full-fill
status, and partial-fill status.

### Trade

Defined in `execution.hpp`.

```cpp
struct Trade {
    OrderSide aggressor_side;
    std::int64_t price_ticks;
    std::int64_t quantity;
    TradeId trade_id = 0;

    std::optional<std::uint64_t> buy_order_id =
        std::nullopt;

    std::optional<std::uint64_t> sell_order_id =
        std::nullopt;

    TradeSequenceNumber execution_sequence = 0;
};
```

Each trade records the execution side, price,
quantity, trade ID, optional order IDs, and
execution sequence.

### ExecutionResult

Defined in `execution.hpp`.

```cpp
struct ExecutionResult {
    OrderSide side;
    std::int64_t requested_quantity;
    std::int64_t executed_quantity;
    std::int64_t remaining_quantity;
    std::optional<std::int64_t> average_price_ticks;
    std::vector<Trade> trades;

    bool fully_filled() const noexcept;
    bool partially_filled() const noexcept;
    bool unfilled() const noexcept;
};
```

Helper method behavior:

- `fully_filled()` returns true when remaining
  quantity equals zero.
- `partially_filled()` returns true when some
  quantity executed and some remains.
- `unfilled()` returns true when executed
  quantity equals zero.

## 3. MatchingEngine API

Header: `matching_engine.hpp`

MatchingEngine coordinates limit-order placement,
market-order execution, cancellation, modification,
and trade-history access.

### Constructor

```cpp
explicit MatchingEngine(OrderBook& order_book) noexcept;
```

Creates a matching engine associated with an OrderBook.

The referenced OrderBook must remain alive while
the engine uses it.

### Place Limit Buy

```cpp
OrderId place_limit_buy(
    std::int64_t price_ticks,
    std::int64_t quantity
);
```

Parameters:

- `price_ticks`: Positive fixed-point limit price.
- `quantity`: Positive order quantity.

Returns an `OrderId`.

### Place Limit Sell

```cpp
OrderId place_limit_sell(
    std::int64_t price_ticks,
    std::int64_t quantity
);
```

Uses the same parameter conventions as
`place_limit_buy`.

Returns an `OrderId`.

### Execute Market Buy

```cpp
ExecutionResult execute_market_buy(
    std::int64_t quantity
);
```

Attempts to buy against available resting sell orders.

Returns an ExecutionResult containing executed
quantity, remaining quantity, average execution
price when available, and resulting trades.

### Execute Market Sell

```cpp
ExecutionResult execute_market_sell(
    std::int64_t quantity
);
```

Attempts to sell against available resting buy orders.

Returns an ExecutionResult.

### Cancel Order

```cpp
bool cancel_order(OrderId order_id);
```

Requests cancellation of an identified order.

Returns a boolean cancellation result.

### Modify Order

```cpp
bool modify_order(
    OrderId order_id,
    std::int64_t new_price_ticks,
    std::int64_t new_quantity
);
```

Requests modification of an order's price and quantity.

Returns a boolean modification result.

The detailed priority-reset and crossing-order
behavior should be checked against the implementation.

### Active Orders

```cpp
const std::vector<LimitOrder>&
active_limit_orders() const noexcept;
```

Returns a const reference to the engine's active
limit-order collection.

The caller must not use this reference after the
owning engine is destroyed.

### Trade History

```cpp
const std::vector<Trade>&
trade_history() const noexcept;
```

Returns a const reference to the recorded trades.

### Clear Trade History

```cpp
void clear_trade_history() noexcept;
```

Clears the engine's recorded trade history.

## 4. OrderManager API

Header: `order_manager.hpp`

OrderManager stores active limit orders and maintains
indexes supporting Order ID lookup and price-time
priority.

### Public Methods

```cpp
void add_order(const LimitOrder& order);

bool cancel_order(OrderId order_id);

bool cancel_order(
    OrderId order_id,
    Removal& removal
);

void restore_removal(Removal& removal);

bool update_sequence_number(
    OrderId order_id,
    SequenceNumber new_sequence_number
);

LimitOrder* find_order(OrderId order_id) noexcept;

const LimitOrder*
find_order(OrderId order_id) const noexcept;

const LimitOrder* best_buy_order() const noexcept;

const LimitOrder* best_sell_order() const noexcept;

const std::vector<LimitOrder>&
orders() const noexcept;
```

Responsibilities:

- Store and retrieve active limit orders.
- Locate orders using Order IDs.
- Maintain buy and sell priority indexes.
- Support cancellation and removal restoration.
- Provide access to the highest-priority orders.

The `Removal` structure stores information needed
to restore a removed order.

The internal data structures include a vector,
an unordered map, and ordered sets.

Buy orders prioritize higher prices first.
Sell orders prioritize lower prices first.

At equal prices, smaller sequence numbers
have higher priority.

## 5. OrderBook API

Header: `order_book.hpp`

OrderBook maintains aggregated bid and ask price levels.

### PriceLevel

```cpp
struct PriceLevel {
    std::int64_t price_ticks;
    std::int64_t quantity;
};
```

### Price Representation

```cpp
static constexpr std::int64_t ticks_per_unit =
    1'000'000;
```

Prices use integer tick representation rather than
floating-point values.

### Snapshot Operations

```cpp
void replace_snapshot(
    std::vector<PriceLevel> bids,
    std::vector<PriceLevel> asks
);

void replace_normalized_snapshot(
    std::vector<PriceLevel> bids,
    std::vector<PriceLevel> asks
);
```

### Price-Level Updates

```cpp
void update_bid(
    std::int64_t price_ticks,
    std::int64_t quantity
);

void update_ask(
    std::int64_t price_ticks,
    std::int64_t quantity
);

void adjust_bid(
    std::int64_t price_ticks,
    std::int64_t quantity_delta
);

void adjust_ask(
    std::int64_t price_ticks,
    std::int64_t quantity_delta
);
```

### Market Data Queries

```cpp
std::vector<PriceLevel> bids() const;
std::vector<PriceLevel> asks() const;

std::optional<PriceLevel> best_bid() const noexcept;
std::optional<PriceLevel> best_ask() const noexcept;

std::optional<std::int64_t>
spread_ticks() const noexcept;

std::optional<std::int64_t>
mid_price_ticks() const noexcept;

std::int64_t bid_depth() const noexcept;
std::int64_t ask_depth() const noexcept;
std::int64_t total_depth() const noexcept;

std::optional<double>
order_book_imbalance() const noexcept;

std::optional<std::int64_t>
bid_vwap_ticks() const noexcept;

std::optional<std::int64_t>
ask_vwap_ticks() const noexcept;

std::optional<std::int64_t>
microprice_ticks() const noexcept;

bool empty() const noexcept;
```

Optional results should be checked before accessing
their contained values.

### Conversion and Formatting

```cpp
static std::int64_t price_to_ticks(
    const std::string& price
);

static std::int64_t quantity_to_fixed(
    const std::string& quantity
);

static std::string format_price(
    std::int64_t price_ticks,
    std::size_t decimal_places = 6
);
```

The OrderBook declares an internal
`std::shared_mutex` for synchronization.

This does not establish thread safety for all
operations of the surrounding MatchingEngine.

## 6. TradeStore API

Header: `trade_store.hpp`

TradeStore maintains an in-memory collection
of executed trades.

### Record Trade

```cpp
const Trade& record_trade(
    OrderSide aggressor_side,
    std::int64_t price_ticks,
    std::int64_t quantity,
    std::optional<std::uint64_t> buy_order_id =
        std::nullopt,
    std::optional<std::uint64_t> sell_order_id =
        std::nullopt
);
```

Validates the trade and appends it to the trade history.

Returns a const reference to the stored trade.

The returned reference can be invalidated by later
changes to the underlying vector.

### Checkpoint

```cpp
Checkpoint checkpoint() const noexcept;
```

Returns the current trade count and next trade
and execution sequence identifiers.

### Rollback

```cpp
void rollback(const Checkpoint& checkpoint);
```

Restores the trade count and identifier state
recorded in a checkpoint.

A checkpoint whose trade count exceeds the
current trade count causes `std::invalid_argument`.

### Read and Clear Trades

```cpp
const std::vector<Trade>& trades() const noexcept;

void clear() noexcept;
```

`trades()` returns a const reference to the trade
history.

`clear()` removes recorded trades. In the supplied
implementation, it does not reset the next trade ID
or execution sequence number.

## 7. Validation

Header: `validation.hpp`

The following functions require positive values:

```cpp
validation::validate_market_order(quantity);

validation::validate_limit_order(
    price_ticks,
    quantity
);

validation::validate_modify_order(
    price_ticks,
    quantity
);

validation::validate_trade(
    price_ticks,
    quantity
);
```

All four validation functions throw
`std::invalid_argument` for nonpositive
required values.

The helper `validation::require_positive`
performs the underlying positive-value check.

Additional exception and rollback behavior of
MatchingEngine operations should be verified
against their `.cpp` implementations.

## 8. Usage Examples

### Create an Engine and Place Limit Orders

```cpp
#include "matching_engine.hpp"
#include "order_book.hpp"

int main() {
    OrderBook book;
    MatchingEngine engine(book);

    const auto buy_id = engine.place_limit_buy(
        OrderBook::price_to_ticks("100.00"),
        10
    );

    const auto sell_id = engine.place_limit_sell(
        OrderBook::price_to_ticks("101.00"),
        5
    );

    (void)buy_id;
    (void)sell_id;
}
```

### Execute a Market Order

```cpp
OrderBook book;
MatchingEngine engine(book);

const auto sell_id = engine.place_limit_sell(
    OrderBook::price_to_ticks("100.00"),
    10
);

const ExecutionResult result =
    engine.execute_market_buy(6);

if (result.fully_filled()) {
    // Requested quantity was fully executed.
}

(void)sell_id;
```

### Cancel or Modify an Order

```cpp
OrderBook book;
MatchingEngine engine(book);

const OrderId order_id = engine.place_limit_buy(
    OrderBook::price_to_ticks("99.00"),
    20
);

const bool modified = engine.modify_order(
    order_id,
    OrderBook::price_to_ticks("98.00"),
    15
);

const bool cancelled = engine.cancel_order(order_id);

(void)modified;
(void)cancelled;
```

### Read Best Bid and Ask

```cpp
OrderBook book;

const auto bid = book.best_bid();
const auto ask = book.best_ask();

if (bid.has_value()) {
    const auto price = bid->price_ticks;
    (void)price;
}

if (ask.has_value()) {
    const auto quantity = ask->quantity;
    (void)quantity;
}
```

The usage examples demonstrate the public
interface syntax. They are not standalone
build-system instructions.

## 9. Design Notes

- Limit and market orders use different result types.
- `OrderId` is the identifier for a limit order.
- Market executions provide detailed ExecutionResult data.
- Trade history is maintained in memory.
- The order manager maintains price-time priority indexes.
- Fixed-point prices avoid floating-point representation
  in core order and trade structures.
- OrderBook provides aggregated market information.
- Internal mutation and rollback details are separate
  from the public MatchingEngine API.

## 10. Related Documentation

- [Architecture Diagrams](ARCHITECTURE_DIAGRAMS.md)
- [Sequence Diagrams](SEQUENCE_DIAGRAMS.md)
- [Detailed Architecture](architecture.md)

## 11. Scope

This document covers the core matching, order management,
order-book, and trade-store interfaces.

Additional modules, including concurrent execution,
analytics, market impact, risk management, and WebSocket
integration, have separate public headers and can be
documented in a future API reference extension.

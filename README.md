# CMarket

CMarket is a modern C++20 exchange-style matching engine implementing market orders, limit orders, automatic matching, price-time priority, multi-level execution, partial fills, and persistent trade history.

---

## Features

### Order Book

- Bid and ask price levels
- Best bid / best ask
- Spread calculation
- Mid-price calculation
- VWAP
- Microprice
- Order book imbalance
- Market depth
- Incremental price-level updates

### Market Orders

- Market Buy
- Market Sell
- Multi-level execution
- Partial fills when liquidity is insufficient
- Average execution price
- Persistent trade history

### Limit Orders

- Limit Buy
- Limit Sell
- Unique Order IDs
- FIFO sequence numbers
- Automatic crossing and matching
- Partial fills
- Resting unmatched quantities
- Cancel orders
- Modify orders

### Matching Engine

- Price-time priority
- Best-price matching
- FIFO priority at the same price
- Crossing-order detection
- Multi-level automatic matching
- Partial and full fills
- Resting-order lifecycle management
- Automatic order-book synchronization
- Persistent trade recording

---

## Matching Behavior

CMarket uses price-time priority when matching limit orders.

Price has priority first. For an incoming buy order, the lowest eligible ask is matched first. For an incoming sell order, the highest eligible bid is matched first.

When multiple resting orders exist at the same price, the order with the earlier sequence number receives priority.

An incoming order continues matching until one of the following occurs:

1. The incoming order is completely filled.
2. No crossing resting orders remain.

If part of a limit order remains after matching, the remaining quantity becomes a resting order.

---

## Multi-Level Matching Example

Suppose the ask side contains:

```text
Price       Quantity
500000      20
505000      30
510000      50
```

An incoming limit buy is submitted for:

```text
Price:      510000
Quantity:   80
```

The order sweeps multiple price levels:

```text
Trade 1: 20 @ 500000
Trade 2: 30 @ 505000
Trade 3: 30 @ 510000
```

The incoming order is completely filled after executing 80 units.

The remaining ask book contains:

```text
Price       Quantity
510000      20
```

Each individual execution is persisted as its own trade record.

---

## Architecture

CMarket separates matching orchestration, active-order lifecycle, price-level state, and trade persistence.

```text
                    MatchingEngine
                         |
          +--------------+--------------+
          |              |              |
          v              v              v
     OrderBook      OrderManager     TradeStore
          |              |              |
          v              v              v
   bids / asks      active orders   executed trades
```

### MatchingEngine

`MatchingEngine` orchestrates order execution and matching.

Its responsibilities include:

- Receiving market and limit orders
- Validating order parameters
- Assigning Order IDs and FIFO sequence numbers
- Finding eligible matches
- Coordinating partial and full fills
- Coordinating cancellation and modification
- Synchronizing active orders with the `OrderBook`
- Recording executions through `TradeStore`

### OrderBook

`OrderBook` owns aggregated price-level state.

Its responsibilities include:

- Bid levels
- Ask levels
- Best bid and best ask
- Spread
- Mid-price
- VWAP
- Microprice
- Order book imbalance
- Price-level updates

### OrderManager

`OrderManager` owns active limit orders and their lifecycle.

Its responsibilities include:

- Adding active orders
- Looking up orders by `OrderId`
- Removing cancelled or fully filled orders
- Providing read-only access to the active-order collection

### TradeStore

`TradeStore` owns persistent executed-trade history.

Its responsibilities include:

- Recording trades
- Assigning unique Trade IDs
- Assigning monotonically increasing execution sequence numbers
- Preserving buy and sell Order IDs when available
- Returning trade history
- Clearing stored history without reusing IDs or sequence numbers

This separation keeps matching orchestration independent from active-order ownership and trade persistence.

---

## Trade History

Every execution is represented by a `Trade`.

The trade data structure contains:

```text
aggressor_side
price_ticks
quantity
trade_id
buy_order_id
sell_order_id
execution_sequence
```

`trade_id` uniquely identifies an execution.

`execution_sequence` preserves execution ordering.

`buy_order_id` and `sell_order_id` identify the participating limit orders when those IDs are available. Market executions may have an absent Order ID for the market-order side.

### Trade History API

Trade history is exposed through `MatchingEngine`:

```cpp
const std::vector<Trade>&
trade_history() const noexcept;
```

History can be cleared with:

```cpp
void clear_trade_history() noexcept;
```

Internally, `MatchingEngine` delegates trade persistence to `TradeStore`.

`TradeStore` exposes:

```cpp
const std::vector<Trade>&
trades() const noexcept;
```

and:

```cpp
void clear() noexcept;
```

Clearing trade history removes stored trade records but does not reset Trade IDs or execution sequence numbers. Newly recorded trades therefore continue with monotonically increasing identifiers.

---

## Tech Stack

- C++20
- CMake
- GoogleTest
- Apple Clang / Clang
- Git
- GitHub

---

## Project Structure

```text
include/
    execution.hpp
    http_client.hpp
    limit_order.hpp
    matching_engine.hpp
    order_book.hpp
    order_id_generator.hpp
    order_manager.hpp
    trade_store.hpp
    websocket_client.hpp

src/
    http_client.cpp
    main.cpp
    matching_engine.cpp
    order_book.cpp
    order_manager.cpp
    websocket_client.cpp

tests/
    execution_tests.cpp
    limit_order_tests.cpp
    matching_edge_cases_tests.cpp
    matching_engine_tests.cpp
    order_book_tests.cpp
    order_id_generator_tests.cpp
    order_manager_tests.cpp
    trade_persistence_tests.cpp
    trade_store_tests.cpp

CMakeLists.txt
README.md
```

---

## Build

Configure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run:

```bash
./build/cmarket
```

---

## Run Tests

Run the complete regression suite:

```bash
ctest --test-dir build --output-on-failure
```

---

## Compiler Warnings

CMarket enables:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
```

for project targets.

---

## Sanitizer Build

AddressSanitizer and UndefinedBehaviorSanitizer can be enabled through CMake:

```bash
cmake -S . -B build-sanitized \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMARKET_ENABLE_SANITIZERS=ON

cmake --build build-sanitized

ctest --test-dir build-sanitized --output-on-failure
```

The sanitizer configuration enables:

```text
-fsanitize=address,undefined
-fno-omit-frame-pointer
```

for supported Clang and GCC builds.

---

## Test Coverage

The automated test suite covers:

- Order-book behavior
- Market orders
- Multi-level market execution
- Limit orders
- Automatic crossing
- Multi-level limit matching
- Partial fills
- Full fills
- Insufficient liquidity
- FIFO priority
- Order IDs
- Cancel orders
- Modify orders
- Matching edge cases
- Persistent trade history
- Trade IDs
- Execution sequence numbers
- Buy and sell Order IDs
- `OrderManager`
- `TradeStore`
- Trade-history clearing behavior

---

## Current Status

The current matching-engine milestone includes:

- Multi-level automatic matching
- Robust partial-fill behavior
- Price-time priority
- Persistent trade history
- Modular matching architecture
- Dedicated active-order management
- Dedicated trade persistence
- Full automated regression suite
- AddressSanitizer validation
- UndefinedBehaviorSanitizer validation
- Strict compiler-warning configuration
- Modern C++20
- CMake build system

---

## Future Improvements

- Iceberg orders
- Stop orders
- Fill-or-Kill orders
- Immediate-or-Cancel orders
- Performance benchmarking
- Multithreaded matching
- Persistent on-disk order and trade storage

---

## License

MIT License
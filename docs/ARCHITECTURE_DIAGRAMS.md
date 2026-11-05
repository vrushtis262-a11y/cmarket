
# cmarket — Architecture Diagrams

## 1. High-Level System Architecture

cmarket is a C++20 exchange-style matching engine that supports
limit orders, market orders, price-time priority matching, partial
fills, cancellation, modification, and persistent in-memory
trade history.

The system separates order execution from active-order storage,
aggregated order-book data, and trade recording.

```mermaid
flowchart TD
    A["Incoming Order / Operation"]

    B["MatchingEngine"]

    C["OrderManager"]
    D["OrderBook"]
    E["TradeStore"]
    F["OrderIdGenerator"]

    G["Active Limit Orders"]
    H["Order ID Lookup Index"]
    I["Buy / Sell Priority Indexes"]

    J["Aggregated Bid Levels"]
    K["Aggregated Ask Levels"]

    L["Executed Trade History"]
    M["Trade Checkpoints"]

    A --> B

    B --> C
    B --> D
    B --> E
    B --> F

    C --> G
    C --> H
    C --> I

    D --> J
    D --> K

    E --> L
    E --> M
```

## 2. Core Components

### MatchingEngine

Coordinates order placement, matching, market-order execution,
cancellation, modification, and rollback.

It enforces price-time priority and synchronizes changes across
the system's stateful components.

### OrderManager

Maintains individual active limit orders.

It uses:
- A vector for authoritative active-order storage
- An unordered index for expected O(1) Order ID lookup
- Ordered buy and sell indexes for price-time priority

### OrderBook

Maintains aggregated bid and ask price levels.

It provides market information such as best bid, best ask,
spread, market depth, VWAP, and other book statistics.

Internal access is synchronized using std::shared_mutex.

### TradeStore

Maintains executed trades in memory, including trade identifiers
and execution ordering.

It also provides checkpoints for rollback-sensitive operations.

### OrderIdGenerator

Generates order identifiers used to distinguish orders.

## 3. Important Architectural Properties

- Matching follows price-time priority.
- FIFO priority is based on sequence numbers.
- Active orders are stored separately from aggregate price levels.
- Trades are recorded in an in-memory history.
- Rollback-sensitive operations use checkpoints and mutation journals.
- OrderBook synchronization does not imply that the entire
  MatchingEngine supports concurrent writers.

## 4. Additional Documentation

For detailed implementation decisions, invariants, complexity
analysis, rollback behavior, and concurrency limitations, see
[Architecture Documentation](architecture.md).

## 5. Component Interaction Diagram

This diagram illustrates the logical interactions between
the matching engine, order manager, order book, and trade store.

```mermaid
flowchart TD
    Client["Caller / Trading Application"]
    Engine["MatchingEngine"]
    Manager["OrderManager"]
    Book["OrderBook"]
    Store["TradeStore"]
    Generator["OrderIdGenerator"]

    Client -->|"Submit limit or market order"| Engine
    Client -->|"Cancel or modify order"| Engine

    Engine -->|"Generate order identifier"| Generator
    Engine -->|"Find resting orders"| Manager
    Engine -->|"Match eligible orders"| Manager
    Engine -->|"Update aggregated price levels"| Book
    Engine -->|"Record executed trades"| Store

    Manager -->|"Return eligible resting orders"| Engine
    Book -->|"Provide market depth and quotes"| Client
    Store -->|"Provide execution history"| Client
```

### Component Responsibilities

| Component | Responsibility |
|---|---|
| MatchingEngine | Coordinates order processing, matching, and execution |
| OrderManager | Stores and retrieves active orders |
| OrderBook | Maintains aggregated bid and ask levels |
| TradeStore | Records executed trades |
| OrderIdGenerator | Generates unique order identifiers |

### Interaction Summary

1. A caller submits an order or requests an operation.
2. MatchingEngine validates and processes the request.
3. OrderManager provides access to active resting orders.
4. MatchingEngine determines and executes eligible matches.
5. OrderBook is updated to reflect changes in market depth.
6. TradeStore records completed executions.
7. The operation returns its result to the caller.

The diagram represents logical interactions rather than
a strict sequence of function calls.

## 6. Limit Order Processing Flow

This diagram illustrates the logical flow of a limit
order through the cmarket matching engine.

```mermaid
flowchart TD
    A["Receive Limit Order"]
    B["Validate Order Parameters"]
    C{"Order Valid?"}
    D["Reject Invalid Order"]
    E["Assign Order ID"]
    F["Check Opposite Side of Order Book"]
    G{"Crossing Order Available?"}
    H["Execute Match"]
    I["Record Trade in TradeStore"]
    J["Update OrderManager and OrderBook"]
    K{"Remaining Quantity?"}
    L["Insert Remaining Quantity as Resting Order"]
    M["Return Execution Result"]

    A --> B
    B --> C

    C -->|No| D
    D --> M

    C -->|Yes| E
    E --> F
    F --> G

    G -->|Yes| H
    H --> I
    I --> J
    J --> K

    K -->|Yes| F
    K -->|No| M

    G -->|No| L
    L --> M
```

### Order Processing Explanation

1. **Receive Order:** The matching engine receives an incoming limit order.
2. **Validation:** The engine validates price, quantity, and other required parameters.
3. **Order Identification:** A valid order receives an order identifier.
4. **Matching:** The engine searches for eligible orders on the opposite side using price-time priority.
5. **Trade Execution:** Compatible orders execute, potentially producing partial fills.
6. **State Updates:** Executed trades are recorded and active order quantities and aggregated price levels are updated.
7. **Remaining Quantity:** Any unfilled quantity is added as a resting limit order when matching is no longer possible.
8. **Result:** The engine returns the result of the operation.

### Price-Time Priority

cmarket prioritizes orders according to two rules:

- **Price Priority:** Better prices execute first.
- **Time Priority:** At the same price, earlier orders execute first.

This supports deterministic FIFO matching among orders at the same price level.

### Partial Fills

If an incoming order is larger than the available quantity
of a matching resting order, that resting order is fully
executed and the engine continues matching the remaining
incoming quantity against the next eligible order.

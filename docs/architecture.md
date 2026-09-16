# CMarket Architecture

## Overview

CMarket is a C++20 matching engine built around explicit separation of matching, active-order management, aggregated order-book state, trade history, and identifier generation.

The architecture has three primary goals:

1. Preserve price-time matching correctness.
2. Provide rollback-safe state transitions.
3. Avoid full active-order scans and unnecessary full-state copies on measured hot paths.

```text
                         +----------------------+
                         |    MatchingEngine    |
                         +----------+-----------+
                                    |
             +----------------------+----------------------+
             |                      |                      |
             v                      v                      v
    +------------------+   +------------------+   +------------------+
    |   OrderManager   |   |    OrderBook     |   |    TradeStore    |
    +--------+---------+   +--------+---------+   +--------+---------+
             |                      |                      |
             v                      v                      v
    +------------------+   +------------------+   +------------------+
    | Active Orders    |   | Aggregated       |   | Trade History    |
    | + ID Index       |   | Price Levels     |   | + Checkpoints    |
    | + Buy Priority   |   |                  |   |                  |
    | + Sell Priority  |   |                  |   |                  |
    +------------------+   +------------------+   +------------------+

                         +----------------------+
                         |  OrderIdGenerator    |
                         +----------------------+
```

`MatchingEngine` coordinates state transitions across these components. It owns matching policy and keeps active orders, aggregate book levels, trade history, identifiers, and FIFO sequence state consistent.

## MatchingEngine

`MatchingEngine` is the orchestration layer.

Its responsibilities include:

- Limit-order placement
- Automatic crossing of marketable limit orders
- Market-order execution
- Cancellation
- Modification
- Price-time priority enforcement
- Partial-fill handling
- Multi-level matching
- Trade creation
- Sequence-number management
- Rollback after failed operations
- Synchronization between active orders and the aggregated order book

Execution priority does not depend on physical vector position.

Priority is determined by:

```text
price
then sequence number
then OrderId as a deterministic tie-breaker
```

This allows active-order storage to use swap-with-back cancellation without violating FIFO matching semantics.

## OrderManager

`OrderManager` owns active limit orders.

Its authoritative order storage is a `std::vector<LimitOrder>`.

The vector remains the primary active-order container while secondary indexes provide efficient lookup and matching priority.

### OrderId Index

An unordered index maps:

```text
OrderId -> vector position
```

This provides expected constant-time lookup for operations such as cancellation, modification, and direct active-order lookup.

When swap-with-back cancellation moves an order in the vector, the moved order's index entry is updated to its new position.

### Price-Time Priority Indexes

`OrderManager` maintains separate ordered priority indexes for buys and sells.

Each priority entry represents:

```text
price_ticks
sequence_number
order_id
```

Buy priority is ordered by:

1. Higher price first.
2. Lower sequence number first.
3. Lower OrderId first.

Sell priority is ordered by:

1. Lower price first.
2. Lower sequence number first.
3. Lower OrderId first.

The beginning of each index therefore identifies the highest-priority resting order for that side.

The active-order vector remains authoritative. The priority indexes are secondary structures used for efficient match discovery.

## FIFO Semantics

FIFO priority is represented by sequence number.

For orders at the same price:

```text
lower sequence number = earlier priority
```

Vector position is not execution priority.

Cancelling an order can move another order through swap-with-back without changing that moved order's FIFO sequence.

## Modification Priority

Modification follows explicit price-time rules.

A same-price quantity decrease preserves the existing sequence number and therefore preserves FIFO priority.

A same-price quantity increase receives a new sequence number and loses its previous FIFO position.

A price-changing modification receives new priority at the new price. If the new price crosses resting liquidity, matching occurs immediately. Any remaining quantity can become a resting order.

## OrderBook

`OrderBook` stores aggregated visible liquidity rather than individual resting orders.

Each price level contains a price and aggregate quantity.

Bid levels are maintained from highest price to lowest price.

Ask levels are maintained from lowest price to highest price.

`OrderManager` retains individual resting orders for FIFO execution, while `OrderBook` provides the aggregate market view.

This separation allows CMarket to maintain both order-level matching semantics and price-level book statistics.

## OrderBook Synchronization

`OrderBook` protects its internal state with `std::shared_mutex`.

Read-only operations use shared locking.

Mutating operations use exclusive locking.

Conceptually:

```text
Readers
   |
   +------ shared lock ------+
   |                         |
   v                         v
best bid                  snapshot
spread                    depth
mid                       other reads

Writer
   |
   +------ unique lock ------> level mutation
```

This synchronization protects `OrderBook` itself.

It does not make the entire `MatchingEngine` a fully concurrent multi-writer matching engine. Matching operations coordinate several related structures and therefore have a larger transaction boundary than the aggregated order book alone.

## OrderBook Copy Semantics

Some rollback paths require an `OrderBook` snapshot.

Because `std::shared_mutex` is not copyable, `OrderBook` implements explicit copy construction and copy assignment.

Copy construction acquires shared access to the source state.

Copy assignment locks the relevant source and destination state before copying.

The mutex itself is not copied.

This preserves snapshot behavior required by rollback paths while keeping synchronization internal to each `OrderBook` instance.

## TradeStore

`TradeStore` owns persistent in-memory execution history.

Trade records preserve execution information including the participating orders, execution price, quantity, identifiers, and execution ordering.

Normal matching appends trades to the store.

## TradeStore Checkpoints

Rollback-capable operations use lightweight `TradeStore` checkpoints instead of copying the complete trade history.

A checkpoint captures the state needed to restore trade storage and associated counters.

Conceptually:

```text
Before operation:

trades: [T1, T2, T3]
checkpoint = size 3

During operation:

trades: [T1, T2, T3, T4, T5]

Rollback:

restore checkpoint

trades: [T1, T2, T3]
```

This prevents rollback cost from growing unnecessarily with the complete accumulated trade history.

## Transactional Order Removal

Rollback-sensitive matching paths may temporarily remove active orders.

`OrderManager` supports transactional removal so the engine can restore an order and its associated index state if a later operation fails.

The transaction preserves the information required to reconstruct:

- The removed order
- Its active-order storage state
- The OrderId index
- The relevant price-time priority index

This avoids rebuilding the complete `OrderManager` during rollback.

## Mutation Journals

Earlier rollback implementations relied more heavily on complete container snapshots.

Although straightforward, full snapshots made hot-path cost grow with the total number of active orders.

Optimized paths instead use targeted mutation journals.

A journal records only state changed by the current operation.

Conceptually:

```text
Operation

mutation 1
mutation 2
mutation 3
    |
    X failure
    |
    v

Rollback

undo mutation 3
undo mutation 2
undo mutation 1
```

Rollback processes recorded mutations in reverse order.

Mutation journals are used in performance-sensitive operations including active-order market execution, crossing limit-order placement, and price-changing modification.

## Market-Order Execution

For an active-order-backed market order, the engine repeatedly obtains the highest-priority resting order from the appropriate priority index.

Conceptually:

```text
Market order
    |
    v
Select best resting order
through priority index
    |
    v
Determine executable quantity
    |
    v
Record trade
    |
    v
Reduce aggregated OrderBook level
    |
    v
Reduce resting order quantity
    |
    +-----------------------------+
    |                             |
    v                             v
partial fill                  full fill
    |                             |
    |                             v
    |                    remove resting order
    |                             |
    +-------------+---------------+
                  |
                  v
          continue if needed
```

Matching continues until the incoming quantity is satisfied or no eligible liquidity remains.

Best-match discovery does not require scanning the complete active-order vector.

## Crossing Limit Orders

A new limit order first determines whether it crosses resting liquidity.

A buy crosses eligible asks when its limit price is high enough.

A sell crosses eligible bids when its limit price is low enough.

```text
Incoming limit order
        |
        v
Find best opposite order
        |
        v
Does price cross?
     /       \
   no         yes
   |           |
   v           v
rest order   execute trade
               |
               v
         more quantity?
           /       \
         no         yes
         |           |
         v           +----> next best match
       done
```

If quantity remains after all eligible crossing liquidity is consumed, the remainder becomes a resting active order and contributes to the aggregated order book.

## Same-Price Modification Fast Path

Same-price modification avoids the general price-changing transaction.

For a quantity decrease:

```text
preserve sequence
adjust aggregate quantity
update remaining quantity
```

For a quantity increase:

```text
adjust aggregate quantity
assign new sequence
update priority index
update remaining quantity
```

This keeps the common same-price modification path small while preserving FIFO policy.

## Price-Changing Modification

Price-changing modification uses a transactional path rather than a complete `OrderManager` snapshot.

At a high level:

```text
Capture sequence state
        |
        v
Create TradeStore checkpoint
        |
        v
Transactionally remove original order
        |
        v
Remove original book quantity
        |
        v
Assign new sequence
        |
        v
Match at new price
        |
        v
Rest remaining quantity if needed
```

If a later step fails, the engine reverses the completed mutations and restores the original operation state.

This removes the previous need to copy the entire active-order manager for each price-changing modification.

## State Ownership

The primary ownership model is:

```text
MatchingEngine
|
+-- OrderManager
|   |
|   +-- authoritative active-order vector
|   +-- OrderId -> vector-position index
|   +-- buy priority index
|   +-- sell priority index
|
+-- OrderBook
|   |
|   +-- aggregated bid levels
|   +-- aggregated ask levels
|   +-- shared/exclusive synchronization
|
+-- TradeStore
|   |
|   +-- trade history
|   +-- checkpoint state
|
+-- OrderIdGenerator
|
+-- matching sequence state
```

These structures represent related views of engine state and must remain consistent after successful operations and after rollback.

## Core Invariants

### Active Orders

Active orders must have valid identifiers, positive prices, positive quantities, and valid remaining quantities.

Active `OrderId` values must be unique.

### OrderId Index

For every active order, its `OrderId` index entry must identify the vector position containing that order.

There must not be an active index entry referring to a nonexistent order.

### Priority Indexes

Every active buy order must have a corresponding buy-priority entry.

Every active sell order must have a corresponding sell-priority entry.

Priority entries must reflect the order's current price, sequence number, and `OrderId`.

### Aggregated Order Book

Visible price levels must have positive prices and quantities.

Bid levels remain descending.

Ask levels remain ascending.

Aggregated quantities must remain consistent with valid resting order state.

### Matching

Matching must respect price-time priority.

FIFO ordering for equal prices is determined by sequence number rather than vector position.

### Trades

Recorded trades must contain valid execution data and preserve execution ordering.

### Failed Operations

Rejected or failed operations must not corrupt:

```text
active orders
OrderId index
priority indexes
aggregated order book
trade history
OrderId state
FIFO sequence state
trade identifier state
execution sequence state
```

## Complexity Characteristics

The indexed architecture changes the dominant cost of several operations.

### Order Lookup

Lookup by `OrderId` uses the unordered index:

```text
expected O(1)
```

### Best Resting Order

The highest-priority buy or sell is available at the beginning of its corresponding ordered priority index.

Accessing the beginning is constant time after index maintenance.

Priority-index insertion and removal are:

```text
O(log n)
```

### Cancellation

Cancellation consists primarily of expected constant-time `OrderId` lookup and vector swap-with-back work plus logarithmic priority-index maintenance.

### Matching

Best-match discovery no longer requires an `O(n)` scan of all active orders.

Each executed resting order still requires the corresponding active-order, priority-index, aggregate-book, and trade updates.

### Rollback

Optimized hot paths restore targeted mutations instead of copying the entire `OrderManager`.

Rollback cost therefore depends primarily on mutations performed by the operation rather than the total active-order population.

## Performance Architecture

The current architecture was shaped by measured profiling and latency-scaling results.

The major bottlenecks removed from measured hot paths include:

```text
linear active-order lookup
linear best-match discovery
full OrderManager copies during market execution
full OrderManager copies during crossing limit placement
full OrderManager copies during price-changing modification
unnecessary full trade-history rollback copies
```

The current benchmark checkpoint is stored in:

```text
benchmarks/results/final_performance_results.txt
```

The earlier pre-index checkpoint is preserved in:

```text
benchmarks/results/pre_index_performance_results.txt
```

Performance results are machine- and environment-dependent and should be interpreted as comparative benchmark measurements rather than deterministic real-time guarantees.

## Exception Safety

Matching operations can update several related structures.

Rollback-capable paths therefore treat these changes transactionally.

Depending on the operation, rollback can use:

- `TradeStore` checkpoints
- Transactional `OrderManager` removal
- Targeted mutation journals
- Order-book quantity restoration
- `OrderBook` snapshots where required
- Identifier and sequence restoration

Dedicated tests verify exception safety and state restoration.

## Concurrency Scope

`OrderBook` is synchronized for concurrent access to its own internal state.

This provides shared read access and exclusive mutation of the aggregated book.

The complete `MatchingEngine` should not be described as a fully concurrent matching engine solely because `OrderBook` uses synchronization.

A matching transaction can coordinate:

```text
OrderManager
OrderBook
TradeStore
OrderIdGenerator
sequence state
```

Full concurrent matching would require synchronization or ownership rules across that complete transaction boundary.

That remains separate from the current `OrderBook` synchronization.

## Testing Architecture

Correctness is validated through complementary layers including:

- Deterministic unit tests
- Matching edge-case tests
- Exception-safety tests
- Order-book exception-safety tests
- Trade-persistence tests
- Randomized invariant tests
- Portable fuzz testing
- Sanitizer-enabled execution

The priority-index implementation has direct coverage for:

- Best-buy selection
- Best-sell selection
- Price ordering
- Same-price FIFO ordering
- Opposite-side isolation
- Empty indexes
- Priority advancement after cancellation
- Sequence-priority updates

The post-optimization full test suite contains:

```text
172 tests
```

All 172 passed during the final Day 2 validation.

## Design Tradeoffs

### Vector Plus Indexes

Keeping a vector as authoritative active-order storage preserves compact storage and the existing observable active-order representation.

The tradeoff is that secondary indexes must remain synchronized whenever vector positions or priority fields change.

### Ordered Priority Indexes

Ordered indexes provide explicit price-time ordering and logarithmic maintenance.

They remove previous full active-order scans while keeping matching semantics straightforward to test.

### Transaction Journals

Mutation journals avoid expensive full-state copies.

The tradeoff is additional rollback bookkeeping and more complex exception-safety logic.

Dedicated regression tests are therefore important for these paths.

### Aggregated Book Separate From Orders

Maintaining individual resting orders separately from aggregate levels allows CMarket to provide both FIFO order-level execution and market-data-style aggregate views.

The tradeoff is that both representations must remain synchronized.

## Current Architecture Summary

The post-optimization engine uses:

```text
C++20 matching engine
integer price ticks
fixed-point quantities
price-time priority
vector-backed active-order ownership
expected O(1) OrderId lookup
ordered buy/sell priority indexes
swap-with-back cancellation
transactional order removal
targeted mutation journals
TradeStore checkpoints
aggregated OrderBook state
shared/exclusive OrderBook synchronization
persistent in-memory trade history
deterministic and randomized correctness testing
fuzz and sanitizer validation
```

The architecture prioritizes correctness and explicit invariants while removing the primary algorithmic bottlenecks identified by the performance benchmarks.
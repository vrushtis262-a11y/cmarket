# Concurrency Architecture

## Overview

cmarket uses a single-worker concurrency model around the matching engine.

Multiple producer threads may submit engine operations concurrently, but all
access to `MatchingEngine` is serialized through one worker thread.

The execution flow is:

```text
Producer Thread 1 ─┐
Producer Thread 2 ─┼──> BlockingQueue<EngineCommandPtr>
Producer Thread 3 ─┘              |
                                  v
                         Matching Engine Worker
                                  |
                                  v
                          MatchingEngine
                           /          \
                          v            v
                     OrderBook     TradeStore
```

This design provides concurrent command submission without requiring the
internal mutable state of `MatchingEngine` to be accessed by multiple threads
at the same time.

## Component Ownership

### BlockingQueue

`BlockingQueue<T>` owns its internal queue, mutex, condition variable, and
closed state.

Its mutex protects only queue state.

The queue lock is released before a command is processed by the matching
engine.

This keeps queue critical sections small and prevents matching work from
blocking producers from interacting with the queue longer than necessary.

### ConcurrentMatchingEngine

`ConcurrentMatchingEngine` owns:

- one `MatchingEngine`
- one `BlockingQueue<EngineCommandPtr>`
- one worker thread

Public operations create commands and submit them to the queue.

Each command owns a `std::promise` used to return either a result or an
exception to the producer through a `std::future`.

### MatchingEngine

The worker thread is the exclusive owner of runtime access to the
`MatchingEngine` instance contained by `ConcurrentMatchingEngine`.

Only the worker calls:

- `place_limit_buy`
- `place_limit_sell`
- `execute_market_buy`
- `execute_market_sell`
- `cancel_order`
- `modify_order`
- `active_limit_orders`
- `trade_history`
- `clear_trade_history`

Producer threads never access that `MatchingEngine` directly.

Because there is exactly one matching-engine consumer, mutable engine state is
serialized without an engine-wide mutex.

This includes:

- order ID generation
- FIFO sequence generation
- active order management
- matching
- trade persistence
- matching-engine-driven order-book mutation

### OrderBook

`OrderBook` retains its own `std::shared_mutex`.

Its internal synchronization protects direct concurrent access to order-book
state.

Readers may obtain book snapshots while another thread performs an order-book
mutation.

The `OrderBook` lock does not make the entire `MatchingEngine` concurrently
safe. Matching-engine state includes additional mutable structures that are
not protected by the order-book mutex.

For that reason, matching-engine operations must still pass through
`ConcurrentMatchingEngine`.

### TradeStore and OrderManager

`TradeStore` and `OrderManager` are not independently shared between producer
threads.

They remain internal to `MatchingEngine` and are accessed only by the single
matching-engine worker.

They therefore do not require additional synchronization in the concurrent
workflow.

## Command Processing

A producer operation follows these steps:

1. Construct a typed engine command.
2. Obtain the command's future.
3. Push the command into `BlockingQueue`.
4. Return the future to the producer.
5. The worker removes the command from the queue.
6. The worker executes the corresponding `MatchingEngine` operation.
7. The worker stores the result in the command promise.
8. The waiting producer receives the result from its future.

If the underlying matching-engine operation throws, the worker stores the
current exception in the promise.

Calling `future.get()` on the producer thread then rethrows that exception.

An invalid command therefore does not terminate the worker thread or prevent
later valid commands from being processed.

## Query Operations

`MatchingEngine::active_limit_orders()` and
`MatchingEngine::trade_history()` expose references in the synchronous engine
API.

The concurrent wrapper does not return those references.

Instead, the worker copies the requested data into:

- `std::vector<LimitOrder>`
- `std::vector<Trade>`

and returns the copy through a future.

This prevents producer threads from retaining references to mutable
matching-engine-owned containers while the worker continues processing
commands.

## Locking and Deadlock Prevention

The concurrency model intentionally avoids nested component locks.

The primary rules are:

1. The queue mutex protects only queue state.
2. The queue mutex is not held while executing matching-engine work.
3. Only one worker accesses `MatchingEngine`.
4. `OrderManager` and `TradeStore` are worker-owned and need no external
   locking.
5. `OrderBook` manages its own internal synchronization.
6. Producer threads communicate with the matching engine only through queued
   commands and futures.

The normal command path is therefore:

```text
queue lock
    |
    v
remove command
    |
    v
release queue lock
    |
    v
execute matching operation
    |
    v
OrderBook may acquire its own lock
```

There is no path where matching-engine processing holds the queue mutex while
waiting for an order-book lock.

This substantially reduces lock-ordering complexity and avoids a common source
of deadlocks.

## Worker Lifecycle

The worker starts when `ConcurrentMatchingEngine` is constructed.

It repeatedly performs a blocking `pop()` from the command queue.

Calling `shutdown()`:

1. closes the command queue
2. prevents new commands from being accepted
3. allows already queued commands to be drained
4. wakes a worker blocked on an empty queue
5. joins the worker thread

The destructor calls `shutdown()` so a live worker is not left behind when the
wrapper is destroyed.

Calling `shutdown()` repeatedly from the owning thread is supported.

Concurrent calls to `shutdown()` from multiple threads are not part of the
current API contract.

## Producer Concurrency

Multiple producer threads may concurrently submit:

- limit orders
- market orders
- cancellations
- modifications
- query requests

`BlockingQueue` synchronizes those submissions.

The matching worker processes accepted commands one at a time in queue order.

The exact interleaving between independent producer threads depends on which
thread successfully enqueues each command first.

Once a command is in the queue, processing remains serialized.

## Why One Matching Consumer

`BlockingQueue<T>` supports multiple consumers, and its tests exercise that
capability.

The matching-engine workflow intentionally uses only one consumer.

Using multiple workers against the same `MatchingEngine` would require
synchronization across several coupled pieces of mutable state, including:

- active orders
- priority indexes
- order IDs
- FIFO sequence numbers
- trade IDs
- execution sequence numbers
- rollback journals
- order-book mutations

A single owner keeps these state transitions deterministic and avoids placing
coarse-grained locks around latency-sensitive matching code.

Concurrency is therefore introduced at the command-submission boundary rather
than inside the matching algorithm itself.

## Current Safety Boundary

The supported concurrent API is:

```text
multiple producers
        |
        v
ConcurrentMatchingEngine
        |
        v
BlockingQueue
        |
        v
single worker
        |
        v
MatchingEngine
```

Direct concurrent calls to the same raw `MatchingEngine` instance remain
unsupported.

`OrderBook` can synchronize its own reads and writes, but that synchronization
must not be interpreted as synchronization for all matching-engine state.

## Validation

The producer/consumer queue is tested for:

- blocking behavior
- FIFO behavior
- close behavior
- single-producer/single-consumer operation
- multiple producers
- multiple consumers
- high-volume producer/consumer operation

The concurrent matching-engine workflow is tested for:

- asynchronous limit placement
- market execution
- cancellation
- modification
- copied active-order snapshots
- copied trade-history snapshots
- exception propagation through futures
- multiple concurrent producers
- concurrent submission and cancellation
- concurrent market-order submission
- serialized matching
- queue draining during shutdown
- rejection of submissions after shutdown
- repeated shutdown from the owning thread

The complete normal test suite currently contains 196 tests.

The project is also validated with AddressSanitizer and
UndefinedBehaviorSanitizer.

ASan and UBSan detect memory-safety and undefined-behavior problems,
respectively. They are not data-race detectors, so passing those sanitizer
runs should not be described as proof that arbitrary concurrent access is
race-free.

The concurrency architecture instead minimizes shared mutable state by
construction: the matching engine has a single worker owner, while the queue
and order book provide synchronization for the state they individually own.

Dedicated multithreaded stress testing is handled separately from these
functional concurrency tests.
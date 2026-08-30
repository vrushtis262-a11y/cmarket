# CMarket

CMarket is a modern C++20 exchange-style matching engine implementing market orders, limit orders, automatic matching, price-time priority, multi-level execution, partial fills, persistent trade history, input validation, exception-safety testing, randomized testing, fuzz testing, and sanitizer-assisted robustness validation.

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
- Aggregated price-level quantities

### Market Orders

- Market Buy
- Market Sell
- Multi-level execution
- Partial fills when liquidity is insufficient
- Average execution price
- Persistent trade history
- Invalid-quantity validation

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
- Invalid-price validation
- Invalid-quantity validation

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
- Defensive input validation
- Exception-safety coverage
- Randomized stress testing
- Portable fuzz-testing harness
- Optional native libFuzzer integration
- AddressSanitizer validation
- UndefinedBehaviorSanitizer validation

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
- Preserving engine invariants across rejected operations
- Maintaining valid state across failed operations

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
- Aggregated depth

### OrderManager

`OrderManager` owns active limit orders and their lifecycle.

Its responsibilities include:

- Adding active orders
- Looking up orders by `OrderId`
- Removing cancelled or fully filled orders
- Providing read-only access to the active-order collection
- Rejecting invalid active-order state

### TradeStore

`TradeStore` owns persistent executed-trade history.

Its responsibilities include:

- Recording trades
- Validating trade price and quantity
- Assigning unique Trade IDs
- Assigning monotonically increasing execution sequence numbers
- Preserving buy and sell Order IDs when available
- Returning trade history
- Clearing stored history without reusing IDs or sequence numbers

This separation keeps matching orchestration independent from active-order ownership and trade persistence.

---

## Validation Philosophy

CMarket validates public API inputs before allowing invalid operations to mutate engine state.

The core validation rules are:

- Limit-order prices must be positive.
- Limit-order quantities must be positive.
- Market-order quantities must be positive.
- Modified prices must be positive.
- Modified quantities must be positive.
- Recorded trade prices must be positive.
- Recorded trade quantities must be positive.
- Invalid operations must not create active orders.
- Invalid operations must not create trades.
- Rejected trade records must not consume Trade IDs or execution sequence numbers.
- Arithmetic overflow must not silently create invalid quantities.
- Rejected operations must preserve previously valid engine state.

Invalid numeric order parameters are rejected with standard exceptions such as:

```cpp
std::invalid_argument
```

Checked arithmetic failures may be rejected with:

```cpp
std::overflow_error
```

The validation layer is intentionally deterministic: the same invalid operation should fail predictably rather than silently corrupting engine state.

Validation is tested through deterministic regression tests, exception-safety tests, fixed-seed randomized tests, fuzz input, AddressSanitizer, and UndefinedBehaviorSanitizer.

---

## Invalid-Order Behavior

Invalid orders are rejected before they can become valid active orders.

Examples include:

```text
Limit price <= 0
Limit quantity <= 0
Market quantity <= 0
Modified price <= 0
Modified quantity <= 0
```

The engine also tests extreme signed 64-bit integer inputs, including boundary values, to ensure malformed input does not cause undefined behavior.

Unknown Order IDs are handled predictably.

Cancellation of an unknown `OrderId` returns failure without corrupting engine state.

Modification of an unknown `OrderId` returns failure without corrupting engine state.

A rejected operation must not leave behind:

- A partially created active order
- A partially applied modification
- An invalid order-book level
- An invalid trade
- Incorrectly consumed identifiers when rollback is required
- Corrupted state that prevents later valid operations

---

## Exception Safety

CMarket tests engine operations for exception safety.

When an operation is rejected, previously valid engine state should remain usable.

The exception-safety tests verify scenarios including:

- A valid limit order can be placed after a rejected limit order.
- A valid market order can execute after a rejected market order.
- A valid modification can succeed after a rejected modification.
- An unknown-order modification does not prevent later valid operations.
- Repeated rejected operations preserve usable engine state.
- Quantity aggregation overflow is detected rather than silently wrapping.
- Invalid order-book snapshots preserve existing valid state.
- Bid aggregation overflow preserves existing state.
- Ask aggregation overflow preserves existing state.
- Valid snapshots still work after repeated failures.
- Failed limit placement does not incorrectly consume an Order ID.
- Failed limit placement does not incorrectly consume a FIFO sequence number.
- Failed modification restores removed order state when necessary.
- Failed modification restores sequence state when necessary.
- Partial fills keep the order book and active-order state synchronized.

The goal is to prevent rejected operations from leaving the matching engine in a corrupted or unusable state.

---

## Engine Invariants

The deterministic, randomized, exception-safety, and fuzz tests exercise important matching-engine invariants.

### Active Orders

For every active limit order:

```text
order_id > 0
price_ticks > 0
original_quantity > 0
remaining_quantity > 0
remaining_quantity <= original_quantity
sequence_number > 0
```

Active Order IDs must remain unique.

FIFO sequence information must remain valid.

Fully filled or cancelled orders must not remain active.

### Trades

For every recorded trade:

```text
trade_id > 0
execution_sequence > 0
price_ticks > 0
quantity > 0
```

Trade IDs must increase monotonically.

Execution sequence numbers must increase monotonically.

Recorded trades must never contain non-positive prices or quantities.

### Order Book

Every visible price level must have:

```text
price_ticks > 0
quantity > 0
```

Bid levels must remain ordered from highest price to lowest price.

Ask levels must remain ordered from lowest price to highest price.

Resting order-book quantities must correspond to valid active orders.

Aggregated quantities must not silently overflow signed integer storage.

### Matching

After automatic matching completes, the engine should not leave immediately matchable crossing orders resting.

When both sides are present:

```text
best_bid < best_ask
```

Matching must continue until the incoming order is filled or no eligible crossing liquidity remains.

### Rejected Operations

Rejected operations must not unexpectedly corrupt:

- Active orders
- Trade history
- Order-book state
- Order IDs
- FIFO sequence state
- Trade IDs
- Execution sequence state
- Future valid operations

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

Clearing trade history removes stored trade records but does not reset Trade IDs or execution sequence numbers.

Newly recorded trades therefore continue with monotonically increasing identifiers.

---

## Randomized Testing

CMarket includes randomized matching tests in addition to deterministic example-based tests.

The randomized tests generate long sequences of matching-engine operations and repeatedly verify engine invariants.

Randomized operations include:

- Limit-order placement
- Market-order execution
- Cancellation
- Modification

The randomized suite is designed to exercise combinations that are easy to miss with individually written unit tests.

Fixed random seeds are used so failures can be reproduced.

The suite uses multiple seeds to explore different operation sequences while preserving deterministic regression behavior.

It also verifies reproducibility properties including:

- The same seed produces the same operation sequence.
- Different seeds produce different operation sequences.
- Replaying the same operation sequence produces the same final state.

Run the randomized tests with:

```bash
ctest \
    --test-dir build \
    -R RandomizedMatchingTest \
    --output-on-failure
```

Randomized testing complements the deterministic regression suite rather than replacing it.

If a randomized test discovers a defect, the failing sequence should be reduced to a deterministic regression test.

---

## Fuzz Testing

CMarket includes a portable matching-engine fuzz harness:

```text
tests/matching_engine_fuzz.cpp
```

Arbitrary bytes are decoded into combinations of:

- Operation type
- Order side
- Price
- Quantity
- `OrderId`

The harness exercises:

- Limit-order placement
- Market-order execution
- Cancellation
- Modification

Price and quantity values are derived from arbitrary signed 64-bit input.

This allows malformed and extreme values to reach the public matching-engine API, including:

- Zero
- Negative values
- Large positive values
- Large negative values
- Signed integer boundary values
- Unknown Order IDs
- Unexpected operation sequences

Expected validation exceptions such as `std::invalid_argument` and checked arithmetic failures such as `std::overflow_error` are handled by the fuzz harness.

After each operation, matching-engine invariants are checked.

The objective is to detect:

- Crashes
- Undefined behavior
- AddressSanitizer failures
- UndefinedBehaviorSanitizer failures
- Integer-related bugs
- Invalid state transitions
- Invalid active orders
- Invalid trades
- Invalid price levels
- Incorrect book ordering
- Crossed resting books
- Unexpected failures caused by malformed operation sequences

A fuzz failure should be preserved and converted into a deterministic regression test.

---

## Portable Fuzz Harness

The fuzz harness can be built as a normal executable even when libFuzzer is unavailable.

Configure:

```bash
cmake \
    -S . \
    -B build-fuzz \
    -DBUILD_TESTING=ON
```

Build:

```bash
cmake \
    --build build-fuzz \
    --target matching_engine_fuzz
```

The executable is:

```text
build-fuzz/matching_engine_fuzz
```

A binary input can be piped directly into the harness:

```bash
printf '\x00\x01\x02\x03\x04\x05\x10\x20\x30\x40\x50\x60\x70\x80' \
    | ./build-fuzz/matching_engine_fuzz
```

A successful run exits normally without a crash or invariant violation.

Random system input can also be supplied:

```bash
head -c 100000 /dev/urandom \
    | ./build-fuzz/matching_engine_fuzz
```

For reproducible standalone fuzz-style testing, deterministic binary inputs can be generated from fixed seeds:

```bash
for seed in 1 2 3 4 5
do
    python3 - "$seed" <<'PY' | ./build-fuzz/matching_engine_fuzz
import random
import sys

seed = int(sys.argv[1])
rng = random.Random(seed)

sys.stdout.buffer.write(
    bytes(
        rng.randrange(256)
        for _ in range(200000)
    )
)
PY

    echo "seed $seed passed"
done
```

Fixed seeds make standalone fuzz failures reproducible.

---

## libFuzzer Integration

CMarket can optionally build the matching-engine fuzz target with native libFuzzer support.

Enable the integration with:

```bash
cmake \
    -S . \
    -B build-libfuzzer \
    -DBUILD_TESTING=ON \
    -DCMARKET_ENABLE_LIBFUZZER=ON
```

The build system checks whether the active compiler can actually compile and link a libFuzzer executable.

When libFuzzer is available, the fuzz target uses:

```text
-fsanitize=fuzzer,address,undefined
-fno-omit-frame-pointer
```

The `CMARKET_LIBFUZZER` compile definition disables the standalone stdin `main()` and allows the libFuzzer runtime to drive:

```cpp
extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size
);
```

### Apple Clang Compatibility

Some Apple Clang installations recognize:

```text
-fsanitize=fuzzer
```

but do not ship the required runtime library:

```text
libclang_rt.fuzzer_osx.a
```

CMarket detects this situation during CMake configuration.

If libFuzzer is requested but its runtime is unavailable, CMake emits a warning and keeps `matching_engine_fuzz` in portable standalone stdin mode rather than failing the project configuration.

This allows the fuzz harness to remain usable with Apple Clang while also supporting native coverage-guided fuzzing on toolchains that provide libFuzzer.

---

## Fuzz Testing With Sanitizers

The portable fuzz harness can be combined with AddressSanitizer and UndefinedBehaviorSanitizer even when native libFuzzer is unavailable.

Configure:

```bash
cmake \
    -S . \
    -B build-fuzz-sanitize \
    -DBUILD_TESTING=ON \
    -DCMARKET_ENABLE_SANITIZERS=ON \
    -DCMARKET_ENABLE_LIBFUZZER=OFF
```

Build:

```bash
cmake \
    --build build-fuzz-sanitize \
    --target matching_engine_fuzz \
    -j
```

Run deterministic fuzz inputs:

```bash
for seed in 1 2 3 4 5
do
    python3 - "$seed" <<'PY' | ./build-fuzz-sanitize/matching_engine_fuzz
import random
import sys

seed = int(sys.argv[1])
rng = random.Random(seed)

sys.stdout.buffer.write(
    bytes(
        rng.randrange(256)
        for _ in range(200000)
    )
)
PY

    echo "sanitized seed $seed passed"
done
```

A sanitizer diagnostic is considered a fuzz failure even if the process otherwise appears to continue normally.

---

## Regression Testing

Every discovered robustness failure should be reduced to a deterministic reproduction and added to the permanent test suite.

This ensures that once a bug is fixed, future changes cannot silently reintroduce it.

The regression strategy combines:

- Deterministic unit tests
- Matching edge-case tests
- Trade-persistence tests
- Invalid-input tests
- Exception-safety tests
- Order-book exception-safety tests
- Randomized tests
- Fuzz testing
- Sanitizer-assisted testing

When randomized testing, fuzz testing, sanitizers, or manual investigation expose a failure:

1. Preserve the failing input or operation sequence.
2. Reproduce the failure deterministically.
3. Reduce it to the smallest practical reproduction.
4. Add a permanent deterministic regression test.
5. Fix the underlying engine defect.
6. Run the complete deterministic suite.
7. Run randomized tests again.
8. Run sanitizer validation again.
9. Run the fuzz harness again.

---

## Property Testing vs Example Testing

CMarket uses multiple testing styles because they detect different classes of defects.

### Example-Based Tests

Example tests verify specific expected scenarios.

Examples include:

- Placing a limit order
- Cancelling an order
- Executing a market order
- Matching two crossing orders
- Preserving FIFO priority
- Modifying an active order
- Recording a trade

Example-based tests generally follow:

```text
known input -> known expected result
```

### Property and Invariant Tests

Property-oriented tests verify conditions that should remain true across many different inputs.

Examples include:

- Active Order IDs remain unique.
- Prices and quantities remain valid.
- Trade IDs remain valid.
- Execution sequence numbers remain ordered.
- Rejected operations do not corrupt state.
- The engine remains usable after invalid input.
- The order book remains correctly sorted.
- The resting book remains uncrossed after matching.
- Active orders remain compatible with order-book state.

Property-oriented tests generally follow:

```text
many operation sequences -> invariants must always remain true
```

### Fuzz Testing

Fuzz testing attacks the API with unexpected byte sequences and operation combinations.

The portable harness allows deterministic and random byte streams to exercise the API.

When a supported libFuzzer toolchain is available, the same harness can be driven using coverage-guided mutation.

Together, example testing, randomized testing, property-oriented invariant checking, fuzz testing, and sanitizers provide broader robustness coverage than any single testing style alone.

---

## Tech Stack

- C++20
- CMake
- GoogleTest
- Apple Clang / Clang
- AddressSanitizer
- UndefinedBehaviorSanitizer
- libFuzzer-compatible fuzz harness
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
    exception_safety_tests.cpp
    execution_tests.cpp
    limit_order_tests.cpp
    matching_edge_cases_tests.cpp
    matching_engine_fuzz.cpp
    matching_engine_tests.cpp
    order_book_exception_safety_tests.cpp
    order_book_tests.cpp
    order_id_generator_tests.cpp
    order_manager_tests.cpp
    randomized_matching_tests.cpp
    trade_persistence_tests.cpp
    trade_store_tests.cpp

CMakeLists.txt
README.md
```

---

## Build

Configure and build:

```bash
cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON

cmake --build build -j
```

Run:

```bash
./build/cmarket
```

---

## Run Tests

Run the complete regression suite:

```bash
ctest \
    --test-dir build \
    --output-on-failure
```

Run exception-safety tests:

```bash
ctest \
    --test-dir build \
    -R ExceptionSafetyTest \
    --output-on-failure
```

Run randomized tests:

```bash
ctest \
    --test-dir build \
    -R RandomizedMatchingTest \
    --output-on-failure
```

---

## Compiler Warnings

CMarket enables strict compiler warnings for project targets:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
```

These warnings help catch suspicious conversions, portability problems, and common C++ mistakes during compilation.

A warning-clean project-source build is part of robustness validation.

Third-party dependencies such as GoogleTest may produce compiler diagnostics that do not originate from CMarket source.

---

## Sanitizer Build

AddressSanitizer and UndefinedBehaviorSanitizer can be enabled through CMake:

```bash
cmake \
    -S . \
    -B build-sanitized \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DCMARKET_ENABLE_SANITIZERS=ON

cmake --build build-sanitized -j

ctest \
    --test-dir build-sanitized \
    --output-on-failure
```

The sanitizer configuration enables:

```text
-fsanitize=address,undefined
-fno-omit-frame-pointer
```

for supported Clang and GCC builds.

### AddressSanitizer

AddressSanitizer helps detect memory-safety problems such as:

- Out-of-bounds accesses
- Use-after-free
- Invalid memory accesses
- Other memory corruption

### UndefinedBehaviorSanitizer

UndefinedBehaviorSanitizer helps detect operations whose behavior is undefined by C++.

This is especially valuable when testing:

- Extreme integer values
- Invalid numeric input
- Randomized operation sequences
- Arbitrary fuzz input
- Complex matching state transitions

---

## Robustness Validation

A complete robustness validation run consists of the following stages.

### 1. Clean Build

```bash
rm -rf build

cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON

cmake --build build -j
```

### 2. Full Regression Suite

```bash
ctest \
    --test-dir build \
    --output-on-failure
```

### 3. Exception-Safety Tests

```bash
ctest \
    --test-dir build \
    -R ExceptionSafetyTest \
    --output-on-failure
```

### 4. Randomized Tests

```bash
ctest \
    --test-dir build \
    -R RandomizedMatchingTest \
    --output-on-failure
```

### 5. Portable Fuzz Harness

Build:

```bash
cmake \
    --build build \
    --target matching_engine_fuzz
```

Run fixed-seed fuzz-style inputs:

```bash
for seed in 1 2 3 4 5
do
    python3 - "$seed" <<'PY' | ./build/matching_engine_fuzz
import random
import sys

seed = int(sys.argv[1])
rng = random.Random(seed)

sys.stdout.buffer.write(
    bytes(
        rng.randrange(256)
        for _ in range(200000)
    )
)
PY

    echo "seed $seed passed"
done
```

### 6. Sanitizer Build

```bash
rm -rf build-sanitized

cmake \
    -S . \
    -B build-sanitized \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DCMARKET_ENABLE_SANITIZERS=ON \
    -DCMARKET_ENABLE_LIBFUZZER=OFF

cmake --build build-sanitized -j
```

### 7. Sanitized Regression Suite

```bash
ctest \
    --test-dir build-sanitized \
    --output-on-failure
```

### 8. Sanitized Fuzz Harness

```bash
for seed in 1 2 3 4 5
do
    python3 - "$seed" <<'PY' | ./build-sanitized/matching_engine_fuzz
import random
import sys

seed = int(sys.argv[1])
rng = random.Random(seed)

sys.stdout.buffer.write(
    bytes(
        rng.randrange(256)
        for _ in range(200000)
    )
)
PY

    echo "sanitized seed $seed passed"
done
```

### 9. Optional libFuzzer Capability Check

```bash
rm -rf build-libfuzzer

cmake \
    -S . \
    -B build-libfuzzer \
    -DBUILD_TESTING=ON \
    -DCMARKET_ENABLE_LIBFUZZER=ON
```

If the runtime is available, native libFuzzer mode is enabled.

If the runtime is unavailable, CMake reports the limitation and falls back to standalone stdin mode.

A successful robustness validation run should complete without:

- Failed deterministic tests
- Randomized-test invariant failures
- Fuzz-harness invariant failures
- Crashes
- AddressSanitizer failures
- UndefinedBehaviorSanitizer failures
- CMarket compiler-warning regressions

---

## Test Coverage

The automated test suite covers:

- Order-book behavior
- Order-book exception safety
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
- Invalid market quantities
- Invalid limit prices
- Invalid limit quantities
- Invalid modification parameters
- Invalid trade prices
- Invalid trade quantities
- Extreme signed integer inputs
- Exception safety
- State preservation after rejected operations
- Quantity aggregation overflow
- Order ID preservation after failed placement
- Sequence-number preservation after failed placement
- Failed-modification rollback
- Partial-fill synchronization
- Randomized operation sequences
- Multiple fixed random seeds
- Randomized reproducibility
- Engine invariant validation
- Arbitrary fuzz-input execution
- Portable fuzz execution
- AddressSanitizer validation
- UndefinedBehaviorSanitizer validation

---

## Robustness Goals

The robustness milestone is designed around several principles:

1. Invalid input should fail predictably.
2. Rejected operations should not corrupt valid engine state.
3. Integer overflow should not silently create invalid quantities.
4. Active orders should always satisfy documented invariants.
5. Recorded trades should always contain valid execution data.
6. The aggregated order book should remain compatible with active orders.
7. Randomized operation sequences should preserve engine invariants.
8. Arbitrary fuzz input should not cause crashes or undefined behavior.
9. Sanitizer-enabled execution should remain clean.
10. Fixed seeds should make randomized failures reproducible.
11. Fuzz failures should be preserved as deterministic regression tests.
12. Unsupported optional tooling should degrade gracefully rather than breaking the normal build.

---

## Current Status

The current matching-engine robustness milestone includes:

- Multi-level automatic matching
- Robust partial-fill behavior
- Price-time priority
- Persistent trade history
- Modular matching architecture
- Dedicated active-order management
- Dedicated trade persistence
- Invalid-input validation
- Exception-safety testing
- Order-book exception-safety testing
- Overflow protection
- Failed-operation state preservation
- Randomized stress testing
- Fixed-seed reproducibility
- Portable fuzz-testing harness
- Optional native libFuzzer integration
- Automatic libFuzzer capability detection
- Graceful standalone fallback when libFuzzer is unavailable
- Engine-invariant testing
- Full automated regression suite
- AddressSanitizer validation
- UndefinedBehaviorSanitizer validation
- Strict compiler-warning configuration
- Modern C++20
- CMake build system

---

## Future Improvements

- Larger fuzz corpora
- Longer-duration fuzz campaigns
- Native libFuzzer campaigns on supported LLVM toolchains
- Automated fuzzing in CI
- Property-based testing framework integration
- Performance benchmarking
- Iceberg orders
- Stop orders
- Fill-or-Kill orders
- Immediate-or-Cancel orders
- Multithreaded matching
- Persistent on-disk order and trade storage

---

## Release Readiness

The robustness milestone targets the following release criteria:

```text
Invalid inputs handled predictably
Exception-safe engine operations
Engine invariants explicitly tested
Randomized stress coverage
Fixed-seed reproducibility
Portable fuzz-testing harness
Optional libFuzzer integration
Graceful fallback when libFuzzer is unavailable
Regression failures preserved as tests
Full deterministic suite passing
AddressSanitizer passing
UndefinedBehaviorSanitizer passing
Strict compiler warnings enabled
CI green
Documentation updated
```

Local robustness validation includes the full deterministic and randomized regression suite, portable fuzz execution, AddressSanitizer, UndefinedBehaviorSanitizer, and strict project compiler warnings.

Native libFuzzer execution depends on the active Clang toolchain providing the required libFuzzer runtime. When that runtime is unavailable, the portable fuzz harness remains available.

Once the final repository state is committed, pushed, and CI is green, the project is ready for the `v1.2.0` robustness release.

---

## License

MIT License
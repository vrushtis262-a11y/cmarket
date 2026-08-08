# CMarket

A modern C++20 exchange-style matching engine implementing market orders, limit orders, automatic matching, price-time (FIFO) priority, and trade execution.

---

## Features

### Order Book

- Bid/Ask order book
- Best bid / best ask
- Spread calculation
- Mid-price calculation
- VWAP
- Microprice
- Order book imbalance
- Market depth

### Market Orders

- Market Buy
- Market Sell
- Multi-level execution
- Partial fills
- Average execution price
- Trade history

### Limit Orders

- Limit Buy
- Limit Sell
- Unique Order IDs
- FIFO sequence numbers
- Automatic matching
- Cancel orders
- Modify orders

### Matching Engine

- Price-time priority (FIFO)
- Crossing order detection
- Partial matching
- Full matching
- Resting orders
- Automatic order book updates

---

## Tech Stack

- C++20
- CMake
- GoogleTest
- Apple Clang
- Git
- GitHub

---

## Project Structure

```
include/
src/
tests/
CMakeLists.txt
README.md
```

---

## Build

```bash
cmake -S . -B build
cmake --build build
```

Run:

```bash
./build/cmarket
```

---

## Run Tests

```bash
ctest --test-dir build --output-on-failure
```

---

## Sanitizer Build

```bash
cmake -S . -B build-sanitize \
    -DCMARKET_ENABLE_SANITIZERS=ON

cmake --build build-sanitize

ctest --test-dir build-sanitize --output-on-failure
```

---

## Test Coverage

The project includes tests for:

- Order book
- Market orders
- Partial fills
- Trade history
- Limit orders
- Cancel orders
- Modify orders
- Automatic matching
- FIFO priority
- Duplicate order IDs

---

## Architecture

```
               MatchingEngine
                     │
          ┌──────────┴──────────┐
          │                     │
    OrderManager          OrderBook
          │                     │
          └──────────┬──────────┘
                     │
                  Trades
```

---

## Current Status

- Production-ready core matching engine
- Full automated test suite
- AddressSanitizer verified
- UndefinedBehaviorSanitizer verified
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
- Persistent order storage

---

## License

MIT License
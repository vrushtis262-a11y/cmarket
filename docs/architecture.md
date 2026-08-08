# CMarket Architecture

## Overview

CMarket is organized around three core components:

```text
                 +------------------+
                 |  MatchingEngine  |
                 +---------+--------+
                           |
             +-------------+-------------+
             |                           |
             v                           v
    +------------------+        +------------------+
    |   OrderManager   |        |    OrderBook     |
    +------------------+        +------------------+
             |                           |
             |                           |
             +-------------+-------------+
                           |
                           v
                    +-------------+
                    |   Trades    |
                    +-------------+
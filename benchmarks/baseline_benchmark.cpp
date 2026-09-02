cmarket Performance Engineering - Day 1 Baseline and Day 2 Results

Build:
Release

Architecture:
Apple Silicon ARM64


============================================================
Day 1 Baseline
============================================================

10,000-order baseline (approximate repeated-run results):

Limit buy placement:
~20.6 us/op
~48.5k ops/sec

Limit sell placement:
~20.4 us/op
~48.9k ops/sec

Order cancellation:
~20.5 us/op
~48.7k ops/sec

Order modification:
~49.2 us/op
~20.3k ops/sec

Market buy matching:
~32.4 us/op
~30.9k ops/sec


100,000-order workload:

Limit buy placement:
198058.35 ns/op
5049 ops/sec

Limit sell placement:
197541.64 ns/op
5062 ops/sec

Order cancellation:
191274.16 ns/op
5228 ops/sec

Order modification:
468559.94 ns/op
2134 ops/sec

Market buy matching:
334099.88 ns/op
2993 ops/sec

Total benchmark runtime:
~197.45 sec


Day 1 profiling findings:

1. MatchingEngine::rebuild_order_book()
   - Primary observed hotspot.
   - Rebuilt the complete aggregated book after mutations.
   - Scanned all active orders.
   - Constructed temporary std::map containers.
   - Constructed temporary PriceLevel vectors.
   - Caused allocation/free activity.

2. Order lookup/storage
   - OrderManager used vector-based storage.
   - Profiling showed significant work around order management.
   - Lookup, duplicate detection, and cancellation were linear scans.

3. MatchingEngine::find_best_match()
   - Scanned active orders to locate the best price/time match.
   - Similar scanning occurred during market-order execution.

Additional candidate:
- Exception-safety backups copied major engine state before mutations.


Scaling observation:

Increasing the workload from 10,000 to 100,000 orders caused
limit-order placement latency to increase from roughly 20 us/op
to roughly 198 us/op.

This demonstrated poor scaling in the original mutation path,
with repeated O(n) work producing approximately O(n^2) behavior
across the benchmark workload.


============================================================
Day 2 - Performance Optimization
============================================================

Goal:

Reduce unnecessary allocation, copying, full-book rebuilding,
and redundant active-order scans while preserving:

- matching correctness
- price-time priority
- exception safety
- existing public behavior


------------------------------------------------------------
Optimization 1 - Move rebuilt vectors into OrderBook
------------------------------------------------------------

Change:

Changed rebuild_order_book() to pass bid and ask vectors using
move semantics instead of copying them into OrderBook.

Result:

No meaningful end-to-end benchmark improvement.

Observed benchmark behavior was mixed and within normal run-to-run
variation.

Decision:

KEPT.

Reason:

The change objectively removes unnecessary vector copies and has
minimal complexity cost, even though the full benchmark did not
show a measurable improvement.


------------------------------------------------------------
Optimization 2 - Avoid redundant snapshot normalization
------------------------------------------------------------

Change:

Added OrderBook::replace_normalized_snapshot().

MatchingEngine::rebuild_order_book() already produced aggregated
and sorted price levels, so passing those levels through
OrderBook::replace_snapshot() caused them to be normalized again.

The new path directly installs already-normalized vectors.

Result:

No meaningful end-to-end benchmark improvement.

Observed benchmark values remained close to the existing baseline.

Decision:

KEPT.

Reason:

The change removes demonstrably redundant aggregation and sorting
work without changing external behavior.


------------------------------------------------------------
Optimization 3 - Incremental OrderBook maintenance
------------------------------------------------------------

Change:

Eliminated MatchingEngine::rebuild_order_book() from normal
matching-engine mutation paths.

Instead, aggregate book levels are updated incrementally through:

- OrderBook::adjust_bid()
- OrderBook::adjust_ask()

Book quantity is now updated directly when:

- a new limit order rests
- a resting order is partially or fully executed
- an order is cancelled
- an order is modified
- active local orders are consumed by market execution

A first local limit order still replaces any pre-existing external
snapshot to preserve previous engine semantics.

Additional OrderBook adjustment tests were added for:

- quantity increases
- quantity decreases
- insertion order
- zero quantity adjustments
- removing levels at zero
- negative-result rejection
- overflow rejection
- INT64_MIN rejection
- negative-price rejection


100,000-order benchmark after Optimization 3:

Limit buy placement:
87281.05 ns/op

Limit sell placement:
85704.72 ns/op

Order cancellation:
81701.65 ns/op

Order modification:
257491.28 ns/op

Market buy matching:
224221.61 ns/op

Total benchmark runtime:
~99.64 sec


Improvement versus Day 1:

Limit buy placement:
~55.9% lower latency

Limit sell placement:
~56.6% lower latency

Order cancellation:
~57.3% lower latency

Order modification:
~45.0% lower latency

Market buy matching:
~32.9% lower latency

Total runtime:
~49.5% lower

Approximately:
~1.98x faster total workload


Decision:

KEPT.

Reason:

This was the largest structural Day 2 improvement and removed the
primary Day 1 hotspot, MatchingEngine::rebuild_order_book().


------------------------------------------------------------
Optimization 4 - Fast path for non-crossing limit placement
------------------------------------------------------------

Change:

Avoided copying the complete OrderManager and TradeStore before
every limit placement.

For non-crossing orders, placement now avoids those expensive
whole-object backups.

The larger backups are only taken when matching is actually
required.

OrderBook, OrderIdGenerator, and sequence state are still backed
up as required for exception safety.


100,000-order benchmark after Optimization 4:

Limit buy placement:
47941.82 ns/op
20859 ops/sec

Limit sell placement:
47405.75 ns/op
21094 ops/sec

Order cancellation:
82121.50 ns/op
12177 ops/sec

Order modification:
257023.23 ns/op
3891 ops/sec

Market buy matching:
225842.42 ns/op
4428 ops/sec

Total benchmark runtime:
80.54 sec


Improvement versus Optimization 3:

Limit buy placement:
~45.1% lower latency

Limit sell placement:
~44.7% lower latency

Total runtime:
~19.2% lower


Improvement versus Day 1:

Limit buy placement:
~75.8% lower latency

Limit sell placement:
~76.0% lower latency

Total runtime:
~59.2% lower

Approximately:
~2.45x faster total workload


Decision:

KEPT.


------------------------------------------------------------
Optimization 5 - unordered_map OrderManager index
------------------------------------------------------------

Experiment:

Added an unordered_map<OrderId, size_t> alongside the existing
vector in OrderManager.

Intended effects:

- O(1) average order lookup
- O(1) average duplicate-ID check
- preserve vector order for FIFO behavior
- cancellation still required vector shifting and index repair

Additional regression tests were added for:

- lookup after cancelling the first order
- lookup after cancelling a middle order
- reuse of a cancelled order ID

These tests are retained even though the optimization was reverted.


100,000-order benchmark with Optimization 5:

Limit buy placement:
47720.02 ns/op

Limit sell placement:
47416.93 ns/op

Order cancellation:
81910.68 ns/op

Order modification:
257400.17 ns/op

Market buy matching:
220290.38 ns/op

Total benchmark runtime:
79.97 sec


Change versus Optimization 4:

Limit buy placement:
~0.46% faster

Limit sell placement:
~0.02% slower

Order cancellation:
~0.26% faster

Order modification:
~0.15% slower

Market buy matching:
~2.46% faster

Total runtime:
~0.71% faster


Profiling:

OrderManager::add_order() remained a dominant placement hotspot
despite the added unordered_map index.

The profile was effectively unchanged in the area the optimization
was intended to improve.


Decision:

REJECTED AND REVERTED.

Reason:

The benchmark improvement was too small to justify the added
memory use, bookkeeping, and implementation complexity.

The additional OrderManager regression tests were kept.


------------------------------------------------------------
Optimization 6 - O(1) preliminary crossing check
------------------------------------------------------------

Observation:

After Optimization 4, non-crossing limit placement still called
MatchingEngine::find_best_match() solely to determine whether the
incoming order crossed an existing resting order.

find_best_match() performs a linear scan of active orders.

However, because the aggregate OrderBook is synchronized with
active local orders, crossing can be determined directly from the
best opposite-side price.


Change:

Added MatchingEngine::crosses_order_book().

For an incoming buy:

best ask <= incoming limit price

means the order crosses.

For an incoming sell:

best bid >= incoming limit price

means the order crosses.

This check uses OrderBook::best_ask() or OrderBook::best_bid()
instead of scanning every active LimitOrder.

The existing find_best_match() implementation remains unchanged
for actual matching, preserving exact price-time priority.

The first-local-order snapshot replacement behavior is also
preserved.


Correctness:

Full test suite after Optimization 6:

158 / 158 tests passed
100% passing

This included:

- limit-order placement
- crossing buy and sell matching
- FIFO matching
- partial fills
- cancellation
- modification
- exception safety
- randomized matching invariants


100,000-order benchmark after Optimization 6:

Limit buy placement:
24254.31 ns/op
41230 ops/sec

Limit sell placement:
24182.93 ns/op
41351 ops/sec

Order cancellation:
81827.89 ns/op
12221 ops/sec

Order modification:
258596.20 ns/op
3867 ops/sec

Market buy matching:
230745.11 ns/op
4334 ops/sec

Total benchmark runtime:
69.43 sec


Change versus Optimization 4:

Limit buy placement:
~49.4% lower latency

Limit sell placement:
~49.0% lower latency

Order cancellation:
~0.4% lower latency

Order modification:
~0.6% higher latency

Market buy matching:
~2.2% higher latency

Total runtime:
~13.8% lower


The small changes in cancellation, modification, and market matching
are treated as benchmark variation because Optimization 6 does not
directly modify those execution paths.


Improvement versus original Day 1 baseline:

Limit buy placement:
198058.35 ns/op -> 24254.31 ns/op
~87.8% lower latency
~8.17x faster

Limit sell placement:
197541.64 ns/op -> 24182.93 ns/op
~87.8% lower latency
~8.17x faster

Order cancellation:
191274.16 ns/op -> 81827.89 ns/op
~57.2% lower latency
~2.34x faster

Order modification:
468559.94 ns/op -> 258596.20 ns/op
~44.8% lower latency
~1.81x faster

Market buy matching:
334099.88 ns/op -> 230745.11 ns/op
~30.9% lower latency
~1.45x faster

Total runtime:
197.45 sec -> 69.43 sec
~64.8% lower
~2.84x faster


Decision:

KEPT.

Reason:

The change removed an unnecessary O(n) scan from the common
non-crossing placement path and produced an approximately 49%
additional reduction in limit placement latency.


------------------------------------------------------------
Final Day 2 profiling
------------------------------------------------------------

A post-Optimization-6 macOS sample profile showed that the old
find_best_match() placement hotspot was no longer dominant.

The primary remaining placement hotspot was:

OrderManager::add_order(LimitOrder const&)
4372 samples

Large sample counts also remained in:

MatchingEngine::cancel_order()
OrderManager::cancel_order()

This is consistent with the remaining vector-based OrderManager
implementation.

OrderManager::add_order() performs duplicate-ID detection by
scanning the vector, while cancellation performs linear lookup and
vector erase work.

An unordered_map index was already measured during Optimization 5
and did not provide a meaningful enough improvement to justify its
complexity.

Therefore no additional OrderManager data-structure change was
accepted during Day 2.


============================================================
Day 2 Final Result
============================================================

Original 100,000-order Day 1 baseline:

Limit buy placement:
198058.35 ns/op

Limit sell placement:
197541.64 ns/op

Order cancellation:
191274.16 ns/op

Order modification:
468559.94 ns/op

Market buy matching:
334099.88 ns/op

Total:
~197.45 sec


Final accepted Day 2 implementation:

Limit buy placement:
24254.31 ns/op

Limit sell placement:
24182.93 ns/op

Order cancellation:
81827.89 ns/op

Order modification:
258596.20 ns/op

Market buy matching:
230745.11 ns/op

Total:
69.43 sec


Key result:

Limit-order placement latency decreased by approximately 87.8%,
from roughly 198 us/op to roughly 24 us/op.

The complete benchmark workload decreased from approximately
197 seconds to 69 seconds, a reduction of approximately 64.8%
and an overall speedup of approximately 2.84x.


Accepted Day 2 optimizations:

1. Move rebuilt vectors instead of copying them.
2. Avoid redundant normalization of already-normalized snapshots.
3. Replace full OrderBook rebuilds with incremental maintenance.
4. Avoid large engine-state backups on non-crossing placements.
5. Replace preliminary find_best_match() scan with an O(1)
   aggregate-book crossing check.

Rejected experiment:

- unordered_map-based OrderManager lookup index.


Remaining performance observations:

- OrderManager::add_order() still performs linear duplicate-ID
  detection.
- OrderManager::cancel_order() still performs linear lookup and
  vector erase.
- Actual matching still scans active orders in find_best_match().
- Market execution over active local orders still scans for the
  best resting order.
- Strong exception guarantees still require state copying on
  several mutation paths.

These are candidates for deeper matching-engine and storage
architecture work rather than additional small Day 2 changes.
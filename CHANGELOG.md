# Changelog

All notable changes to this project will be documented in this file.

## [0.3.0] - 2026-02-09

### Added

- **Alien/Shadow Query Detection**: Proactive intelligence for suspicious queries
  - `is_new` flag: Detects first-time-seen query fingerprints (potential intrusion)
  - `is_anomalous` flag: Flags latency (3× baseline) or high scan ratios (>100:1)
  - `empty_app_count`: Tracks queries without application_name (suspicious pattern)
  - `scan_ratio`: Rows scanned / rows returned (efficiency metric)
  - `total_rows_returned`: Cumulative rows returned for efficiency analysis
  - New view `pgtrace_alien_queries`: Query `WHERE is_new OR is_anomalous`
- **Extended `pgtrace_query_stats` view**: 13 columns including alien detection flags
- **Baseline latency calculation**: Dynamic baseline for 3× anomaly threshold
- **Upgrade path**: `pgtrace--0.2--0.3.sql` for seamless v0.2 → v0.3 migration

### Technical

- `QueryStats` structure expanded with alien detection fields
- `pgtrace_hash_record()` signature updated: now accepts app_name, rows_scanned, rows_returned
- `pgtrace_hash_get_baseline_latency()`: Computes mean average latency across all queries
- Lock optimization: Baseline computed before exclusive lock to prevent deadlock
- Double precision for scan ratio calculations

## [0.2.0] - 2026-02-08

### Added

- **Per-query fingerprinting**: Track individual query performance with 64-bit hash fingerprints
- **Query normalization**: Strip literals and normalize whitespace for accurate deduplication
- **New view `pgtrace_query_stats`**: Per-query metrics (8 columns)
- **Slow query ring buffer**: Fixed-size circular buffer (1000 entries)
- **Error tracking (experimental)**: Infrastructure for capturing query failures
- **Shared memory hash tables** with concurrent lock-protected access
- **Upgrade path**: `pgtrace--0.1--0.2.sql`
- **New function `pgtrace_query_count()`**: Returns count of tracked unique queries

### Technical

- FNV-1a 64-bit hash algorithm
- Fixed-size circular buffer for slow queries
- All shared memory structures protected by LWLock
- All shared memory structures protected by LWLock

## [0.1.0] - 2026-01-XX

- Global query metrics tracking
- Latency histogram (6 buckets)
- GUCs: `pgtrace.enabled`, `pgtrace.slow_query_ms`

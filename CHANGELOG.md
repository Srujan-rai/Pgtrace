# Changelog

All notable changes to this project will be documented in this file.

## [0.2.0] - 2026-02-08

### Added

- **Per-query fingerprinting**: Track individual query performance with 64-bit hash fingerprints
- **Query normalization**: Strip literals and normalize whitespace for accurate deduplication
- **New view `pgtrace_query_stats`**: Per-query metrics including:
  - `fingerprint`, `calls`, `errors`
  - `total_time_ms`, `avg_time_ms`, `max_time_ms`
  - `first_seen`, `last_seen` timestamps
- **Slow query ring buffer**: Fixed-size circular buffer (1000 entries) capturing worst queries with:
  - `fingerprint`, `duration_ms`, `query_time` (timestamp)
  - `application_name`, `db_user`, `rows_processed`
  - New view `pgtrace_slow_queries` for operational visibility
- **Error tracking (experimental)**: Infrastructure for capturing query failures by SQLSTATE:
  - New view `pgtrace_failing_queries` (fingerprint, error_code, error_count, last_error_at)
  - Error hash table (1000 entries) in shared memory
  - Note: emit_log_hook integration disabled pending debugging
- **Shared memory hash tables**: 
  - Query stats (10k unique queries)
  - Slow queries (1000 ring buffer)
  - Error tracking (1000 entries)
  - All with concurrent lock-protected access
- **Upgrade path**: `pgtrace--0.1--0.2.sql` for in-place upgrades
- **New function `pgtrace_query_count()`**: Returns count of tracked unique queries

### Technical

- New source files:
  - `fingerprint.c/h`: Query normalization + FNV-1a hashing
  - `query_hash.c/h`: Per-query stats hash table
  - `slow_query.c/h`: Slow query ring buffer with context
  - `error_track.c/h`: Error frequency tracking
  - `error_hook.c`: emit_log_hook implementation (disabled)
- FNV-1a 64-bit hash algorithm for fingerprint computation
- Fixed-size circular buffer for slow queries
- All shared memory structures protected by LWLock

## [0.1.0] - 2026-01-XX

- Global query metrics tracking
- Latency histogram (6 buckets)
- GUCs: `pgtrace.enabled`, `pgtrace.slow_query_ms`

# Changelog

All notable changes to this project will be documented in this file.

## [0.2.0] - 2026-02-07

### Added

- **Per-query fingerprinting**: Track individual query performance with 64-bit hash fingerprints
- **Query normalization**: Strip literals and normalize whitespace for accurate deduplication
- **New view `pgtrace_query_stats`**: Per-query metrics including:
  - `fingerprint`, `calls`, `errors`
  - `total_time_ms`, `avg_time_ms`, `max_time_ms`
  - `first_seen`, `last_seen` timestamps
- **Shared memory hash table**: Fixed-size hash table (10k queries) with linear probing
- **Upgrade path**: `pgtrace--0.1--0.2.sql` for in-place upgrades

### Technical

- New source files: `fingerprint.c`, `fingerprint.h`, `query_hash.c`, `query_hash.h`
- FNV-1a hash algorithm for fingerprint computation
- Concurrent lock-protected access to query hash table

## [0.1.0] - Initial Release

- Global query metrics tracking
- Latency histogram (6 buckets)
- GUCs: `pgtrace.enabled`, `pgtrace.slow_query_ms`

## [Unreleased]

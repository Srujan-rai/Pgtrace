Lightweight PostgreSQL extension for query tracing and latency metrics.

### Overview

Pgtrace hooks into the PostgreSQL executor to record aggregate query metrics in shared memory. It exposes views for total/failed/slow queries and a latency histogram. The design is intentionally minimal to keep overhead low.

### Features

- Per-query fingerprinting with detailed stats (calls, errors, latency, timestamps)
- Tracks total, failed, and slow queries (global metrics)
- Latency histogram across 6 buckets
- GUCs for enable/disable and slow-query threshold
- Shared-memory metrics (cross-backend)
- Context propagation (request_id + app/user/database correlation)
- Per-query latency percentiles (p95, p99)
- Structured audit events (optional, bounded buffer)

### Requirements

- PostgreSQL 15+ (tested on PostgreSQL 16)
- `postgresql-server-dev-16` (or matching server dev package)
- Build tools (`make`, `gcc`/`clang`)

### Build & Install

```bash
make
sudo make install
```

### Configure

Pgtrace uses shared memory, so it must be loaded via `shared_preload_libraries`.

```bash
echo "shared_preload_libraries = 'pgtrace'" | sudo tee -a /etc/postgresql/16/main/postgresql.conf
sudo systemctl restart postgresql@16-main
```

### Create the Extension

```bash
sudo -u postgres psql -c "CREATE EXTENSION pgtrace;"
```

### Usage

#### Per-Query Stats

```sql
SELECT * FROM pgtrace_query_stats;
```

Columns:

- `fingerprint` (bigint) - 64-bit hash of normalized query
- `calls` (bigint) - Number of executions
- `errors` (bigint) - Number of failed executions
- `total_time_ms` (double precision) - Total execution time
- `avg_time_ms` (double precision) - Average execution time
- `max_time_ms` (double precision) - Maximum execution time
- `first_seen` (timestamptz) - First execution timestamp
- `last_seen` (timestamptz) - Last execution timestamp
- **`is_new` (boolean)** - First execution (potential intrusion)
- **`is_anomalous` (boolean)** - Latency or scan anomaly detected
- **`empty_app_count` (bigint)** - Times executed without application_name
- **`scan_ratio` (double precision)** - Rows scanned / rows returned (efficiency)
- **`total_rows_returned` (bigint)** - Cumulative rows returned
- **`last_app_name` (text)** - Latest application_name seen for this fingerprint
- **`last_user` (text)** - Latest database user for this fingerprint
- **`last_database` (text)** - Latest database name for this fingerprint
- **`last_request_id` (text)** - Latest request_id set via GUC
- **`p95_ms` (double precision)** - 95th percentile latency per query
- **`p99_ms` (double precision)** - 99th percentile latency per query

#### Context Propagation (Production Grade)

Set a request ID per session or request:

```sql
SET pgtrace.request_id = 'abc123';
```

Pgtrace captures:

- `application_name`
- `user`
- `database`
- optional `request_id`

This enables service-level correlation in production environments.

#### Per-Query Percentiles (Tail Latency)

Query p95/p99 per fingerprint:

```sql
SELECT
  fingerprint,
  ROUND(p95_ms::numeric, 2) AS p95_ms,
  ROUND(p99_ms::numeric, 2) AS p99_ms,
  ROUND(avg_time_ms::numeric, 2) AS avg_ms
FROM pgtrace_query_stats
ORDER BY p99_ms DESC
LIMIT 20;
```

#### Rows Scanned vs Returned (Optimization Gold)

Pgtrace tracks `rows_scanned` vs `rows_returned` to identify optimization opportunities:

```sql
SELECT
  fingerprint,
  calls,
  ROUND(scan_ratio::numeric, 2) as scan_ratio,
  total_rows_returned,
  ROUND(avg_time_ms::numeric, 2) as avg_ms
FROM pgtrace_query_stats
WHERE scan_ratio > 10
ORDER BY scan_ratio DESC
LIMIT 20;
```

High `scan_ratio` indicates:

- **Bad index usage**: Full table scans instead of indexed lookups (ratio > 10)
- **Inefficient filters**: WHERE clause examines many rows before filtering (ratio > 50)
- **Sequential scans**: Table scanned sequentially when index could apply (ratio > 100)

**Typical ratios:**

- `1.0` = Efficient (all rows examined are returned)
- `10.0` = 10 rows examined per row returned (consider adding index)
- `100+` = Full table scan (major optimization target)

#### Alien/Shadow Query Detection

Proactive intelligence for suspicious queries:

```sql
SELECT * FROM pgtrace_alien_queries
WHERE is_new OR is_anomalous
ORDER BY avg_time_ms DESC;
```

Identifies:

- **New fingerprints**: Never-before-seen queries (potential unauthorized access)
- **Anomalous latency**: Queries running 3× slower than baseline
- **Inefficient scans**: Rows scanned > 100× rows returned (full table scans)
- **Missing app context**: Queries with empty `application_name` (suspicious)

This view combines all alien indicators in one place for rapid response.

### Slow Queries

Capture recent worst-performing queries for actionable optimization. Stores:

```sql
SELECT * FROM pgtrace_slow_queries
ORDER BY duration_ms DESC
LIMIT 20;
```

Columns:

- `fingerprint` (bigint) - Query identifier
- `duration_ms` (double precision) - Execution time
- `query_time` (timestamptz) - When query ran
- `application_name` (text) - App that issued query
- `db_user` (text) - Database user
- `rows_processed` (bigint) - Rows returned/affected

### Management Functions

```sql
-- Get count of currently tracked queries
SELECT pgtrace_query_count();

-- Clear all query stats
SELECT pgtrace_reset();
```

### Failing Queries (Error Tracking)

Identify which queries are failing and why. Tracks SQLSTATE codes for every error:

```sql
SELECT * FROM pgtrace_failing_queries
ORDER BY error_count DESC
LIMIT 20;
```

Columns:

- `fingerprint` (bigint) - Query identifier
- `error_code` (text) - SQLSTATE (e.g., "23505" for unique violation)
- `error_count` (bigint) - Number of failures
- `last_error_at` (timestamptz) - Last failure timestamp

Directly answers: "Which query keeps breaking and why?"

### Structured Audit Events (v0.3+)

For compliance or high-control environments, Pgtrace stores structured audit events in a bounded buffer:

```sql
SELECT * FROM pgtrace_audit_events
ORDER BY event_timestamp DESC
LIMIT 50;
```

Columns:

- `fingerprint` (bigint) - Query identifier
- `operation` (text) - SELECT/INSERT/UPDATE/DELETE/DDL/UNKNOWN
- `db_user` (text) - Database user executing query
- `database` (text) - Database name
- `rows_affected` (bigint) - Rows affected by operation
- `duration_ms` (double precision) - Execution time
- `event_timestamp` (timestamptz) - When event occurred

### Core Metrics

```sql
SELECT * FROM pgtrace_metrics;
```

Columns:

- `queries_total` (bigint)
- `queries_failed` (bigint)
- `slow_queries` (bigint)

#### Latency Histogram

```sql
SELECT * FROM pgtrace_latency_histogram;
```

Columns:

- `bucket_upper_ms` (int, NULL means >500ms)
- `count` (bigint)

### Configuration (GUCs)

```sql
SHOW pgtrace.enabled;
SHOW pgtrace.slow_query_ms;
SHOW pgtrace.request_id;
```

Defaults:

- `pgtrace.enabled = on`
- `pgtrace.slow_query_ms = 200`
- `pgtrace.request_id = NULL`

### Troubleshooting

- **Server fails to start after enabling**: ensure the extension requests shared memory via `shmem_request_hook` (required in PostgreSQL 15+).
- **MODULE_PATHNAME errors**: ensure `module_pathname = '$libdir/pgtrace'` is set in `pgtrace.control` and the extension is reinstalled.
- **Missing views**: recreate the extension after SQL changes.

### Development

```bash
make clean
make
sudo make install
sudo systemctl restart postgresql@16-main
```

### Version History

**v0.3** (Current)

- Alien/Shadow Query Detection (is_new, is_anomalous flags)
- Context Propagation (last_app_name, last_user, last_database, last_request_id)
- Per-query percentiles (p95_ms, p99_ms)
- Structured audit events with operation tracking
- Scan ratio efficiency metrics
- All SRF memory context issues resolved

**v0.2**

- Basic query fingerprinting and metrics
- Latency histogram
- Error tracking with SQLSTATE codes
- Slow query buffer

### Project Status

Stable and production-ready with comprehensive query tracking. All v0.3 features verified and tested. Contributions welcome for additional features (Prometheus output, per-user/db breakdowns, real-time alerting).

### Project Files

This repository includes the following project files:

- **License**: `LICENSE` (MIT)
- **Contribution guide**: `CONTRIBUTING.md`
- **Code of Conduct**: `CODE_OF_CONDUCT.md`
- **Security policy**: `SECURITY.md`
- **Changelog**: `CHANGELOG.md`

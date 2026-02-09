# README

Lightweight PostgreSQL extension for query tracing and latency metrics.

### Overview

Pgtrace hooks into the PostgreSQL executor to record aggregate query metrics in shared memory. It exposes views for total/failed/slow queries and a latency histogram. The design is intentionally minimal to keep overhead low.

### Features

- Per-query fingerprinting with detailed stats (calls, errors, latency, timestamps)
- Tracks total, failed, and slow queries (global metrics)
- Latency histogram across 6 buckets
- GUCs for enable/disable and slow-query threshold
- Shared-memory metrics (cross-backend)

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
```

Defaults:

- `pgtrace.enabled = on`
- `pgtrace.slow_query_ms = 200`

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

### Project Status

Stable and functional for basic aggregate metrics. Contributions welcome for additional features (reset function, Prometheus output, per-user/db breakdowns).

### Project Files

This repository includes the following project files:

- **License**: `LICENSE` (MIT)
- **Contribution guide**: `CONTRIBUTING.md`
- **Code of Conduct**: `CODE_OF_CONDUCT.md`
- **Security policy**: `SECURITY.md`
- **Changelog**: `CHANGELOG.md`

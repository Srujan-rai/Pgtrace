/* PgTrace v0.3 - Alien/Shadow Query Detection, Context Propagation, Percentiles */

/* Global metrics view */
CREATE FUNCTION pgtrace_internal_metrics()
RETURNS TABLE (queries_total bigint, queries_failed bigint, slow_queries bigint)
AS 'MODULE_PATHNAME', 'pgtrace_internal_metrics'
LANGUAGE C STRICT;

CREATE VIEW pgtrace_metrics AS SELECT * FROM pgtrace_internal_metrics();

CREATE FUNCTION pgtrace_internal_latency()
RETURNS TABLE (
  bucket text,
  queries bigint
)
AS 'MODULE_PATHNAME', 'pgtrace_internal_latency'
LANGUAGE C STRICT;

CREATE VIEW pgtrace_latency_histogram AS
SELECT * FROM pgtrace_internal_latency()
ORDER BY
  CASE bucket
    WHEN '0-1ms' THEN 1
    WHEN '1-10ms' THEN 2
    WHEN '10-100ms' THEN 3
    WHEN '100-1000ms' THEN 4
    WHEN '1000-10000ms' THEN 5
    ELSE 6
  END;

/* Per-query stats view with alien detection, context propagation, and percentiles */

CREATE FUNCTION pgtrace_internal_query_stats()
RETURNS TABLE (
  fingerprint bigint,
  calls bigint,
  errors bigint,
  total_time_ms double precision,
  avg_time_ms double precision,
  max_time_ms double precision,
  first_seen timestamptz,
  last_seen timestamptz,
  is_new boolean,
  is_anomalous boolean,
  empty_app_count bigint,
  scan_ratio double precision,
  total_rows_returned bigint,
  last_app_name text,
  last_user text,
  last_database text,
  last_request_id text,
  p95_ms double precision,
  p99_ms double precision
)
AS 'MODULE_PATHNAME', 'pgtrace_internal_query_stats'
LANGUAGE C STRICT;

CREATE VIEW pgtrace_query_stats AS
SELECT * FROM pgtrace_internal_query_stats()
ORDER BY total_time_ms DESC;

/* Alien/Shadow Query Detection View */
CREATE VIEW pgtrace_alien_queries AS
SELECT 
  fingerprint,
  calls,
  avg_time_ms,
  max_time_ms,
  is_new,
  is_anomalous,
  empty_app_count,
  scan_ratio,
  total_rows_returned,
  last_app_name,
  last_user,
  last_database,
  last_request_id,
  p95_ms,
  p99_ms,
  first_seen,
  last_seen
FROM pgtrace_internal_query_stats()
WHERE is_new OR is_anomalous
ORDER BY 
  is_new DESC,
  is_anomalous DESC,
  avg_time_ms DESC;

/* Slow query view */

CREATE FUNCTION pgtrace_internal_slow_queries()
RETURNS TABLE (
  fingerprint bigint,
  duration_ms double precision,
  query_time timestamptz,
  application_name text,
  db_user text,
  rows_processed bigint
)
AS 'MODULE_PATHNAME', 'pgtrace_internal_slow_queries'
LANGUAGE C STRICT;

CREATE VIEW pgtrace_slow_queries AS SELECT * FROM pgtrace_internal_slow_queries();

CREATE FUNCTION pgtrace_reset()
RETURNS void
AS 'MODULE_PATHNAME', 'pgtrace_reset'
LANGUAGE C STRICT;

CREATE FUNCTION pgtrace_query_count()
RETURNS bigint
AS 'MODULE_PATHNAME', 'pgtrace_query_count'
LANGUAGE C STRICT;

/* Error tracking view */

CREATE FUNCTION pgtrace_internal_failing_queries()
RETURNS TABLE (
  fingerprint bigint,
  error_code text,
  error_count bigint,
  last_error_at timestamptz
)
AS 'MODULE_PATHNAME', 'pgtrace_internal_failing_queries'
LANGUAGE C STRICT;

CREATE VIEW pgtrace_failing_queries AS SELECT * FROM pgtrace_internal_failing_queries();

/* Structured Audit Events (V2.5 - Optional) */

CREATE FUNCTION pgtrace_internal_audit_events()
RETURNS TABLE (
  fingerprint bigint,
  operation text,
  db_user text,
  database text,
  rows_affected bigint,
  duration_ms double precision,
  event_timestamp timestamptz
)
AS 'MODULE_PATHNAME', 'pgtrace_internal_audit_events'
LANGUAGE C STRICT;

CREATE VIEW pgtrace_audit_events AS SELECT * FROM pgtrace_internal_audit_events()
ORDER BY event_timestamp DESC;

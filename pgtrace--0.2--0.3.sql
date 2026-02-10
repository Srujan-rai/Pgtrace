/* Upgrade from v0.2 to v0.3 - Alien/Shadow Query Detection */

/* Drop and recreate pgtrace_internal_query_stats with new columns */
DROP VIEW IF EXISTS pgtrace_query_stats CASCADE;
DROP FUNCTION IF EXISTS pgtrace_internal_query_stats();

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

/* New: Alien/Shadow Query Detection View */
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

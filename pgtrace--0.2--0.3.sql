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
  total_rows_returned bigint
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
  first_seen,
  last_seen
FROM pgtrace_internal_query_stats()
WHERE is_new OR is_anomalous
ORDER BY 
  is_new DESC,
  is_anomalous DESC,
  avg_time_ms DESC;

-- V2: Per-query fingerprinting and stats

CREATE FUNCTION pgtrace_internal_query_stats()
RETURNS TABLE (
  fingerprint bigint,
  calls bigint,
  errors bigint,
  total_time_ms double precision,
  avg_time_ms double precision,
  max_time_ms double precision,
  first_seen timestamptz,
  last_seen timestamptz
)
AS 'MODULE_PATHNAME', 'pgtrace_internal_query_stats'
LANGUAGE C STRICT;

CREATE VIEW pgtrace_query_stats AS
SELECT * FROM pgtrace_internal_query_stats()
ORDER BY total_time_ms DESC;

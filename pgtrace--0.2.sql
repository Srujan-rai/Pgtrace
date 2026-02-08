CREATE FUNCTION pgtrace_internal_metrics()
RETURNS TABLE (
  queries_total bigint,
  queries_failed bigint,
  slow_queries bigint
)
AS 'MODULE_PATHNAME', 'pgtrace_internal_metrics'
LANGUAGE C STRICT;

CREATE FUNCTION pgtrace_internal_latency()
RETURNS TABLE (
  bucket_upper_ms int,
  count bigint
)
AS 'MODULE_PATHNAME', 'pgtrace_internal_latency'
LANGUAGE C STRICT;

CREATE VIEW pgtrace_metrics AS
SELECT
  queries_total,
  queries_failed,
  slow_queries
FROM pgtrace_internal_metrics();

CREATE VIEW pgtrace_latency_histogram AS
SELECT * FROM pgtrace_internal_latency();

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

CREATE VIEW pgtrace_slow_queries AS
SELECT * FROM pgtrace_internal_slow_queries()
ORDER BY duration_ms DESC;

/* Management functions */

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

CREATE VIEW pgtrace_failing_queries AS
SELECT * FROM pgtrace_internal_failing_queries()
ORDER BY error_count DESC;

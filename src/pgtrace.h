#pragma once

#include <postgres.h>
#include <fmgr.h>
#include <utils/timestamp.h>
#include <storage/lwlock.h>

#define PGTRACE_BUCKETS 6

typedef struct PgTraceMetrics
{
    uint64 queries_total;
    uint64 queries_failed;
    uint64 slow_queries;
    uint64 latency_buckets[PGTRACE_BUCKETS];
    TimestampTz start_time;
    LWLock lock;
} PgTraceMetrics;

extern PgTraceMetrics *pgtrace_metrics;

/* V2: Per-query tracking */
#include "fingerprint.h"
#include "query_hash.h"
#include "slow_query.h"
#include "error_track.h"
#include "audit.h"

extern bool pgtrace_enabled;
extern int pgtrace_slow_query_ms;
extern char *pgtrace_request_id;

/* init functions */
void pgtrace_init_guc(void);
void pgtrace_shmem_request(void);
void pgtrace_shmem_startup(void);
void pgtrace_init_hooks(void);
void pgtrace_remove_hooks(void);

/* metrics */
void pgtrace_record_query(long duration_ms, bool failed);
PGDLLEXPORT Datum pgtrace_internal_metrics(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pgtrace_internal_latency(PG_FUNCTION_ARGS);

/* V2: per-query stats */
PGDLLEXPORT Datum pgtrace_internal_query_stats(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pgtrace_reset(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pgtrace_query_count(PG_FUNCTION_ARGS);

/* V2: slow queries */
PGDLLEXPORT Datum pgtrace_internal_slow_queries(PG_FUNCTION_ARGS);

/* V2: error tracking */
void pgtrace_set_current_fingerprint(uint64 fingerprint);
void pgtrace_init_error_hook(void);
void pgtrace_remove_error_hook(void);
PGDLLEXPORT Datum pgtrace_internal_failing_queries(PG_FUNCTION_ARGS);

/* V2.5: audit events */
PGDLLEXPORT Datum pgtrace_internal_audit_events(PG_FUNCTION_ARGS);

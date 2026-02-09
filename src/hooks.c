#include <postgres.h>
#include <executor/executor.h>
#include <utils/timestamp.h>
#include <tcop/utility.h>
#include <miscadmin.h>
#include <catalog/pg_authid.h>
#include "pgtrace.h"

static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;

static TimestampTz query_start_time;
static uint64 current_fingerprint = 0;

static void
pgtrace_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
    query_start_time = GetCurrentTimestamp();

    /* V2: Compute fingerprint from query source text */
    if (queryDesc->sourceText)
        current_fingerprint = pgtrace_compute_fingerprint(queryDesc->sourceText);
    else
        current_fingerprint = 0;

    /* Set fingerprint for error tracking */
    pgtrace_set_current_fingerprint(current_fingerprint);

    if (prev_ExecutorStart)
        prev_ExecutorStart(queryDesc, eflags);
    else
        standard_ExecutorStart(queryDesc, eflags);
}

static void
pgtrace_ExecutorEnd(QueryDesc *queryDesc)
{
    TimestampTz end;
    long secs;
    int usecs;
    long ms;
    const char *app_name;
    const char *user_name;
    int64 rows_returned;
    int64 rows_scanned;

    end = GetCurrentTimestamp();

    TimestampDifference(query_start_time, end, &secs, &usecs);
    ms = secs * 1000 + usecs / 1000;

    /* V1: Global metrics */
    pgtrace_record_query(ms, false);

    /* V2: Per-query tracking with alien detection */
    if (current_fingerprint != 0)
    {
        app_name = application_name ? application_name : "";
        user_name = GetUserNameFromId(GetUserId(), false);
        rows_returned = (queryDesc->estate && queryDesc->estate->es_processed) ? queryDesc->estate->es_processed : 0;

        /* Estimate rows scanned (use rows_returned as proxy if no better info available) */
        rows_scanned = rows_returned;
        if (queryDesc->totaltime)
        {
            /* If we have timing info, we might have better scan estimates */
            rows_scanned = rows_returned; /* Simplified: would need executor state for actual scans */
        }

        pgtrace_hash_record(current_fingerprint, (double)ms, false,
                            app_name, rows_scanned, rows_returned);

        /* V2: Record slow queries */
        if (ms > pgtrace_slow_query_ms)
        {
            pgtrace_slow_query_record(current_fingerprint, (double)ms,
                                      app_name, user_name, rows_returned);
        }
    }

    if (prev_ExecutorEnd)
        prev_ExecutorEnd(queryDesc);
    else
        standard_ExecutorEnd(queryDesc);
}

void pgtrace_init_hooks(void)
{
    prev_ExecutorStart = ExecutorStart_hook;
    ExecutorStart_hook = pgtrace_ExecutorStart;

    prev_ExecutorEnd = ExecutorEnd_hook;
    ExecutorEnd_hook = pgtrace_ExecutorEnd;
}

void pgtrace_remove_hooks(void)
{
    ExecutorStart_hook = prev_ExecutorStart;
    ExecutorEnd_hook = prev_ExecutorEnd;
}

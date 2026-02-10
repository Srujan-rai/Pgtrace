#include <postgres.h>
#include <executor/executor.h>
#include <utils/timestamp.h>
#include <tcop/utility.h>
#include <miscadmin.h>
#include <catalog/pg_authid.h>
#include <commands/dbcommands.h>
#include <nodes/parsenodes.h>
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
    const char *db_name;
    const char *req_id;
    int64 rows_returned;
    int64 rows_scanned;
    PlanState *plan_state;

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
        db_name = get_database_name(MyDatabaseId);
        req_id = pgtrace_request_id ? pgtrace_request_id : "";
        rows_returned = (queryDesc->estate && queryDesc->estate->es_processed) ? queryDesc->estate->es_processed : 0;

        /*
         * Track rows_scanned vs rows_returned (Optimization Gold)
         * Extract tuple count from executor state for accurate scan accounting.
         * This identifies:
         * - Bad index usage (high ratio = full table scan instead of index)
         * - Inefficient filters (high ratio = many rows examined before filter)
         * - Sequential scans that could use indexes
         */
        rows_scanned = rows_returned;
        if (queryDesc->estate && queryDesc->planstate)
        {
            plan_state = queryDesc->planstate;

            /*
             * Use instrumentation data if available.
             * Instrumentation tracks actual rows examined at each plan node.
             */
            if (plan_state->instrument)
            {
                /* Use actual tuple count from instrumentation */
                rows_scanned = plan_state->instrument->tuplecount;
            }
            else
            {
                /* Fallback: use rows processed (conservative estimate) */
                rows_scanned = rows_returned;
            }
        }

        pgtrace_hash_record(current_fingerprint, (double)ms, false,
                            app_name, user_name, db_name, req_id,
                            rows_scanned, rows_returned);

        /* V2: Record slow queries */
        if (ms > pgtrace_slow_query_ms)
        {
            pgtrace_slow_query_record(current_fingerprint, (double)ms,
                                      app_name, user_name, rows_returned);
        }

        /* V2.5: Record audit event if enabled */
        if (pgtrace_enabled)
        {
            AuditOpType op_type = AUDIT_UNKNOWN;

            switch (queryDesc->operation)
            {
            case CMD_SELECT:
                op_type = AUDIT_SELECT;
                break;
            case CMD_INSERT:
                op_type = AUDIT_INSERT;
                break;
            case CMD_UPDATE:
                op_type = AUDIT_UPDATE;
                break;
            case CMD_DELETE:
                op_type = AUDIT_DELETE;
                break;
            default:
                op_type = AUDIT_UNKNOWN;
                break;
            }

            pgtrace_audit_record(current_fingerprint, op_type,
                                 user_name, db_name, rows_returned, (double)ms);
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

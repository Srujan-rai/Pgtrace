#include <postgres.h>
#include <executor/executor.h>
#include <utils/timestamp.h>
#include "pgtrace.h"

static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;

static TimestampTz query_start_time;

static void
pgtrace_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
    query_start_time = GetCurrentTimestamp();

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

    end = GetCurrentTimestamp();

    TimestampDifference(query_start_time, end, &secs, &usecs);
    ms = secs * 1000 + usecs / 1000;

    pgtrace_record_query(ms, false);

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

#include <postgres.h>
#include "pgtrace.h"
#include "storage/shmem.h"

PgTraceMetrics *pgtrace_metrics = NULL;

void pgtrace_shmem_request(void)
{
    RequestAddinShmemSpace(sizeof(PgTraceMetrics));
    RequestNamedLWLockTranche("pgtrace", 1);

    /* V2: Request space for query hash table */
    pgtrace_hash_request_shmem();

    /* V2: Request space for slow query ring buffer */
    pgtrace_slow_query_request_shmem();

    /* V2: Request space for error tracking buffer */
    pgtrace_error_request_shmem();
}

void pgtrace_shmem_startup(void)
{
    bool found;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    pgtrace_metrics = ShmemInitStruct(
        "pgtrace_metrics",
        sizeof(PgTraceMetrics),
        &found);

    if (!found)
    {
        memset(pgtrace_metrics, 0, sizeof(PgTraceMetrics));
        pgtrace_metrics->start_time = GetCurrentTimestamp();
    }

    LWLockRelease(AddinShmemInitLock);

    /* V2: Initialize query hash table */
    pgtrace_hash_startup();

    /* V2: Initialize slow query buffer */
    pgtrace_slow_query_startup();

    /* V2: Initialize error tracking buffer */
    pgtrace_error_startup();
}

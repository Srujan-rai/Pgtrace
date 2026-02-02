#include <postgres.h>
#include "pgtrace.h"
#include "storage/shmem.h"

PgTraceMetrics *pgtrace_metrics = NULL;

void pgtrace_shmem_request(void)
{
    RequestAddinShmemSpace(sizeof(PgTraceMetrics));
    RequestNamedLWLockTranche("pgtrace", 1);
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
}

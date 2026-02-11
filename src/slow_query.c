#include <postgres.h>
#include <storage/shmem.h>
#include <storage/lwlock.h>
#include <utils/timestamp.h>
#include <miscadmin.h>
#include "slow_query.h"

SlowQueryRingBuffer *pgtrace_slow_query_buffer = NULL;

void pgtrace_slow_query_request_shmem(void)
{
    RequestAddinShmemSpace(sizeof(SlowQueryRingBuffer));
    RequestNamedLWLockTranche("pgtrace_slow_query", 1);
}

void pgtrace_slow_query_startup(void)
{
    bool found;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    pgtrace_slow_query_buffer = ShmemInitStruct(
        "pgtrace_slow_query_buffer",
        sizeof(SlowQueryRingBuffer),
        &found);

    if (!found)
    {
        memset(pgtrace_slow_query_buffer, 0, sizeof(SlowQueryRingBuffer));
    }

    LWLockRelease(AddinShmemInitLock);
}

void pgtrace_slow_query_record(uint64 fingerprint, double duration_ms,
                               const char *app_name, const char *user,
                               int64 rows_processed)
{
    SlowQueryEntry *entry;
    LWLockPadded *lock;

    if (!pgtrace_slow_query_buffer)
        return;

    lock = GetNamedLWLockTranche("pgtrace_slow_query");
    LWLockAcquire(&lock->lock, LW_EXCLUSIVE);

    entry = &pgtrace_slow_query_buffer->entries[pgtrace_slow_query_buffer->write_pos];

    entry->fingerprint = fingerprint;
    entry->duration_ms = duration_ms;
    entry->timestamp = GetCurrentTimestamp();
    entry->rows_processed = rows_processed;
    entry->valid = true;

    if (app_name)
        snprintf(entry->application_name, sizeof(entry->application_name), "%s", app_name);
    else
        entry->application_name[0] = '\0';

    if (user)
        snprintf(entry->user, sizeof(entry->user), "%s", user);
    else
        entry->user[0] = '\0';

    pgtrace_slow_query_buffer->total_slow_queries++;

    pgtrace_slow_query_buffer->write_pos =
        (pgtrace_slow_query_buffer->write_pos + 1) % PGTRACE_SLOW_QUERY_BUFFER_SIZE;

    LWLockRelease(&lock->lock);
}

uint32
pgtrace_slow_query_count(void)
{
    uint32 count = 0;
    uint32 i;
    LWLockPadded *lock;

    if (!pgtrace_slow_query_buffer)
        return 0;

    lock = GetNamedLWLockTranche("pgtrace_slow_query");
    LWLockAcquire(&lock->lock, LW_SHARED);

    for (i = 0; i < PGTRACE_SLOW_QUERY_BUFFER_SIZE; i++)
    {
        if (pgtrace_slow_query_buffer->entries[i].valid)
            count++;
    }

    LWLockRelease(&lock->lock);

    return count;
}

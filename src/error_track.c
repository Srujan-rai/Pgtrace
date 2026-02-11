#include <postgres.h>
#include <storage/shmem.h>
#include <storage/lwlock.h>
#include <utils/timestamp.h>
#include "error_track.h"

ErrorTrackBuffer *pgtrace_error_buffer = NULL;

void pgtrace_error_request_shmem(void)
{
    RequestAddinShmemSpace(sizeof(ErrorTrackBuffer));
    RequestNamedLWLockTranche("pgtrace_error_track", 1);
}

void pgtrace_error_startup(void)
{
    bool found;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    pgtrace_error_buffer = ShmemInitStruct(
        "pgtrace_error_buffer",
        sizeof(ErrorTrackBuffer),
        &found);

    if (!found)
    {
        memset(pgtrace_error_buffer, 0, sizeof(ErrorTrackBuffer));
    }

    LWLockRelease(AddinShmemInitLock);
}

static ErrorTrackEntry *
find_or_create_error_entry(uint64 fingerprint, uint32 sqlstate)
{
    uint32 i;

    for (i = 0; i < pgtrace_error_buffer->num_entries; i++)
    {
        ErrorTrackEntry *entry = &pgtrace_error_buffer->entries[i];
        if (entry->fingerprint == fingerprint && entry->sqlstate == sqlstate)
            return entry;
    }

    if (pgtrace_error_buffer->num_entries < PGTRACE_ERROR_BUFFER_SIZE)
    {
        ErrorTrackEntry *entry = &pgtrace_error_buffer->entries[pgtrace_error_buffer->num_entries];
        entry->fingerprint = fingerprint;
        entry->sqlstate = sqlstate;
        entry->error_count = 0;
        entry->valid = true;
        pgtrace_error_buffer->num_entries++;
        return entry;
    }

    return NULL;
}

void pgtrace_error_record(uint64 fingerprint, uint32 sqlstate)
{
    ErrorTrackEntry *entry;
    LWLockPadded *lock;

    if (!pgtrace_error_buffer || fingerprint == 0 || sqlstate == 0)
        return;

    lock = GetNamedLWLockTranche("pgtrace_error_track");
    LWLockAcquire(&lock->lock, LW_EXCLUSIVE);

    entry = find_or_create_error_entry(fingerprint, sqlstate);
    if (entry)
    {
        entry->error_count++;
        entry->last_error_at = GetCurrentTimestamp();
    }

    LWLockRelease(&lock->lock);
}

uint32
pgtrace_error_count(void)
{
    uint32 count;
    LWLockPadded *lock;

    if (!pgtrace_error_buffer)
        return 0;

    lock = GetNamedLWLockTranche("pgtrace_error_track");
    LWLockAcquire(&lock->lock, LW_SHARED);
    count = pgtrace_error_buffer->num_entries;
    LWLockRelease(&lock->lock);

    return count;
}

#include <postgres.h>
#include <storage/shmem.h>
#include <storage/lwlock.h>
#include <utils/timestamp.h>
#include <miscadmin.h>
#include "audit.h"

AuditEventBuffer *pgtrace_audit_buffer = NULL;

/*
 * Request shared memory for audit event buffer.
 */
void pgtrace_audit_request_shmem(void)
{
    RequestAddinShmemSpace(sizeof(AuditEventBuffer));
    RequestNamedLWLockTranche("pgtrace_audit", 1);
}

/*
 * Initialize audit event buffer in shared memory.
 */
void pgtrace_audit_startup(void)
{
    bool found;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    pgtrace_audit_buffer = ShmemInitStruct(
        "pgtrace_audit_buffer",
        sizeof(AuditEventBuffer),
        &found);

    if (!found)
    {
        memset(pgtrace_audit_buffer, 0, sizeof(AuditEventBuffer));
    }

    LWLockRelease(AddinShmemInitLock);
}

/*
 * Record an audit event in the circular buffer.
 * Overwrites oldest entries as buffer fills.
 */
void pgtrace_audit_record(uint64 fingerprint, AuditOpType op_type,
                          const char *user, const char *database,
                          int64 rows_affected, double duration_ms)
{
    AuditEvent *entry;
    LWLockPadded *lock;

    if (!pgtrace_audit_buffer)
        return;

    lock = GetNamedLWLockTranche("pgtrace_audit");
    LWLockAcquire(&lock->lock, LW_EXCLUSIVE);

    entry = &pgtrace_audit_buffer->entries[pgtrace_audit_buffer->write_pos];

    entry->fingerprint = fingerprint;
    entry->op_type = op_type;
    entry->rows_affected = rows_affected;
    entry->duration_ms = duration_ms;
    entry->timestamp = GetCurrentTimestamp();
    entry->valid = true;

    /* Safely copy user and database */
    if (user)
        snprintf(entry->user, sizeof(entry->user), "%s", user);
    else
        entry->user[0] = '\0';

    if (database)
        snprintf(entry->database, sizeof(entry->database), "%s", database);
    else
        entry->database[0] = '\0';

    pgtrace_audit_buffer->total_events++;

    /* Advance ring buffer position */
    pgtrace_audit_buffer->write_pos =
        (pgtrace_audit_buffer->write_pos + 1) % PGTRACE_AUDIT_BUFFER_SIZE;

    LWLockRelease(&lock->lock);
}

/*
 * Get count of audit events in buffer.
 */
uint32
pgtrace_audit_count(void)
{
    uint32 count = 0;
    uint32 i;
    LWLockPadded *lock;

    if (!pgtrace_audit_buffer)
        return 0;

    lock = GetNamedLWLockTranche("pgtrace_audit");
    LWLockAcquire(&lock->lock, LW_SHARED);

    for (i = 0; i < PGTRACE_AUDIT_BUFFER_SIZE; i++)
    {
        if (pgtrace_audit_buffer->entries[i].valid)
            count++;
    }

    LWLockRelease(&lock->lock);

    return count;
}

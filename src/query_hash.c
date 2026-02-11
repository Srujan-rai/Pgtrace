#include <postgres.h>
#include <storage/shmem.h>
#include <storage/lwlock.h>
#include <utils/timestamp.h>
#include "query_hash.h"

PgTraceQueryHash *pgtrace_query_hash = NULL;

void pgtrace_hash_request_shmem(void)
{
    RequestAddinShmemSpace(sizeof(PgTraceQueryHash));
    RequestNamedLWLockTranche("pgtrace_query_hash", 1);
}

void pgtrace_hash_startup(void)
{
    bool found;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    pgtrace_query_hash = ShmemInitStruct(
        "pgtrace_query_hash",
        sizeof(PgTraceQueryHash),
        &found);

    if (!found)
    {
        memset(pgtrace_query_hash, 0, sizeof(PgTraceQueryHash));
    }

    LWLockRelease(AddinShmemInitLock);
}

static inline uint64
hash_bucket(uint64 fingerprint)
{
    return fingerprint % PGTRACE_HASH_TABLE_SIZE;
}

static QueryStats *
find_entry(uint64 fingerprint)
{
    uint64 bucket = hash_bucket(fingerprint);
    uint64 i;

    for (i = 0; i < PGTRACE_HASH_TABLE_SIZE; i++)
    {
        uint64 idx = (bucket + i) % PGTRACE_HASH_TABLE_SIZE;
        QueryStats *entry = &pgtrace_query_hash->entries[idx];

        if (!entry->valid)
            return NULL;

        if (entry->fingerprint == fingerprint)
            return entry;
    }

    return NULL;
}

static QueryStats *
find_or_create_entry(uint64 fingerprint)
{
    uint64 bucket = hash_bucket(fingerprint);
    uint64 i;

    for (i = 0; i < PGTRACE_HASH_TABLE_SIZE; i++)
    {
        uint64 idx = (bucket + i) % PGTRACE_HASH_TABLE_SIZE;
        QueryStats *entry = &pgtrace_query_hash->entries[idx];

        if (!entry->valid)
        {
            memset(entry, 0, sizeof(QueryStats));
            entry->fingerprint = fingerprint;
            entry->valid = true;
            entry->first_seen = GetCurrentTimestamp();
            entry->last_seen = entry->first_seen;
            pgtrace_query_hash->num_entries++;

            if (i > 0)
                pgtrace_query_hash->collisions++;

            return entry;
        }

        if (entry->fingerprint == fingerprint)
            return entry;
    }

    return NULL;
}

void pgtrace_hash_record(uint64 fingerprint, double duration_ms, bool failed,
                         const char *app_name, const char *user_name, const char *db_name,
                         const char *req_id, uint64 rows_scanned, uint64 rows_returned)
{
    QueryStats *entry;
    double baseline_latency;
    bool is_first_call;

    if (!pgtrace_query_hash)
        return;

    baseline_latency = pgtrace_hash_get_baseline_latency();

    LWLockPadded *lock = GetNamedLWLockTranche("pgtrace_query_hash");
    LWLockAcquire(&lock->lock, LW_EXCLUSIVE);

    entry = find_or_create_entry(fingerprint);
    if (entry)
    {
        is_first_call = (entry->calls == 0);

        entry->calls++;
        entry->total_time_ms += duration_ms;
        entry->last_seen = GetCurrentTimestamp();

        if (failed)
            entry->errors++;

        if (duration_ms > entry->max_time_ms)
            entry->max_time_ms = duration_ms;

        entry->is_new = is_first_call;

        if (app_name == NULL || app_name[0] == '\0')
            entry->empty_app_count++;

        entry->total_rows_scanned += rows_scanned;
        entry->total_rows_returned += rows_returned;

        if (app_name)
            snprintf(entry->last_app_name, sizeof(entry->last_app_name), "%s", app_name);
        if (user_name)
            snprintf(entry->last_user, sizeof(entry->last_user), "%s", user_name);
        if (db_name)
            snprintf(entry->last_database, sizeof(entry->last_database), "%s", db_name);
        if (req_id)
            snprintf(entry->last_request_id, sizeof(entry->last_request_id), "%s", req_id);

        entry->latency_samples[entry->sample_pos] = duration_ms;
        entry->sample_pos = (entry->sample_pos + 1) % PGTRACE_LATENCY_BUCKETS;
        if (entry->sample_count < PGTRACE_LATENCY_BUCKETS)
            entry->sample_count++;

        entry->is_anomalous = false;

        if (baseline_latency > 0 && duration_ms > (baseline_latency * 3.0))
            entry->is_anomalous = true;

        if (rows_returned > 0 && ((double)rows_scanned / (double)rows_returned) > 100.0)
            entry->is_anomalous = true;
    }

    LWLockRelease(&lock->lock);
}

QueryStats *
pgtrace_hash_get(uint64 fingerprint)
{
    QueryStats *entry;
    LWLockPadded *lock;

    if (!pgtrace_query_hash)
        return NULL;

    lock = GetNamedLWLockTranche("pgtrace_query_hash");
    LWLockAcquire(&lock->lock, LW_SHARED);
    entry = find_entry(fingerprint);
    LWLockRelease(&lock->lock);

    return entry;
}

uint64
pgtrace_hash_count(void)
{
    uint64 count;
    LWLockPadded *lock;

    if (!pgtrace_query_hash)
        return 0;

    lock = GetNamedLWLockTranche("pgtrace_query_hash");
    LWLockAcquire(&lock->lock, LW_SHARED);
    count = pgtrace_query_hash->num_entries;
    LWLockRelease(&lock->lock);

    return count;
}

void pgtrace_hash_reset(void)
{
    LWLockPadded *lock;

    if (!pgtrace_query_hash)
        return;

    lock = GetNamedLWLockTranche("pgtrace_query_hash");
    LWLockAcquire(&lock->lock, LW_EXCLUSIVE);
    memset(pgtrace_query_hash->entries, 0, sizeof(pgtrace_query_hash->entries));
    pgtrace_query_hash->num_entries = 0;
    pgtrace_query_hash->collisions = 0;
    LWLockRelease(&lock->lock);
}

double pgtrace_hash_get_baseline_latency(void)
{
    double sum = 0.0;
    uint64 count = 0;
    uint64 i;
    LWLockPadded *lock;

    if (!pgtrace_query_hash)
        return 0.0;

    lock = GetNamedLWLockTranche("pgtrace_query_hash");
    LWLockAcquire(&lock->lock, LW_SHARED);

    for (i = 0; i < PGTRACE_HASH_TABLE_SIZE; i++)
    {
        QueryStats *entry = &pgtrace_query_hash->entries[i];
        if (entry->valid && entry->calls > 0)
        {
            sum += (entry->total_time_ms / entry->calls);
            count++;
        }
    }

    LWLockRelease(&lock->lock);

    return (count > 0) ? (sum / count) : 0.0;
}

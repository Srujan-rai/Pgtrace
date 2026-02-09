#include <postgres.h>
#include <storage/shmem.h>
#include <storage/lwlock.h>
#include <utils/timestamp.h>
#include "query_hash.h"

PgTraceQueryHash *pgtrace_query_hash = NULL;

/*
 * Request shared memory for the query hash table.
 */
void pgtrace_hash_request_shmem(void)
{
    RequestAddinShmemSpace(sizeof(PgTraceQueryHash));
    RequestNamedLWLockTranche("pgtrace_query_hash", 1);
}

/*
 * Initialize hash table in shared memory.
 */
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

/*
 * Hash function: simple modulo for fingerprint -> bucket.
 */
static inline uint64
hash_bucket(uint64 fingerprint)
{
    return fingerprint % PGTRACE_HASH_TABLE_SIZE;
}

/*
 * Find entry in hash table or return NULL.
 * Caller must hold lock.
 */
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

/*
 * Find or create entry in hash table.
 * Caller must hold exclusive lock.
 */
static QueryStats *
find_or_create_entry(uint64 fingerprint)
{
    uint64 bucket = hash_bucket(fingerprint);
    uint64 i;

    /* First pass: look for existing entry */
    for (i = 0; i < PGTRACE_HASH_TABLE_SIZE; i++)
    {
        uint64 idx = (bucket + i) % PGTRACE_HASH_TABLE_SIZE;
        QueryStats *entry = &pgtrace_query_hash->entries[idx];

        if (!entry->valid)
        {
            /* Found empty slot: initialize new entry */
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

    /* Hash table full: return NULL */
    return NULL;
}

/*
 * Record query execution in hash table with alien detection.
 */
void pgtrace_hash_record(uint64 fingerprint, double duration_ms, bool failed,
                         const char *app_name, uint64 rows_scanned, uint64 rows_returned)
{
    QueryStats *entry;
    double baseline_latency;
    bool is_first_call;

    if (!pgtrace_query_hash)
        return;

    /* Compute baseline before taking exclusive lock to avoid lock recursion */
    baseline_latency = pgtrace_hash_get_baseline_latency();

    LWLockAcquire(&pgtrace_query_hash->lock, LW_EXCLUSIVE);

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

        /* Track alien query indicators */
        entry->is_new = is_first_call;

        if (app_name == NULL || app_name[0] == '\0')
            entry->empty_app_count++;

        entry->total_rows_scanned += rows_scanned;
        entry->total_rows_returned += rows_returned;

        /* Detect anomalous behavior */
        entry->is_anomalous = false;

        /* Check: latency 3× baseline */
        if (baseline_latency > 0 && duration_ms > (baseline_latency * 3.0))
            entry->is_anomalous = true;

        /* Check: scan ratio threshold (rows_scanned / rows_returned > 100) */
        if (rows_returned > 0 && ((double)rows_scanned / (double)rows_returned) > 100.0)
            entry->is_anomalous = true;
    }

    LWLockRelease(&pgtrace_query_hash->lock);
}

/*
 * Get query stats by fingerprint (caller must copy under lock).
 */
QueryStats *
pgtrace_hash_get(uint64 fingerprint)
{
    QueryStats *entry;

    if (!pgtrace_query_hash)
        return NULL;

    LWLockAcquire(&pgtrace_query_hash->lock, LW_SHARED);
    entry = find_entry(fingerprint);
    LWLockRelease(&pgtrace_query_hash->lock);

    return entry;
}

/*
 * Get total number of tracked queries.
 */
uint64
pgtrace_hash_count(void)
{
    uint64 count;

    if (!pgtrace_query_hash)
        return 0;

    LWLockAcquire(&pgtrace_query_hash->lock, LW_SHARED);
    count = pgtrace_query_hash->num_entries;
    LWLockRelease(&pgtrace_query_hash->lock);

    return count;
}

/*
 * Reset all query stats in hash table.
 */
void pgtrace_hash_reset(void)
{
    if (!pgtrace_query_hash)
        return;

    LWLockAcquire(&pgtrace_query_hash->lock, LW_EXCLUSIVE);
    memset(pgtrace_query_hash->entries, 0, sizeof(pgtrace_query_hash->entries));
    pgtrace_query_hash->num_entries = 0;
    pgtrace_query_hash->collisions = 0;
    LWLockRelease(&pgtrace_query_hash->lock);
}

/*
 * Calculate baseline latency (median of average latencies across all queries).
 * Used for alien query detection.
 */
double pgtrace_hash_get_baseline_latency(void)
{
    double sum = 0.0;
    uint64 count = 0;
    uint64 i;

    if (!pgtrace_query_hash)
        return 0.0;

    /* Compute mean average latency across all queries (simplified baseline) */
    LWLockAcquire(&pgtrace_query_hash->lock, LW_SHARED);

    for (i = 0; i < PGTRACE_HASH_TABLE_SIZE; i++)
    {
        QueryStats *entry = &pgtrace_query_hash->entries[i];
        if (entry->valid && entry->calls > 0)
        {
            sum += (entry->total_time_ms / entry->calls);
            count++;
        }
    }

    LWLockRelease(&pgtrace_query_hash->lock);

    return (count > 0) ? (sum / count) : 0.0;
}

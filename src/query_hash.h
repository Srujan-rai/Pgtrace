#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

/*
 * Per-query stats entry stored in shared memory.
 * Hash table maps fingerprint -> QueryStats.
 */
typedef struct QueryStats
{
    uint64 fingerprint;     /* 64-bit hash of normalized query */
    uint64 calls;           /* Number of executions */
    uint64 errors;          /* Number of failed executions */
    double total_time_ms;   /* Total execution time in milliseconds */
    double max_time_ms;     /* Maximum execution time */
    TimestampTz first_seen; /* First execution timestamp */
    TimestampTz last_seen;  /* Last execution timestamp */
    bool valid;             /* Entry is in use */
} QueryStats;

/*
 * Shared memory hash table configuration.
 * Fixed-size hash table with open addressing (linear probing).
 */
#define PGTRACE_MAX_QUERIES 10000
#define PGTRACE_HASH_TABLE_SIZE (PGTRACE_MAX_QUERIES * 2) /* 50% load factor */

typedef struct PgTraceQueryHash
{
    QueryStats entries[PGTRACE_HASH_TABLE_SIZE];
    uint64 num_entries; /* Current number of active entries */
    uint64 collisions;  /* Number of hash collisions */
    LWLock lock;        /* Lock for concurrent access */
} PgTraceQueryHash;

extern PgTraceQueryHash *pgtrace_query_hash;

/* Hash table operations */
void pgtrace_hash_init(void);
void pgtrace_hash_request_shmem(void);
void pgtrace_hash_startup(void);
void pgtrace_hash_record(uint64 fingerprint, double duration_ms, bool failed);
QueryStats *pgtrace_hash_get(uint64 fingerprint);
uint64 pgtrace_hash_count(void);

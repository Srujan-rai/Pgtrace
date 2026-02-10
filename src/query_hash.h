#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

#define PGTRACE_REQUEST_ID_LEN 64
#define PGTRACE_LATENCY_BUCKETS 100 /* For p95, p99 calculation per query */

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

    /* Alien/Shadow Query Detection */
    bool is_new;                /* First call (calls == 1) */
    bool is_anomalous;          /* Latency or scan anomaly detected */
    uint64 empty_app_count;     /* Times executed with empty application_name */
    uint64 total_rows_scanned;  /* Cumulative rows examined */
    uint64 total_rows_returned; /* Cumulative rows returned */

    /* Context Propagation (Production Grade) */
    char last_request_id[PGTRACE_REQUEST_ID_LEN]; /* Latest request_id seen */
    char last_app_name[64];                       /* Latest application_name */
    char last_user[32];                           /* Latest database user */
    char last_database[64];                       /* Latest database name */

    /* Per-Query Percentiles (Tail Latency Detection) */
    double latency_samples[PGTRACE_LATENCY_BUCKETS]; /* Ring buffer of samples */
    uint32 sample_pos;                               /* Current write position */
    uint32 sample_count;                             /* Total samples collected */
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
} PgTraceQueryHash;

extern PgTraceQueryHash *pgtrace_query_hash;

/* Hash table operations */
void pgtrace_hash_init(void);
void pgtrace_hash_request_shmem(void);
void pgtrace_hash_startup(void);
void pgtrace_hash_record(uint64 fingerprint, double duration_ms, bool failed,
                         const char *app_name, const char *user_name, const char *db_name,
                         const char *req_id, uint64 rows_scanned, uint64 rows_returned);
QueryStats *pgtrace_hash_get(uint64 fingerprint);
uint64 pgtrace_hash_count(void);
void pgtrace_hash_reset(void);
double pgtrace_hash_get_baseline_latency(void);

#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

#define PGTRACE_REQUEST_ID_LEN 64
#define PGTRACE_LATENCY_BUCKETS 100

typedef struct QueryStats
{
    uint64 fingerprint;
    uint64 calls;
    uint64 errors;
    double total_time_ms;
    double max_time_ms;
    TimestampTz first_seen;
    TimestampTz last_seen;
    bool valid;

    bool is_new;
    bool is_anomalous;
    uint64 empty_app_count;
    uint64 total_rows_scanned;
    uint64 total_rows_returned;

    char last_request_id[PGTRACE_REQUEST_ID_LEN];
    char last_app_name[64];
    char last_user[32];
    char last_database[64];

    double latency_samples[PGTRACE_LATENCY_BUCKETS];
    uint32 sample_pos;
    uint32 sample_count;
} QueryStats;

#define PGTRACE_MAX_QUERIES 10000
#define PGTRACE_HASH_TABLE_SIZE (PGTRACE_MAX_QUERIES * 2)

typedef struct PgTraceQueryHash
{
    QueryStats entries[PGTRACE_HASH_TABLE_SIZE];
    uint64 num_entries;
    uint64 collisions;
} PgTraceQueryHash;

extern PgTraceQueryHash *pgtrace_query_hash;

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

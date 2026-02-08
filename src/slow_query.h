#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

/*
 * Slow query ring buffer entry.
 * Fixed-size buffer to track worst queries with context.
 */
typedef struct SlowQueryEntry
{
    uint64 fingerprint;         /* 64-bit query fingerprint */
    double duration_ms;         /* Execution time in milliseconds */
    TimestampTz timestamp;      /* When query ran */
    char application_name[64];  /* App name (truncated) */
    char user[32];              /* Database user (truncated) */
    int64 rows_processed;       /* Rows returned/affected */
    bool valid;                 /* Entry is in use */
} SlowQueryEntry;

/*
 * Ring buffer configuration.
 * Stores the N slowest queries.
 */
#define PGTRACE_SLOW_QUERY_BUFFER_SIZE 1000

typedef struct SlowQueryRingBuffer
{
    SlowQueryEntry entries[PGTRACE_SLOW_QUERY_BUFFER_SIZE];
    uint32 write_pos;           /* Current write position */
    uint64 total_slow_queries;  /* Total slow queries recorded */
    LWLock lock;                /* Lock for concurrent access */
} SlowQueryRingBuffer;

extern SlowQueryRingBuffer *pgtrace_slow_query_buffer;

/* Ring buffer operations */
void pgtrace_slow_query_request_shmem(void);
void pgtrace_slow_query_startup(void);
void pgtrace_slow_query_record(uint64 fingerprint, double duration_ms,
                               const char *app_name, const char *user,
                               int64 rows_processed);
uint32 pgtrace_slow_query_count(void);

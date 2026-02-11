#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

typedef struct SlowQueryEntry
{
    uint64 fingerprint;
    double duration_ms;
    TimestampTz timestamp;
    char application_name[64];
    char user[32];
    int64 rows_processed;
    bool valid;
} SlowQueryEntry;

#define PGTRACE_SLOW_QUERY_BUFFER_SIZE 1000

typedef struct SlowQueryRingBuffer
{
    SlowQueryEntry entries[PGTRACE_SLOW_QUERY_BUFFER_SIZE];
    uint32 write_pos;
    uint64 total_slow_queries;
} SlowQueryRingBuffer;

extern SlowQueryRingBuffer *pgtrace_slow_query_buffer;

void pgtrace_slow_query_request_shmem(void);
void pgtrace_slow_query_startup(void);
void pgtrace_slow_query_record(uint64 fingerprint, double duration_ms,
                               const char *app_name, const char *user,
                               int64 rows_processed);
uint32 pgtrace_slow_query_count(void);

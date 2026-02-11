#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

typedef struct ErrorTrackEntry
{
    uint64 fingerprint;
    uint32 sqlstate;
    uint64 error_count;
    TimestampTz last_error_at;
    bool valid;
} ErrorTrackEntry;

#define PGTRACE_ERROR_BUFFER_SIZE 1000

typedef struct ErrorTrackBuffer
{
    ErrorTrackEntry entries[PGTRACE_ERROR_BUFFER_SIZE];
    uint32 num_entries;
} ErrorTrackBuffer;

extern ErrorTrackBuffer *pgtrace_error_buffer;

void pgtrace_error_request_shmem(void);
void pgtrace_error_startup(void);
void pgtrace_error_record(uint64 fingerprint, uint32 sqlstate);
uint32 pgtrace_error_count(void);

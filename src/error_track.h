#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

/*
 * Error tracking per query fingerprint.
 * Maps fingerprint -> error code (SQLSTATE) -> frequency.
 */
typedef struct ErrorTrackEntry
{
    uint64 fingerprint;        /* Query fingerprint */
    uint32 sqlstate;           /* SQLSTATE code (e.g., 23505 for unique violation) */
    uint64 error_count;        /* Number of failures */
    TimestampTz last_error_at; /* Last failure timestamp */
    bool valid;                /* Entry is in use */
} ErrorTrackEntry;

/*
 * Error hash table.
 * Tracks per-query/per-error-code failures.
 */
#define PGTRACE_ERROR_BUFFER_SIZE 1000

typedef struct ErrorTrackBuffer
{
    ErrorTrackEntry entries[PGTRACE_ERROR_BUFFER_SIZE];
    uint32 num_entries; /* Current number of active entries */
} ErrorTrackBuffer;

extern ErrorTrackBuffer *pgtrace_error_buffer;

/* Error tracking operations */
void pgtrace_error_request_shmem(void);
void pgtrace_error_startup(void);
void pgtrace_error_record(uint64 fingerprint, uint32 sqlstate);
uint32 pgtrace_error_count(void);

#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

/*
 * Structured Audit Events (Optional V2.5)
 * For compliance or high-control environments.
 * Tracks operation type, user, rows affected, and duration.
 */

typedef enum AuditOpType
{
    AUDIT_SELECT = 0,
    AUDIT_INSERT = 1,
    AUDIT_UPDATE = 2,
    AUDIT_DELETE = 3,
    AUDIT_DDL = 4,
    AUDIT_UNKNOWN = 5
} AuditOpType;

typedef struct AuditEvent
{
    uint64 fingerprint;    /* Query fingerprint for correlation */
    AuditOpType op_type;   /* Operation type */
    char user[32];         /* Database user */
    char database[64];     /* Database name */
    int64 rows_affected;   /* Rows affected by operation */
    double duration_ms;    /* Query duration */
    TimestampTz timestamp; /* When event occurred */
    bool valid;            /* Entry is in use */
} AuditEvent;

/*
 * Audit event buffer configuration.
 * Bounded circular buffer for compliance events.
 */
#define PGTRACE_AUDIT_BUFFER_SIZE 5000

typedef struct AuditEventBuffer
{
    AuditEvent entries[PGTRACE_AUDIT_BUFFER_SIZE];
    uint32 write_pos;    /* Current write position */
    uint64 total_events; /* Total events recorded */
} AuditEventBuffer;

extern AuditEventBuffer *pgtrace_audit_buffer;

/* Audit operations */
void pgtrace_audit_request_shmem(void);
void pgtrace_audit_startup(void);
void pgtrace_audit_record(uint64 fingerprint, AuditOpType op_type,
                          const char *user, const char *database,
                          int64 rows_affected, double duration_ms);
uint32 pgtrace_audit_count(void);

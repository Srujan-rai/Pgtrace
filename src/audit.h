#pragma once

#include <postgres.h>
#include <utils/timestamp.h>

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
    uint64 fingerprint;
    AuditOpType op_type;
    char user[32];
    char database[64];
    int64 rows_affected;
    double duration_ms;
    TimestampTz timestamp;
    bool valid;
} AuditEvent;

#define PGTRACE_AUDIT_BUFFER_SIZE 5000

typedef struct AuditEventBuffer
{
    AuditEvent entries[PGTRACE_AUDIT_BUFFER_SIZE];
    uint32 write_pos;
    uint64 total_events;
} AuditEventBuffer;

extern AuditEventBuffer *pgtrace_audit_buffer;

void pgtrace_audit_request_shmem(void);
void pgtrace_audit_startup(void);
void pgtrace_audit_record(uint64 fingerprint, AuditOpType op_type,
                          const char *user, const char *database,
                          int64 rows_affected, double duration_ms);
uint32 pgtrace_audit_count(void);

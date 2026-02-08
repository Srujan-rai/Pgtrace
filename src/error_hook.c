#include <postgres.h>
#include <miscadmin.h>
#include <utils/elog.h>
#include "pgtrace.h"

static emit_log_hook_type prev_emit_log_hook = NULL;
static uint64 current_query_fingerprint = 0;

/*
 * emit_log_hook for capturing query errors.
 * Called when PostgreSQL logs any message.
 */
static void
pgtrace_emit_log_hook(ErrorData *edata)
{
    /* Chain to previous hook */
    if (prev_emit_log_hook)
        prev_emit_log_hook(edata);

    /* Only track ERROR level messages with valid SQLSTATE */
    if (edata->elevel == ERROR && edata->sqlerrcode != ERRCODE_SUCCESSFUL_COMPLETION)
    {
        /* Map to fingerprint if we're in an executor context */
        if (current_query_fingerprint != 0)
        {
            /* PostgreSQL stores SQLSTATE as a 32-bit code; use it directly */
            uint32 sqlstate = (uint32)edata->sqlerrcode;
            pgtrace_error_record(current_query_fingerprint, sqlstate);
        }
    }
}

/*
 * Set current query fingerprint (called from hooks.c during ExecutorStart).
 */
void pgtrace_set_current_fingerprint(uint64 fingerprint)
{
    current_query_fingerprint = fingerprint;
}

/*
 * Initialize error logging hook.
 */
void pgtrace_init_error_hook(void)
{
    prev_emit_log_hook = emit_log_hook;
    emit_log_hook = pgtrace_emit_log_hook;
}

/*
 * Remove error logging hook on unload.
 */
void pgtrace_remove_error_hook(void)
{
    emit_log_hook = prev_emit_log_hook;
}

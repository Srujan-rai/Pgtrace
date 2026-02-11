#include <postgres.h>
#include <miscadmin.h>
#include <utils/elog.h>
#include "pgtrace.h"

static emit_log_hook_type prev_emit_log_hook = NULL;
static uint64 current_query_fingerprint = 0;

static void
pgtrace_emit_log_hook(ErrorData *edata)
{
    if (prev_emit_log_hook)
        prev_emit_log_hook(edata);

    if (edata->elevel == ERROR && edata->sqlerrcode != ERRCODE_SUCCESSFUL_COMPLETION)
    {
        if (current_query_fingerprint != 0)
        {
            uint32 sqlstate = (uint32)edata->sqlerrcode;
            pgtrace_error_record(current_query_fingerprint, sqlstate);
        }
    }
}

void pgtrace_set_current_fingerprint(uint64 fingerprint)
{
    current_query_fingerprint = fingerprint;
}

void pgtrace_init_error_hook(void)
{
    prev_emit_log_hook = emit_log_hook;
    emit_log_hook = pgtrace_emit_log_hook;
}

void pgtrace_remove_error_hook(void)
{
    emit_log_hook = prev_emit_log_hook;
}

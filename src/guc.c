#include <postgres.h>
#include "pgtrace.h"
#include "utils/guc.h"

bool pgtrace_enabled = true;
int pgtrace_slow_query_ms = 200;
char *pgtrace_request_id = NULL;

void pgtrace_init_guc(void)
{
    DefineCustomBoolVariable(
        "pgtrace.enabled",
        "Enable PgTrace",
        NULL,
        &pgtrace_enabled,
        true,
        PGC_SUSET,
        0,
        NULL, NULL, NULL);

    DefineCustomIntVariable(
        "pgtrace.slow_query_ms",
        "Slow query threshold",
        NULL,
        &pgtrace_slow_query_ms,
        200,
        1,
        60000,
        PGC_SUSET,
        0,
        NULL, NULL, NULL);

    DefineCustomStringVariable(
        "pgtrace.request_id",
        "Context propagation request ID for correlation",
        NULL,
        &pgtrace_request_id,
        NULL,
        PGC_USERSET,
        0,
        NULL, NULL, NULL);
}

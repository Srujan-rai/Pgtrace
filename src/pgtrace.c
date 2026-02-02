#include <postgres.h>
#include <miscadmin.h>
#include <storage/ipc.h>
#include "pgtrace.h"

PG_MODULE_MAGIC;

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

static void
pgtrace_shmem_request_hook(void)
{
    if (prev_shmem_request_hook)
        prev_shmem_request_hook();

    pgtrace_shmem_request();
}

static void
pgtrace_shmem_startup_hook(void)
{
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();

    pgtrace_shmem_startup();
}

void _PG_init(void)
{
    if (!process_shared_preload_libraries_in_progress)
        return;

    pgtrace_init_guc();

    prev_shmem_request_hook = shmem_request_hook;
    shmem_request_hook = pgtrace_shmem_request_hook;

    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = pgtrace_shmem_startup_hook;
    pgtrace_init_hooks();
}

void _PG_fini(void)
{
    pgtrace_remove_hooks();
    shmem_request_hook = prev_shmem_request_hook;
    shmem_startup_hook = prev_shmem_startup_hook;
}

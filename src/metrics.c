#include <postgres.h>
#include <funcapi.h>
#include "pgtrace.h"

static int
bucket_for_latency(long ms)
{
    if (ms <= 5)
        return 0;
    if (ms <= 10)
        return 1;
    if (ms <= 50)
        return 2;
    if (ms <= 100)
        return 3;
    if (ms <= 500)
        return 4;
    return 5;
}

void pgtrace_record_query(long duration_ms, bool failed)
{
    if (!pgtrace_enabled || !pgtrace_metrics)
        return;

    LWLockAcquire(&pgtrace_metrics->lock, LW_EXCLUSIVE);

    pgtrace_metrics->queries_total++;

    if (failed)
        pgtrace_metrics->queries_failed++;

    if (duration_ms > pgtrace_slow_query_ms)
        pgtrace_metrics->slow_queries++;

    pgtrace_metrics->latency_buckets[bucket_for_latency(duration_ms)]++;

    LWLockRelease(&pgtrace_metrics->lock);
}

PG_FUNCTION_INFO_V1(pgtrace_internal_metrics);

PGDLLEXPORT Datum pgtrace_internal_metrics(PG_FUNCTION_ARGS)
{
    TupleDesc tupdesc;
    Datum values[3];
    bool nulls[3] = {false, false, false};
    uint64 queries_total = 0;
    uint64 queries_failed = 0;
    uint64 slow_queries = 0;

    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("pgtrace_internal_metrics must be called in a context that accepts a record")));

    if (pgtrace_metrics)
    {
        LWLockAcquire(&pgtrace_metrics->lock, LW_SHARED);
        queries_total = pgtrace_metrics->queries_total;
        queries_failed = pgtrace_metrics->queries_failed;
        slow_queries = pgtrace_metrics->slow_queries;
        LWLockRelease(&pgtrace_metrics->lock);
    }

    values[0] = UInt64GetDatum(queries_total);
    values[1] = UInt64GetDatum(queries_failed);
    values[2] = UInt64GetDatum(slow_queries);

    PG_RETURN_DATUM(HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}

typedef struct PgTraceLatencyState
{
    uint64 counts[PGTRACE_BUCKETS];
    int32 upper_ms[PGTRACE_BUCKETS];
} PgTraceLatencyState;

PG_FUNCTION_INFO_V1(pgtrace_internal_latency);

PGDLLEXPORT Datum pgtrace_internal_latency(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    PgTraceLatencyState *state;

    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldcontext;
        TupleDesc tupdesc;
        static const int32 upper_ms[PGTRACE_BUCKETS] = {5, 10, 50, 100, 500, -1};

        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("pgtrace_internal_latency must be called in a context that accepts a record")));

        funcctx->tuple_desc = BlessTupleDesc(tupdesc);

        state = palloc0(sizeof(PgTraceLatencyState));
        memcpy(state->upper_ms, upper_ms, sizeof(upper_ms));

        if (pgtrace_metrics)
        {
            LWLockAcquire(&pgtrace_metrics->lock, LW_SHARED);
            memcpy(state->counts, pgtrace_metrics->latency_buckets, sizeof(state->counts));
            LWLockRelease(&pgtrace_metrics->lock);
        }

        funcctx->user_fctx = state;
        funcctx->max_calls = PGTRACE_BUCKETS;

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    state = (PgTraceLatencyState *)funcctx->user_fctx;

    if (funcctx->call_cntr < funcctx->max_calls)
    {
        Datum values[2];
        bool nulls[2] = {false, false};
        int i = funcctx->call_cntr;
        HeapTuple tuple;

        if (state->upper_ms[i] < 0)
            nulls[0] = true;
        else
            values[0] = Int32GetDatum(state->upper_ms[i]);

        values[1] = UInt64GetDatum(state->counts[i]);

        tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    }

    SRF_RETURN_DONE(funcctx);
}

/*
 * V2: pgtrace_internal_query_stats()
 * Returns per-query statistics from the hash table.
 */
PG_FUNCTION_INFO_V1(pgtrace_internal_query_stats);

PGDLLEXPORT Datum pgtrace_internal_query_stats(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    QueryStats *snapshot;
    uint64 *num_entries_ptr;

    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldcontext;
        TupleDesc tupdesc;
        uint64 i, j, count;

        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("pgtrace_internal_query_stats must be called in a context that accepts a record")));

        funcctx->tuple_desc = BlessTupleDesc(tupdesc);

        /* Take snapshot of hash table under lock */
        count = 0;
        snapshot = palloc0(PGTRACE_MAX_QUERIES * sizeof(QueryStats));

        if (pgtrace_query_hash)
        {
            LWLockAcquire(&pgtrace_query_hash->lock, LW_SHARED);

            for (i = 0, j = 0; i < PGTRACE_HASH_TABLE_SIZE && j < PGTRACE_MAX_QUERIES; i++)
            {
                QueryStats *entry = &pgtrace_query_hash->entries[i];
                if (entry->valid)
                {
                    memcpy(&snapshot[j], entry, sizeof(QueryStats));
                    j++;
                    count++;
                }
            }

            LWLockRelease(&pgtrace_query_hash->lock);
        }

        funcctx->user_fctx = snapshot;
        num_entries_ptr = palloc(sizeof(uint64));
        *num_entries_ptr = count;
        funcctx->max_calls = count;
        funcctx->attinmeta = (AttInMetadata *)num_entries_ptr; /* Abuse attinmeta to store count */

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    snapshot = (QueryStats *)funcctx->user_fctx;

    if (funcctx->call_cntr < funcctx->max_calls)
    {
        Datum values[8];
        bool nulls[8] = {false, false, false, false, false, false, false, false};
        HeapTuple tuple;
        QueryStats *entry = &snapshot[funcctx->call_cntr];
        double avg_time_ms;

        values[0] = UInt64GetDatum(entry->fingerprint);
        values[1] = UInt64GetDatum(entry->calls);
        values[2] = UInt64GetDatum(entry->errors);
        values[3] = Float8GetDatum(entry->total_time_ms);

        avg_time_ms = (entry->calls > 0) ? (entry->total_time_ms / entry->calls) : 0.0;
        values[4] = Float8GetDatum(avg_time_ms);

        values[5] = Float8GetDatum(entry->max_time_ms);
        values[6] = TimestampTzGetDatum(entry->first_seen);
        values[7] = TimestampTzGetDatum(entry->last_seen);

        tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    }

    SRF_RETURN_DONE(funcctx);
}

/*
 * pgtrace_query_count()
 * Returns the number of unique queries being tracked.
 */
PG_FUNCTION_INFO_V1(pgtrace_query_count);

PGDLLEXPORT Datum pgtrace_query_count(PG_FUNCTION_ARGS)
{
    uint64 count = pgtrace_hash_count();
    PG_RETURN_INT64(count);
}

/*
 * pgtrace_reset()
 * Clears all query stats in the hash table.
 */
PG_FUNCTION_INFO_V1(pgtrace_reset);

PGDLLEXPORT Datum pgtrace_reset(PG_FUNCTION_ARGS)
{
    pgtrace_hash_reset();
    PG_RETURN_VOID();
}

/*
 * pgtrace_internal_failing_queries()
 * Returns queries that failed with error tracking.
 */
PG_FUNCTION_INFO_V1(pgtrace_internal_failing_queries);

PGDLLEXPORT Datum pgtrace_internal_failing_queries(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    ErrorTrackEntry *snapshot;
    uint32 *num_entries_ptr;

    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldcontext;
        TupleDesc tupdesc;
        uint32 i, j, count;

        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("pgtrace_internal_failing_queries must be called in a context that accepts a record")));

        funcctx->tuple_desc = BlessTupleDesc(tupdesc);

        /* Take snapshot of error buffer under lock */
        count = 0;
        snapshot = palloc0(PGTRACE_ERROR_BUFFER_SIZE * sizeof(ErrorTrackEntry));

        if (pgtrace_error_buffer)
        {
            LWLockAcquire(&pgtrace_error_buffer->lock, LW_SHARED);

            for (i = 0, j = 0; i < pgtrace_error_buffer->num_entries; i++)
            {
                ErrorTrackEntry *entry = &pgtrace_error_buffer->entries[i];
                if (entry->valid)
                {
                    memcpy(&snapshot[j], entry, sizeof(ErrorTrackEntry));
                    j++;
                    count++;
                }
            }

            LWLockRelease(&pgtrace_error_buffer->lock);
        }

        funcctx->user_fctx = snapshot;
        num_entries_ptr = palloc(sizeof(uint32));
        *num_entries_ptr = count;
        funcctx->max_calls = count;
        funcctx->attinmeta = (AttInMetadata *)num_entries_ptr;

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    snapshot = (ErrorTrackEntry *)funcctx->user_fctx;

    if (funcctx->call_cntr < funcctx->max_calls)
    {
        Datum values[4];
        bool nulls[4] = {false, false, false, false};
        HeapTuple tuple;
        ErrorTrackEntry *entry = &snapshot[funcctx->call_cntr];
        char sqlstate_str[6];

        values[0] = UInt64GetDatum(entry->fingerprint);

        /* Format SQLSTATE as 5-char string (e.g., "23505") */
        snprintf(sqlstate_str, sizeof(sqlstate_str), "%05u", entry->sqlstate);
        values[1] = CStringGetDatum(sqlstate_str);

        values[2] = UInt64GetDatum(entry->error_count);
        values[3] = TimestampTzGetDatum(entry->last_error_at);

        tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    }

    SRF_RETURN_DONE(funcctx);
}

/*
 * pgtrace_internal_slow_queries()
 * Returns recent slow queries from the ring buffer.
 */
PG_FUNCTION_INFO_V1(pgtrace_internal_slow_queries);

PGDLLEXPORT Datum pgtrace_internal_slow_queries(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    SlowQueryEntry *snapshot;
    uint32 *num_entries_ptr;

    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldcontext;
        TupleDesc tupdesc;
        uint32 i, j, count;

        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("pgtrace_internal_slow_queries must be called in a context that accepts a record")));

        funcctx->tuple_desc = BlessTupleDesc(tupdesc);

        /* Take snapshot of ring buffer under lock */
        count = 0;
        snapshot = palloc0(PGTRACE_SLOW_QUERY_BUFFER_SIZE * sizeof(SlowQueryEntry));

        if (pgtrace_slow_query_buffer)
        {
            LWLockAcquire(&pgtrace_slow_query_buffer->lock, LW_SHARED);

            for (i = 0, j = 0; i < PGTRACE_SLOW_QUERY_BUFFER_SIZE; i++)
            {
                SlowQueryEntry *entry = &pgtrace_slow_query_buffer->entries[i];
                if (entry->valid)
                {
                    memcpy(&snapshot[j], entry, sizeof(SlowQueryEntry));
                    j++;
                    count++;
                }
            }

            LWLockRelease(&pgtrace_slow_query_buffer->lock);
        }

        funcctx->user_fctx = snapshot;
        num_entries_ptr = palloc(sizeof(uint32));
        *num_entries_ptr = count;
        funcctx->max_calls = count;
        funcctx->attinmeta = (AttInMetadata *)num_entries_ptr;

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    snapshot = (SlowQueryEntry *)funcctx->user_fctx;

    if (funcctx->call_cntr < funcctx->max_calls)
    {
        Datum values[7];
        bool nulls[7] = {false, false, false, false, false, false, false};
        HeapTuple tuple;
        SlowQueryEntry *entry = &snapshot[funcctx->call_cntr];

        values[0] = UInt64GetDatum(entry->fingerprint);
        values[1] = Float8GetDatum(entry->duration_ms);
        values[2] = TimestampTzGetDatum(entry->timestamp);
        values[3] = CStringGetDatum(entry->application_name);
        values[4] = CStringGetDatum(entry->user);
        values[5] = Int64GetDatum(entry->rows_processed);

        tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    }

    SRF_RETURN_DONE(funcctx);
}

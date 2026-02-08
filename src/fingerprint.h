#pragma once

#include <postgres.h>

/*
 * Query fingerprinting for v2
 * Normalizes query text and computes a 64-bit hash.
 */

uint64 pgtrace_compute_fingerprint(const char *query_text);
char *pgtrace_normalize_query(const char *query_text);

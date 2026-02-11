#pragma once

#include <postgres.h>

uint64 pgtrace_compute_fingerprint(const char *query_text);
char *pgtrace_normalize_query(const char *query_text);

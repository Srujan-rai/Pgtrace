#include <postgres.h>
#include <ctype.h>
#include <utils/builtins.h>
#include "fingerprint.h"

/*
 * Normalize query: strip literals, collapse whitespace, lowercase keywords.
 * Minimal implementation: strips string/numeric literals and collapses whitespace.
 */
char *
pgtrace_normalize_query(const char *query_text)
{
    if (!query_text)
        return NULL;

    size_t len = strlen(query_text);
    char *normalized = palloc(len + 1);
    const char *src = query_text;
    char *dst = normalized;
    bool in_string = false;
    bool in_number = false;
    bool prev_space = false;

    while (*src)
    {
        char c = *src;

        /* Handle string literals */
        if (c == '\'')
        {
            in_string = !in_string;
            if (!in_string)
            {
                /* End of string: replace entire literal with '?' */
                *dst++ = '?';
                prev_space = false;
            }
            src++;
            continue;
        }

        if (in_string)
        {
            src++;
            continue;
        }

        /* Handle numeric literals */
        if (isdigit((unsigned char)c))
        {
            if (!in_number)
            {
                *dst++ = '?';
                in_number = true;
                prev_space = false;
            }
            src++;
            continue;
        }
        else
        {
            in_number = false;
        }

        /* Collapse whitespace */
        if (isspace((unsigned char)c))
        {
            if (!prev_space)
            {
                *dst++ = ' ';
                prev_space = true;
            }
            src++;
            continue;
        }

        /* Copy regular character */
        *dst++ = tolower((unsigned char)c);
        prev_space = false;
        src++;
    }

    /* Trim trailing space */
    if (dst > normalized && *(dst - 1) == ' ')
        dst--;

    *dst = '\0';
    return normalized;
}

/*
 * Compute 64-bit FNV-1a hash for fingerprint.
 * FNV-1a is fast, simple, and good enough for query deduplication.
 */
uint64
pgtrace_compute_fingerprint(const char *query_text)
{
    const uint64 FNV_PRIME = 0x100000001b3ULL;
    const uint64 FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;

    if (!query_text)
        return 0;

    char *normalized = pgtrace_normalize_query(query_text);
    if (!normalized)
        return 0;

    uint64 hash = FNV_OFFSET_BASIS;
    const unsigned char *p = (const unsigned char *)normalized;

    while (*p)
    {
        hash ^= (uint64)(*p++);
        hash *= FNV_PRIME;
    }

    pfree(normalized);
    return hash;
}

EXTENSION = pgtrace
MODULE_big = pgtrace

OBJS = \
    src/pgtrace.o \
    src/hooks.o \
    src/metrics.o \
    src/shmem.o \
    src/guc.o \
    src/fingerprint.o \
    src/query_hash.o \
    src/slow_query.o \
    src/error_track.o \
    src/error_hook.o \
    src/audit.o

DATA = pgtrace--0.3.sql pgtrace--0.2--0.3.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

EXTENSION = pgtrace
MODULE_big = pgtrace

OBJS = \
    src/pgtrace.o \
    src/hooks.o \
    src/metrics.o \
    src/shmem.o \
    src/guc.o \
    src/fingerprint.o \
    src/query_hash.o

DATA = pgtrace--0.1.sql pgtrace--0.2.sql pgtrace--0.1--0.2.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

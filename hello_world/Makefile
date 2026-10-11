MODULES = demo_extension
EXTENSION = demo_extension
DATA = demo_extension--1.0.sql
PGFILEDESC = "demo_extension - my hello_world() extension"

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
endif

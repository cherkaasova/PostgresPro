MODULES = pg_array_stats
EXTENSION = pg_array_stats
DATA = pg_array_stats--1.0.sql
REGRESS = 01_basic_mean 02_basic_median 03_variance_stddev 04_percentile 05_cov_corr 06_error_empty 07_error_null_elem 08_error_type 09_error_length 10_error_single

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
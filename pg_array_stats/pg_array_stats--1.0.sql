\echo Use "CREATE EXTENSION pg_array_stats" to load this file. \quit

CREATE FUNCTION array_mean(anyarray)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_mean'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION array_median(anyarray)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_median'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION array_variance(anyarray)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_variance'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION array_stddev(anyarray)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_stddev'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION array_percentile(anyarray, numeric)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_percentile'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION array_cov(anyarray, anyarray)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_cov'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION array_corrcoef(anyarray, anyarray)
RETURNS numeric
AS 'MODULE_PATHNAME', 'array_corrcoef'
LANGUAGE C IMMUTABLE STRICT;
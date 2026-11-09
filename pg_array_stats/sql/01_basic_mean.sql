CREATE EXTENSION IF NOT EXISTS pg_array_stats;
SELECT array_mean(ARRAY[1,2,3,4,5]);
SELECT array_mean(ARRAY[[1,2],[3,4]]);
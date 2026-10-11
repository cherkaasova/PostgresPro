/* contrib/dict_int/dict_int--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION demo_extension" to load this file. \quit

CREATE FUNCTION hello_world()
        RETURNS text
        AS 'MODULE_PATHNAME', 'hello_world'
        LANGUAGE C IMMUTABLE;
/*
 * =============================================================================
 *
 *       Filename:  reveldb.h
 *
 *    Description:  reveldb: REstful leVELDB implementation.
 *
 *        Created:  12/11/2012 11:35:08 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef _REVELDB_H_
#define _REVELDB_H_
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <leveldb/c.h>

typedef struct reveldb_config_s_ reveldb_config_t;
typedef struct reveldb_instance_s_ reveldb_instance_t;

struct reveldb_config_s_ {
    char *dbname; /** default database name.*/
    unsigned int lru_cache_size; /** leveldb's lru cache size */
    bool create_if_missing; /** create database if it doesn't exist. */
    bool error_if_exist; /** open database throws an error if exist. */
    unsigned int write_buffer_size; /** leveldb's write buffer size */
    bool paranoid_checks; /**paranoid checks */
    unsigned int max_open_files; /** max open files */
    unsigned block_size; /** block size */
    unsigned int block_restart_interval; /*block restart interval */
    /** compression support, 0: no compression, 1: snappy compression.*/
    bool compression_support; 
    bool verify_checksums; /** set true to verify checksums when read. */
    bool fill_cache; /** set true if want to fill cache. */
    bool sync; /** set true to enable sync when write. */
};

struct reveldb_instance_s_ {
    /* leveldb instance. */
    leveldb_t *db;
    leveldb_cache_t *cache;
    leveldb_comparator_t *comparator;
    leveldb_env_t *env;
    leveldb_filterpolicy_t *filterpolicy;
    leveldb_iterator_t *iterator;
    leveldb_logger_t *logger;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_snapshot_t *snapshot;
    leveldb_writebatch_t *writebatch;
    leveldb_writeoptions_t *woptions;

    char *err;
};

#endif // _REVELDB_H_

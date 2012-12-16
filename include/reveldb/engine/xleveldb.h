/*
 * =============================================================================
 *
 *       Filename:  xleveldb.h
 *
 *    Description:  leveldb storage engine.
 *
 *        Created:  12/11/2012 11:35:08 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef _REVELDB_XLEVELDB_H_
#define _REVELDB_XLEVELDB_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <leveldb/c.h>

#include <reveldb/util/xconfig.h>

typedef struct xleveldb_config_s_ xleveldb_config_t;
typedef struct xleveldb_instance_s_ xleveldb_instance_t;

/* xleveldb_config_s_ is the leveldb specified configuration,
 * I added "x" as the prefix on purpose to avoid the potential
 * conflicts with leveldb.
 * */
struct xleveldb_config_s_ {
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
    bool compression; 
    bool verify_checksums; /** set true to verify checksums when read. */
    bool fill_cache; /** set true if want to fill cache. */
    bool sync; /** set true to enable sync when write. */
};

/* xleveldb_instance_s_ indicates a leveldb instance. reveldb consists of
 * more than one leveldb instance on design, and each instance can be
 * connected from client.*/
struct xleveldb_instance_s_ {
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

    xleveldb_config_t *config;
};

extern xleveldb_config_t * xleveldb_config_init(const char* dbname,
        reveldb_db_config_t *db_config);

extern xleveldb_instance_t * xleveldb_instance_init(xleveldb_config_t *config);

extern void xleveldb_instance_fini(xleveldb_instance_t *instance);

extern void xleveldb_instance_destroy(xleveldb_instance_t *instance);

#endif // _REVELDB_XLEVELDB_H_

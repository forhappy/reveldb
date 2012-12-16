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

#include <reveldb/util/evhttpx.h>
#include <reveldb/util/rbtree.h>
#include <reveldb/util/xconfig.h>

#define REVELDB_MAX_KV_RESPONSE_BUFFER_SIZE (1024 * 1024 *2)
#define REVELDB_MAX_ERROR_RESPONSE_BUFFER_SIZE 1024
#define REVELDB_RPC_MAX_PORTS_LISTENING_ON 32

typedef struct xleveldb_config_s_ xleveldb_config_t;
typedef struct xleveldb_instance_s_ xleveldb_instance_t;
typedef struct reveldb_s_ reveldb_t;

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
    bool compression_support; 
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

/* reveldb_s_ is a red-black tree structure containing all leveldb instance. */
struct reveldb_s_ {
    char *dbname;

    xleveldb_instance_t *instance;

    struct rb_node node;
};

extern reveldb_t * reveldb_init(const char *dbname);
extern reveldb_t * reveldb_search_db(struct rb_root *root, const char *dbname);
extern int reveldb_insert_db(struct rb_root *root, reveldb_t *db);
extern void reveldb_free_db(reveldb_t *db);

#endif // _REVELDB_H_

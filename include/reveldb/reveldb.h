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

#include <reveldb/engine/xleveldb.h>
#include <reveldb/evhttpx/evhttpx.h>
#include <reveldb/util/rbtree.h>
#include <reveldb/util/xconfig.h>

#define REVELDB_MAX_KV_RESPONSE_BUFFER_SIZE (1024 * 1024 *2)
#define REVELDB_MAX_ERROR_RESPONSE_BUFFER_SIZE 1024
#define REVELDB_RPC_MAX_PORTS_LISTENING_ON 32
#define REVELDB_REST_MAX_PORTS_LISTENING_ON 32

typedef struct reveldb_s_ reveldb_t;

/* reveldb_s_ is a red-black tree structure containing all leveldb instance. */
struct reveldb_s_ {
    char *dbname;

    xleveldb_instance_t *instance;

    struct rb_node node;
};

extern reveldb_t * reveldb_init(const char *dbname,
        reveldb_config_t *config);
extern reveldb_t * reveldb_search_db(struct rb_root *root,
        const char *dbname);
extern int reveldb_insert_db(struct rb_root *root, reveldb_t *db);
extern void reveldb_free_db(reveldb_t *db);

#endif // _REVELDB_H_

/*
 * =============================================================================
 *
 *       Filename:  writebatch.h
 *
 *    Description:  leveldb writebatch wrapper.
 *
 *        Created:  12/23/2012 11:23:23 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef _XLEVELDB_WRITEBATCH_H_
#define _XLEVELDB_WRITEBATCH_H_
#include <reveldb/reveldb.h>

struct rb_node;

typedef struct xleveldb_writebatch_s_ xleveldb_writebatch_t;

struct xleveldb_writebatch_s_ {
    char *uuid;
    leveldb_writebatch_t *writebatch;
    reveldb_t *reveldb;
    struct rb_node node;
};

extern xleveldb_writebatch_t * xleveldb_init_writebatch(
        const char *uuid,
        reveldb_t *reveldb);

extern xleveldb_writebatch_t * xleveldb_search_writebatch(
        struct rb_root *root,
        const char *uuid);

extern int xleveldb_insert_writebatch(
        struct rb_root *root,
        xleveldb_writebatch_t *writebatch);

extern void xleveldb_free_writebatch(
        xleveldb_writebatch_t *writebatch);
#endif /* _XLEVELDB_WRITEBATCH_H_ */


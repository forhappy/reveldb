/*
 * =============================================================================
 *
 *       Filename:  snapshot.h
 *
 *    Description:  leveldb snapshot wrapper.
 *
 *        Created:  12/23/2012 10:43:53 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#ifndef _XLEVELDB_SNAPSHOT_H_
#define _XLEVELDB_SNAPSHOT_H_
#include <reveldb/reveldb.h>

struct rb_node;

typedef struct xleveldb_snapshot_s_ xleveldb_snapshot_t;

struct xleveldb_snapshot_s_ {
    char *uuid;
    const leveldb_snapshot_t *snapshot;
    reveldb_t *reveldb;
    struct rb_node node;
};

extern xleveldb_snapshot_t * xleveldb_init_snapshot(
        const char *uuid,
        reveldb_t *reveldb);

extern xleveldb_snapshot_t * xleveldb_search_snapshot(
        struct rb_root *root,
        const char *uuid);

extern int xleveldb_insert_snapshot(
        struct rb_root *root,
        xleveldb_snapshot_t *snapshot);

extern void xleveldb_free_snapshot(
        xleveldb_snapshot_t *snapshot);
#endif /* _XLEVELDB_SNAPSHOT_H_ */


/*
 * =============================================================================
 *
 *       Filename:  snapshot.c
 *
 *    Description:  leveldb snapshot wrapper.
 *
 *        Created:  12/23/2012 10:48:01 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "snapshot.h"

xleveldb_snapshot_t *
xleveldb_init_snapshot(const char *uuid, reveldb_t *reveldb)
{
    assert(uuid != NULL);
    assert(reveldb != NULL);
    size_t uuid_len = strlen(uuid);

    xleveldb_snapshot_t *snapshot = (xleveldb_snapshot_t *)
        malloc(sizeof(xleveldb_snapshot_t) + uuid_len + 1);
    memset(snapshot, 0, (sizeof(xleveldb_snapshot_t) + uuid_len + 1));
    snapshot->uuid = (char *)snapshot + sizeof(xleveldb_snapshot_t);
    memcpy(snapshot->uuid, uuid, uuid_len);
    snapshot->reveldb = reveldb;
    snapshot->snapshot = leveldb_create_snapshot(reveldb->instance->db);
    return snapshot;
}

xleveldb_snapshot_t *
xleveldb_search_snapshot(
        struct rb_root *root,
        const char *uuid)
{
    struct rb_node *node = root->rb_node;

    while (node) {
        xleveldb_snapshot_t *snapshot = container_of(node, xleveldb_snapshot_t, node);
        int result;

        result = strcmp(uuid, snapshot->uuid);

        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return snapshot;
    }
    return NULL;
}

int
xleveldb_insert_snapshot(
        struct rb_root *root,
        xleveldb_snapshot_t *snapshot)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    /* Figure out where to put new node */
    while (*new) {
        xleveldb_snapshot_t *this = container_of(*new, xleveldb_snapshot_t, node);
        int result = strcmp(snapshot->uuid, this->uuid);

        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else
            return 0;
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&snapshot->node, parent, new);
    rb_insert_color(&snapshot->node, root);

    return 1;
}

void
xleveldb_free_snapshot(xleveldb_snapshot_t *snapshot)
{
    if (snapshot != NULL) {
        if (snapshot->snapshot != NULL) {
            leveldb_release_snapshot(snapshot->reveldb->instance->db,
                    snapshot->snapshot);
            snapshot->snapshot = NULL;
        }
        free(snapshot);
    }
}


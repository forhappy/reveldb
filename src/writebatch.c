/*
 * =============================================================================
 *
 *       Filename:  writebatch.c
 *
 *    Description:  leveldb writebatch wrapper.
 *
 *        Created:  12/23/2012 11:28:28 PM
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

#include "writebatch.h"

xleveldb_writebatch_t *
xleveldb_init_writebatch(const char *uuid, reveldb_t *reveldb)
{
    assert(uuid != NULL);
    assert(reveldb != NULL);
    size_t uuid_len = strlen(uuid);

    xleveldb_writebatch_t *writebatch = (xleveldb_writebatch_t *)
        malloc(sizeof(xleveldb_writebatch_t) + uuid_len + 1);
    memset(writebatch, 0, (sizeof(xleveldb_writebatch_t) + uuid_len + 1));
    writebatch->uuid = (char *)writebatch + sizeof(xleveldb_writebatch_t);
    memcpy(writebatch->uuid, uuid, uuid_len);
    writebatch->reveldb = reveldb;
    writebatch->writebatch = leveldb_writebatch_create();
    return writebatch;
}

xleveldb_writebatch_t *
xleveldb_search_writebatch(
        struct rb_root *root,
        const char *uuid)
{
    struct rb_node *node = root->rb_node;

    while (node) {
        xleveldb_writebatch_t *writebatch =
            container_of(node, xleveldb_writebatch_t, node);
        int result;

        result = strcmp(uuid, writebatch->uuid);

        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return writebatch;
    }
    return NULL;
}

int
xleveldb_insert_writebatch(
        struct rb_root *root,
        xleveldb_writebatch_t *writebatch)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    /* Figure out where to put new node */
    while (*new) {
        xleveldb_writebatch_t *this =
            container_of(*new, xleveldb_writebatch_t, node);
        int result = strcmp(writebatch->uuid, this->uuid);

        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else
            return 0;
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&writebatch->node, parent, new);
    rb_insert_color(&writebatch->node, root);

    return 1;
}

void
xleveldb_free_writebatch(xleveldb_writebatch_t *writebatch)
{
    if (writebatch != NULL) {
        if (writebatch->writebatch != NULL) {
            leveldb_writebatch_destroy(writebatch->writebatch);
            writebatch->writebatch = NULL;
        }
        free(writebatch);
    }
}


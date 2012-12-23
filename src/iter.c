/*
 * =============================================================================
 *
 *       Filename:  iter.c
 *
 *    Description:  leveldb iterator wrapper.
 *
 *        Created:  12/23/2012 02:45:53 PM
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

#include "iter.h"

xleveldb_iter_t *
xleveldb_init_iter(const char *uuid, reveldb_t *reveldb)
{
    assert(uuid != NULL);
    assert(reveldb != NULL);
    size_t uuid_len = strlen(uuid);

    xleveldb_iter_t *iter = (xleveldb_iter_t *)malloc(sizeof(xleveldb_iter_t) + uuid_len + 1);
    memset(iter, 0, (sizeof(xleveldb_iter_t) + uuid_len + 1));
    iter->uuid = (char *)iter + sizeof(xleveldb_iter_t);
    memcpy(iter->uuid, uuid, uuid_len);
    iter->reveldb = reveldb;
    iter->iter = leveldb_create_iterator(reveldb->instance->db,
            reveldb->instance->roptions);
    leveldb_iter_seek_to_first(iter->iter);
    return iter;
}

xleveldb_iter_t *
xleveldb_search_iter(
        struct rb_root *root,
        const char *uuid)
{
    struct rb_node *node = root->rb_node;

    while (node) {
        xleveldb_iter_t *iter = container_of(node, xleveldb_iter_t, node);
        int result;

        result = strcmp(uuid, iter->uuid);

        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return iter;
    }
    return NULL;
}

int
xleveldb_insert_iter(
        struct rb_root *root,
        xleveldb_iter_t *iter)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    /* Figure out where to put new node */
    while (*new) {
        xleveldb_iter_t *this = container_of(*new, xleveldb_iter_t, node);
        int result = strcmp(iter->uuid, this->uuid);

        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else
            return 0;
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&iter->node, parent, new);
    rb_insert_color(&iter->node, root);

    return 1;
}

void
xleveldb_free_iter(xleveldb_iter_t *iter)
{
    if (iter != NULL) {
        if (iter->uuid != NULL) {
            free(iter->uuid);
            iter->uuid = NULL;
        }
        if (iter->iter != NULL) {
            leveldb_iter_destroy(iter->iter);
            iter->iter = NULL;
        }
        free(iter);
        iter = NULL;
    }
}

unsigned char
xleveldb_iter_valid(const xleveldb_iter_t *iter)
{
    return leveldb_iter_valid(iter->iter);
}

void
xleveldb_iter_seek_to_first(xleveldb_iter_t *iter)
{
    leveldb_iter_seek_to_first(iter->iter);
    return;
}

void
xleveldb_iter_seek_to_last(xleveldb_iter_t *iter)
{
    leveldb_iter_seek_to_last(iter->iter);
    return;
}

void
xleveldb_iter_seek(
        xleveldb_iter_t *iter,
        const char* k, size_t klen)
{
    leveldb_iter_seek(iter->iter, k, klen);
    return;
}

void
xleveldb_iter_next(xleveldb_iter_t *iter)
{
    leveldb_iter_next(iter->iter);
    return;
}

void
xleveldb_iter_prev(xleveldb_iter_t *iter)
{
    leveldb_iter_prev(iter->iter);
    return;
}

void
xleveldb_iter_forward(
        xleveldb_iter_t *iter,
        unsigned int step)
{
    int i;
    for(i = 0; i< step; i++) {
        leveldb_iter_next(iter->iter);
    }
    return;
}

void
xleveldb_iter_backward(
        xleveldb_iter_t *iter,
        unsigned int step)
{
    int i;
    for(i = 0; i< step; i++) {
        leveldb_iter_prev(iter->iter);
    }
    return;

}

const char *
xleveldb_iter_key(
        const xleveldb_iter_t *iter,
        size_t *klen)
{
    return leveldb_iter_key(iter->iter, klen);
}

const char *
xleveldb_iter_value(
        const xleveldb_iter_t *iter,
        size_t *vlen)
{
    return leveldb_iter_value(iter->iter, vlen);
}

void
xleveldb_iter_kv(const xleveldb_iter_t *iter,
        const char **key, size_t *klen,
        const char **value, size_t *vlen)
{
    *key = leveldb_iter_key(iter->iter, klen);
    *value = leveldb_iter_value(iter->iter, vlen);
    return;
}


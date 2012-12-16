/*
 * =============================================================================
 *
 *       Filename:  reveldb.c
 *
 *    Description:  reveldb: REstful leVELDB implementation.
 *
 *        Created:  12/11/2012 11:35:58 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <reveldb/reveldb.h>
#include <reveldb/engine/xleveldb.h>
#include <reveldb/util/rbtree.h>

reveldb_t * reveldb_init(const char *dbname, reveldb_config_t *config)
{
    size_t dbname_len = strlen(dbname);

    reveldb_t *db = (reveldb_t *)malloc(sizeof(reveldb_t));
    if (db == NULL) return NULL;
    db->dbname = (char *)malloc(sizeof(char) * (dbname_len + 1));
    memset(db->dbname, 0, (dbname_len + 1));
    strncpy(db->dbname, dbname, dbname_len);

    xleveldb_config_t *xleveldb_config = xleveldb_config_init(dbname,
            config->db_config);
    xleveldb_instance_t *instance = xleveldb_instance_init(xleveldb_config);

    db->instance = instance;

    return db;
}

reveldb_t * reveldb_search_db(struct rb_root *root, const char *dbname)
{
    struct rb_node *node = root->rb_node;

    while (node) {
        reveldb_t *db = container_of(node, reveldb_t, node);
        int result;

        result = strcmp(dbname, db->dbname);

        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return db;
    }
    return NULL;
}

int reveldb_insert_db(struct rb_root *root, reveldb_t *db)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    /* Figure out where to put new node */
    while (*new) {
        reveldb_t *this = container_of(*new, reveldb_t, node);
        int result = strcmp(db->dbname, this->dbname);

        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else
            return 0;
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&db->node, parent, new);
    rb_insert_color(&db->node, root);

    return 1;
}

void reveldb_free_db(reveldb_t *db)
{
    if (db != NULL) {
        if (db->dbname != NULL) {
            free(db->dbname);
            db->dbname = NULL;
        }
        if (db->instance != NULL) {
            xleveldb_instance_fini(db->instance);
            db->instance = NULL;
        }
        free(db);
        db = NULL;
    }
}


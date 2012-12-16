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
#include <reveldb/util/rbtree.h>

xleveldb_config_t *
xleveldb_config_init(const char* dbname)
{
    xleveldb_config_t *config = (xleveldb_config_t *)
        malloc(sizeof(xleveldb_config_t));

    unsigned int dbname_len = strlen(dbname);
    config->dbname = (char *)malloc(sizeof(char) * (dbname_len + 1));
    memset(config->dbname, 0, (dbname_len + 1));
    strncpy(config->dbname, dbname, dbname_len);

    config->lru_cache_size = 65536;
    config->create_if_missing = true;
    config->error_if_exist = false;
    config->write_buffer_size = 65536;
    config->paranoid_checks = true;
    config->max_open_files = 32;
    config->block_size = 1024;
    config->block_restart_interval = 8;
    config->compression_support = false;
    config->verify_checksums = false;
    config->fill_cache = false;
    config->sync = false;

    return config;
}

xleveldb_instance_t *
xleveldb_instance_init(xleveldb_config_t *config)
{
    xleveldb_instance_t *instance = (xleveldb_instance_t *)
        malloc(sizeof(xleveldb_instance_t));
    
    instance->comparator = NULL;
    instance->env = leveldb_create_default_env();
    instance->cache = leveldb_cache_create_lru(config->lru_cache_size);
    instance->filterpolicy = NULL;
    instance->iterator = NULL;
    instance->logger = NULL;
    instance->snapshot = NULL;
    instance->writebatch = NULL;
    instance->options = leveldb_options_create();
    leveldb_options_set_error_if_exists(instance->options, config->error_if_exist);
    leveldb_options_set_create_if_missing(instance->options, config->create_if_missing);
    leveldb_options_set_cache(instance->options, instance->cache);
    leveldb_options_set_env(instance->options, instance->env);
    leveldb_options_set_info_log(instance->options, NULL);
    leveldb_options_set_write_buffer_size(instance->options, config->write_buffer_size);
    leveldb_options_set_paranoid_checks(instance->options, config->paranoid_checks);
    leveldb_options_set_max_open_files(instance->options, 10);
    leveldb_options_set_block_size(instance->options, 1024);
    leveldb_options_set_block_restart_interval(instance->options, 8);
    leveldb_options_set_compression(instance->options, leveldb_no_compression);
    
    instance->roptions = leveldb_readoptions_create();
    leveldb_readoptions_set_verify_checksums(instance->roptions, config->verify_checksums);
    leveldb_readoptions_set_fill_cache(instance->roptions, config->fill_cache);
    
    instance->woptions = leveldb_writeoptions_create();
    leveldb_writeoptions_set_sync(instance->woptions, config->sync);
    
    instance->db = leveldb_open(instance->options, config->dbname, &(instance->err));
    instance->config = config;

    return instance;
}

void
xleveldb_instance_fini(xleveldb_instance_t *instance)
{
    assert(instance != NULL);

    if (instance->comparator != NULL) {
        leveldb_comparator_destroy(instance->comparator);
        instance->comparator = NULL;
    }
    if (instance->env != NULL) {
        leveldb_env_destroy(instance->env);
        instance->env = NULL;
    }
    if (instance->cache != NULL) {
        leveldb_cache_destroy(instance->cache);
        instance->cache = NULL;
    }
    if (instance->filterpolicy != NULL) {
        leveldb_filterpolicy_destroy(instance->filterpolicy);
        instance->filterpolicy = NULL;
    }
    if (instance->iterator != NULL) {
        leveldb_iter_destroy(instance->iterator);
        instance->iterator = NULL;
    }
    if (instance->snapshot != NULL) {
        leveldb_release_snapshot(instance->db, instance->snapshot);
        instance->snapshot = NULL;
    }
    if (instance->writebatch != NULL) {
        leveldb_writebatch_destroy(instance->writebatch);
        instance->writebatch = NULL;
    }
    if (instance->options != NULL) {
        leveldb_options_destroy(instance->options);
        instance->options = NULL;
    }
    if (instance->roptions != NULL) {
        leveldb_readoptions_destroy(instance->roptions);
        instance->roptions = NULL;
    }
    if (instance->woptions != NULL) {
        leveldb_writeoptions_destroy(instance->woptions);
        instance->woptions = NULL;
    }
    if (instance->db != NULL) {
        leveldb_close(instance->db);
        instance->db = NULL;
    }

    free(instance);
}

void
xleveldb_instance_destroy(xleveldb_instance_t *instance)
{
    assert(instance != NULL);

    leveldb_destroy_db(instance->options,
            instance->config->dbname,
            &(instance->err));
}

reveldb_t * reveldb_init(const char *dbname)
{
    size_t dbname_len = strlen(dbname);

    reveldb_t *db = (reveldb_t *)malloc(sizeof(reveldb_t));
    if (db == NULL) return NULL;
    db->dbname = (char *)malloc(sizeof(char) * (dbname_len + 1));
    memset(db->dbname, 0, (dbname_len + 1));
    strncpy(db->dbname, dbname, dbname_len);

    xleveldb_config_t *config = xleveldb_config_init(dbname);
    xleveldb_instance_t *instance = xleveldb_instance_init(config);

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


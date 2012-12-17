/*
 * =============================================================================
 *
 *       Filename:  xleveldb.c
 *
 *    Description:  leveldb storage engine.
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

#include <reveldb/engine/xleveldb.h>
#include <reveldb/util/xconfig.h>

xleveldb_config_t *
xleveldb_config_init(const char* dbname,
        reveldb_db_config_t *db_config)
{
    assert(dbname != NULL);
    assert(db_config != NULL);

    xleveldb_config_t *config = (xleveldb_config_t *)
        malloc(sizeof(xleveldb_config_t));

    unsigned int dbname_len = strlen(dbname);
    config->dbname = (char *)malloc(sizeof(char) * (dbname_len + 1));
    memset(config->dbname, 0, (dbname_len + 1));
    strncpy(config->dbname, dbname, dbname_len);

    config->lru_cache_size = db_config->lru_cache_size;
    config->create_if_missing = db_config->create_if_missing;
    config->error_if_exist = db_config->error_if_exist;
    config->write_buffer_size = db_config->write_buffer_size;
    config->paranoid_checks = db_config->paranoid_checks;
    config->max_open_files = db_config->max_open_files;
    config->block_size = db_config->block_size;
    config->block_restart_interval = db_config->block_restart_interval;
    config->compression = db_config->compression;
    config->verify_checksums = db_config->verify_checksums;
    config->fill_cache = db_config->fill_cache;
    config->sync = db_config->sync;

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
    leveldb_options_set_max_open_files(instance->options, config->max_open_files);
    leveldb_options_set_block_size(instance->options, config->block_size);
    leveldb_options_set_block_restart_interval(instance->options, config->block_restart_interval);
    leveldb_options_set_compression(instance->options, config->compression);
    
    instance->roptions = leveldb_readoptions_create();
    leveldb_readoptions_set_verify_checksums(instance->roptions, config->verify_checksums);
    leveldb_readoptions_set_fill_cache(instance->roptions, config->fill_cache);
    
    instance->woptions = leveldb_writeoptions_create();
    leveldb_writeoptions_set_sync(instance->woptions, config->sync);
    
    instance->db = leveldb_open(instance->options, config->dbname, &(instance->err));
    instance->config = config;
    instance->err = NULL;

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

void
xleveldb_reset_err(xleveldb_instance_t *instance)
{
    assert(instance != NULL);
    if (instance->err != NULL) {
        leveldb_free(instance->err);
        instance->err = NULL;
    }
}


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

reveldb_config_t *
reveldb_config_init(const char* conf)
{
    reveldb_config_t *config = (reveldb_config_t *)
        malloc(sizeof(reveldb_config_t));

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

reveldb_instance_t *
reveldb_instance_init(leveldb_config_t *config)
{
    reveldb_instance_t *instance = (reveldb_instance_t *)
        malloc(sizeof(reveldb_instance_t));
    
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

    return instance;

}


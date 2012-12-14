/*
 * =============================================================================
 *
 *       Filename:  config.h
 *
 *    Description:  reveldb configuration parser.
 *
 *        Created:  12/12/2012 10:30:20 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

typedef struct reveldb_config_s_ reveldb_config_t;
typedef struct reveldb_server_config_s_ reveldb_server_config_t;
typedef struct reveldb_db_config_s_ reveldb_db_config_t;
typedef struct reveldb_log_config_s_ reveldb_log_config_t

struct reveldb_config_s_ {
    reveldb_server_config_t * server_conf;
    reveldb_db_config_t     * db_conf;
    reveldb_log_config_t    * log_conf;
};

struct reveldb_server_config_s_ {

};

struct reveldb_db_config_s_ {
    char *dbname; /** default database name.*/
    unsigned int lru_cache_size; /** leveldb's lru cache size */
    bool create_if_missing; /** create database if it doesn't exist. */
    bool error_if_exist; /** open database throws an error if exist. */
    unsigned int write_buffer_size; /** leveldb's write buffer size */
    bool paranoid_checks; /**paranoid checks */
    unsigned int max_open_files; /** max open files */
    unsigned block_size; /** block size */
    unsigned int block_restart_interval; /*block restart interval */
    /** compression support, 0: no compression, 1: snappy compression.*/
    bool compression_support; 
    bool verify_checksums; /** set true to verify checksums when read. */
    bool fill_cache; /** set true if want to fill cache. */
    bool sync; /** set true to enable sync when write. */
};

struct reveldb_log_config_s_ {
    
};

#endif // _CONFIG_H_

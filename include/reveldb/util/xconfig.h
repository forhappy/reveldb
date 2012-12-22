/*
 * =============================================================================
 *
 *       Filename:  xconfig.h
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

#ifndef _XCONFIG_H_
#define _XCONFIG_H_

typedef struct reveldb_config_s_ reveldb_config_t;
typedef struct reveldb_server_config_s_ reveldb_server_config_t;
typedef struct reveldb_db_config_s_ reveldb_db_config_t;
typedef struct reveldb_log_config_s_ reveldb_log_config_t;
typedef struct reveldb_ssl_config_s_ reveldb_ssl_config_t;

struct reveldb_config_s_ {
    reveldb_server_config_t *server_config;
    reveldb_db_config_t     *db_config;
    reveldb_log_config_t    *log_config;
    reveldb_ssl_config_t    *ssl_config;
};

struct reveldb_server_config_s_ {
    char *host; /* reveldb server host. */
    char *rpcports; /* rpc protocol bind port, reveldb can listen on multiple ports. */
    char *restports; /* REST protocol bind port, reveldb can listen on multiple ports. */
    bool https; /* https enabled. */
    unsigned int backlog; /* backlog of epoll. */
    char *username; /* reveldb root username. */
    char *password; /* password. */
    char *datadir; /* data directory. */
    char *pidfile; /* reveldb server pid file. */
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
    bool compression; 
    bool verify_checksums; /** set true to verify checksums when read. */
    bool fill_cache; /** set true if want to fill cache. */
    bool sync; /** set true to enable sync when write. */
};

struct reveldb_log_config_s_ {
    char *level; /* reveldb log level. */
    char *stream; /* log stream: stdout, stderr or file. */
};

struct reveldb_ssl_config_s_ {
    char *key;
    char *cert;
    char *capath;
    char *ciphers;
    unsigned int ssl_ctx_timeout;
    bool verify_peer;
    unsigned int verify_depth;
};

extern reveldb_config_t * reveldb_config_init(const char *file);
void reveldb_config_fini(reveldb_config_t *config);

#endif // _XCONFIG_H_

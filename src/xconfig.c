
/*
 * =============================================================================
 *
 *       Filename:  xconfig.c
 *
 *    Description:  reveldb configuration parser.
 *
 *        Created:  12/12/2012 10:30:58 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <reveldb/util/xconfig.h>

#include "cJSON.h"
#include "log.h"

static char *
_xconfig_strip_comment(const char *src, size_t len)
{
    int i = 0, j = 0;
    bool quote = false, comment = false;
    if (src == NULL) return "";

    /* make a plenty of space. */
    char *out = (char *)malloc(sizeof(char) * (len + 1));
    if (out == NULL) {
        fprintf(stderr, "out of memory in \n");
        exit(EXIT_FAILURE);
    }
    memset(out, 0, sizeof(char) * (len + 1));
    char *pout = out;

    for (; i < len; i++) {
        switch(src[i]) {
            case '\\':
                if (comment == true) break;
                *(pout + j++) = src[i];
                *(pout + j++) = src[++i];
                break;
            case '\'':
            case '\"':
                if (comment == true) break;
                *(pout + j++) = src[i];
                if (!quote) {
                    quote = src[i];
                } else if (quote == src[i]) {
                    quote = 0;
                }
                break;
            case '/':
                if (quote) {
                    *(pout + j++) = src[i];
                } else if (src[i + 1] == '/') {
                    i += strchr(src + i + 1, '\n') - (src + i);
                } else if (src[i + 1] == '*') {
                    comment = true;
                    i++;
                } else if (!comment) {
                    *(pout + j++) = src[i];
                }
                break;
            case '*':
                if (quote) {
                    *(pout + j++) = src[i];
                } else if (comment && src[i + 1] == '/') {
                    comment = false;
                    i++;
                    break;
                } else if (comment) break;
                *(pout + j++) = src[i];
                break;
            default:
                if (!comment)
                    *(pout + j++) = src[i];
                break;
        }
    }
    return out;
}

static char *
_xconfig_load_config_body(const char *filename)
{
    assert(filename != NULL);
    int err = -1;
    long file_len = -1;
    FILE *fp = NULL;
    char *json_buf = NULL;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        LOG_ERROR(("failed to open configuration file: %s", filename));
        return NULL;
    }

    err = fseek(fp, 0, SEEK_END);
    if (err != 0) {
        LOG_ERROR(("failed to seek: %s", strerror(errno)));
        return NULL;
    }

    file_len = ftell(fp);

    err = fseek(fp, 0, SEEK_SET);
    if (err != 0) {
        LOG_ERROR(("failed to seek: %s", strerror(errno)));
        return NULL;
    }

    json_buf = (char *) malloc(sizeof(char) * (file_len + 1));
    memset(json_buf, 0, (file_len + 1));
    err = fread(json_buf, 1, file_len, fp);
    if (err != file_len) {
        fclose(fp);
        free(json_buf);
        LOG_ERROR(("failed to read file: %s", filename));
        return NULL;
    }

    char *striped = _xconfig_strip_comment(json_buf, strlen(json_buf));
    free(json_buf);
    fclose(fp);
    return striped;
}

static reveldb_config_t *
_xconfig_init_internal_config(const char *config)
{
    assert(config != NULL);

    cJSON *root = NULL;
    cJSON *server = NULL;
    cJSON *db = NULL;
    cJSON *log = NULL;
    cJSON *ssl = NULL;
    cJSON *iter = NULL;
    reveldb_config_t *reveldb_config = NULL;

    /* config value length */
    size_t config_vlen = -1;

    reveldb_config = (reveldb_config_t *) malloc(sizeof(reveldb_config_t));
    if (reveldb_config == NULL) {
        LOG_ERROR(("failed to make room for reveldb_config_t."));
        return NULL;
    }

    root = cJSON_Parse(config);
    if (!root) {
        LOG_ERROR(("parsing json error before: [%s]\n",
                   cJSON_GetErrorPtr()));
        free(reveldb_config);
        return NULL;
    } else {
        server = cJSON_GetObjectItem(root, "server");
        reveldb_server_config_t *server_config =
            (reveldb_server_config_t *)
            malloc(sizeof(reveldb_server_config_t));
        if (server_config == NULL) {
            LOG_ERROR(("failed to make room for reveldb_server_config_t."));
            free(reveldb_config);
            cJSON_Delete(root);
            return NULL;
        }
        reveldb_db_config_t *db_config =
            (reveldb_db_config_t *) malloc(sizeof(reveldb_db_config_t));
        if (db_config == NULL) {
            LOG_ERROR(("failed to make room for reveldb_db_config_t."));
            free(reveldb_config);
            free(server_config);
            cJSON_Delete(root);
            return NULL;
        }
        reveldb_log_config_t *log_config =
            (reveldb_log_config_t *) malloc(sizeof(reveldb_log_config_t));
        if (log_config == NULL) {
            LOG_ERROR(("failed to make room for reveldb_log_config_t."));
            free(reveldb_config);
            free(server_config);
            free(db_config);
            cJSON_Delete(root);
            return NULL;
        }
        reveldb_ssl_config_t *ssl_config =
            (reveldb_ssl_config_t *) malloc(sizeof(reveldb_ssl_config_t));
        if (ssl_config == NULL) {
            LOG_ERROR(("failed to make room for reveldb_ssl_config_t."));
            free(reveldb_config);
            free(server_config);
            free(db_config);
            free(log_config);
            cJSON_Delete(root);
            return NULL;
        }

        iter = cJSON_GetObjectItem(server, "host");
        config_vlen = strlen(iter->valuestring);
        server_config->host =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(server_config->host, 0, (config_vlen + 1));
        strncpy(server_config->host, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(server, "rpcports");
        config_vlen = strlen(iter->valuestring);
        server_config->rpcports =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(server_config->rpcports, 0, (config_vlen + 1));
        strncpy(server_config->rpcports, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(server, "restports");
        config_vlen = strlen(iter->valuestring);
        server_config->restports =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(server_config->restports, 0, (config_vlen + 1));
        strncpy(server_config->restports, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(server, "https");
        server_config->https =
            (iter->valueint == 1) ? true : false;

        iter = cJSON_GetObjectItem(server, "backlog");
        server_config->backlog = iter->valueint;

        iter = cJSON_GetObjectItem(server, "username");
        config_vlen = strlen(iter->valuestring);
        server_config->username =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(server_config->username, 0, (config_vlen + 1));
        strncpy(server_config->username, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(server, "password");
        config_vlen = strlen(iter->valuestring);
        server_config->password =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(server_config->password, 0, (config_vlen + 1));
        strncpy(server_config->password, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(server, "datadir");
        config_vlen = strlen(iter->valuestring);
        server_config->datadir =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(server_config->datadir, 0, (config_vlen + 1));
        strncpy(server_config->datadir, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(server, "pidfile");
        config_vlen = strlen(iter->valuestring);
        server_config->pidfile =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(server_config->pidfile, 0, (config_vlen + 1));
        strncpy(server_config->pidfile, iter->valuestring, config_vlen);
        config_vlen = -1;

        db = cJSON_GetObjectItem(root, "db");

        iter = cJSON_GetObjectItem(db, "dbname");
        config_vlen = strlen(iter->valuestring);
        db_config->dbname =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(db_config->dbname, 0, (config_vlen + 1));
        strncpy(db_config->dbname, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(db, "lru_cache_size");
        db_config->lru_cache_size = iter->valueint;

        iter = cJSON_GetObjectItem(db, "create_if_missing");
        db_config->create_if_missing =
            (iter->valueint == 1) ? true : false;

        iter = cJSON_GetObjectItem(db, "error_if_exist");
        db_config->error_if_exist = (iter->valueint == 1) ? true : false;

        iter = cJSON_GetObjectItem(db, "write_buffer_size");
        db_config->write_buffer_size = iter->valueint;

        iter = cJSON_GetObjectItem(db, "paranoid_checks");
        db_config->paranoid_checks = (iter->valueint == 1) ? true : false;

        iter = cJSON_GetObjectItem(db, "max_open_files");
        db_config->max_open_files = iter->valueint;

        iter = cJSON_GetObjectItem(db, "block_size");
        db_config->block_size = iter->valueint;

        iter = cJSON_GetObjectItem(db, "block_restart_interval");
        db_config->block_restart_interval = iter->valueint;

        iter = cJSON_GetObjectItem(db, "compression");
        db_config->compression = (iter->valueint == 1) ? true : false;

        iter = cJSON_GetObjectItem(db, "verify_checksums");
        db_config->verify_checksums = (iter->valueint == 1) ? true : false;

        iter = cJSON_GetObjectItem(db, "fill_cache");
        db_config->fill_cache = (iter->valueint == 1) ? true : false;

        iter = cJSON_GetObjectItem(db, "sync");
        db_config->sync = (iter->valueint == 1) ? true : false;

        log = cJSON_GetObjectItem(root, "log");

        iter = cJSON_GetObjectItem(log, "level");
        config_vlen = strlen(iter->valuestring);
        log_config->level =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(log_config->level, 0, (config_vlen + 1));
        strncpy(log_config->level, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(log, "stream");
        config_vlen = strlen(iter->valuestring);
        log_config->stream =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(log_config->stream, 0, (config_vlen + 1));
        strncpy(log_config->stream, iter->valuestring, config_vlen);

        ssl = cJSON_GetObjectItem(root, "ssl");
        
        iter = cJSON_GetObjectItem(ssl, "key");
        config_vlen = strlen(iter->valuestring);
        ssl_config->key =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(ssl_config->key, 0, (config_vlen + 1));
        strncpy(ssl_config->key, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(ssl, "cert");
        config_vlen = strlen(iter->valuestring);
        ssl_config->cert =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(ssl_config->cert, 0, (config_vlen + 1));
        strncpy(ssl_config->cert, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(ssl, "capath");
        config_vlen = strlen(iter->valuestring);
        ssl_config->capath =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(ssl_config->capath, 0, (config_vlen + 1));
        strncpy(ssl_config->capath, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(ssl, "ciphers");
        config_vlen = strlen(iter->valuestring);
        ssl_config->ciphers =
            (char *) malloc(sizeof(char) * (config_vlen + 1));
        memset(ssl_config->ciphers, 0, (config_vlen + 1));
        strncpy(ssl_config->ciphers, iter->valuestring, config_vlen);
        config_vlen = -1;

        iter = cJSON_GetObjectItem(ssl, "ssl_ctx_timeout");
        ssl_config->ssl_ctx_timeout = iter->valueint;

        iter = cJSON_GetObjectItem(ssl, "verify_peer");
        ssl_config->verify_peer = (iter->valueint == 1) ? true : false;

        iter = cJSON_GetObjectItem(ssl, "verify_depth");
        ssl_config->verify_depth = iter->valueint;

        reveldb_config->server_config = server_config;
        reveldb_config->db_config = db_config;
        reveldb_config->log_config = log_config;
        reveldb_config->ssl_config = ssl_config;

        cJSON_Delete(root);
    }

    return reveldb_config;
}

reveldb_config_t *
reveldb_config_init(const char *file)
{
    assert(file != NULL);

    reveldb_config_t *reveldb_config = NULL;
    char *json_buf = _xconfig_load_config_body(file);

    reveldb_config = _xconfig_init_internal_config(json_buf);

    free(json_buf);
    return reveldb_config;
}

void
reveldb_config_fini(reveldb_config_t * config)
{
    assert(config != NULL);

    if (config->server_config != NULL) {
        if (config->server_config->host != NULL) {
            free(config->server_config->host);
            config->server_config->host = NULL;
        }if (config->server_config->rpcports != NULL) {
            free(config->server_config->rpcports);
            config->server_config->rpcports = NULL;
        }
        if (config->server_config->restports != NULL) {
            free(config->server_config->restports);
            config->server_config->restports = NULL;
        }
        if (config->server_config->username != NULL) {
            free(config->server_config->username);
            config->server_config->username = NULL;
        }
        if (config->server_config->password != NULL) {
            free(config->server_config->password);
            config->server_config->password = NULL;
        }
        if (config->server_config->datadir != NULL) {
            free(config->server_config->datadir);
            config->server_config->datadir = NULL;
        }
        if (config->server_config->pidfile != NULL) {
            free(config->server_config->pidfile);
            config->server_config->pidfile = NULL;
        }
        free(config->server_config);
    }

    if (config->db_config != NULL) {
        if (config->db_config->dbname != NULL) {
            free(config->db_config->dbname);
            config->db_config->dbname = NULL;
        }
        free(config->db_config);
    }

    if (config->log_config != NULL) {
        if (config->log_config->level != NULL) {
            free(config->log_config->level);
            config->log_config->level = NULL;
        }
        if (config->log_config->stream != NULL) {
            free(config->log_config->stream);
            config->log_config->stream = NULL;
        }
        free(config->log_config);
    }

    if (config->ssl_config != NULL) {
        if (config->ssl_config->key != NULL) {
            free(config->ssl_config->key);
            config->ssl_config->key = NULL;
        }
        if (config->ssl_config->cert != NULL) {
            free(config->ssl_config->cert);
            config->ssl_config->cert = NULL;
        }
        if (config->ssl_config->capath != NULL) {
            free(config->ssl_config->capath);
            config->ssl_config->capath = NULL;
        }
        if (config->ssl_config->ciphers != NULL) {
            free(config->ssl_config->ciphers);
            config->ssl_config->ciphers = NULL;
        }
        free(config->ssl_config);
    }

    free(config);
}

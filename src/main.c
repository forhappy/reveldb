/*
 * =============================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  main routine.
 *
 *        Created:  12/13/2012 05:04:08 PM
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

#include <leveldb/c.h>

#include <reveldb/reveldb.h>
#include <evhttpx/evhttpx.h>

#include "log.h"
#include "xconfig.h"

reveldb_log_t *reveldb_log = NULL;
struct rb_root reveldb = RB_ROOT;

#define REVELDB_MAX_KV_RESPONSE_BUFFER_SIZE (1024 * 1024 *2)
#define REVELDB_MAX_ERROR_RESPONSE_BUFFER_SIZE 1024

static void URI_rpc_new_cb(evhttpx_request_t *req, void *userdata);
static void URI_rpc_get_cb(evhttpx_request_t *req, void *userdata);
static void URI_rpc_set_cb(evhttpx_request_t *req, void *userdata);
static void URI_rpc_del_cb(evhttpx_request_t *req, void *userdata);

static char *
tinydb_jsonfy_kv_response(const char *key, const char *val)
{
    assert(key != NULL);
    assert(val != NULL);

    char *response = (char *)malloc(sizeof(char) *
            REVELDB_MAX_KV_RESPONSE_BUFFER_SIZE);
    if (response == NULL) {
        fprintf(stderr, "malloc error due to out of memory.\n");
        return NULL;
    } else {
        sprintf(response, "{\"key\": \"%s\",\"val\":\"%s\"}", key, val);
    }
    return response;
}

static char *
tinydb_jsonfy_error_response(const char *err, const char *msg)
{
    assert(err != NULL);
    assert(msg != NULL);

    char *response = (char *)malloc(sizeof(char) *
            REVELDB_MAX_ERROR_RESPONSE_BUFFER_SIZE);
    if (response == NULL) {
        fprintf(stderr, "malloc error due to out of memory.\n");
        return NULL;
    } else {
        sprintf(response, "{\"err\": \"%s\",\"msg\":\"%s\"}", err, msg);
    }
    return response;
}

static void
URI_rpc_new_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        response = tinydb_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = tinydb_jsonfy_error_response("HTTPMethodError",
                "HTTP method error, you may have to use GET to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }


    /* request query from client */
    evhttpx_query_t *query = req->uri->query;
    const char *dbname = evhttpx_kv_find(query, "db");
    reveldb_t *db = reveldb_init(dbname);
    reveldb_insert_db(&reveldb, db);
    if (db != NULL) {
        response = tinydb_jsonfy_kv_response("OK", "Create new database successfully.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    } else {
        response = tinydb_jsonfy_error_response("NoSuchKey", "No such key exists, please check agein.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    }

    return;
}

static void
URI_rpc_get_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;
    char *value = NULL;
    unsigned int value_len = -1;

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        response = tinydb_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = tinydb_jsonfy_error_response("HTTPMethodError",
                "HTTP method error, you may have to use GET to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        free(response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        return;
    }


    /* request query from client */
    evhttpx_query_t *query = req->uri->query;
    const char *key = evhttpx_kv_find(query, "key");
    const char *dbname = evhttpx_kv_find(query, "db");
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    value = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (!(db->instance->err != NULL)) {
        char *buf = (char *)malloc(sizeof(char) * (value_len + 1));
        memset(buf, 0, value_len + 1);
        snprintf(buf, value_len + 1, "%s", value);
        response = tinydb_jsonfy_kv_response(key, buf);
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        
        free(buf);
        free(value);
        free(response);
    } else {
        response = tinydb_jsonfy_error_response("NoSuchKey", "No such key exists, please check agein.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        free(response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
    }

    return;
}

static void
URI_rpc_set_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        response = tinydb_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = tinydb_jsonfy_error_response("HTTPMethodError",
                "HTTP method error, you may have to use GET to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        free(response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        return;
    }
    /* request query from client */
    evhttpx_query_t *query = req->uri->query;
    const char *key = evhttpx_kv_find(query, "key");
    const char *value = evhttpx_kv_find(query, "value");
    const char *dbname = evhttpx_kv_find(query, "db");
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    leveldb_put(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            value, strlen(value),
            &(db->instance->err));
    if (db->instance->err != NULL) {
        response = tinydb_jsonfy_error_response("SetKVError", "Set key-value pair error.");
    } else {
        response = tinydb_jsonfy_error_response("OK", "Set key successfully.");
    }

    evbuffer_add_printf(req->buffer_out, "%s", response);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);

    free(response);
    return;
}

static void
URI_rpc_del_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        response = tinydb_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = tinydb_jsonfy_error_response("HTTPMethodError",
                "HTTP method error, you may have to use GET to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        free(response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        return;
    }

    /* request query from client */
    evhttpx_query_t *query = req->uri->query;
    const char *key = evhttpx_kv_find(query, "key");
    const char *dbname = evhttpx_kv_find(query, "db");
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    leveldb_delete(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            &(db->instance->err));
    if (db->instance->err != NULL) {
        response = tinydb_jsonfy_error_response("DeleteKVError", "Delete key-value pair error.");
    } else {
        response = tinydb_jsonfy_error_response("OK", "Delete key successfully.");
    }
    evbuffer_add_printf(req->buffer_out, "%s", response);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);

    free(response);
    return;
}

int main(int argc, const char *argv[])
{
    reveldb_config_t *config = reveldb_config_init("./conf/reveldb.json");
    reveldb_log = reveldb_log_init(config->log_config->stream,
            config->log_config->level);
    reveldb_t * default_db = reveldb_init(config->db_config->dbname);
    reveldb_insert_db(&reveldb, default_db);

    LOG_DEBUG(("initializing reveldb server..."));

    evbase_t *evbase = event_base_new();
    evhttpx_t *httpx = evhttpx_new(evbase, NULL);
    evhttpx_callback_t * rpc_new_cb = NULL;
    evhttpx_callback_t * rpc_get_cb = NULL;
    evhttpx_callback_t * rpc_set_cb = NULL;
    evhttpx_callback_t * rpc_del_cb = NULL;

    rpc_new_cb = evhttpx_set_cb(httpx, "/rpc/new", URI_rpc_new_cb, NULL);
    rpc_get_cb = evhttpx_set_cb(httpx, "/rpc/get", URI_rpc_get_cb, NULL);
    rpc_set_cb = evhttpx_set_cb(httpx, "/rpc/set", URI_rpc_set_cb, NULL);
    rpc_del_cb = evhttpx_set_cb(httpx, "/rpc/del", URI_rpc_del_cb, NULL);

    evhttpx_bind_socket(httpx, "0.0.0.0", 8088, 1024);

    event_base_loop(evbase, 0);

    reveldb_log_free(reveldb_log);
    reveldb_config_fini(config);

    evhttpx_unbind_socket(httpx);
    evhttpx_callback_free(rpc_new_cb);
    evhttpx_callback_free(rpc_get_cb);
    evhttpx_callback_free(rpc_set_cb);
    evhttpx_callback_free(rpc_del_cb);

    evhttpx_free(httpx);
    event_base_free(evbase);

    return 0;
}

/*
 * =============================================================================
 *
 *       Filename:  rpc.c
 *
 *    Description:  reveldb rpc implementation
 *
 *        Created:  12/15/2012 04:44:03 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "rpc.h"
#include "main.h"
static char *
_rpc_jsonfy_kv_response(const char *key, const char *val)
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
_rpc_jsonfy_error_response(const char *err, const char *msg)
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
URI_rpc_void_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        response = _rpc_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = _rpc_jsonfy_error_response("HTTPMethodError",
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
        response = _rpc_jsonfy_kv_response("OK", "Create new database successfully.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    } else {
        response = _rpc_jsonfy_error_response("NoSuchKey", "No such key exists, please check agein.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    }

    return;
}

static void
URI_rpc_echo_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_head_cb(evhttpx_request_t *req, void *userdata)
{}


static void
URI_rpc_report_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_status_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_property_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_new_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        response = _rpc_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = _rpc_jsonfy_error_response("HTTPMethodError",
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
        response = _rpc_jsonfy_kv_response("OK", "Create new database successfully.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    } else {
        response = _rpc_jsonfy_error_response("NoSuchKey", "No such key exists, please check agein.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    }

    return;
}

static void
URI_rpc_compact_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_size_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_repair_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_destroy_cb(evhttpx_request_t *req, void *userdata)
{}


static void
URI_rpc_add_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_set_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        response = _rpc_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = _rpc_jsonfy_error_response("HTTPMethodError",
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
        response = _rpc_jsonfy_error_response("SetKVError", "Set key-value pair error.");
    } else {
        response = _rpc_jsonfy_error_response("OK", "Set key successfully.");
    }

    evbuffer_add_printf(req->buffer_out, "%s", response);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);

    free(response);
    return;
}

static void
URI_rpc_mset_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_append_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_prepend_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_insert_cb(evhttpx_request_t *req, void *userdata)
{}


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
        response = _rpc_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = _rpc_jsonfy_error_response("HTTPMethodError",
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
        response = _rpc_jsonfy_kv_response(key, buf);
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        
        free(buf);
        free(value);
        free(response);
    } else {
        response = _rpc_jsonfy_error_response("NoSuchKey", "No such key exists, please check agein.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        free(response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
    }

    return;
}

static void
URI_rpc_mget_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_seize_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_mseize_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_range_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_regex_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_incr_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_decr_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_cas_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_replace_cb(evhttpx_request_t *req, void *userdata)
{}


static void
URI_rpc_del_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        response = _rpc_jsonfy_error_response("ProtocalError",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
        return;
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        response = _rpc_jsonfy_error_response("HTTPMethodError",
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
        response = _rpc_jsonfy_error_response("DeleteKVError", "Delete key-value pair error.");
    } else {
        response = _rpc_jsonfy_error_response("OK", "Delete key successfully.");
    }
    evbuffer_add_printf(req->buffer_out, "%s", response);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);

    free(response);
    return;
}

static void
URI_rpc_mdel_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_remove_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_clear_cb(evhttpx_request_t *req, void *userdata)
{}


static void
URI_rpc_sync_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_check_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_exists_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_version_cb(evhttpx_request_t *req, void *userdata)
{}

reveldb_rpc_t *
reveldb_rpc_init()
{
    reveldb_rpc_t *rpc = (reveldb_rpc_t *)malloc(sizeof(reveldb_rpc_t));
    if (rpc == NULL) {
        LOG_ERROR(("failed to malloc reveldb_rpc_t."));
        return NULL;
    }
    reveldb_rpc_callbacks_t *callbacks = (reveldb_rpc_callbacks_t *)
        malloc(sizeof(reveldb_rpc_callbacks_t));
    if (callbacks == NULL) {
        LOG_ERROR(("failed to malloc reveldb_rpc_callbacks_t."));
        free(rpc);
        return NULL;
    }

    rpc->evbase = event_base_new();
    rpc->httpx = evhttpx_new(rpc->evbase, NULL);

    /* only for server status test. */
    callbacks->rpc_void_cb = evhttpx_set_cb(rpc->httpx, "/rpc/void", URI_rpc_void_cb, NULL);
    callbacks->rpc_echo_cb = evhttpx_set_cb(rpc->httpx, "/rpc/echo", URI_rpc_echo_cb, NULL);
    callbacks->rpc_head_cb = evhttpx_set_cb(rpc->httpx, "/rpc/head", URI_rpc_head_cb, NULL);
    
    /* reveldb reports and internal leveldb storage engine status. */
    callbacks->rpc_report_cb = evhttpx_set_cb(rpc->httpx, "/rpc/report", URI_rpc_report_cb, NULL);
    callbacks->rpc_status_cb = evhttpx_set_cb(rpc->httpx, "/rpc/status", URI_rpc_status_cb, NULL);
    callbacks->rpc_property_cb = evhttpx_set_cb(rpc->httpx, "/rpc/property", URI_rpc_property_cb, NULL);

    /* admin operations. */
    callbacks->rpc_new_cb = evhttpx_set_cb(rpc->httpx, "/rpc/new", URI_rpc_new_cb, NULL);
    callbacks->rpc_compact_cb = evhttpx_set_cb(rpc->httpx, "/rpc/compact", URI_rpc_compact_cb, NULL);
    callbacks->rpc_size_cb = evhttpx_set_cb(rpc->httpx, "/rpc/size", URI_rpc_size_cb, NULL);
    callbacks->rpc_repair_cb = evhttpx_set_cb(rpc->httpx, "/rpc/repair", URI_rpc_repair_cb, NULL);
    callbacks->rpc_destroy_cb = evhttpx_set_cb(rpc->httpx, "/rpc/destroy", URI_rpc_destroy_cb, NULL);

    /* set(C), get(R), update(U), delete(D) (CRUD)operations. */

    /* set related operations. */
    callbacks->rpc_add_cb = evhttpx_set_cb(rpc->httpx, "/rpc/add", URI_rpc_add_cb, NULL);
    callbacks->rpc_set_cb = evhttpx_set_cb(rpc->httpx, "/rpc/set", URI_rpc_set_cb, NULL);
    callbacks->rpc_mset_cb = evhttpx_set_cb(rpc->httpx, "/rpc/mset", URI_rpc_mset_cb, NULL);
    callbacks->rpc_append_cb = evhttpx_set_cb(rpc->httpx, "/rpc/append", URI_rpc_append_cb, NULL);
    callbacks->rpc_prepend_cb = evhttpx_set_cb(rpc->httpx, "/rpc/prepend", URI_rpc_prepend_cb, NULL);
    callbacks->rpc_insert_cb = evhttpx_set_cb(rpc->httpx, "/rpc/insert", URI_rpc_insert_cb, NULL);

    /* get related operations. */
    callbacks->rpc_get_cb = evhttpx_set_cb(rpc->httpx, "/rpc/get", URI_rpc_get_cb, NULL);
    callbacks->rpc_mget_cb = evhttpx_set_cb(rpc->httpx, "/rpc/mget", URI_rpc_mget_cb, NULL);
    callbacks->rpc_seize_cb = evhttpx_set_cb(rpc->httpx, "/rpc/seize", URI_rpc_seize_cb, NULL);
    callbacks->rpc_mseize_cb = evhttpx_set_cb(rpc->httpx, "/rpc/mseize", URI_rpc_mseize_cb, NULL);
    callbacks->rpc_range_cb = evhttpx_set_cb(rpc->httpx, "/rpc/range", URI_rpc_range_cb, NULL);
    callbacks->rpc_regex_cb = evhttpx_set_cb(rpc->httpx, "/rpc/regex", URI_rpc_regex_cb, NULL);

    /* update related operations. */
    callbacks->rpc_incr_cb = evhttpx_set_cb(rpc->httpx, "/rpc/incr", URI_rpc_incr_cb, NULL);
    callbacks->rpc_decr_cb = evhttpx_set_cb(rpc->httpx, "/rpc/decr", URI_rpc_decr_cb, NULL);
    callbacks->rpc_cas_cb = evhttpx_set_cb(rpc->httpx, "/rpc/cas", URI_rpc_cas_cb, NULL);
    callbacks->rpc_replace_cb = evhttpx_set_cb(rpc->httpx, "/rpc/replace", URI_rpc_replace_cb, NULL);

    /* delete related operations. */
    callbacks->rpc_del_cb = evhttpx_set_cb(rpc->httpx, "/rpc/del", URI_rpc_del_cb, NULL);
    callbacks->rpc_mdel_cb = evhttpx_set_cb(rpc->httpx, "/rpc/mdel", URI_rpc_mdel_cb, NULL);
    callbacks->rpc_remove_cb = evhttpx_set_cb(rpc->httpx, "/rpc/remove", URI_rpc_remove_cb, NULL);
    callbacks->rpc_clear_cb = evhttpx_set_cb(rpc->httpx, "/rpc/clear", URI_rpc_clear_cb, NULL);

    /* miscs operations. */
    callbacks->rpc_sync_cb = evhttpx_set_cb(rpc->httpx, "/rpc/sync", URI_rpc_sync_cb, NULL);
    callbacks->rpc_check_cb = evhttpx_set_cb(rpc->httpx, "/rpc/check", URI_rpc_check_cb, NULL);
    callbacks->rpc_exists_cb = evhttpx_set_cb(rpc->httpx, "/rpc/exists", URI_rpc_exists_cb, NULL);
    callbacks->rpc_version_cb = evhttpx_set_cb(rpc->httpx, "/rpc/version", URI_rpc_version_cb, NULL);

    rpc->callbacks = callbacks;

    return rpc;

}

void
reveldb_rpc_run(reveldb_rpc_t *rpc)
{

    assert(rpc != NULL);

    // evhttpx_bind_socket(rpc->httpx, "0.0.0.0", 8087, 1024);
    evhttpx_bind_socket(rpc->httpx, "0.0.0.0", 8088, 1024);

    event_base_loop(rpc->evbase, 0);
}

void
reveldb_rpc_stop(reveldb_rpc_t *rpc)
{
    assert(rpc != NULL);

    evhttpx_unbind_socket(rpc->httpx);

    evhttpx_callback_free(rpc->callbacks->rpc_void_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_echo_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_head_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_report_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_status_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_property_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_new_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_compact_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_size_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_repair_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_destroy_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_add_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_set_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_mset_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_append_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_prepend_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_insert_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_get_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_mget_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_seize_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_mseize_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_range_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_regex_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_incr_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_decr_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_cas_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_replace_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_del_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_mdel_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_remove_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_clear_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_sync_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_check_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_exists_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_version_cb);

    evhttpx_free(rpc->httpx);
    event_base_free(rpc->evbase);
    free(rpc->callbacks);
    free(rpc);
}

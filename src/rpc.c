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

#include <reveldb/rpc.h>

#include "log.h"
#include "cJSON.h"
#include "tstring.h"
#include "server.h"
#include "utility.h"

static int
_rpc_parse_kv_pair(evhttpx_kv_t *kv, void *arg)
{
    cJSON *root = (cJSON *)arg;
    unsigned int value = 0;
    if (strcmp(kv->val, "true") == 0) {
        cJSON_AddTrueToObject(root, kv->key);
        return 0;
    }
    if (strcmp(kv->val, "false") == 0) {
        cJSON_AddFalseToObject(root, kv->key);
        return 0;
    }
    if (safe_strtoul(kv->val, &value) == true) {
        cJSON_AddNumberToObject(root, kv->key, value);
    } else {
        cJSON_AddStringToObject(root, kv->key, kv->val);
    }

    return 0;
}

static cJSON * 
_rpc_jsonfy_kv_pairs(evhttpx_kvs_t *kvs)
{
    cJSON *root = cJSON_CreateObject();
    evhttpx_kvs_for_each(kvs, _rpc_parse_kv_pair, root);
    return root;
}

static char *
_rpc_jsonfy_quiet_response_on_kv(const char *key, const char *value)
{
    assert(key != NULL);
    assert(value != NULL);

    size_t extra_sapce = 64;
    size_t key_len = strlen(key);
    size_t value_len = strlen(value);
    size_t total = extra_sapce + key_len + value_len;

    char *out = (char *)malloc(sizeof(char) * (total));
    memset(out, 0, total);
    sprintf(out, "{\"%s\": \"%s\"}", key, value);

    return out;
}

static char *
_rpc_jsonfy_response_on_kv(const char *key, const char *value)
{
    assert(key != NULL);
    assert(value != NULL);
    char *out = NULL;
    char *now = gmttime_now();

    cJSON *root = cJSON_CreateObject();
    cJSON *kv = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "code", EVHTTPX_RES_OK);
    cJSON_AddStringToObject(root, "status", "OK");
    cJSON_AddStringToObject(root, "message", "Get key-value pair done.");
    cJSON_AddStringToObject(root, "date", now);
    cJSON_AddItemToObject(root, "kv", kv);
    cJSON_AddStringToObject(kv, key, value);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    free(now);
    cJSON_Delete(root);
    return out;
}

static int
_rpc_jsonfy_kv_pair(evhttpx_kv_t *kv, void *arg)
{
    cJSON *root = (cJSON *)arg;

    cJSON *jsonkv = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "kv", jsonkv);
    cJSON_AddStringToObject(jsonkv, kv->key, kv->val);
    return 0;
}

/* Note: the following function is different
 * from _rpc_jsonfy_kv_pairs. */
static cJSON * 
_rpc_jsonfy_kv_pairs2nd(evhttpx_kvs_t *kvs)
{
    cJSON *root = cJSON_CreateObject();
    evhttpx_kvs_for_each(kvs, _rpc_jsonfy_kv_pair, root);
    return root;
}

static char *
_rpc_jsonfy_quiet_response_on_kvs(evhttpx_kvs_t *kvs)
{
    assert(kvs != NULL);

    char *out = NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON *jsonkvs = _rpc_jsonfy_kv_pairs2nd(kvs);
    cJSON_AddItemToObject(root, "kvs", jsonkvs);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
    return out;
}

static char *
_rpc_jsonfy_response_on_kvs(evhttpx_kvs_t *kvs)
{
    assert(kvs != NULL);

    char *out = NULL;
    char *now = gmttime_now();

    cJSON *root = cJSON_CreateObject();
    cJSON *jsonkvs = _rpc_jsonfy_kv_pairs2nd(kvs);

    cJSON_AddNumberToObject(root, "code", EVHTTPX_RES_OK);
    cJSON_AddStringToObject(root, "status", "OK");
    cJSON_AddStringToObject(root, "message", "Get key-value pair done.");
    cJSON_AddStringToObject(root, "date", now);
    cJSON_AddItemToObject(root, "kvs", jsonkvs);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    free(now);
    cJSON_Delete(root);
    return out;
}

static char *
_rpc_jsonfy_quiet_response(unsigned int code)
{
    char *response = (char *)malloc(sizeof(char) * 32);
    memset(response, 0, 32);
    sprintf(response, "{\"code\": %d}", code);
    return response;
}

static char *
_rpc_jsonfy_general_response(
        unsigned int code,
        const char *status,
        const char *message)
{
    assert(status != NULL);
    assert(message != NULL);

    char *out = NULL;
    char *now = gmttime_now();

    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddStringToObject(root, "status", status);
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddStringToObject(root, "date", now);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    free(now);
    cJSON_Delete(root);
    return out;
}

// error response.
// {
//     "code": 404, // HTTP Code
//     "status": "Not Found", // description
//     "message": "Key does not exists.", // more detailed description.
//     "date": "2012-12-12 12:12:12",
//     "request": { // request info, optional
//         "headers": { // standard HTTP request headers, optional
//             "Host": "http://www.example.com:8088",
//             "User-Agent": "Mozilla/5.0...",
//             "Other-Headers": "Values",
//         },
//         "arguments": { // user arguments, optional
//             "key": "hello",
//             "db": "default",
//             "expires": "3600",
//             "others": "values"
//         }
//     }
// }
static char *
_rpc_jsonfy_response_on_error(evhttpx_request_t *req,
        unsigned int code,
        const char *status,
        const char *message)
{
    /* json formatted response. */
    char *now = NULL;
    char *response = NULL;
    cJSON *root = NULL;
    cJSON *request = NULL;
    /* request headers from client */
    evhttpx_headers_t *request_headers = req->headers_in;
    /* request query pairs. */
    evhttpx_query_t *uri_query = req->uri->query;

    now = gmttime_now();

    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddStringToObject(root, "status", status);
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddStringToObject(root, "date", now);
    cJSON_AddItemToObject(root,"request", request = cJSON_CreateObject());
    cJSON_AddItemToObject(request,"headers",
            _rpc_jsonfy_kv_pairs(request_headers));
    cJSON_AddItemToObject(request,"arguments",
            _rpc_jsonfy_kv_pairs(uri_query));

    /* unformatted json has less data. */
    response = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
    free(now);
    return response;
}

static char *
_rpc_jsonfy_response_on_sanity_check(
        unsigned int code,
        const char *status,
        const char *message)
{
    assert(status != NULL);
    assert(message != NULL);

    char *out = NULL;
    char *now = gmttime_now();

    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddStringToObject(root, "status", status);
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddStringToObject(root, "date", now);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    free(now);
    cJSON_Delete(root);
    return out;
}

static char *
_rpc_proto_and_method_sanity_check(
        evhttpx_request_t *req,
        unsigned int *code)
{
    assert(req != NULL);

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        *code = EVHTTPX_RES_BADREQ;
        return _rpc_jsonfy_response_on_sanity_check(
                EVHTTPX_RES_BADREQ,
                "Bad Request",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        *code = EVHTTPX_RES_METHNALLOWED;
        return _rpc_jsonfy_response_on_sanity_check(
                EVHTTPX_RES_METHNALLOWED,
                "Method Not Allowed",
                "HTTP method error, you may have to use GET to do request.");

    }

    *code = EVHTTPX_RES_OK;
    return NULL;
}

/* check the sanity of a query parameter, which is specified by @param_name,
 * if it is a valid request parameter, the pointer to it will be stored
 * in @param and return NULL, otherwise *@param will NULL but return error
 * message that will be sent to the client.
 * */
static char *
_rpc_query_param_sanity_check(
        evhttpx_request_t *req,
        const char **param,
        const char *param_name,
        const char *errmsg_if_invalid)
{
    assert(req != NULL);
    
    evhttpx_query_t *query = req->uri->query;
    
    *param = evhttpx_kv_find(query, param_name);

    if (*param == NULL) {
        return _rpc_jsonfy_response_on_sanity_check(
                EVHTTPX_RES_BADREQ,
                "Bad Request",
                errmsg_if_invalid);
    }

    return NULL;
}

/* check user query is quiet or not, valid quiet query are "quiet=1",
 * "quiet=true", "quiet=0" and "quiet=false".
 *
 * if user specified "quiet" param, reveldb will only send HTTP code
 * to the client, otherwise reveldb will send the normal response.
 *
 * if "quiet=1" or "quiet=true", _rpc_query_quiet_check will return true,
 * otherwise return false.
 * */
static bool 
_rpc_query_quiet_check(evhttpx_request_t *req)
{
    assert(req != NULL);
    const char *quiet = NULL;
    
    evhttpx_query_t *query = req->uri->query;
    
    quiet = evhttpx_kv_find(query, "quiet");

    if (quiet != NULL) {
        if (strcmp(quiet, "1") == 0 || strcmp(quiet, "true") == 0) {
            return true;
        }
        if (strcmp(quiet, "0") == 0 || strcmp(quiet, "false") == 0) {
            return false;
        }
    }
    return false;
}

static void 
URI_rpc_void_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    char *response = NULL;

    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, code);
        free(response);
        return;
    }

    response = _rpc_jsonfy_response_on_sanity_check(
            EVHTTPX_RES_OK,
            "OK",
            "Reveldb RPC is healthy! :-)");
    evbuffer_add_printf(req->buffer_out, "%s", response);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);
    free(response);
    return;
}

static void
URI_rpc_echo_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    char *now = NULL;
    char *response = NULL;
    cJSON *root = NULL;
    cJSON *request = NULL;
    /* request headers from client */
    evhttpx_headers_t *request_headers = req->headers_in;
    /* request query pairs. */
    evhttpx_query_t *uri_query = req->uri->query;

    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, code);
        free(response);
        return;
    }

    now = gmttime_now();

    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "code", EVHTTPX_RES_OK);
    cJSON_AddStringToObject(root, "status", "OK");
    cJSON_AddStringToObject(root, "message",
            "Reveldb echoed the HTTP headers and query arguments of your request.");
    cJSON_AddStringToObject(root, "date", now);
    cJSON_AddItemToObject(root,"request", request = cJSON_CreateObject());
    cJSON_AddItemToObject(request,"headers",
            _rpc_jsonfy_kv_pairs(request_headers));
    cJSON_AddItemToObject(request,"arguments",
            _rpc_jsonfy_kv_pairs(uri_query));

    /* unformatted json has less data. */
    response = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
    evbuffer_add_printf(req->buffer_out, "%s", response);
    evhttpx_send_reply(req, code);

    free(now);
    free(response);
    return;
}

static void
URI_rpc_head_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    char *response = NULL;

    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, code);
        free(response);
        return;
    }

    response = _rpc_jsonfy_response_on_sanity_check(
            EVHTTPX_RES_OK,
            "OK",
            "Reveldb RPC is healthy! :-)");
    evbuffer_add_printf(req->buffer_out, "%s", response);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);
    free(response);
    return;
}


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
    unsigned int code = 0;
    bool is_quiet = false;
    const char *dbname = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, code);
        free(response);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req, &dbname, "db",
            "Database not specified, please check.");
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_BADREQ);
        free(response);
        return;
    }

    /* init new leveldb instance and insert into reveldb. */
    reveldb_t *db = reveldb_init(dbname, reveldb_config);
    reveldb_insert_db(&reveldb, db);

    if (db != NULL) {
        if (is_quiet == true) {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        } else response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Create new leveldb instance done.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    } else {
        response = _rpc_jsonfy_response_on_error(req,
                EVHTTPX_RES_SERVERR, "Internal Server Error", "Failed to create new leveldb instance.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_SERVERR);
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
    unsigned int code = 0;
    const char *key = NULL;
    const char *value = NULL;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, code);
        free(response);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to set.");
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_BADREQ);
        free(response);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &value, "value",
            "You have to set value along with the key you specified.");
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_BADREQ);
        free(response);
        return;
    }

    response = _rpc_query_param_sanity_check(req, &dbname, "db",
            "Database not specified, use the default database.");
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_NOTFOUND);
        free(response);
        return;
    }

    leveldb_put(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            value, strlen(value),
            &(db->instance->err));
    if (db->instance->err != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                     "Internal Server Error", db->instance->err);
        }
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_SERVERR);
        xleveldb_reset_err(db->instance);
        free(response);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Set key-value pair done.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    }

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
    unsigned int code = 0;
    bool is_quiet = false;
    char *value = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *dbname = NULL;
    unsigned int value_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, code);
        free(response);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to get.");
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_BADREQ);
        free(response);
        return;
    }

    response = _rpc_query_param_sanity_check(req, &dbname, "db",
            "Database not specified, use the default database.");
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_NOTFOUND);
        free(response);
        return;
    }
   
    value = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        char *buf = (char *)malloc(sizeof(char) * (value_len + 1));
        memset(buf, 0, value_len + 1);
        snprintf(buf, value_len + 1, "%s", value);
       
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_kv(key, buf);
        } else {
            response = _rpc_jsonfy_quiet_response_on_kv(key, buf);
        }
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        
        free(buf);
        free(value);
        free(response);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key value pair not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key value pair not found.");
        }
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_NOTFOUND);
        free(response);
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
    unsigned int code = 0;
    char *response = NULL;
    const char *key = NULL;
    const char *dbname = NULL;
    bool is_quiet = false;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, code);
        free(response);
        return;
    }
    
    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to delete.");
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_BADREQ);
        free(response);
        return;
    }
 
    response = _rpc_query_param_sanity_check(req, &dbname, "db",
            "Database not specified, use the default database.");
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_NOTFOUND);
        free(response);
        return;
    }

    leveldb_delete(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            &(db->instance->err));
    if (db->instance->err != NULL) {
        if (is_quiet == false ) {
        response = _rpc_jsonfy_response_on_error(req,
                EVHTTPX_RES_SERVERR, 
                "Internal Server Error",
                db->instance->err);
        } else {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR, 
                "Internal Server Error",
                db->instance->err);
        }
        xleveldb_reset_err(db->instance);
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_SERVERR);
        free(response);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOCONTENT,
                    "No Content", "Delete key done.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_NOCONTENT);
        }
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, EVHTTPX_RES_OK);
        free(response);
    }

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
    evhttpx_bind_socket(rpc->httpx, "0.0.0.0", 9000, 1024);

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

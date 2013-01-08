/*
 * =============================================================================
 *
 *       Filename:  rest.c
 *
 *    Description:  reveldb native rest api implementation.
 *
 *        Created:  01/08/2013 09:11:34 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <reveldb/rest.h>
#include <regex/regex.h>

#include "log.h"
#include "iter.h"
#include "snapshot.h"
#include "writebatch.h"
#include "cJSON.h"
#include "tstring.h"
#include "server.h"
#include "utility.h"
#include "uuid/uuid.h"

# define eq(x, y) (tolower(x) == tolower(y))

static void
_rest_fill_ports(reveldb_rest_t *rest, const char *ports)
{
    assert(ports != NULL);

    char *pch = NULL;
    uint32_t port = 0;
    size_t ports_len = strlen(ports);

    rest->num_ports = 0;

    char *tmpports = (char *)malloc(sizeof(char) * (ports_len + 1));
    memset(tmpports, 0, (ports_len + 1));
    strncpy(tmpports, ports, ports_len);

    pch = strtok(tmpports, ",");
    while (pch != NULL) {
        if (safe_strtoul(pch, &port)) {
            rest->ports[rest->num_ports++] = port;
        }
        pch = strtok(NULL, ",");
    }
    return;
}

static int
_rest_parse_kv_pair(evhttpx_kv_t *kv, void *arg)
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
_rest_jsonfy_kv_pairs(evhttpx_kvs_t *kvs)
{
    cJSON *root = cJSON_CreateObject();
    evhttpx_kvs_for_each(kvs, _rest_parse_kv_pair, root);
    return root;
}

static char *
_rest_jsonfy_quiet_response_on_kv(const char *key, const char *value)
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
_rest_jsonfy_quiet_response_on_kv_with_len(
        const char *key, size_t key_len,
        const char *value, size_t value_len)
{
    assert(key != NULL);
    assert(value != NULL);

    size_t extra_sapce = 64;
    size_t total = extra_sapce + key_len + value_len;

    char *out = (char *)malloc(sizeof(char) * (total));
    memset(out, 0, total);

    // FIXME: this is very tricky, constructing response 
    // by concatenating pieces of str.
    strncpy(out, "{\"", 2);
    strncpy(out + 2, key, key_len);
    strncpy(out + 2 + key_len, "\": \"", 4);
    strncpy(out + 2 + key_len + 4, value, value_len);
    strncpy(out + 2 + key_len + 4 + value_len, "\"}", 2);

    return out;
}

/* alter the default message when request kv,
 * this may be useful when seizing a specified kv pair,
 * in that case you probably want to change your message
 * to the client other than the default one.
 * */
static char *
_rest_jsonfy_msgalt_response_on_kv_with_len(
        const char *key, size_t key_len,
        const char *value, size_t value_len,
        const char *message)
{
    assert(key != NULL);
    assert(value != NULL);
    char *out = NULL;
    char *now = gmttime_now();

    cJSON *root = cJSON_CreateObject();
    cJSON *kv = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "code", EVHTTPX_RES_OK);
    cJSON_AddStringToObject(root, "status", "OK");
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddStringToObject(root, "date", now);
    cJSON_AddItemToObject(root, "kv", kv);
    cJSON_AddStringToObjectWithLength(kv, key, key_len, value, value_len);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    free(now);
    cJSON_Delete(root);
    return out;
}

static char *
_rest_jsonfy_response_on_kv_with_len(
        const char *key, size_t key_len,
        const char *value, size_t value_len)
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
    cJSON_AddStringToObjectWithLength(kv, key, key_len, value, value_len);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    free(now);
    cJSON_Delete(root);
    return out;
}

static char *
_rest_jsonfy_response_on_kv(const char *key, const char *value)
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
_rest_jsonfy_kv_pair(evhttpx_kv_t *kv, void *arg)
{
    cJSON *root = (cJSON *)arg;

    cJSON *jsonkv = cJSON_CreateObject();
    cJSON_AddItemToArray(root, jsonkv);
    cJSON_AddStringToObject(jsonkv, kv->key, kv->val);
    return 0;
}

/* Note: the following function is different
 * from _rest_jsonfy_kv_pairs. */
static cJSON * 
_rest_jsonfy_kv_pairs2nd(evhttpx_kvs_t *kvs)
{
    cJSON *root = cJSON_CreateArray();
    evhttpx_kvs_for_each(kvs, _rest_jsonfy_kv_pair, root);
    return root;
}

static char *
_rest_jsonfy_quiet_response_on_kvs(evhttpx_kvs_t *kvs)
{
    assert(kvs != NULL);

    char *out = NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON *jsonkvs = _rest_jsonfy_kv_pairs2nd(kvs);
    cJSON_AddItemToObject(root, "kvs", jsonkvs);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
    return out;
}

static char *
_rest_jsonfy_response_on_kvs(evhttpx_kvs_t *kvs)
{
    assert(kvs != NULL);

    char *out = NULL;
    char *now = gmttime_now();

    cJSON *root = cJSON_CreateObject();
    cJSON *jsonkvs = _rest_jsonfy_kv_pairs2nd(kvs);

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
_rest_jsonfy_quiet_response(unsigned int code)
{
    char *response = (char *)malloc(sizeof(char) * 32);
    memset(response, 0, 32);
    sprintf(response, "{\"code\": %d}", code);
    return response;
}

static char *
_rest_jsonfy_general_response(
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
_rest_jsonfy_response_on_error(evhttpx_request_t *req,
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
            _rest_jsonfy_kv_pairs(request_headers));
    cJSON_AddItemToObject(request,"arguments",
            _rest_jsonfy_kv_pairs(uri_query));

    /* unformatted json has less data. */
    response = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
    free(now);
    return response;
}

static char *
_rest_jsonfy_response_on_sanity_check(
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
_rest_jsonfy_version_response(int major, int minor, bool quiet)
{
    char *out = NULL;
    char *now = NULL;
    size_t out_len = 64;

    if (quiet == false) {
        now = gmttime_now();
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "code", EVHTTPX_RES_OK);
        cJSON_AddStringToObject(root, "status", "OK");
        cJSON_AddStringToObject(root, "message", "Get leveldb storage engine version.");
        cJSON_AddStringToObject(root, "date", now);
        cJSON_AddNumberToObject(root, "major", major);
        cJSON_AddNumberToObject(root, "minor", minor);
        // out = cJSON_Print(root);
        /* unformatted json has less data. */
        out = cJSON_PrintUnformatted(root);
        free(now);
        cJSON_Delete(root);
        return out;
    } else {
        out = (char *)malloc(sizeof(char) * out_len);
        memset(out, 0, out_len);
        sprintf(out, "{\"major\": %d, \"minor\": %d}", major, minor);
        return out;
    }
}

static char *
_rest_proto_and_method_sanity_check(
        evhttpx_request_t *req,
        unsigned int *code)
{
    assert(req != NULL);

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        *code = EVHTTPX_RES_BADREQ;
        return _rest_jsonfy_response_on_sanity_check(
                EVHTTPX_RES_BADREQ,
                "Bad Request",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != http_method_GET) {
        *code = EVHTTPX_RES_METHNALLOWED;
        return _rest_jsonfy_response_on_sanity_check(
                EVHTTPX_RES_METHNALLOWED,
                "Method Not Allowed",
                "HTTP method error, you may have to use GET to do request.");

    }

    *code = EVHTTPX_RES_OK;
    return NULL;
}

static char *
_rest_proto_and_method_sanity_check2nd(
        evhttpx_request_t *req,
        int expected, /**< expected method. */
        unsigned int *code)
{
    assert(req != NULL);

    /* HTTP protocol used */
    evhttpx_proto proto = req->proto;
    if (proto != evhttpx_PROTO_11) {
        *code = EVHTTPX_RES_BADREQ;
        return _rest_jsonfy_response_on_sanity_check(
                EVHTTPX_RES_BADREQ,
                "Bad Request",
                "Protocal error, you may have to use HTTP/1.1 to do request.");
    }
    /* request method. */
    int method= evhttpx_request_get_method(req);
    if (method != expected) {
        *code = EVHTTPX_RES_METHNALLOWED;
        return _rest_jsonfy_response_on_sanity_check(
                EVHTTPX_RES_METHNALLOWED,
                "Method Not Allowed",
                "HTTP method error, you may have to use the right method to do request.");

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
_rest_query_param_sanity_check(
        evhttpx_request_t *req,
        const char **param,
        const char *param_name,
        const char *errmsg_if_invalid)
{
    assert(req != NULL);
    
    evhttpx_query_t *query = req->uri->query;
    
    *param = evhttpx_kv_find(query, param_name);

    if (*param == NULL) {
        return _rest_jsonfy_response_on_sanity_check(
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
 * if "quiet=1" or "quiet=true", _rest_query_quiet_check will return true,
 * otherwise return false.
 * */
static bool 
_rest_query_quiet_check(evhttpx_request_t *req)
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
_rest_query_database_check(
        evhttpx_request_t *req,
        const char **dbname)
{
    assert(req != NULL);

    evhttpx_query_t *query = req->uri->query;
    *dbname = evhttpx_kv_find(query, "db");
    return;
}

static char *
_rest_pattern_unescape(const char *pattern)
{
    return safe_urldecode(pattern);
}

static char *
_rest_do_mget(evhttpx_request_t *req, reveldb_t *db, bool quiet)
{
    assert(req != NULL);
    cJSON *root = NULL;
    cJSON *keys = NULL;
    char *response = NULL;
    int items = 0;
    int arridx = -1;
    size_t value_len = -1;
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    /* buffer containing data from client */
    evbuf_t *buffer_in = req->buffer_in;
    size_t buffer_in_size = -1;
    char *inbuffer =
        evbuffer_readln(buffer_in, &buffer_in_size, EVBUFFER_EOL_CRLF);

    root = cJSON_Parse(inbuffer);
    if (!root) {
        if (quiet == false) {
            response = _rest_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    keys = cJSON_GetObjectItem(root, "keys");
    if (keys != NULL) {
        items = cJSON_GetArraySize(keys);
        for (arridx = 0; arridx < items; arridx++) {
            char *key = cJSON_GetArrayItem(keys, arridx)->valuestring;
            char *value = leveldb_get(
                    db->instance->db,
                    db->instance->roptions,
                    key, strlen(key),
                    &value_len,
                    &(db->instance->err));
            if (value != NULL) {
                evhttpx_kv_t *kv =
                    evhttpx_kvlen_new(key, strlen(key), value, value_len, 1, 1);
                evhttpx_kvs_add_kv(kvs, kv);
                leveldb_free(value);
            }
        }
    } else {
        if (quiet == false) {
            response = _rest_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    if (quiet == false) {
        response = _rest_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rest_jsonfy_quiet_response_on_kvs(kvs);
    }
    return response;
}

static char *
_rest_do_mseize(evhttpx_request_t *req, reveldb_t *db, bool quiet)
{
    assert(req != NULL);
    cJSON *root = NULL;
    cJSON *keys = NULL;
    char *response = NULL;
    int items = 0;
    int arridx = -1;
    size_t value_len = -1;
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    /* buffer containing data from client */
    evbuf_t *buffer_in = req->buffer_in;
    size_t buffer_in_size = -1;
    char *inbuffer =
        evbuffer_readln(buffer_in, &buffer_in_size, EVBUFFER_EOL_CRLF);

    root = cJSON_Parse(inbuffer);
    if (!root) {
        if (quiet == false) {
            response = _rest_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    keys = cJSON_GetObjectItem(root, "keys");
    if (keys != NULL) {
        items = cJSON_GetArraySize(keys);
        for (arridx = 0; arridx < items; arridx++) {
            char *key = cJSON_GetArrayItem(keys, arridx)->valuestring;
            char *value = leveldb_get(
                    db->instance->db,
                    db->instance->roptions,
                    key, strlen(key),
                    &value_len,
                    &(db->instance->err));
            if (value != NULL) {
                evhttpx_kv_t *kv =
                    evhttpx_kvlen_new(key, strlen(key), value, value_len, 1, 1);
                evhttpx_kvs_add_kv(kvs, kv);
                leveldb_delete(
                        db->instance->db,
                        db->instance->woptions,
                        key, strlen(key),
                        &(db->instance->err));
                if (db->instance->err != NULL)
                    xleveldb_reset_err(db->instance);
                leveldb_free(value);
            }
        }
    } else {
        if (quiet == false) {
            response = _rest_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    if (quiet == false) {
        response = _rest_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rest_jsonfy_quiet_response_on_kvs(kvs);
    }
    return response;
}

static char *
_rest_do_mset(evhttpx_request_t *req, reveldb_t *db, bool quiet)
{
    assert(req != NULL);
    cJSON *root = NULL;
    cJSON *keys = NULL;
    cJSON *values = NULL;
    char *response = NULL;
    int items = 0;
    int arridx = -1;

    /* buffer containing data from client */
    evbuf_t *buffer_in = req->buffer_in;
    size_t buffer_in_size = -1;
    char *inbuffer =
        evbuffer_readln(buffer_in, &buffer_in_size, EVBUFFER_EOL_CRLF);

    root = cJSON_Parse(inbuffer);
    if (!root) {
        if (quiet == false) {
            response = _rest_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    keys = cJSON_GetObjectItem(root, "keys");
    values = cJSON_GetObjectItem(root, "values");
    if (keys != NULL && values != NULL) {
        if ((items = cJSON_GetArraySize(keys)) != cJSON_GetArraySize(values)) {
            if (quiet == false) {
                response = _rest_jsonfy_response_on_error(
                        req, EVHTTPX_RES_BADREQ, "Bad Request",
                        "Keys and values does not equal.");
            } else {
                response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
            }
            return response;
        }
        for (arridx = 0; arridx < items; arridx++) {
            char *key = cJSON_GetArrayItem(keys, arridx)->valuestring;
            char *value = cJSON_GetArrayItem(values, arridx)->valuestring;
            leveldb_put(
                    db->instance->db,
                    db->instance->woptions,
                    key, strlen(key),
                    value, strlen(value),
                    &(db->instance->err));
            if (db->instance->err != NULL) {
                if (quiet == false) {
                    response = _rest_jsonfy_response_on_error(req,
                            EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
                } else {
                    response = _rest_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                            "Internal Server Error", db->instance->err);
                }
                xleveldb_reset_err(db->instance);
                return response;
            }
        }
    } else {
        if (quiet == false) {
            response = _rest_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    if (quiet == false) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Multiple set done.");
    } else {
        response = _rest_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }
    return response;
}

static char *
_rest_do_mdel(evhttpx_request_t *req, reveldb_t *db, bool quiet)
{
    assert(req != NULL);
    cJSON *root = NULL;
    cJSON *keys = NULL;
    char *response = NULL;
    int items = 0;
    int arridx = -1;

    /* buffer containing data from client */
    evbuf_t *buffer_in = req->buffer_in;
    size_t buffer_in_size = -1;
    char *inbuffer =
        evbuffer_readln(buffer_in, &buffer_in_size, EVBUFFER_EOL_CRLF);

    root = cJSON_Parse(inbuffer);
    if (!root) {
        if (quiet == false) {
            response = _rest_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    keys = cJSON_GetObjectItem(root, "keys");
    if (keys != NULL) {
        items = cJSON_GetArraySize(keys);
        for (arridx = 0; arridx < items; arridx++) {
            char *key = cJSON_GetArrayItem(keys, arridx)->valuestring;
            leveldb_delete(
                    db->instance->db,
                    db->instance->woptions,
                    key, strlen(key),
                    &(db->instance->err));
            if (db->instance->err != NULL) {
                if (quiet == false) {
                    response = _rest_jsonfy_response_on_error(req,
                            EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
                } else {
                    response = _rest_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                            "Internal Server Error", db->instance->err);
                }
                xleveldb_reset_err(db->instance);
                return response;
            }
        }
    } else {
        if (quiet == false) {
            response = _rest_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    if (quiet == false) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Multiple set done.");
    } else {
        response = _rest_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }
    return response;
}

static void
_rest_send_reply(evhttpx_request_t *req,
        char *response, unsigned int code)
{
    if (req == NULL) return;
    if (response != NULL) {
        evbuffer_add_printf(req->buffer_out, "%s", response);
        evhttpx_send_reply(req, code);
        free(response);
    }
    return;
}

static void
URI_rest_new_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    char *response = NULL;
    unsigned int code = 0;
    bool is_quiet = false;
    const char *dbname = NULL;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    response = _rest_query_param_sanity_check(req, &dbname, "db",
            "Database not specified, please check.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    /* init new leveldb instance and insert it into reveldb. */
    reveldb_t *db = reveldb_init(dbname, reveldb_config);
    reveldb_insert_db(&reveldb, db);

    if (db != NULL) {
        if (is_quiet == true) {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_OK);
        } else response = _rest_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Created new leveldb instance OK.");
        _rest_send_reply(req, response, EVHTTPX_RES_OK);
    } else {
        response = _rest_jsonfy_response_on_error(req,
                EVHTTPX_RES_SERVERR, "Internal Server Error",
                "Failed to create new leveldb instance.");
        _rest_send_reply(req, response, EVHTTPX_RES_SERVERR);
    }

    return;
}

static void
URI_rest_add_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *key = NULL;
    const char *value = NULL;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    response = _rest_query_param_sanity_check(req,
            &key, "key", "Please specify which key to add.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rest_query_param_sanity_check(req, &value, "value",
            "Please set value along with the key you've specified.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
            response = _rest_jsonfy_response_on_error(req,
                    EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
        } else {
             response = _rest_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                     "Internal Server Error", db->instance->err);
        }
        _rest_send_reply(req, response, EVHTTPX_RES_SERVERR);
        xleveldb_reset_err(db->instance);
    } else {
        if (is_quiet == false) {
            response = _rest_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Set key-value pair done.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rest_send_reply(req, response, EVHTTPX_RES_OK);
    }

    return;
}

static void
URI_rest_set_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *key = NULL;
    const char *value = NULL;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    response = _rest_query_param_sanity_check(req,
            &key, "key", "Please specify which key to set.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rest_query_param_sanity_check(req, &value, "value",
            "Please set value along with the key you've specified.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
            response = _rest_jsonfy_response_on_error(req,
                    EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
        } else {
             response = _rest_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                     "Internal Server Error", db->instance->err);
        }
        xleveldb_reset_err(db->instance);
        _rest_send_reply(req, response, EVHTTPX_RES_SERVERR);
    } else {
        if (is_quiet == false) {
            response = _rest_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Set key-value pair done.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rest_send_reply(req, response, EVHTTPX_RES_OK);
    }

    return;
}

static void
URI_rest_mset_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rest_proto_and_method_sanity_check2nd(req, http_method_POST, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    response = _rest_do_mset(req, db, is_quiet);
    _rest_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rest_get_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *dbname = NULL;
    unsigned int value_len = 0;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    response = _rest_query_param_sanity_check(req,
            &key, "key", "Please specify which key to get.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }


    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        if (is_quiet == false) {
            response = _rest_jsonfy_response_on_kv_with_len(
                    key, strlen(key), value, value_len);
        } else {
            response = _rest_jsonfy_quiet_response_on_kv_with_len(
                    key, strlen(key), value, value_len);
        }
        free(value);
        _rest_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rest_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

static void
URI_rest_mget_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rest_proto_and_method_sanity_check2nd(req, http_method_POST, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    response = _rest_do_mget(req, db, is_quiet);
    _rest_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rest_seize_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *dbname = NULL;
    unsigned int value_len = 0;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    response = _rest_query_param_sanity_check(req,
            &key, "key", "Please specify which key to seize.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        leveldb_delete(
                db->instance->db,
                db->instance->woptions,
                key, strlen(key),
                &(db->instance->err));
        if (db->instance->err == NULL) {
            if (is_quiet == false) {
                response = _rest_jsonfy_msgalt_response_on_kv_with_len(
                        key, strlen(key), value, value_len,
                        "Seize key value pair OK, but note that "
                        "you have just deleted the pair on reveldb server");
            } else {
                response = _rest_jsonfy_quiet_response_on_kv_with_len(
                        key, strlen(key), value, value_len);
            }
        } else {
             if (is_quiet == false) {
                response = _rest_jsonfy_msgalt_response_on_kv_with_len(
                        key, strlen(key), value, value_len,
                        "Get kv pair OK, but note that you cannot delete "
                        "the pair on reveldb server for some reasons");
            } else {
                response = _rest_jsonfy_quiet_response_on_kv_with_len(
                        key, strlen(key), value, value_len);
            }
             xleveldb_reset_err(db->instance);
        } 
        free(value);
        _rest_send_reply(req, response, EVHTTPX_RES_OK);
    } else {
        if (is_quiet == false) {
            response = _rest_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key value pair not found.");
        } else {
             response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key value pair not found.");
        }
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

static void
URI_rest_mseize_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rest_proto_and_method_sanity_check2nd(req, http_method_POST, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    response = _rest_do_mseize(req, db, is_quiet);
    _rest_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rest_cas_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *oval = NULL;
    const char *nval = NULL;
    const char *dbname = NULL;
    unsigned int value_len = 0;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    response = _rest_query_param_sanity_check(req,
            &key, "key", "Please specify which key to get.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    response = _rest_query_param_sanity_check(req,
            &oval, "oval", "Please specify old value to compare.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    response = _rest_query_param_sanity_check(req,
            &nval, "nval", "Please specify new value to swap.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        if (value_len != strlen(oval)) {
            if (is_quiet == false) {
                response = _rest_jsonfy_response_on_error(req,
                        EVHTTPX_RES_NOTFOUND, "Not Found", "Value not found.");
            } else {
                response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                        "Not Found", "Value not found.");
            }
            _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
            return;
        } else {
            if (strncmp(value, oval, value_len) == 0) {
                leveldb_put(
                        db->instance->db,
                        db->instance->woptions,
                        key, strlen(key),
                        nval, strlen(nval),
                        &(db->instance->err));
                if (db->instance->err != NULL) {
                    if (is_quiet == false) {
                        response = _rest_jsonfy_response_on_error(req,
                                EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
                    } else {
                        response = _rest_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                                "Internal Server Error", db->instance->err);
                    }
                    xleveldb_reset_err(db->instance);
                    _rest_send_reply(req, response, EVHTTPX_RES_SERVERR);
                } else {
                    if (is_quiet == false) {
                        response = _rest_jsonfy_response_on_kv_with_len(
                                key, strlen(key), value, value_len);
                    } else {
                        response = _rest_jsonfy_quiet_response_on_kv_with_len(
                                key, strlen(key), value, value_len);
                    }
                    free(value);
                    _rest_send_reply(req, response, EVHTTPX_RES_OK); 
                }
                return;
            } else {
                if (is_quiet == false) {
                    response = _rest_jsonfy_response_on_error(req,
                            EVHTTPX_RES_NOTFOUND, "Not Found", "Value not found.");
                } else {
                    response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                            "Not Found", "Value not found.");
                }
                _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
                return;
            }
        }
    } else {
        if (is_quiet == false) {
            response = _rest_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }
    return;
}

static void
URI_rest_replace_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value_old = NULL;
    tstring_t *value_new = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *value = NULL;
    const char *dbname = NULL;
    unsigned int value_old_len = 0;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    response = _rest_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to replace.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rest_query_param_sanity_check(req, &value, "value",
            "You have to set value along with the key you specified.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value_old = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_old_len,
            &(db->instance->err));
    if (value_old != NULL) {
        value_new = tstring_new_len(value, strlen(value));
        leveldb_put(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            tstring_data(value_new), tstring_size(value_new),
            &(db->instance->err));
        if (db->instance->err != NULL) {
            if (is_quiet == false) {
                response = _rest_jsonfy_response_on_error(req,
                        EVHTTPX_RES_SERVERR, "Internal Server Error",
                        db->instance->err);
            } else {
                response = _rest_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                        "Internal Server Error", db->instance->err);
            }
        } else {
            if (is_quiet == false) {
                response = _rest_jsonfy_general_response(EVHTTPX_RES_OK,
                        "OK", "Replace value done.");
            } else {
                response = _rest_jsonfy_quiet_response(EVHTTPX_RES_OK);
            }
        } 
        leveldb_free(value_old);
        tstring_free(value_new);

        _rest_send_reply(req, response, EVHTTPX_RES_OK);
    } else {

        if (is_quiet == false) {
            response = _rest_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key value pair not found.");
        } else {
             response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key value pair not found.");
        }
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

static void
URI_rest_del_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    char *response = NULL;
    const char *key = NULL;
    const char *dbname = NULL;
    bool is_quiet = false;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }
    
    is_quiet = _rest_query_quiet_check(req);

    response = _rest_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to delete.");
    if (response != NULL) {
        _rest_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
 
    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    leveldb_delete(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            &(db->instance->err));
    if (db->instance->err != NULL) {
        if (is_quiet == false ) {
        response = _rest_jsonfy_response_on_error(req,
                EVHTTPX_RES_SERVERR, 
                "Internal Server Error",
                db->instance->err);
        } else {
            response = _rest_jsonfy_general_response(EVHTTPX_RES_SERVERR, 
                "Internal Server Error",
                db->instance->err);
        }
        xleveldb_reset_err(db->instance);
        _rest_send_reply(req, response, EVHTTPX_RES_SERVERR);
    } else {
        if (is_quiet == false) {
            response = _rest_jsonfy_general_response(EVHTTPX_RES_NOCONTENT,
                    "No Content", "Delete key done.");
        } else {
            response = _rest_jsonfy_quiet_response(EVHTTPX_RES_NOCONTENT);
        }
        _rest_send_reply(req, response, EVHTTPX_RES_OK);
    }

    return;
}

static void
URI_rest_mdel_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rest_proto_and_method_sanity_check2nd(req, http_method_POST, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);

    _rest_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rest_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rest_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    response = _rest_do_mdel(req, db, is_quiet);
    _rest_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rest_version_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rest_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rest_send_reply(req, response, code);
        return;
    }

    is_quiet = _rest_query_quiet_check(req);
    
    response = _rest_jsonfy_version_response(leveldb_major_version(),
            leveldb_minor_version(), is_quiet);
    
    _rest_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}


reveldb_rest_t *
reveldb_rest_init(reveldb_config_t *config)
{
    reveldb_rest_t *rest = (reveldb_rest_t *)malloc(sizeof(reveldb_rest_t));
    if (rest == NULL) {
        LOG_ERROR(("failed to malloc reveldb_rest_t."));
        return NULL;
    }

    rest->evbase = event_base_new();
    rest->httpx = evhttpx_new(rest->evbase, NULL);

    reveldb_rest_callbacks_t *callbacks = (reveldb_rest_callbacks_t *)
        malloc(sizeof(reveldb_rest_callbacks_t));
    if (callbacks == NULL) {
        LOG_ERROR(("failed to malloc reveldb_rest_callbacks_t."));
        free(rest);
        return NULL;
    }
    
    evhttpx_ssl_cfg_t *sslcfg = (evhttpx_ssl_cfg_t *)
        malloc(sizeof(evhttpx_ssl_cfg_t));
    if (sslcfg == NULL) {
        LOG_ERROR(("failed to malloc evhttpx_ssl_cfg_t"));
        free(rest);
        free(callbacks);
        return NULL;
    }

    callbacks->rest_new_cb     = evhttpx_set_cb(rest->httpx, "/rest/new", URI_rest_new_cb, NULL);
    /* set(C), get(R), update(U), delete(D) (CRUD)operations. */

    callbacks->rest_add_cb     = evhttpx_set_cb(rest->httpx, "/rest/add", URI_rest_add_cb, NULL);
    callbacks->rest_set_cb     = evhttpx_set_cb(rest->httpx, "/rest/set", URI_rest_set_cb, NULL);
    callbacks->rest_mset_cb    = evhttpx_set_cb(rest->httpx, "/rest/mset", URI_rest_mset_cb, NULL);

    /* get related operations. */
    callbacks->rest_get_cb     = evhttpx_set_cb(rest->httpx, "/rest/get", URI_rest_get_cb, NULL);
    callbacks->rest_mget_cb    = evhttpx_set_cb(rest->httpx, "/rest/mget", URI_rest_mget_cb, NULL);
    callbacks->rest_seize_cb   = evhttpx_set_cb(rest->httpx, "/rest/seize", URI_rest_seize_cb, NULL);
    callbacks->rest_mseize_cb  = evhttpx_set_cb(rest->httpx, "/rest/mseize", URI_rest_mseize_cb, NULL);
    
    callbacks->rest_cas_cb     = evhttpx_set_cb(rest->httpx, "/rest/cas", URI_rest_cas_cb, NULL);
    callbacks->rest_replace_cb = evhttpx_set_cb(rest->httpx, "/rest/replace", URI_rest_replace_cb, NULL);

    /* delete related operations. */
    callbacks->rest_del_cb    = evhttpx_set_cb(rest->httpx, "/rest/del", URI_rest_del_cb, NULL);
    callbacks->rest_mdel_cb = evhttpx_set_cb(rest->httpx, "/rest/mdel", URI_rest_mdel_cb, NULL);

    callbacks->rest_version_cb = evhttpx_set_cb(rest->httpx, "/rest/version", URI_rest_version_cb, NULL);

    sslcfg->pemfile            = config->ssl_config->key;
    sslcfg->privfile           = config->ssl_config->key;
    sslcfg->cafile             = config->ssl_config->cert;
    sslcfg->capath             = config->ssl_config->capath;
    sslcfg->ciphers            = config->ssl_config->ciphers;
    sslcfg->ssl_ctx_timeout    = config->ssl_config->ssl_ctx_timeout;
    sslcfg->verify_peer        = config->ssl_config->verify_peer;
    sslcfg->verify_depth       = config->ssl_config->verify_depth;
    sslcfg->x509_verify_cb     = NULL,
    sslcfg->x509_chk_issued_cb = NULL,
    sslcfg->scache_type        = evhttpx_ssl_scache_type_internal,
    sslcfg->scache_size        = 1024,
    sslcfg->scache_timeout     = 1024,
    sslcfg->scache_init        = NULL,
    sslcfg->scache_add         = NULL,
    sslcfg->scache_get         = NULL,
    sslcfg->scache_del         = NULL,

    rest->sslcfg = sslcfg;
    rest->callbacks = callbacks;
    rest->config = config;
    _rest_fill_ports(rest, config->server_config->restports);

    return rest;
}

void
reveldb_rest_run(reveldb_rest_t *rest)
{
    assert(rest != NULL);
    int i;
    reveldb_config_t *config = rest->config;

    if (rest->config->server_config->https == true) {
        evhttpx_ssl_init(rest->httpx, rest->sslcfg);
    }
    // evhttpx_use_threads(rest->httpx, NULL, 4, NULL);
    for (i = 0; i < rest->num_ports; i++) {
        evhttpx_bind_socket(rest->httpx,
                config->server_config->host,
                rest->ports[i],
                config->server_config->backlog);
    }

    event_base_loop(rest->evbase, 0);
}

void
reveldb_rest_stop(reveldb_rest_t *rest)
{
    assert(rest != NULL);

    evhttpx_unbind_socket(rest->httpx);

    evhttpx_callback_free(rest->callbacks->rest_new_cb);
   
    evhttpx_callback_free(rest->callbacks->rest_add_cb);
    evhttpx_callback_free(rest->callbacks->rest_set_cb);
    evhttpx_callback_free(rest->callbacks->rest_mset_cb);
  
    evhttpx_callback_free(rest->callbacks->rest_get_cb);
    evhttpx_callback_free(rest->callbacks->rest_mget_cb);
    evhttpx_callback_free(rest->callbacks->rest_seize_cb);
    evhttpx_callback_free(rest->callbacks->rest_mseize_cb);
 
    evhttpx_callback_free(rest->callbacks->rest_del_cb);
    evhttpx_callback_free(rest->callbacks->rest_mdel_cb);

    evhttpx_callback_free(rest->callbacks->rest_version_cb);

    evhttpx_free(rest->httpx);
    event_base_free(rest->evbase);
    free(rest->sslcfg);
    free(rest->callbacks);
    free(rest);
}

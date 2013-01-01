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
_rpc_fill_ports(reveldb_rpc_t *rpc, const char *ports)
{
    assert(ports != NULL);

    char *pch = NULL;
    uint32_t port = 0;
    size_t ports_len = strlen(ports);

    rpc->num_ports = 0;

    char *tmpports = (char *)malloc(sizeof(char) * (ports_len + 1));
    memset(tmpports, 0, (ports_len + 1));
    strncpy(tmpports, ports, ports_len);

    pch = strtok(tmpports, ",");
    while (pch != NULL) {
        if (safe_strtoul(pch, &port)) {
            rpc->ports[rpc->num_ports++] = port;
        }
        pch = strtok(NULL, ",");
    }
    return;
}

static unsigned int
_rpc_levenshtein(const char *dst, size_t dst_len,
        const char *src, size_t src_len)
{
    unsigned int len1 = dst_len,
                 len2 = src_len;
    unsigned int *v = calloc(len2 + 1, sizeof(unsigned int));
    unsigned int i, j, current, next, cost;

    /* strip common prefixes */
    while (len1 > 0 && len2 > 0 && eq(dst[0], dst[0]))
        dst++, dst++, len1--, len2--;

    /* handle degenerate cases */
    if (!len1) return len2;
    if (!len2) return len1;
    
    /* initialize the column vector */
    for (j = 0; j < len2 + 1; j++)
        v[j] = j;

    for (i = 0; i < len1; i++) {
        /* set the value of the first row */
        current = i + 1;
        /* for each row in the column, compute the cost */
        for (j = 0; j < len2; j++) {
            /*
             * cost of replacement is 0 if the two chars are the same, or have
             * been transposed with the chars immediately before. otherwise 1.
             */
            cost = !(eq(dst[i], dst[j]) || (i && j &&
                     eq(dst[i-1], dst[j]) && eq(dst[i], dst[j-1])));
            /* find the least cost of insertion, deletion, or replacement */
            next = min(min(v[j+1] + 1,
                            current + 1),
                            v[j] + cost);
            /* stash the previous row's cost in the column vector */
            v[j] = current;
            /* make the cost of the next transition current */
            current = next;
        }
        /* keep the final cost at the bottom of the column */
        v[len2] = next;
    }
    free(v);
    return next;
}

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
_rpc_jsonfy_quiet_response_on_kv_with_len(
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
_rpc_jsonfy_msgalt_response_on_kv_with_len(
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
_rpc_jsonfy_response_on_kv_with_len(
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
_rpc_jsonfy_quiet_response_on_iter(const char *uuid)
{
    assert(uuid != NULL);

    size_t extra_sapce = 64;
    size_t uuid_len = strlen(uuid);
    size_t total = extra_sapce + uuid_len;

    char *out = (char *)malloc(sizeof(char) * (total));
    memset(out, 0, total);
    sprintf(out, "{\"%s\": \"%s\"}", "id", uuid);

    return out;
}

static char *
_rpc_jsonfy_response_on_iter(const char *uuid)
{
    assert(uuid != NULL);
    char *out = NULL;
    char *now = gmttime_now();

    cJSON *root = cJSON_CreateObject();
    cJSON *kv = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "code", EVHTTPX_RES_OK);
    cJSON_AddStringToObject(root, "status", "OK");
    cJSON_AddStringToObject(root, "message", "Create new iterator done.");
    cJSON_AddStringToObject(root, "date", now);
    cJSON_AddItemToObject(root, "iter", kv);
    cJSON_AddStringToObject(kv, "id", uuid);
    // out = cJSON_Print(root);
    /* unformatted json has less data. */
    out = cJSON_PrintUnformatted(root);

    free(now);
    cJSON_Delete(root);
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
    cJSON_AddItemToArray(root, jsonkv);
    cJSON_AddStringToObject(jsonkv, kv->key, kv->val);
    return 0;
}

/* Note: the following function is different
 * from _rpc_jsonfy_kv_pairs. */
static cJSON * 
_rpc_jsonfy_kv_pairs2nd(evhttpx_kvs_t *kvs)
{
    cJSON *root = cJSON_CreateArray();
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
_rpc_jsonfy_size_response(const char *start_key,
        const char *limit_key,
        uint64_t size, bool quiet)
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
        cJSON_AddStringToObject(root, "start", start_key);
        cJSON_AddStringToObject(root, "limit", limit_key);
        cJSON_AddNumberToObject(root, "size", size);
        // out = cJSON_Print(root);
        /* unformatted json has less data. */
        out = cJSON_PrintUnformatted(root);
        free(now);
        cJSON_Delete(root);
        return out;
    } else {
        out = (char *)malloc(sizeof(char) * out_len);
        memset(out, 0, out_len);
        sprintf(out, "{\"size\": %llu}", size);
        return out;
    }
}

static char *
_rpc_jsonfy_version_response(int major, int minor, bool quiet)
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

static char *
_rpc_proto_and_method_sanity_check2nd(
        evhttpx_request_t *req,
        int expected, /**< expected method. */
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
    if (method != expected) {
        *code = EVHTTPX_RES_METHNALLOWED;
        return _rpc_jsonfy_response_on_sanity_check(
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
_rpc_query_database_check(
        evhttpx_request_t *req,
        const char **dbname)
{
    assert(req != NULL);

    evhttpx_query_t *query = req->uri->query;
    *dbname = evhttpx_kv_find(query, "db");
    return;
}

static void 
_rpc_query_iter_check(
        evhttpx_request_t *req,
        const char **dbname)
{
    assert(req != NULL);

    evhttpx_query_t *query = req->uri->query;
    *dbname = evhttpx_kv_find(query, "iter");
    return;
}

static void 
_rpc_query_snapshot_check(
        evhttpx_request_t *req,
        const char **dbname)
{
    assert(req != NULL);

    evhttpx_query_t *query = req->uri->query;
    *dbname = evhttpx_kv_find(query, "snapshot");
    return;
}

static void 
_rpc_query_batch_check(
        evhttpx_request_t *req,
        const char **dbname)
{
    assert(req != NULL);

    evhttpx_query_t *query = req->uri->query;
    *dbname = evhttpx_kv_find(query, "batch");
    return;
}

static char *
_rpc_pattern_unescape(const char *pattern)
{
    return safe_urldecode(pattern);
}

static char *
_rpc_do_mget(evhttpx_request_t *req, reveldb_t *db, bool quiet)
{
    return NULL;
}

static char *
_rpc_do_mseize(evhttpx_request_t *req, reveldb_t *db, bool quiet)
{
    return NULL;
}

static char *
_rpc_do_mset(evhttpx_request_t *req, reveldb_t *db, bool quiet)
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
            response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    keys = cJSON_GetObjectItem(root, "keys");
    values = cJSON_GetObjectItem(root, "values");
    if (keys != NULL && values != NULL) {
        if ((items = cJSON_GetArraySize(keys)) != cJSON_GetArraySize(values)) {
            if (quiet == false) {
                response = _rpc_jsonfy_response_on_error(
                        req, EVHTTPX_RES_BADREQ, "Bad Request",
                        "Keys and values does not equal.");
            } else {
                response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
            }
            return response;
        }
        for (arridx = 0; arridx < items; arridx++) {
            char *key = cJSON_GetArrayItem(keys, arridx)->valuestring;
            char *value = cJSON_GetArrayItem(keys, arridx)->valuestring;
            leveldb_put(
                    db->instance->db,
                    db->instance->woptions,
                    key, strlen(key),
                    value, strlen(value),
                    &(db->instance->err));
            if (db->instance->err != NULL) {
                if (quiet == false) {
                    response = _rpc_jsonfy_response_on_error(req,
                            EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
                } else {
                    response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                            "Internal Server Error", db->instance->err);
                }
                xleveldb_reset_err(db->instance);
                return response;
            }
        }
    } else {
        if (quiet == false) {
            response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    if (quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Multiple set done.");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }
    return response;
}

static char *
_rpc_do_mdel(evhttpx_request_t *req, reveldb_t *db, bool quiet)
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
            response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    keys = cJSON_GetObjectItem(root, "keys");
    if (keys != NULL) {
        for (arridx = 0; arridx < items; arridx++) {
            char *key = cJSON_GetArrayItem(keys, arridx)->valuestring;
            leveldb_delete(
                    db->instance->db,
                    db->instance->woptions,
                    key, strlen(key),
                    &(db->instance->err));
            if (db->instance->err != NULL) {
                if (quiet == false) {
                    response = _rpc_jsonfy_response_on_error(req,
                            EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
                } else {
                    response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                            "Internal Server Error", db->instance->err);
                }
                xleveldb_reset_err(db->instance);
                return response;
            }
        }
    } else {
        if (quiet == false) {
            response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid post filed format.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_BADREQ);
        }
        return response;
    }

    if (quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Multiple set done.");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }
    return response;
}

static void
_rpc_send_reply(evhttpx_request_t *req,
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
URI_rpc_void_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    char *response = NULL;

    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    response = _rpc_jsonfy_response_on_sanity_check(
            EVHTTPX_RES_OK,
            "OK",
            "Reveldb RPC is healthy! :-)");
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
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
        _rpc_send_reply(req, response, code);
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
    free(now);

    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
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
        _rpc_send_reply(req, response, code);
        return;
    }

    response = _rpc_jsonfy_response_on_sanity_check(
            EVHTTPX_RES_OK,
            "OK",
            "Reveldb RPC is healthy! :-)");
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
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
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *content = NULL;
    char *response = NULL;
    const char *property = NULL;
    const char *dbname = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &property, "property", "Please specify valid leveldb property.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!(strcmp(property, "leveldb.stats")
                || strcmp(property, "leveldb.sstables")
                || strncmp(property, "leveldb.num-files-at-level",
                    strlen("leveldb.num-files-at-level")))) {
            response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                    "Bad Request", "Invalid leveldb property.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    content = leveldb_property_value(
            db->instance->db,
            property);
    if (content != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_kv("property", content);
        } else {
            response = _rpc_jsonfy_quiet_response_on_kv("property", content);
        }
        free(content);
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
        return;
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_SERVERR, "Internal Server Error",
                    "Failed to get leveldb property.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                     "Internal Server Error", "Failed to get leveldb property.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
    }

    return;
}

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
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req, &dbname, "db",
            "Database not specified, please check.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    /* init new leveldb instance and insert it into reveldb. */
    reveldb_t *db = reveldb_init(dbname, reveldb_config);
    reveldb_insert_db(&reveldb, db);

    if (db != NULL) {
        if (is_quiet == true) {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        } else response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Created new leveldb instance OK.");
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    } else {
        response = _rpc_jsonfy_response_on_error(req,
                EVHTTPX_RES_SERVERR, "Internal Server Error",
                "Failed to create new leveldb instance.");
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
    }

    return;
}

static void
URI_rpc_compact_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *start_key = NULL;
    const char *end_key = NULL;
    const char *dbname = NULL;
    evhttpx_query_t *query = req->uri->query;

    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    start_key = evhttpx_kv_find(query, "start");
    end_key = evhttpx_kv_find(query, "end");
    dbname = evhttpx_kv_find(query, "db");

    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    /* do compaction. */
    leveldb_compact_range(
            db->instance->db,
            start_key, (start_key ? strlen(start_key) : 0),
            end_key, (end_key ? strlen(end_key) : 0));

    if (is_quiet == false) {
        response = _rpc_jsonfy_general_response(
                EVHTTPX_RES_OK, "OK", "Range compaction done.");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_size_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *start_key = NULL;
    const char *limit_key = NULL;
    size_t start_key_len = -1;
    size_t limit_key_len = -1;
    uint64_t size = -1;
    const char *dbname = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &start_key, "start", "Please specify start key "
            "from which to compute size");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    _rpc_query_param_sanity_check(req,
            &limit_key, "limit", "Please specify limit key");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
 
    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    start_key_len = strlen(start_key);
    limit_key_len = strlen(limit_key);
   
    leveldb_approximate_sizes(
            db->instance->db,
            1,
            &start_key, &start_key_len,
            &limit_key, &limit_key_len,
            &size);
    response = _rpc_jsonfy_size_response(
            start_key, limit_key, size, is_quiet);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_repair_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *dbname = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_database_check(req, &dbname);
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    char *datadir = reveldb_config->server_config->datadir;
    tstring_t *fullpath = tstring_new(datadir);

    if (tstring_data(fullpath)[tstring_size(fullpath) - 1] == '/')
        tstring_append(fullpath, dbname);
    else {
        tstring_append(fullpath, "/");
        tstring_append(fullpath, dbname); 
    }

    leveldb_repair_db(db->instance->options,
            tstring_data(fullpath),
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
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Repair db done.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
    }

    tstring_free(fullpath);

    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_destroy_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *dbname = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req, &dbname, "db",
            "Database not specified."); 
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
    
    char *datadir = reveldb_config->server_config->datadir;
    tstring_t *fullpath = tstring_new(datadir);

    if (tstring_data(fullpath)[tstring_size(fullpath) - 1] == '/')
        tstring_append(fullpath, dbname);
    else {
        tstring_append(fullpath, "/");
        tstring_append(fullpath, dbname); 
    }

    leveldb_close(db->instance->db);
    leveldb_destroy_db(db->instance->options,
            tstring_data(fullpath),
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
    } else {
        rb_erase(&(db->node), &reveldb);
        // reveldb_free_db(db);
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Destroy db done.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
    }
    tstring_free(fullpath);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_add_cb(evhttpx_request_t *req, void *userdata)
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
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to add.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &value, "value",
            "Please set value along with the key you've specified.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
        xleveldb_reset_err(db->instance);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Set key-value pair done.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    }

    return;
}

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
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to set.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &value, "value",
            "Please set value along with the key you've specified.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
        xleveldb_reset_err(db->instance);
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Set key-value pair done.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    }

    return;
}

static void
URI_rpc_mset_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check2nd(req, http_method_POST, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    response = _rpc_do_mset(req, db, is_quiet);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    free(response);
    return;
}

static void
URI_rpc_prepend_cb(evhttpx_request_t *req, void *userdata)
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
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to prepend");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &value, "value",
            "You have to set value along with the key you specified.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value_old = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_old_len,
            &(db->instance->err));
    if (value_old != NULL) {
        value_new = tstring_new_len(value_old, value_old_len);
        tstring_prepend_len(value_new, value, strlen(value));
        leveldb_put(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            tstring_data(value_new), tstring_size(value_new),
            &(db->instance->err));
        if (db->instance->err != NULL) {
            if (is_quiet == false) {
                response = _rpc_jsonfy_response_on_error(req,
                        EVHTTPX_RES_SERVERR, "Internal Server Error",
                        db->instance->err);
            } else {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                        "Internal Server Error", db->instance->err);
            }
        } else {
            if (is_quiet == false) {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                        "OK", "Prepend value done.");
            } else {
                response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
            }
        } 
        leveldb_free(value_old);
        tstring_free(value_new);
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    } else {

        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

static void
URI_rpc_insert_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value_old = NULL;
    tstring_t *value_new = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *value = NULL;
    const char *pos = NULL; 
    uint32_t inspos = 0;
    const char *dbname = NULL;
    unsigned int value_old_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to insert");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &value, "value",
            "Please set value along with the key you specified.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &pos, "pos",
            "You have to set position in which the value will be inserted.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoul(pos, &inspos)) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_BADREQ,
                    "Bad Request", "Pos invalid");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value_old = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_old_len,
            &(db->instance->err));
    if (value_old != NULL) {
        value_new = tstring_new_len(value_old, value_old_len);
        tstring_insert_len(value_new, inspos,
                value, strlen(value));
        leveldb_put(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            tstring_data(value_new), tstring_size(value_new),
            &(db->instance->err));
        if (db->instance->err != NULL) {
            if (is_quiet == false) {
                response = _rpc_jsonfy_response_on_error(req,
                        EVHTTPX_RES_SERVERR, "Internal Server Error",
                        db->instance->err);
            } else {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                        "Internal Server Error", db->instance->err);
            }
        } else {
            if (is_quiet == false) {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                        "OK", "Insert value done.");
            } else {
                response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
            }
        } 
        leveldb_free(value_old);
        tstring_free(value_new);
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    } else {

        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

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
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;
    unsigned int value_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to get.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }


    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    value = leveldb_get(
            db->instance->db,
            (snapshot == NULL) ? db->instance->roptions : roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_kv_with_len(
                    key, strlen(key), value, value_len);
        } else {
            response = _rpc_jsonfy_quiet_response_on_kv_with_len(
                    key, strlen(key), value, value_len);
        }
        free(value);
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }
    
    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }

    return;
}

static void
URI_rpc_mget_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check2nd(req, http_method_POST, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    response = _rpc_do_mget(req, db, is_quiet);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    free(response);
    return;
}

static void
URI_rpc_seize_cb(evhttpx_request_t *req, void *userdata)
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
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to seize.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
                response = _rpc_jsonfy_msgalt_response_on_kv_with_len(
                        key, strlen(key), value, value_len,
                        "Seize key value pair OK, but note that "
                        "you have just deleted the pair on reveldb server");
            } else {
                response = _rpc_jsonfy_quiet_response_on_kv_with_len(
                        key, strlen(key), value, value_len);
            }
        } else {
             if (is_quiet == false) {
                response = _rpc_jsonfy_msgalt_response_on_kv_with_len(
                        key, strlen(key), value, value_len,
                        "Get kv pair OK, but note that you cannot delete "
                        "the pair on reveldb server for some reasons");
            } else {
                response = _rpc_jsonfy_quiet_response_on_kv_with_len(
                        key, strlen(key), value, value_len);
            }
             xleveldb_reset_err(db->instance);
        } 
        free(value);
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key value pair not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key value pair not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

static void
URI_rpc_mseize_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check2nd(req, http_method_POST, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    response = _rpc_do_mseize(req, db, is_quiet);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    free(response);
    return;
}

static void
URI_rpc_range_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *start_key = NULL;
    const char *end_key = NULL;
    bool has_end_key = false;
    const char *dbname = NULL;
    leveldb_iterator_t* iter = NULL;
    evhttpx_query_t *query = req->uri->query;
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    start_key = evhttpx_kv_find(query, "start");
    end_key = evhttpx_kv_find(query, "end");
    dbname = evhttpx_kv_find(query, "db");

    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    iter = leveldb_create_iterator(db->instance->db,
            db->instance->roptions);
    if (start_key == NULL) {
        leveldb_iter_seek_to_first(iter);
        assert(leveldb_iter_valid(iter));
    } else {
        leveldb_iter_seek(iter, start_key, strlen(start_key));
        assert(leveldb_iter_valid(iter));
    }

    if (end_key != NULL) has_end_key = true;

    while(true) {
        if (!leveldb_iter_valid(iter)) break;
        size_t key_len = -1;
        size_t value_len = -1;
        const char *key = leveldb_iter_key(iter, &key_len);
        const char *value = NULL; 
        if ((has_end_key == true)
                && (strlen(end_key) == key_len)
                && (strncmp(key, end_key, key_len) == 0)) break;
        value = leveldb_iter_value(iter, &value_len);
        if (value != NULL) {
            evhttpx_kv_t *kv =
                evhttpx_kvlen_new(key, key_len, value, value_len, 1, 1);
            evhttpx_kvs_add_kv(kvs, kv);
        }
        leveldb_iter_next(iter);
    }
    leveldb_iter_destroy(iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rpc_jsonfy_quiet_response_on_kvs(kvs);
    }

    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_regex_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    char *key_pattern = NULL;
    char *val_pattern = NULL;
    struct re_pattern_buffer key_pattern_buf;
    struct re_pattern_buffer val_pattern_buf;
    const char *param_key_pattern = NULL;
    const char *param_val_pattern = NULL;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;
    const char *dbname = NULL;
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    assert(kvs != NULL);
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &param_key_pattern, "kregex",
            "You have to specify key pattern to match.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        key_pattern = _rpc_pattern_unescape(param_key_pattern);
    }
    
    response = _rpc_query_param_sanity_check(req,
            &param_val_pattern, "vregex",
            "You have to specify value pattern to match.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        val_pattern = _rpc_pattern_unescape(param_val_pattern);
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

     _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    key_pattern_buf.translate = 0; 
    key_pattern_buf.fastmap = 0;
    key_pattern_buf.buffer = 0;
    key_pattern_buf.allocated = 0;

    val_pattern_buf.translate = 0; 
    val_pattern_buf.fastmap = 0;
    val_pattern_buf.buffer = 0;
    val_pattern_buf.allocated = 0;

    re_syntax_options = RE_SYNTAX_EGREP;
    re_compile_pattern(key_pattern, strlen(key_pattern), &key_pattern_buf);
    re_compile_pattern(val_pattern, strlen(val_pattern), &val_pattern_buf);

    leveldb_iterator_t* iter = leveldb_create_iterator(db->instance->db,
            (snapshot == NULL) ? db->instance->roptions : roptions);

    leveldb_iter_seek_to_first(iter);
    while(true) {
        if (!leveldb_iter_valid(iter)) break;
        int matches = -1;
        size_t key_len = -1;
        size_t value_len = -1;
        const char *key = leveldb_iter_key(iter, &key_len);
        const char *value = NULL; 
        if ((matches = re_match(&key_pattern_buf, key, key_len, 0, NULL)) >= 0) {
            matches = -1;
            value = leveldb_iter_value(iter, &value_len);
            if ((matches = re_match(&val_pattern_buf, value, value_len, 0, NULL)) >= 0) {
                evhttpx_kv_t *kv =
                    evhttpx_kvlen_new(key, key_len, value, value_len, 1, 1);
                evhttpx_kvs_add_kv(kvs, kv);
            }
        }
        leveldb_iter_next(iter);
    }
    leveldb_iter_destroy(iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rpc_jsonfy_quiet_response_on_kvs(kvs);
    }

    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }

    evhttpx_kvs_free(kvs);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_kregex_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    char *pattern = NULL;
    struct re_pattern_buffer pattern_buf;
    const char *param_key_pattern = NULL;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;
    const char *dbname = NULL;
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    assert(kvs != NULL);
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &param_key_pattern, "pattern",
            "You have to specify key pattern to match.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        pattern = _rpc_pattern_unescape(param_key_pattern);
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

     _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }
  
    pattern_buf.translate = 0; 
    pattern_buf.fastmap = 0;
    pattern_buf.buffer = 0;
    pattern_buf.allocated = 0;
    re_syntax_options = RE_SYNTAX_EGREP;
    re_compile_pattern(pattern, strlen(pattern), &pattern_buf);

    leveldb_iterator_t* iter = leveldb_create_iterator(db->instance->db,
            (snapshot == NULL) ? db->instance->roptions : roptions);

    leveldb_iter_seek_to_first(iter);
    while(true) {
        if (!leveldb_iter_valid(iter)) break;
        int matches = -1;
        size_t key_len = -1;
        size_t value_len = -1;
        const char *key = leveldb_iter_key(iter, &key_len);
        const char *value = NULL; 
        if ((matches = re_match(&pattern_buf, key, key_len, 0, NULL)) >= 0) {
            value = leveldb_iter_value(iter, &value_len);
            if (value != NULL) {                    
                evhttpx_kv_t *kv =
                    evhttpx_kvlen_new(key, key_len, value, value_len, 1, 1);
                evhttpx_kvs_add_kv(kvs, kv);
            }
        }
        leveldb_iter_next(iter);
    }
    leveldb_iter_destroy(iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rpc_jsonfy_quiet_response_on_kvs(kvs);
    }

    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }

    evhttpx_kvs_free(kvs);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_vregex_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    char *pattern = NULL;
    struct re_pattern_buffer pattern_buf;
    const char *param_key_pattern = NULL;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;
    const char *dbname = NULL;
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    assert(kvs != NULL);
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &param_key_pattern, "pattern",
            "You have to specify value pattern to match.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        pattern = _rpc_pattern_unescape(param_key_pattern);
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
    _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    pattern_buf.translate = 0; 
    pattern_buf.fastmap = 0;
    pattern_buf.buffer = 0;
    pattern_buf.allocated = 0;
    re_syntax_options = RE_SYNTAX_EGREP;
    re_compile_pattern(pattern, strlen(pattern), &pattern_buf);

    leveldb_iterator_t* iter = leveldb_create_iterator(db->instance->db,
            (snapshot == NULL) ? db->instance->roptions : roptions);

    leveldb_iter_seek_to_first(iter);
    while(true) {
        if (!leveldb_iter_valid(iter)) break;
        int matches = -1;
        size_t key_len = -1;
        size_t value_len = -1;
        const char *key = NULL; 
        const char *value = leveldb_iter_value(iter, &value_len);
        if ((matches = re_match(&pattern_buf, value, value_len, 0, NULL)) >= 0) {
            key = leveldb_iter_key(iter, &key_len);
            evhttpx_kv_t *kv =
                evhttpx_kvlen_new(key, key_len, value, value_len, 1, 1);
            evhttpx_kvs_add_kv(kvs, kv);
        }
        leveldb_iter_next(iter);
    }
    leveldb_iter_destroy(iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rpc_jsonfy_quiet_response_on_kvs(kvs);
    }

    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }

    evhttpx_kvs_free(kvs);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_similar_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *ksimilar = NULL;
    const char *vsimilar = NULL;
    const char *kdistance = NULL;
    const char *vdistance = NULL;
    size_t klimit = 0;
    size_t vlimit = 0;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;
    const char *dbname = NULL;
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    assert(kvs != NULL);
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &ksimilar, "ksimilar",
            "You have to specify the similar key to match.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } 

    response = _rpc_query_param_sanity_check(req,
            &vsimilar, "vsimilar",
            "You have to specify the similar value to match.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } 

    response = _rpc_query_param_sanity_check(req,
            &kdistance, "kdistance",
            "You have to specify the distance of keys to adopt.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoul(kdistance, &klimit)) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_BADREQ, "Bad Request",
                    "Key distance is not numerical.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }
    
    response = _rpc_query_param_sanity_check(req,
            &vdistance, "vdistance",
            "You have to specify the distance of values to adopt.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoul(vdistance, &vlimit)) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_BADREQ, "Bad Request",
                    "Distance is not numerical.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
    
    _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    leveldb_iterator_t* iter = leveldb_create_iterator(db->instance->db,
            (snapshot == NULL) ? db->instance->roptions : roptions);

    leveldb_iter_seek_to_first(iter);
    while(true) {
        if (!leveldb_iter_valid(iter)) break;
        size_t key_len = -1;
        size_t value_len = -1;
        const char *key = leveldb_iter_key(iter, &key_len);
        const char *value = NULL; 
        if (_rpc_levenshtein(key, key_len,
                        ksimilar, strlen(ksimilar)) <= klimit) {
            value = leveldb_iter_key(iter, &value_len);
            if (_rpc_levenshtein(value, value_len,
                            vsimilar, strlen(vsimilar)) <= vlimit) {
                evhttpx_kv_t *kv =
                    evhttpx_kvlen_new(key, key_len, value, value_len, 1, 1);
                evhttpx_kvs_add_kv(kvs, kv);
            }
        }
        leveldb_iter_next(iter);
    }
    leveldb_iter_destroy(iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rpc_jsonfy_quiet_response_on_kvs(kvs);
    }

    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }

    evhttpx_kvs_free(kvs);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_ksimilar_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *similar = NULL;
    const char *distance = NULL;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;
    size_t limit = 0;
    const char *dbname = NULL;
 
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    assert(kvs != NULL);
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &similar, "similar",
            "You have to specify the similar value to match.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } 
    
    response = _rpc_query_param_sanity_check(req,
            &distance, "distance",
            "You have to specify the distance of values to adopt.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoul(distance, &limit)) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_BADREQ, "Bad Request",
                    "Distance is not numerical.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
    
    _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    leveldb_iterator_t* iter = leveldb_create_iterator(db->instance->db,
            (snapshot == NULL) ? db->instance->roptions : roptions);

    leveldb_iter_seek_to_first(iter);
    while(true) {
        if (!leveldb_iter_valid(iter)) break;
        size_t key_len = -1;
        size_t value_len = -1;
        const char *key = leveldb_iter_key(iter, &key_len);
        const char *value = NULL; 
        if (_rpc_levenshtein(key, key_len,
                        similar, strlen(similar)) <= limit) {
            value = leveldb_iter_value(iter, &value_len);
            evhttpx_kv_t *kv =
                evhttpx_kvlen_new(key, key_len, value, value_len, 1, 1);
            evhttpx_kvs_add_kv(kvs, kv);
        }
        leveldb_iter_next(iter);
    }
    leveldb_iter_destroy(iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rpc_jsonfy_quiet_response_on_kvs(kvs);
    }

    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }

    evhttpx_kvs_free(kvs);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_vsimilar_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *similar = NULL;
    const char *distance = NULL;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;
    size_t limit = 0;
    const char *dbname = NULL;
    evhttpx_kvs_t *kvs = evhttpx_kvs_new();

    assert(kvs != NULL);
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &similar, "similar",
            "You have to specify the similar value to match.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } 
    
    response = _rpc_query_param_sanity_check(req,
            &distance, "distance",
            "You have to specify the distance of values to adopt.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoul(distance, &limit)) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_BADREQ, "Bad Request",
                    "Distance is not numerical.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
    
    _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    leveldb_iterator_t* iter = leveldb_create_iterator(db->instance->db,
            (snapshot == NULL) ? db->instance->roptions : roptions);

    leveldb_iter_seek_to_first(iter);
    while(true) {
        if (!leveldb_iter_valid(iter)) break;
        size_t key_len = -1;
        size_t value_len = -1;
        const char *key = NULL; 
        const char *value = leveldb_iter_value(iter, &value_len);
        if (_rpc_levenshtein(value, value_len,
                        similar, strlen(similar)) <= limit) {
            key = leveldb_iter_key(iter, &key_len);
            evhttpx_kv_t *kv =
                evhttpx_kvlen_new(key, key_len, value, value_len, 1, 1);
            evhttpx_kvs_add_kv(kvs, kv);
        }
        leveldb_iter_next(iter);
    }
    leveldb_iter_destroy(iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_kvs(kvs);
    } else {
        response = _rpc_jsonfy_quiet_response_on_kvs(kvs);
    }

    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }

    evhttpx_kvs_free(kvs);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_incr_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *step = NULL;
    long long llstep = -1;
    const char *dbname = NULL;
    unsigned int value_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to incr.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req,
            &step, "step", "You have to specify step length to incr.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoll(step, &llstep)) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_BADREQ,
                    "Bad Request", "Step you have specified must be numerical.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        long long llvalue = -1;
        if (safe_strntoll(value, value_len, &llvalue)) {
            char valuebuf[64] = {0};
            llvalue += llstep;
            sprintf(valuebuf, "%lld", llvalue);
            leveldb_put(
                    db->instance->db,
                    db->instance->woptions,
                    key, strlen(key),
                    valuebuf, strlen(valuebuf),
                    &(db->instance->err));
            if (db->instance->err != NULL) {
                if (is_quiet == false) {
                    response = _rpc_jsonfy_response_on_error(req,
                            EVHTTPX_RES_SERVERR, "Internal Server Error",
                            db->instance->err);
                } else {
                    response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                            "Internal Server Error", db->instance->err);
                }
                _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
            } else {
                if (is_quiet == false) {
                    response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                            "OK", "Incr value done.");
                } else {
                    response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
                }
                _rpc_send_reply(req, response, EVHTTPX_RES_OK);
            }
        } else {
            if (is_quiet == false) {
                response = _rpc_jsonfy_response_on_error(req,
                        EVHTTPX_RES_BADREQ, "Bad Request",
                        "Value is not numerical, incr is not allowed.");
            } else {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_BADREQ,
                        "Bad Request", "Value is not numerical, incr is not allowed.");
            }
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        } 

        free(value);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key value pair not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key value pair not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

static void
URI_rpc_decr_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *step = NULL;
    long long llstep = -1;
    const char *dbname = NULL;
    unsigned int value_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to decr.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req,
            &step, "step", "You have to specify step length to decr.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoll(step, &llstep)) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_BADREQ,
                    "Bad Request", "Step you have specified must be numerical.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        long long llvalue = -1;
        if (safe_strntoll(value, value_len, &llvalue)) {
            char valuebuf[64] = {0};
            llvalue -= llstep;
            sprintf(valuebuf, "%lld", llvalue);
            leveldb_put(
                    db->instance->db,
                    db->instance->woptions,
                    key, strlen(key),
                    valuebuf, strlen(valuebuf),
                    &(db->instance->err));
            if (db->instance->err != NULL) {
                if (is_quiet == false) {
                    response = _rpc_jsonfy_response_on_error(req,
                            EVHTTPX_RES_SERVERR, "Internal Server Error",
                            db->instance->err);
                } else {
                    response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                            "Internal Server Error", db->instance->err);
                }
                _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
            } else {
                if (is_quiet == false) {
                    response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                            "OK", "Decr value done.");
                } else {
                    response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
                }
                _rpc_send_reply(req, response, EVHTTPX_RES_OK);
            }
        } else {
            if (is_quiet == false) {
                response = _rpc_jsonfy_response_on_error(req,
                        EVHTTPX_RES_BADREQ, "Bad Request",
                        "Value is not numerical, incr is not allowed.");
            } else {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_BADREQ,
                        "Bad Request", "Value is not numerical, incr is not allowed.");
            }
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        } 

        free(value);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key value pair not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key value pair not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

static void
URI_rpc_cas_cb(evhttpx_request_t *req, void *userdata)
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
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to get.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    response = _rpc_query_param_sanity_check(req,
            &oval, "oval", "Please specify old value to compare.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    response = _rpc_query_param_sanity_check(req,
            &nval, "nval", "Please specify new value to swap.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
                response = _rpc_jsonfy_response_on_error(req,
                        EVHTTPX_RES_NOTFOUND, "Not Found", "Value not found.");
            } else {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                        "Not Found", "Value not found.");
            }
            _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
                        response = _rpc_jsonfy_response_on_error(req,
                                EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
                    } else {
                        response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                                "Internal Server Error", db->instance->err);
                    }
                    xleveldb_reset_err(db->instance);
                    _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
                } else {
                    if (is_quiet == false) {
                        response = _rpc_jsonfy_response_on_kv_with_len(
                                key, strlen(key), value, value_len);
                    } else {
                        response = _rpc_jsonfy_quiet_response_on_kv_with_len(
                                key, strlen(key), value, value_len);
                    }
                    free(value);
                    _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
                }
                return;
            } else {
                if (is_quiet == false) {
                    response = _rpc_jsonfy_response_on_error(req,
                            EVHTTPX_RES_NOTFOUND, "Not Found", "Value not found.");
                } else {
                    response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                            "Not Found", "Value not found.");
                }
                _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
                return;
            }
        }
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }
    return;
}

static void
URI_rpc_replace_cb(evhttpx_request_t *req, void *userdata)
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
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to replace.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &value, "value",
            "You have to set value along with the key you specified.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
                response = _rpc_jsonfy_response_on_error(req,
                        EVHTTPX_RES_SERVERR, "Internal Server Error",
                        db->instance->err);
            } else {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                        "Internal Server Error", db->instance->err);
            }
        } else {
            if (is_quiet == false) {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                        "OK", "Replace value done.");
            } else {
                response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
            }
        } 
        leveldb_free(value_old);
        tstring_free(value_new);

        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    } else {

        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key value pair not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key value pair not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

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
        _rpc_send_reply(req, response, code);
        return;
    }
    
    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to delete.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
 
    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
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
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOCONTENT,
                    "No Content", "Delete key done.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_NOCONTENT);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    }

    return;
}

static void
URI_rpc_mdel_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check2nd(req, http_method_POST, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    response = _rpc_do_mdel(req, db, is_quiet);
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    free(response);
    return;
}

static void
URI_rpc_append_cb(evhttpx_request_t *req, void *userdata)
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
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to append.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &value, "value",
            "Please set value along with the key you specified.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    value_old = leveldb_get(
            db->instance->db,
            db->instance->roptions,
            key, strlen(key),
            &value_old_len,
            &(db->instance->err));
    if (value_old != NULL) {
        value_new = tstring_new_len(value_old, value_old_len);
        tstring_append_len(value_new, value, strlen(value));
        leveldb_put(
            db->instance->db,
            db->instance->woptions,
            key, strlen(key),
            tstring_data(value_new), tstring_size(value_new),
            &(db->instance->err));
        if (db->instance->err != NULL) {
            if (is_quiet == false) {
                response = _rpc_jsonfy_response_on_error(req,
                        EVHTTPX_RES_SERVERR, "Internal Server Error",
                        db->instance->err);
            } else {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                        "Internal Server Error", db->instance->err);
            }
        } else {
            if (is_quiet == false) {
                response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                        "OK", "Append value done.");
            } else {
                response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
            }
        } 
        leveldb_free(value_old);
        tstring_free(value_new);
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    } else {

        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    return;
}

static void
URI_rpc_remove_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *start_key = NULL;
    const char *end_key = NULL;
    bool has_end_key = false;
    const char *dbname = NULL;
    leveldb_iterator_t* iter = NULL;
    evhttpx_query_t *query = req->uri->query;

    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    start_key = evhttpx_kv_find(query, "start");
    end_key = evhttpx_kv_find(query, "end");
    dbname = evhttpx_kv_find(query, "db");

    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
   
    iter = leveldb_create_iterator(db->instance->db,
            db->instance->roptions);
    if (start_key == NULL) {
        leveldb_iter_seek_to_first(iter);
        assert(leveldb_iter_valid(iter));
    } else {
        leveldb_iter_seek(iter, start_key, strlen(start_key));
        assert(leveldb_iter_valid(iter));
    }

    if (end_key != NULL) has_end_key = true;

    while(true) {
        if (!leveldb_iter_valid(iter)) break;
        size_t key_len = -1;
        const char *key = leveldb_iter_key(iter, &key_len);
        if ((has_end_key == true)
                && (strlen(end_key) == key_len)
                && (strncmp(key, end_key, key_len) == 0)) break;
        leveldb_delete(
                db->instance->db,
                db->instance->woptions,
                key, key_len,
                &(db->instance->err));
        if (db->instance->err != NULL) {
            xleveldb_reset_err(db->instance);
        }
        leveldb_iter_next(iter);
    }
    leveldb_iter_destroy(iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOCONTENT,
                "No Content", "Range remove done.");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_NOCONTENT);
    }

    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_iter_new_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    afsUUID id;
    char uuid_str[64] = {0};
    char *response = NULL;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;

    const char *dbname = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }
    _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    uuid_create(&id);
    uuid_to_string(&id, uuid_str, sizeof(uuid_str));
    /* init new leveldb iterator and insert it into dbiter. */
    xleveldb_iter_t *iter = xleveldb_init_iter(uuid_str, db, roptions,
            (snapshot == NULL) ? false : true);
    xleveldb_insert_iter(&dbiter, iter);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_iter(uuid_str);
    } else {
        response = _rpc_jsonfy_quiet_response_on_iter(uuid_str);
    }
    _rpc_send_reply(req, response, EVHTTPX_RES_OK); 

    return;
}

static void
URI_rpc_iter_first_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_iter_seek_to_first(iter);

    if (xleveldb_iter_valid(iter)) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                    "Iterator moved to first.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_last_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_iter_seek_to_last(iter);

    if (xleveldb_iter_valid(iter)) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                    "Iterator moved to last");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_next_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_iter_next(iter);
    if (xleveldb_iter_valid(iter)) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                    "Iterator moved to the next.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator, out of bounds.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_prev_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_iter_prev(iter);
    if (xleveldb_iter_valid(iter)) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                    "Iterator moved to the previous");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator, out of bounds.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_forward_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    const char *step_str = NULL;
    unsigned int step = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    response = _rpc_query_param_sanity_check(req, &step_str, "step",
            "Please set the step you want to move forward");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoul(step_str, &step)) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_BADREQ, "Bad Request",
                    "Step is not numerical.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_iter_forward(iter, step);
    if (xleveldb_iter_valid(iter)) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                    "Iterator step forward done");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator, out of bounds.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_backward_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    const char *step_str = NULL;
    unsigned int step = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    response = _rpc_query_param_sanity_check(req, &step_str, "step",
            "Please set the step you want to move forward");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    } else {
        if (!safe_strtoul(step_str, &step)) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_BADREQ, "Bad Request",
                    "Step is not numerical.");
            _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
            return;
        }
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_iter_backward(iter, step);
    if (xleveldb_iter_valid(iter)) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                    "Iterator step backward done");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator, out of bounds.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_seek_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    const char *key = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    response = _rpc_query_param_sanity_check(req, &key, "key",
            "Please set the key you want to seek");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_iter_seek(iter, key, strlen(key));
    if (xleveldb_iter_valid(iter)) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                    "Iterator seek done");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator, out of bounds.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_key_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    const char *key = NULL;
    unsigned int key_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    key = xleveldb_iter_key(iter, &key_len);
    if (key != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_msgalt_response_on_kv_with_len(
                    "key", 3, key, key_len, "Get the key of current iterator.");
        } else {
            response = _rpc_jsonfy_quiet_response_on_kv_with_len(
                    "key", 3, key, key_len);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_value_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    const char *value = NULL;
    unsigned int value_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    value = xleveldb_iter_value(iter, &value_len);
    if (value != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_msgalt_response_on_kv_with_len(
                    "value", 5, value, value_len, "Get the value of current iterator.");
        } else {
            response = _rpc_jsonfy_quiet_response_on_kv_with_len(
                    "value", 5, value, value_len);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_kv_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    const char *key = NULL;
    unsigned int key_len = 0;
    const char *value = NULL;
    unsigned int value_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_iter_kv(iter, &key, &key_len, &value, &value_len);
    if (key != NULL && value != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_kv_with_len(
                    key, key_len, value, value_len);
        } else {
            response = _rpc_jsonfy_quiet_response_on_kv_with_len(
                    key, key_len, value, value_len);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                    "Internal Server Error", "Invalid iterator.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_SERVERR);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR); 
    }

    return;
}

static void
URI_rpc_iter_destroy_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *iter_id = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &iter_id);
    if ((iter_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Iterator ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_iter_t *iter = xleveldb_search_iter(&dbiter, iter_id);
    if (iter == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Iterator not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_free_iter(iter);
    rb_erase(&(iter->node), &dbiter);
    if (is_quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                "Iterator destroyed");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }
    _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    
    return;
}

static void
URI_rpc_snapshot_new_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    afsUUID id;
    char uuid_str[64] = {0};
    char *response = NULL;
    const char *dbname = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    uuid_create(&id);
    uuid_to_string(&id, uuid_str, sizeof(uuid_str));
    /* init new leveldb iterator and insert it into dbiter. */
    xleveldb_snapshot_t *snapshot = xleveldb_init_snapshot(uuid_str, db);
    xleveldb_insert_snapshot(&dbsnapshot, snapshot);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_iter(uuid_str);
    } else {
        response = _rpc_jsonfy_quiet_response_on_iter(uuid_str);
    }
    _rpc_send_reply(req, response, EVHTTPX_RES_OK); 

    return;
}

static void
URI_rpc_snapshot_release_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *snapshot_id = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &snapshot_id);
    if ((snapshot_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Snapshot ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_snapshot_t *snapshot =
        xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
    if (snapshot == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Snapshot not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_free_snapshot(snapshot);
    rb_erase(&(snapshot->node), &dbsnapshot);
    if (is_quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                "Snapshot released.");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }

    _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    return;
}

static void
URI_rpc_writebatch_new_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    afsUUID id;
    char uuid_str[64] = {0};
    char *response = NULL;
    const char *dbname = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    uuid_create(&id);
    uuid_to_string(&id, uuid_str, sizeof(uuid_str));
    /* init new leveldb iterator and insert it into dbiter. */
    xleveldb_writebatch_t *writebatch= xleveldb_init_writebatch(uuid_str, db);
    xleveldb_insert_writebatch(&dbwritebatch, writebatch);

    if (is_quiet == false) {
        response = _rpc_jsonfy_response_on_iter(uuid_str);
    } else {
        response = _rpc_jsonfy_quiet_response_on_iter(uuid_str);
    }

    _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    return;
}

static void
URI_rpc_writebatch_put_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *key = NULL;
    const char *value = NULL;
    const char *batch_id = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to set.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }
    
    response = _rpc_query_param_sanity_check(req, &value, "value",
            "Please set value along with the key you've specified.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_batch_check(req, &batch_id);
    if ((batch_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Batch ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_writebatch_t *batch =
        xleveldb_search_writebatch(&dbwritebatch, batch_id);
    if (batch == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Batch not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }


    leveldb_writebatch_put(
            batch->writebatch,
            key, strlen(key),
            value, strlen(value));
    
    if (is_quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Set writebatch key-value pair done.");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }

    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_writebatch_del_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *key = NULL;
    const char *batch_id = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "Please specify which key to set.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_batch_check(req, &batch_id);
    if ((batch_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Batch ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_writebatch_t *batch =
        xleveldb_search_writebatch(&dbwritebatch, batch_id);
    if (batch == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Batch not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    leveldb_writebatch_delete(
            batch->writebatch,
            key, strlen(key));
    
    if (is_quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Delete writebatch key-value pair done.");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }

    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_writebatch_clear_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *batch_id = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_batch_check(req, &batch_id);
    if ((batch_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Batch ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_writebatch_t *batch =
        xleveldb_search_writebatch(&dbwritebatch, batch_id);
    if (batch == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Batch not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    leveldb_writebatch_clear(batch->writebatch);
    
    if (is_quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                "OK", "Clear writebatch done.");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }
    
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}

static void
URI_rpc_writebatch_commit_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    const char *dbname = NULL;
    const char *batch_id = NULL;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);
    
    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    _rpc_query_batch_check(req, &batch_id);
    if ((batch_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Batch ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_writebatch_t *batch =
        xleveldb_search_writebatch(&dbwritebatch, batch_id);
    if (batch == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Batch not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    leveldb_write(
            db->instance->db,
            db->instance->woptions,
            batch->writebatch,
            &(db->instance->err));
    if (db->instance->err != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_SERVERR, "Internal Server Error", db->instance->err);
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_SERVERR,
                     "Internal Server Error", db->instance->err);
        }
        xleveldb_reset_err(db->instance);
        _rpc_send_reply(req, response, EVHTTPX_RES_SERVERR);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Writebatch done.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    }

    return;
}

static void
URI_rpc_writebatch_destroy_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    const char *batch_id = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    _rpc_query_iter_check(req, &batch_id);
    if ((batch_id == NULL)) {
        response = _rpc_jsonfy_response_on_error(req, EVHTTPX_RES_BADREQ,
                "Bad Request", "Writebatch ID must be specified.");
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    xleveldb_writebatch_t *batch=
        xleveldb_search_writebatch(&dbwritebatch, batch_id);
    if (batch == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Writebatch not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    xleveldb_free_writebatch(batch);
    rb_erase(&(batch->node), &dbwritebatch);
    if (is_quiet == false) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK, "OK",
                "Writebatch destroyed");
    } else {
        response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
    }
    _rpc_send_reply(req, response, EVHTTPX_RES_OK); 
    
    return;
}

static void
URI_rpc_clear_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_sync_cb(evhttpx_request_t *req, void *userdata)
{}

static void
URI_rpc_check_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *dbname = NULL;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;

    unsigned int value_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to check");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    value = leveldb_get(
            db->instance->db,
            (snapshot == NULL) ? db->instance->roptions : roptions,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Key exists.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }

        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }
    
    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }

    return;
}

static void
URI_rpc_exists_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *value = NULL;
    char *response = NULL;
    const char *key = NULL;
    const char *dbname = NULL;
    const char *snapshot_id = NULL;
    xleveldb_snapshot_t *snapshot = NULL;
    leveldb_readoptions_t *roptions = NULL;

    unsigned int value_len = 0;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);

    response = _rpc_query_param_sanity_check(req,
            &key, "key", "You have to specify which key to check exists.");
    if (response != NULL) {
        _rpc_send_reply(req, response, EVHTTPX_RES_BADREQ);
        return;
    }

    _rpc_query_database_check(req, &dbname);
    if ((dbname == NULL)) dbname =
        reveldb_config->db_config->dbname;
    reveldb_t *db = reveldb_search_db(&reveldb, dbname);
    if (db == NULL) {
        response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                "Not Found", "Database not found, please check.");
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
        return;
    }

    _rpc_query_snapshot_check(req, &snapshot_id);
    if (snapshot_id != NULL) {
        snapshot = xleveldb_search_snapshot(&dbsnapshot, snapshot_id);
        roptions = leveldb_readoptions_create();
        leveldb_readoptions_set_verify_checksums(roptions,
                reveldb_config->db_config->verify_checksums);
        leveldb_readoptions_set_fill_cache(roptions,
                reveldb_config->db_config->fill_cache);
        leveldb_readoptions_set_snapshot(roptions,
                snapshot->snapshot);
    }

    value = leveldb_get(
            db->instance->db,
            (snapshot == NULL) ? db->instance->roptions :roptions ,
            key, strlen(key),
            &value_len,
            &(db->instance->err));
    if (value != NULL) {
        if (is_quiet == false) {
            response = _rpc_jsonfy_general_response(EVHTTPX_RES_OK,
                    "OK", "Key exists.");
        } else {
            response = _rpc_jsonfy_quiet_response(EVHTTPX_RES_OK);
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    } else {
        if (is_quiet == false) {
            response = _rpc_jsonfy_response_on_error(req,
                    EVHTTPX_RES_NOTFOUND, "Not Found", "Key not found.");
        } else {
             response = _rpc_jsonfy_general_response(EVHTTPX_RES_NOTFOUND,
                     "Not Found", "Key not found.");
        }
        _rpc_send_reply(req, response, EVHTTPX_RES_NOTFOUND);
    }

    if (snapshot != NULL) {
        leveldb_readoptions_set_snapshot(roptions, NULL);
        leveldb_readoptions_destroy(roptions);
    }
    return;
}

static void
URI_rpc_version_cb(evhttpx_request_t *req, void *userdata)
{
    /* json formatted response. */
    unsigned int code = 0;
    bool is_quiet = false;
    char *response = NULL;
    
    response = _rpc_proto_and_method_sanity_check(req, &code);
    if (response != NULL) {
        _rpc_send_reply(req, response, code);
        return;
    }

    is_quiet = _rpc_query_quiet_check(req);
    
    response = _rpc_jsonfy_version_response(leveldb_major_version(),
            leveldb_minor_version(), is_quiet);
    
    _rpc_send_reply(req, response, EVHTTPX_RES_OK);
    return;
}


reveldb_rpc_t *
reveldb_rpc_init(reveldb_config_t *config)
{
    reveldb_rpc_t *rpc = (reveldb_rpc_t *)malloc(sizeof(reveldb_rpc_t));
    if (rpc == NULL) {
        LOG_ERROR(("failed to malloc reveldb_rpc_t."));
        return NULL;
    }

    rpc->evbase = event_base_new();
    rpc->httpx = evhttpx_new(rpc->evbase, NULL);

    reveldb_rpc_callbacks_t *callbacks = (reveldb_rpc_callbacks_t *)
        malloc(sizeof(reveldb_rpc_callbacks_t));
    if (callbacks == NULL) {
        LOG_ERROR(("failed to malloc reveldb_rpc_callbacks_t."));
        free(rpc);
        return NULL;
    }
    
    evhttpx_ssl_cfg_t *sslcfg = (evhttpx_ssl_cfg_t *)
        malloc(sizeof(evhttpx_ssl_cfg_t));
    if (sslcfg == NULL) {
        LOG_ERROR(("failed to malloc evhttpx_ssl_cfg_t"));
        free(rpc);
        free(callbacks);
        return NULL;
    }

    /* only for server status test. */
    callbacks->rpc_void_cb = evhttpx_set_cb(rpc->httpx, "/rpc/void", URI_rpc_void_cb, NULL);
    callbacks->rpc_echo_cb = evhttpx_set_cb(rpc->httpx, "/rpc/echo", URI_rpc_echo_cb, NULL);
    callbacks->rpc_head_cb = evhttpx_set_cb(rpc->httpx, "/rpc/head", URI_rpc_head_cb, NULL);
    
    /* reveldb reports and internal leveldb storage engine status. */
    callbacks->rpc_report_cb   = evhttpx_set_cb(rpc->httpx, "/rpc/report", URI_rpc_report_cb, NULL);
    callbacks->rpc_status_cb   = evhttpx_set_cb(rpc->httpx, "/rpc/status", URI_rpc_status_cb, NULL);
    callbacks->rpc_property_cb = evhttpx_set_cb(rpc->httpx, "/rpc/property", URI_rpc_property_cb, NULL);

    /* admin operations. */
    callbacks->rpc_new_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/new", URI_rpc_new_cb, NULL);
    callbacks->rpc_compact_cb = evhttpx_set_cb(rpc->httpx, "/rpc/compact", URI_rpc_compact_cb, NULL);
    callbacks->rpc_size_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/size", URI_rpc_size_cb, NULL);
    callbacks->rpc_repair_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/repair", URI_rpc_repair_cb, NULL);
    callbacks->rpc_destroy_cb = evhttpx_set_cb(rpc->httpx, "/rpc/destroy", URI_rpc_destroy_cb, NULL);

    /* set(C), get(R), update(U), delete(D) (CRUD)operations. */

    /* set related operations. */
    callbacks->rpc_add_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/add", URI_rpc_add_cb, NULL);
    callbacks->rpc_set_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/set", URI_rpc_set_cb, NULL);
    callbacks->rpc_mset_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/mset", URI_rpc_mset_cb, NULL);
    callbacks->rpc_append_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/append", URI_rpc_append_cb, NULL);
    callbacks->rpc_prepend_cb = evhttpx_set_cb(rpc->httpx, "/rpc/prepend", URI_rpc_prepend_cb, NULL);
    callbacks->rpc_insert_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/insert", URI_rpc_insert_cb, NULL);

    /* get related operations. */
    callbacks->rpc_get_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/get", URI_rpc_get_cb, NULL);
    callbacks->rpc_mget_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/mget", URI_rpc_mget_cb, NULL);
    callbacks->rpc_seize_cb   = evhttpx_set_cb(rpc->httpx, "/rpc/seize", URI_rpc_seize_cb, NULL);
    callbacks->rpc_mseize_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/mseize", URI_rpc_mseize_cb, NULL);
    callbacks->rpc_range_cb   = evhttpx_set_cb(rpc->httpx, "/rpc/range", URI_rpc_range_cb, NULL);
    callbacks->rpc_regex_cb   = evhttpx_set_cb(rpc->httpx, "/rpc/regex", URI_rpc_regex_cb, NULL);
    callbacks->rpc_kregex_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/kregex", URI_rpc_kregex_cb, NULL);
    callbacks->rpc_vregex_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/vregex", URI_rpc_vregex_cb, NULL);
    callbacks->rpc_similar_cb = evhttpx_set_cb(rpc->httpx, "/rpc/similar", URI_rpc_similar_cb, NULL);
    callbacks->rpc_similar_cb = evhttpx_set_cb(rpc->httpx, "/rpc/ksimilar", URI_rpc_ksimilar_cb, NULL);
    callbacks->rpc_similar_cb = evhttpx_set_cb(rpc->httpx, "/rpc/vsimilar", URI_rpc_vsimilar_cb, NULL);

    /* update related operations. */
    callbacks->rpc_incr_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/incr", URI_rpc_incr_cb, NULL);
    callbacks->rpc_decr_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/decr", URI_rpc_decr_cb, NULL);
    callbacks->rpc_cas_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/cas", URI_rpc_cas_cb, NULL);
    callbacks->rpc_replace_cb = evhttpx_set_cb(rpc->httpx, "/rpc/replace", URI_rpc_replace_cb, NULL);

    /* delete related operations. */
    callbacks->rpc_del_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/del", URI_rpc_del_cb, NULL);
    callbacks->rpc_mdel_cb   = evhttpx_set_cb(rpc->httpx, "/rpc/mdel", URI_rpc_mdel_cb, NULL);
    callbacks->rpc_remove_cb = evhttpx_set_cb(rpc->httpx, "/rpc/remove", URI_rpc_remove_cb, NULL);
    callbacks->rpc_clear_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/clear", URI_rpc_clear_cb, NULL);

    /* iterator related operations. */
    callbacks->rpc_iter_new_cb      = evhttpx_set_cb(rpc->httpx, "/rpc/iter/new", URI_rpc_iter_new_cb, NULL);
    callbacks->rpc_iter_first_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/iter/first", URI_rpc_iter_first_cb, NULL);
    callbacks->rpc_iter_last_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/iter/last", URI_rpc_iter_last_cb, NULL);
    callbacks->rpc_iter_next_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/iter/next", URI_rpc_iter_next_cb, NULL);
    callbacks->rpc_iter_prev_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/iter/prev", URI_rpc_iter_prev_cb, NULL);
    callbacks->rpc_iter_forward_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/iter/forward", URI_rpc_iter_forward_cb, NULL);
    callbacks->rpc_iter_backward_cb = evhttpx_set_cb(rpc->httpx, "/rpc/iter/backward", URI_rpc_iter_backward_cb, NULL);
    callbacks->rpc_iter_seek_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/iter/seek", URI_rpc_iter_seek_cb, NULL);
    callbacks->rpc_iter_key_cb      = evhttpx_set_cb(rpc->httpx, "/rpc/iter/key", URI_rpc_iter_key_cb, NULL);
    callbacks->rpc_iter_value_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/iter/value", URI_rpc_iter_value_cb, NULL);
    callbacks->rpc_iter_kv_cb       = evhttpx_set_cb(rpc->httpx, "/rpc/iter/kv", URI_rpc_iter_kv_cb, NULL);
    callbacks->rpc_iter_destroy_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/iter/destroy", URI_rpc_iter_destroy_cb, NULL);

    /* snapshot related operations. */
    callbacks->rpc_snapshot_new_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/snapshot/new", URI_rpc_snapshot_new_cb, NULL);
    callbacks->rpc_snapshot_release_cb = evhttpx_set_cb(rpc->httpx, "/rpc/snapshot/release", URI_rpc_snapshot_release_cb, NULL);

    /* writebatch related operations. */
    callbacks->rpc_writebatch_new_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/batch/new", URI_rpc_writebatch_new_cb, NULL);
    callbacks->rpc_writebatch_put_cb     = evhttpx_set_cb(rpc->httpx, "/rpc/batch/put", URI_rpc_writebatch_put_cb, NULL);
    callbacks->rpc_writebatch_delete_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/batch/del", URI_rpc_writebatch_del_cb, NULL);
    callbacks->rpc_writebatch_clear_cb   = evhttpx_set_cb(rpc->httpx, "/rpc/batch/clear", URI_rpc_writebatch_clear_cb, NULL);
    callbacks->rpc_writebatch_commit_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/batch/commit", URI_rpc_writebatch_commit_cb, NULL);
    callbacks->rpc_writebatch_destroy_cb = evhttpx_set_cb(rpc->httpx, "/rpc/batch/destroy", URI_rpc_writebatch_destroy_cb, NULL);

    /* miscs operations. */
    callbacks->rpc_sync_cb    = evhttpx_set_cb(rpc->httpx, "/rpc/sync", URI_rpc_sync_cb, NULL);
    callbacks->rpc_check_cb   = evhttpx_set_cb(rpc->httpx, "/rpc/check", URI_rpc_check_cb, NULL);
    callbacks->rpc_exists_cb  = evhttpx_set_cb(rpc->httpx, "/rpc/exists", URI_rpc_exists_cb, NULL);
    callbacks->rpc_version_cb = evhttpx_set_cb(rpc->httpx, "/rpc/version", URI_rpc_version_cb, NULL);

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

    rpc->sslcfg = sslcfg;
    rpc->callbacks = callbacks;
    rpc->config = config;
    _rpc_fill_ports(rpc, config->server_config->rpcports);

    return rpc;
}

void
reveldb_rpc_run(reveldb_rpc_t *rpc)
{
    assert(rpc != NULL);
    int i;
    reveldb_config_t *config = rpc->config;

    if (rpc->config->server_config->https == true) {
        evhttpx_ssl_init(rpc->httpx, rpc->sslcfg);
    }
    evhttpx_use_threads(rpc->httpx, NULL, 4, NULL);
    for (i = 0; i < rpc->num_ports; i++) {
        evhttpx_bind_socket(rpc->httpx,
                config->server_config->host,
                rpc->ports[i],
                config->server_config->backlog);
    }

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
    evhttpx_callback_free(rpc->callbacks->rpc_kregex_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_vregex_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_similar_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_ksimilar_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_vsimilar_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_incr_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_decr_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_cas_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_replace_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_del_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_mdel_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_remove_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_clear_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_iter_new_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_first_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_last_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_next_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_prev_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_forward_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_backward_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_seek_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_key_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_value_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_kv_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_iter_destroy_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_snapshot_new_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_snapshot_release_cb);
    
    evhttpx_callback_free(rpc->callbacks->rpc_writebatch_new_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_writebatch_put_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_writebatch_delete_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_writebatch_clear_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_writebatch_commit_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_writebatch_destroy_cb);

    evhttpx_callback_free(rpc->callbacks->rpc_sync_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_check_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_exists_cb);
    evhttpx_callback_free(rpc->callbacks->rpc_version_cb);

    evhttpx_free(rpc->httpx);
    event_base_free(rpc->evbase);
    free(rpc->sslcfg);
    free(rpc->callbacks);
    free(rpc);
}

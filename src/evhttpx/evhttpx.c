#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <strings.h>
#include <inttypes.h>
#include <sys/socket.h>
#ifndef NO_SYS_UN
#include <sys/un.h>
#endif
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <sys/tree.h>

#include <reveldb/evhttpx/evhttpx.h>

static int _evhttpx_request_parser_start(http_parser_t * p);
static int _evhttpx_request_parser_path(http_parser_t * p, const char * data, size_t len);
static int _evhttpx_request_parser_args(http_parser_t * p, const char * data, size_t len);
static int _evhttpx_request_parser_header_key(http_parser_t * p, const char * data, size_t len);
static int _evhttpx_request_parser_header_val(http_parser_t * p, const char * data, size_t len);
static int _evhttpx_request_parser_hostname(http_parser_t * p, const char * data, size_t len);
static int _evhttpx_request_parser_headers(http_parser_t * p);
static int _evhttpx_request_parser_body(http_parser_t * p, const char * data, size_t len);
static int _evhttpx_request_parser_fini(http_parser_t * p);
static int _evhttpx_request_parser_chunk_new(http_parser_t * p);
static int _evhttpx_request_parser_chunk_fini(http_parser_t * p);
static int _evhttpx_request_parser_chunks_fini(http_parser_t * p);
static int _evhttpx_request_parser_headers_start(http_parser_t * p);

static void _evhttpx_connection_readcb(evbev_t * bev, void * arg);

static evhttpx_connection_t * _evhttpx_connection_new(evhttpx_t * httpx, int sock);

static evhttpx_uri_t * _evhttpx_uri_new(void);
static void _evhttpx_uri_free(evhttpx_uri_t * uri);

static evhttpx_path_t * _evhttpx_path_new(const char * data, size_t len);
static void _evhttpx_path_free(evhttpx_path_t * path);

#define HOOK_AVAIL(var, hook_name) (var->hooks && var->hooks->hook_name)
#define HOOK_FUNC(var, hook_name) (var->hooks->hook_name)
#define HOOK_ARGS(var, hook_name) var->hooks->hook_name ## _arg

#define HOOK_REQUEST_RUN(request, hook_name, ...)  do {                                       \
        if (HOOK_AVAIL(request, hook_name)) {                                                 \
            return HOOK_FUNC(request, hook_name) (request, __VA_ARGS__,                       \
                                                  HOOK_ARGS(request, hook_name));             \
        }                                                                                     \
                                                                                              \
        if (HOOK_AVAIL(evhttpx_request_get_connection(request), hook_name)) {                   \
            return HOOK_FUNC(request->conn, hook_name) (request, __VA_ARGS__,                 \
                                                        HOOK_ARGS(request->conn, hook_name)); \
        }                                                                                     \
} while (0)

#define HOOK_REQUEST_RUN_NARGS(request, hook_name) do {                                       \
        if (HOOK_AVAIL(request, hook_name)) {                                                 \
            return HOOK_FUNC(request, hook_name) (request,                                    \
                                                  HOOK_ARGS(request, hook_name));             \
        }                                                                                     \
                                                                                              \
        if (HOOK_AVAIL(request->conn, hook_name)) {                                           \
            return HOOK_FUNC(request->conn, hook_name) (request,                              \
                                                        HOOK_ARGS(request->conn, hook_name)); \
        }                                                                                     \
} while (0);

#ifndef EVHTTPX_DISABLE_EVTHR
#define _evhttpx_lock(h)                             do { \
        if (h->lock) {                                  \
            pthread_mutex_lock(h->lock);                \
        }                                               \
} while (0)

#define _evhttpx_unlock(h)                           do { \
        if (h->lock) {                                  \
            pthread_mutex_unlock(h->lock);              \
        }                                               \
} while (0)
#else
#define _evhttpx_lock(h)                             do {} while (0)
#define _evhttpx_unlock(h)                           do {} while (0)
#endif

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)        \
    for ((var) = TAILQ_FIRST((head));                     \
         (var) && ((tvar) = TAILQ_NEXT((var), field), 1); \
         (var) = (tvar))
#endif

static int scode_tree_initialized = 0;

/**
 * @brief An RBTREE entry for the status code -> str matcher
 */
struct status_code {
    evhttpx_res    code;
    const char * str;

    RB_ENTRY(status_code) entry;
};


static int
status_code_cmp(void * _a, void * _b)
{
    struct status_code * a = _a;
    struct status_code * b = _b;

    return b->code - a->code;
}

RB_HEAD(status_code_tree, status_code) status_code_head = RB_INITIALIZER(&status_code_head);
RB_GENERATE(status_code_tree, status_code, entry, status_code_cmp)

#define scode_add(scode, cstr) do {                                  \
        struct status_code * c = malloc(sizeof(struct status_code)); \
                                                                     \
        c->code = scode;                                             \
        c->str  = cstr;                                              \
                                                                     \
        RB_INSERT(status_code_tree, &status_code_head, c);           \
} while (0)

static void
status_code_init(void)
{
    if (scode_tree_initialized) {
        /* Already initialized. */
        return;
    }

    /* 100 codes */
    scode_add(EVHTTPX_RES_CONTINUE, "Continue");
    scode_add(EVHTTPX_RES_SWITCH_PROTO, "Switching Protocols");
    scode_add(EVHTTPX_RES_PROCESSING, "Processing");
    scode_add(EVHTTPX_RES_URI_TOOLONG, "URI Too Long");

    /* 200 codes */
    scode_add(EVHTTPX_RES_200, "OK");
    scode_add(EVHTTPX_RES_CREATED, "Created");
    scode_add(EVHTTPX_RES_ACCEPTED, "Accepted");
    scode_add(EVHTTPX_RES_NAUTHINFO, "No Auth Info");
    scode_add(EVHTTPX_RES_NOCONTENT, "No Content");
    scode_add(EVHTTPX_RES_RSTCONTENT, "Reset Content");
    scode_add(EVHTTPX_RES_PARTIAL, "Partial Content");
    scode_add(EVHTTPX_RES_MSTATUS, "Multi-Status");
    scode_add(EVHTTPX_RES_IMUSED, "IM Used");

    /* 300 codes */
    scode_add(EVHTTPX_RES_300, "Redirect");
    scode_add(EVHTTPX_RES_MOVEDPERM, "Moved Permanently");
    scode_add(EVHTTPX_RES_FOUND, "Found");
    scode_add(EVHTTPX_RES_SEEOTHER, "See Other");
    scode_add(EVHTTPX_RES_NOTMOD, "Not Modified");
    scode_add(EVHTTPX_RES_USEPROXY, "Use Proxy");
    scode_add(EVHTTPX_RES_SWITCHPROXY, "Switch Proxy");
    scode_add(EVHTTPX_RES_TMPREDIR, "Temporary Redirect");

    /* 400 codes */
    scode_add(EVHTTPX_RES_400, "Bad Request");
    scode_add(EVHTTPX_RES_UNAUTH, "Unauthorized");
    scode_add(EVHTTPX_RES_PAYREQ, "Payment Required");
    scode_add(EVHTTPX_RES_FORBIDDEN, "Forbidden");
    scode_add(EVHTTPX_RES_NOTFOUND, "Not Found");
    scode_add(EVHTTPX_RES_METHNALLOWED, "Not Allowed");
    scode_add(EVHTTPX_RES_NACCEPTABLE, "Not Acceptable");
    scode_add(EVHTTPX_RES_PROXYAUTHREQ, "Proxy Authentication Required");
    scode_add(EVHTTPX_RES_TIMEOUT, "Request Timeout");
    scode_add(EVHTTPX_RES_CONFLICT, "Conflict");
    scode_add(EVHTTPX_RES_GONE, "Gone");
    scode_add(EVHTTPX_RES_LENREQ, "Length Required");
    scode_add(EVHTTPX_RES_PRECONDFAIL, "Precondition Failed");
    scode_add(EVHTTPX_RES_ENTOOLARGE, "Entity Too Large");
    scode_add(EVHTTPX_RES_URITOOLARGE, "Request-URI Too Long");
    scode_add(EVHTTPX_RES_UNSUPPORTED, "Unsupported Media Type");
    scode_add(EVHTTPX_RES_RANGENOTSC, "Requested Range Not Satisfiable");
    scode_add(EVHTTPX_RES_EXPECTFAIL, "Expectation Failed");
    scode_add(EVHTTPX_RES_IAMATEAPOT, "I'm a teapot");

    /* 500 codes */
    scode_add(EVHTTPX_RES_SERVERR, "Internal Server Error");
    scode_add(EVHTTPX_RES_NOTIMPL, "Not Implemented");
    scode_add(EVHTTPX_RES_BADGATEWAY, "Bad Gateway");
    scode_add(EVHTTPX_RES_SERVUNAVAIL, "Service Unavailable");
    scode_add(EVHTTPX_RES_GWTIMEOUT, "Gateway Timeout");
    scode_add(EVHTTPX_RES_VERNSUPPORT, "HTTP Version Not Supported");
    scode_add(EVHTTPX_RES_BWEXEED, "Bandwidth Limit Exceeded");

    scode_tree_initialized = 1;
}     /* status_code_init */

const char *
status_code_to_str(evhttpx_res code)
{
    struct status_code   c;
    struct status_code * found;

    c.code = code;

    if (!(found = RB_FIND(status_code_tree, &status_code_head, &c))) {
        return "DERP";
    }

    return found->str;
}

/**
 * @brief callback definitions for request processing from http_parser
 */
static http_parse_hooks_t request_psets = {
    .on_msg_begin       = _evhttpx_request_parser_start,
    .method             = NULL,
    .scheme             = NULL,
    .host               = NULL,
    .port               = NULL,
    .path               = _evhttpx_request_parser_path,
    .args               = _evhttpx_request_parser_args,
    .uri                = NULL,
    .on_hdrs_begin      = _evhttpx_request_parser_headers_start,
    .hdr_key            = _evhttpx_request_parser_header_key,
    .hdr_val            = _evhttpx_request_parser_header_val,
    .hostname           = _evhttpx_request_parser_hostname,
    .on_hdrs_complete   = _evhttpx_request_parser_headers,
    .on_new_chunk       = _evhttpx_request_parser_chunk_new,
    .on_chunk_complete  = _evhttpx_request_parser_chunk_fini,
    .on_chunks_complete = _evhttpx_request_parser_chunks_fini,
    .body               = _evhttpx_request_parser_body,
    .on_msg_complete    = _evhttpx_request_parser_fini
};

#ifndef EVHTTPX_DISABLE_SSL
static int             session_id_context    = 1;
static int             ssl_num_locks;
static evhttpx_mutex_t * ssl_locks;
static int             ssl_locks_initialized = 0;
#endif

/*
 * COMPAT FUNCTIONS
 */

#ifdef NO_STRNLEN
static size_t
strnlen(const char * s, size_t maxlen)
{
    const char * e;
    size_t       n;

    for (e = s, n = 0; *e && n < maxlen; e++, n++) {
        ;
    }

    return n;
}

#endif

#ifdef NO_STRNDUP
static char *
strndup(const char * s, size_t n)
{
    size_t len = strnlen(s, n);
    char * ret;

    if (len < n) {
        return strdup(s);
    }

    ret    = malloc(n + 1);
    ret[n] = '\0';

    strncpy(ret, s, n);
    return ret;
}

#endif

/*
 * PRIVATE FUNCTIONS
 */

/**
 * @brief a weak hash function
 *
 * @param str a null terminated string
 *
 * @return an unsigned integer hash of str
 */
static inline unsigned int
_evhttpx_quick_hash(const char * str)
{
    unsigned int h = 0;

    for (; *str; str++) {
        h = 31 * h + *str;
    }

    return h;
}

/**
 * @brief helper function to determine if http version is HTTP/1.0
 *
 * @param major the major version number
 * @param minor the minor version number
 *
 * @return 1 if HTTP/1.0, else 0
 */
static inline int
_evhttpx_is_http_10(const char major, const char minor)
{
    if (major >= 1 && minor <= 0) {
        return 1;
    }

    return 0;
}

/**
 * @brief helper function to determine if http version is HTTP/1.1
 *
 * @param major the major version number
 * @param minor the minor version number
 *
 * @return 1 if HTTP/1.1, else 0
 */
static inline int
_evhttpx_is_http_11(const char major, const char minor)
{
    if (major >= 1 && minor >= 1) {
        return 1;
    }

    return 0;
}

/**
 * @brief returns the HTTP protocol version
 *
 * @param major the major version number
 * @param minor the minor version number
 *
 * @return EVHTTPX_PROTO_10 if HTTP/1.0, EVHTTPX_PROTO_11 if HTTP/1.1, otherwise
 *         EVHTTPX_PROTO_INVALID
 */
static inline evhttpx_proto
_evhttpx_protocol(const char major, const char minor)
{
    if (_evhttpx_is_http_10(major, minor)) {
        return evhttpx_PROTO_10;
    }

    if (_evhttpx_is_http_11(major, minor)) {
        return evhttpx_PROTO_11;
    }

    return evhttpx_PROTO_INVALID;
}

/**
 * @brief runs the user-defined on_path hook for a request
 *
 * @param request the request structure
 * @param path the path structure
 *
 * @return EVHTTPX_RES_OK on success, otherwise something else.
 */
static inline evhttpx_res
_evhttpx_path_hook(evhttpx_request_t * request, evhttpx_path_t * path)
{
    HOOK_REQUEST_RUN(request, on_path, path);

    return EVHTTPX_RES_OK;
}

/**
 * @brief runs the user-defined on_header hook for a request
 *
 * once a full key: value header has been parsed, this will call the hook
 *
 * @param request the request strucutre
 * @param header the header structure
 *
 * @return EVHTTPX_RES_OK on success, otherwise something else.
 */
static inline evhttpx_res
_evhttpx_header_hook(evhttpx_request_t * request, evhttpx_header_t * header)
{
    HOOK_REQUEST_RUN(request, on_header, header);

    return EVHTTPX_RES_OK;
}

/**
 * @brief runs the user-defined on_Headers hook for a request after all headers
 *        have been parsed.
 *
 * @param request the request structure
 * @param headers the headers tailq structure
 *
 * @return EVHTTPX_RES_OK on success, otherwise something else.
 */
static inline evhttpx_res
_evhttpx_headers_hook(evhttpx_request_t * request, evhttpx_headers_t * headers)
{
    HOOK_REQUEST_RUN(request, on_headers, headers);

    return EVHTTPX_RES_OK;
}

/**
 * @brief runs the user-defined on_body hook for requests containing a body.
 *        the data is stored in the request->buffer_in so the user may either
 *        leave it, or drain upon being called.
 *
 * @param request the request strucutre
 * @param buf a evbuffer containing body data
 *
 * @return EVHTTPX_RES_OK on success, otherwise something else.
 */
static inline evhttpx_res
_evhttpx_body_hook(evhttpx_request_t * request, evbuf_t * buf)
{
    HOOK_REQUEST_RUN(request, on_read, buf);

    return EVHTTPX_RES_OK;
}

/**
 * @brief runs the user-defined hook called just prior to a request been
 *        free()'d
 *
 * @param request therequest structure
 *
 * @return EVHTTPX_RES_OK on success, otherwise treated as an error
 */
static inline evhttpx_res
_evhttpx_request_fini_hook(evhttpx_request_t * request)
{
    HOOK_REQUEST_RUN_NARGS(request, on_request_fini);

    return EVHTTPX_RES_OK;
}

static inline evhttpx_res
_evhttpx_chunk_new_hook(evhttpx_request_t * request, uint64_t len)
{
    HOOK_REQUEST_RUN(request, on_new_chunk, len);

    return EVHTTPX_RES_OK;
}

static inline evhttpx_res
_evhttpx_chunk_fini_hook(evhttpx_request_t * request)
{
    HOOK_REQUEST_RUN_NARGS(request, on_chunk_fini);

    return EVHTTPX_RES_OK;
}

static inline evhttpx_res
_evhttpx_chunks_fini_hook(evhttpx_request_t * request)
{
    HOOK_REQUEST_RUN_NARGS(request, on_chunks_fini);

    return EVHTTPX_RES_OK;
}

static inline evhttpx_res
_evhttpx_headers_start_hook(evhttpx_request_t * request)
{
    HOOK_REQUEST_RUN_NARGS(request, on_headers_start);

    return EVHTTPX_RES_OK;
}

/**
 * @brief runs the user-definedhook called just prior to a connection being
 *        closed
 *
 * @param connection the connection structure
 *
 * @return EVHTTPX_RES_OK on success, but pretty much ignored in any case.
 */
static inline evhttpx_res
_evhttpx_connection_fini_hook(evhttpx_connection_t * connection)
{
    if (connection->hooks && connection->hooks->on_connection_fini) {
        return (connection->hooks->on_connection_fini)(connection,
                                                       connection->hooks->on_connection_fini_arg);
    }

    return EVHTTPX_RES_OK;
}

static inline evhttpx_res
_evhttpx_hostname_hook(evhttpx_request_t * r, const char * hostname)
{
    HOOK_REQUEST_RUN(r, on_hostname, hostname);

    return EVHTTPX_RES_OK;
}

static inline evhttpx_res
_evhttpx_connection_write_hook(evhttpx_connection_t * connection)
{
    if (connection->hooks && connection->hooks->on_write) {
        return (connection->hooks->on_write)(connection,
                                             connection->hooks->on_write_arg);
    }

    return EVHTTPX_RES_OK;
}

/**
 * @brief glob/wildcard type pattern matching.
 *
 * Note: This code was derived from redis's (v2.6) stringmatchlen() function.
 *
 * @param pattern
 * @param string
 *
 * @return
 */
static int
_evhttpx_glob_match(const char * pattern, const char * string)
{
    size_t pat_len;
    size_t str_len;

    if (!pattern || !string) {
        return 0;
    }

    pat_len = strlen(pattern);
    str_len = strlen(string);

    while (pat_len) {
        if (pattern[0] == '*') {
            while (pattern[1] == '*') {
                pattern++;
                pat_len--;
            }

            if (pat_len == 1) {
                return 1;
            }

            while (str_len) {
                if (_evhttpx_glob_match(pattern + 1, string)) {
                    return 1;
                }

                string++;
                str_len--;
            }

            return 0;
        } else {
            if (pattern[0] != string[0]) {
                return 0;
            }

            string++;
            str_len--;
        }

        pattern++;
        pat_len--;

        if (str_len == 0) {
            while (*pattern == '*') {
                pattern++;
                pat_len--;
            }
            break;
        }
    }

    if (pat_len == 0 && str_len == 0) {
        return 1;
    }

    return 0;
} /* _evhttpx_glob_match */

static evhttpx_callback_t *
_evhttpx_callback_find(evhttpx_callbacks_t * cbs,
                     const char        * path,
                     unsigned int      * start_offset,
                     unsigned int      * end_offset)
{
    evhttpx_callback_t * callback;

    if (cbs == NULL) {
        return NULL;
    }

    TAILQ_FOREACH(callback, cbs, next) {
        switch (callback->type) {
            case evhttpx_callback_type_hash:
                if (strcmp(callback->val.path, path) == 0) {
                    *start_offset = 0;
                    *end_offset   = (unsigned int)strlen(path);
                    return callback;
                }
                break;
            case evhttpx_callback_type_glob:
                if (_evhttpx_glob_match(callback->val.glob, path) == 1) {
                    *start_offset = 0;
                    *end_offset   = (unsigned int)strlen(path);
                    return callback;
                }
            default:
                break;
        } /* switch */
    }

    return NULL;
}         /* _evhttpx_callback_find */

/**
 * @brief Creates a new evhttpx_request_t
 *
 * @param c
 *
 * @return evhttpx_request_t structure on success, otherwise NULL
 */
static evhttpx_request_t *
_evhttpx_request_new(evhttpx_connection_t * c)
{
    evhttpx_request_t * req;

    if (!(req = calloc(sizeof(evhttpx_request_t), 1))) {
        return NULL;
    }

    req->conn        = c;
    req->httpx         = c->httpx;
    req->status      = EVHTTPX_RES_OK;
    req->buffer_in   = evbuffer_new();
    req->buffer_out  = evbuffer_new();
    req->headers_in  = malloc(sizeof(evhttpx_headers_t));
    req->headers_out = malloc(sizeof(evhttpx_headers_t));

    TAILQ_INIT(req->headers_in);
    TAILQ_INIT(req->headers_out);

    return req;
}

/**
 * @brief frees all data in an evhttpx_request_t along with calling finished hooks
 *
 * @param request the request structure
 */
static void
_evhttpx_request_free(evhttpx_request_t * request)
{
    if (request == NULL) {
        return;
    }

    _evhttpx_request_fini_hook(request);
    _evhttpx_uri_free(request->uri);

    evhttpx_headers_free(request->headers_in);
    evhttpx_headers_free(request->headers_out);


    if (request->buffer_in) {
        evbuffer_free(request->buffer_in);
    }

    if (request->buffer_out) {
        evbuffer_free(request->buffer_out);
    }

    free(request->hooks);
    free(request);
}

/**
 * @brief create an overlay URI structure
 *
 * @return evhttpx_uri_t
 */
static evhttpx_uri_t *
_evhttpx_uri_new(void)
{
    evhttpx_uri_t * uri;

    if (!(uri = calloc(sizeof(evhttpx_uri_t), 1))) {
        return NULL;
    }

    return uri;
}

/**
 * @brief frees an overlay URI structure
 *
 * @param uri evhttpx_uri_t
 */
static void
_evhttpx_uri_free(evhttpx_uri_t * uri)
{
    if (uri == NULL) {
        return;
    }

    evhttpx_query_free(uri->query);
    _evhttpx_path_free(uri->path);

    free(uri->fragment);
    free(uri->query_raw);
    free(uri);
}

/**
 * @brief parses the path and file from an input buffer
 *
 * @details in order to properly create a structure that can match
 *          both a path and a file, this will parse a string into
 *          what it considers a path, and a file.
 *
 * @details if for example the input was "/a/b/c", the parser will
 *          consider "/a/b/" as the path, and "c" as the file.
 *
 * @param data raw input data (assumes a /path/[file] structure)
 * @param len length of the input data
 *
 * @return evhttpx_request_t * on success, NULL on error.
 */
static evhttpx_path_t *
_evhttpx_path_new(const char * data, size_t len)
{
    evhttpx_path_t * req_path;
    const char   * data_end = (const char *)(data + len);
    char         * path     = NULL;
    char         * file     = NULL;

    if (!(req_path = calloc(sizeof(evhttpx_path_t), 1))) {
        return NULL;
    }

    if (len == 0) {
        /*
         * odd situation here, no preceding "/", so just assume the path is "/"
         */
        path = strdup("/");
    } else if (*data != '/') {
        /* request like GET stupid HTTP/1.0, treat stupid as the file, and
         * assume the path is "/"
         */
        path = strdup("/");
        file = strndup(data, len);
    } else {
        if (data[len - 1] != '/') {
            /*
             * the last character in data is assumed to be a file, not the end of path
             * loop through the input data backwards until we find a "/"
             */
            size_t i;

            for (i = (len - 1); i != 0; i--) {
                if (data[i] == '/') {
                    /*
                     * we have found a "/" representing the start of the file,
                     * and the end of the path
                     */
                    size_t path_len;
                    size_t file_len;

                    path_len = (size_t)(&data[i] - data) + 1;
                    file_len = (size_t)(data_end - &data[i + 1]);

                    /* check for overflow */
                    if ((const char *)(data + path_len) > data_end) {
                        fprintf(stderr, "PATH Corrupted.. (path_len > len)\n");
                        free(req_path);
                        return NULL;
                    }

                    /* check for overflow */
                    if ((const char *)(&data[i + 1] + file_len) > data_end) {
                        fprintf(stderr, "FILE Corrupted.. (file_len > len)\n");
                        free(req_path);
                        return NULL;
                    }

                    path = strndup(data, path_len);
                    file = strndup(&data[i + 1], file_len);

                    break;
                }
            }

            if (i == 0 && data[i] == '/' && !file && !path) {
                /* drops here if the request is something like GET /foo */
                path = strdup("/");

                if (len > 1) {
                    file = strndup((const char *)(data + 1), len);
                }
            }
        } else {
            /* the last character is a "/", thus the request is just a path */
            path = strndup(data, len);
        }
    }

    if (len != 0) {
        req_path->full = strndup(data, len);
    }

    req_path->path = path;
    req_path->file = file;

    return req_path;
}     /* _evhttpx_path_new */

static void
_evhttpx_path_free(evhttpx_path_t * path)
{
    if (path == NULL) {
        return;
    }

    free(path->full);

    free(path->path);
    free(path->file);
    free(path->match_start);
    free(path->match_end);

    free(path);
}

static int
_evhttpx_request_parser_start(http_parser_t * p)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);

    if (c->request) {
        if (c->request->finished == 1) {
            _evhttpx_request_free(c->request);
        } else {
            return -1;
        }
    }

    if (!(c->request = _evhttpx_request_new(c))) {
        return -1;
    }

    return 0;
}

static int
_evhttpx_request_parser_args(http_parser_t * p, const char * data, size_t len)
{
    evhttpx_connection_t * c   = http_parser_get_userdata(p);
    evhttpx_uri_t        * uri = c->request->uri;

    if (!(uri->query = evhttpx_parse_query(data, len))) {
        c->request->status = EVHTTPX_RES_ERROR;
        return -1;
    }

    uri->query_raw = calloc(len + 1, 1);
    memcpy(uri->query_raw, data, len);

    return 0;
}

static int
_evhttpx_request_parser_headers_start(http_parser_t * p)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);

    if ((c->request->status = _evhttpx_headers_start_hook(c->request)) != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
}

static int
_evhttpx_request_parser_header_key(http_parser_t * p, const char * data, size_t len)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);
    char               * key_s;     /* = strndup(data, len); */
    evhttpx_header_t     * hdr;

    key_s      = malloc(len + 1);
    key_s[len] = '\0';
    memcpy(key_s, data, len);

    if ((hdr = evhttpx_header_key_add(c->request->headers_in, key_s, 0)) == NULL) {
        c->request->status = EVHTTPX_RES_FATAL;
        return -1;
    }

    hdr->k_heaped = 1;
    return 0;
}

static int
_evhttpx_request_parser_header_val(http_parser_t * p, const char * data, size_t len)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);
    char               * val_s;
    evhttpx_header_t     * header;

    val_s      = malloc(len + 1);
    val_s[len] = '\0';
    memcpy(val_s, data, len);

    if ((header = evhttpx_header_val_add(c->request->headers_in, val_s, 0)) == NULL) {
        c->request->status = EVHTTPX_RES_FATAL;
        return -1;
    }

    header->v_heaped = 1;

    if ((c->request->status = _evhttpx_header_hook(c->request, header)) != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
}

static inline evhttpx_t *
_evhttpx_request_find_vhost(evhttpx_t * evhttpx, const char * name)
{
    evhttpx_t       * evhttpx_vhost;
    evhttpx_alias_t * evhttpx_alias;

    TAILQ_FOREACH(evhttpx_vhost, &evhttpx->vhosts, next_vhost) {
        if (evhttpx_vhost->server_name == NULL) {
            continue;
        }

        if (_evhttpx_glob_match(evhttpx_vhost->server_name, name) == 1) {
            return evhttpx_vhost;
        }

        TAILQ_FOREACH(evhttpx_alias, &evhttpx_vhost->aliases, next) {
            if (evhttpx_alias->alias == NULL) {
                continue;
            }

            if (_evhttpx_glob_match(evhttpx_alias->alias, name) == 1) {
                return evhttpx_vhost;
            }
        }
    }

    return NULL;
}

static inline int
_evhttpx_request_set_callbacks(evhttpx_request_t * request)
{
    evhttpx_t            * evhttpx;
    evhttpx_connection_t * conn;
    evhttpx_uri_t        * uri;
    evhttpx_path_t       * path;
    evhttpx_hooks_t      * hooks;
    evhttpx_callback_t   * callback;
    evhttpx_callback_cb    cb;
    void               * cbarg;

    if (request == NULL) {
        return -1;
    }

    if ((evhttpx = request->httpx) == NULL) {
        return -1;
    }

    if ((conn = request->conn) == NULL) {
        return -1;
    }

    if ((uri = request->uri) == NULL) {
        return -1;
    }

    if ((path = uri->path) == NULL) {
        return -1;
    }

    hooks    = NULL;
    callback = NULL;
    cb       = NULL;
    cbarg    = NULL;

    if ((callback = _evhttpx_callback_find(evhttpx->callbacks, path->full,
                                         &path->matched_soff, &path->matched_eoff))) {
        /* matched a callback using both path and file (/a/b/c/d) */
        cb    = callback->cb;
        cbarg = callback->cbarg;
        hooks = callback->hooks;
    } else if ((callback = _evhttpx_callback_find(evhttpx->callbacks, path->path,
                                                &path->matched_soff, &path->matched_eoff))) {
        /* matched a callback using *just* the path (/a/b/c/) */
        cb    = callback->cb;
        cbarg = callback->cbarg;
        hooks = callback->hooks;
    } else {
        /* no callbacks found for either case, use defaults */
        cb    = evhttpx->defaults.cb;
        cbarg = evhttpx->defaults.cbarg;

        path->matched_soff = 0;
        path->matched_eoff = (unsigned int)strlen(path->full);
    }

    if (path->match_start == NULL) {
        path->match_start = calloc(strlen(path->full) + 1, 1);
    }

    if (path->match_end == NULL) {
        path->match_end = calloc(strlen(path->full) + 1, 1);
    }

    if (path->matched_eoff - path->matched_soff) {
        memcpy(path->match_start, (void *)(path->full + path->matched_soff),
               path->matched_eoff - path->matched_soff);
    } else {
        memcpy(path->match_start, (void *)(path->full + path->matched_soff),
               strlen((const char *)(path->full + path->matched_soff)));
    }

    memcpy(path->match_end,
           (void *)(path->full + path->matched_eoff),
           strlen(path->full) - path->matched_eoff);

    if (hooks != NULL) {
        if (request->hooks == NULL) {
            request->hooks = malloc(sizeof(evhttpx_hooks_t));
        }

        memcpy(request->hooks, hooks, sizeof(evhttpx_hooks_t));
    }

    request->cb    = cb;
    request->cbarg = cbarg;

    return 0;
} /* _evhttpx_request_set_callbacks */

static int
_evhttpx_request_parser_hostname(http_parser_t * p, const char * data, size_t len)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);
    evhttpx_t            * evhttpx;
    evhttpx_t            * evhttpx_vhost;

#ifndef EVHTTPX_DISABLE_SSL
    if (c->vhost_via_sni == 1 && c->ssl != NULL) {
        /* use the SNI set hostname instead of the header hostname */
        const char * host;

        host = SSL_get_servername(c->ssl, TLSEXT_NAMETYPE_host_name);

        if ((c->request->status = _evhttpx_hostname_hook(c->request, host)) != EVHTTPX_RES_OK) {
            return -1;
        }

        return 0;
    }
#endif

    evhttpx = c->httpx;

    /* since this is called after _evhttpx_request_parser_path(), which already
     * setup callbacks for the URI, we must now attempt to find callbacks which
     * are specific to this host.
     */
    _evhttpx_lock(evhttpx);
    {
        if ((evhttpx_vhost = _evhttpx_request_find_vhost(evhttpx, data))) {
            _evhttpx_lock(evhttpx_vhost);
            {
                /* if we found a match for the host, we must set the http
                 * variables for both the connection and the request.
                 */
                c->httpx          = evhttpx_vhost;
                c->request->httpx = evhttpx_vhost;

                _evhttpx_request_set_callbacks(c->request);
            }
            _evhttpx_unlock(evhttpx_vhost);
        }
    }
    _evhttpx_unlock(evhttpx);

    if ((c->request->status = _evhttpx_hostname_hook(c->request, data)) != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
} /* _evhttpx_request_parser_hostname */

static int
_evhttpx_request_parser_path(http_parser_t * p, const char * data, size_t len)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);
    evhttpx_uri_t        * uri;
    evhttpx_path_t       * path;

    if (!(uri = _evhttpx_uri_new())) {
        c->request->status = EVHTTPX_RES_FATAL;
        return -1;
    }

    if (!(path = _evhttpx_path_new(data, len))) {
        _evhttpx_uri_free(uri);
        c->request->status = EVHTTPX_RES_FATAL;
        return -1;
    }

    uri->path          = path;
    uri->scheme        = http_parser_get_scheme(p);

    c->request->method = http_parser_get_method(p);
    c->request->uri    = uri;

    _evhttpx_lock(c->httpx);
    {
        _evhttpx_request_set_callbacks(c->request);
    }
    _evhttpx_unlock(c->httpx);

    if ((c->request->status = _evhttpx_path_hook(c->request, path)) != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
}     /* _evhttpx_request_parser_path */

static int
_evhttpx_request_parser_headers(http_parser_t * p)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);

    /* XXX proto should be set with http_parser on_hdrs_begin hook */
    c->request->keepalive = http_parser_should_keep_alive(p);
    c->request->proto     = _evhttpx_protocol(http_parser_get_major(p), http_parser_get_minor(p));
    c->request->status    = _evhttpx_headers_hook(c->request, c->request->headers_in);

    if (c->request->status != EVHTTPX_RES_OK) {
        return -1;
    }

    if (!evhttpx_header_find(c->request->headers_in, "Expect")) {
        return 0;
    }

    evbuffer_add_printf(bufferevent_get_output(c->bev),
                        "HTTP/%d.%d 100 Continue\r\n\r\n",
                        http_parser_get_major(p),
                        http_parser_get_minor(p));

    return 0;
}

static int
_evhttpx_request_parser_body(http_parser_t * p, const char * data, size_t len)
{
    evhttpx_connection_t * c   = http_parser_get_userdata(p);
    evbuf_t            * buf;
    int                  res = 0;

    if (c->max_body_size > 0 && c->body_bytes_read + len >= c->max_body_size) {
        c->error           = 1;
        c->request->status = EVHTTPX_RES_DATA_TOO_LONG;

        return -1;
    }

    buf = evbuffer_new();
    evbuffer_add(buf, data, len);

    if ((c->request->status = _evhttpx_body_hook(c->request, buf)) != EVHTTPX_RES_OK) {
        res = -1;
    }

    if (evbuffer_get_length(buf)) {
        evbuffer_add_buffer(c->request->buffer_in, buf);
    }

    evbuffer_free(buf);

    c->body_bytes_read += len;

    return res;
}

static int
_evhttpx_request_parser_chunk_new(http_parser_t * p)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);

    if ((c->request->status = _evhttpx_chunk_new_hook(c->request,
                                                    http_parser_get_content_length(p))) != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
}

static int
_evhttpx_request_parser_chunk_fini(http_parser_t * p)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);

    if ((c->request->status = _evhttpx_chunk_fini_hook(c->request)) != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
}

static int
_evhttpx_request_parser_chunks_fini(http_parser_t * p)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);

    if ((c->request->status = _evhttpx_chunks_fini_hook(c->request)) != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
}

/**
 * @brief determines if the request body contains the query arguments.
 *        if the query is NULL and the contenet length of the body has never
 *        been drained, and the content-type is x-www-form-urlencoded, the
 *        function returns 1
 *
 * @param req
 *
 * @return 1 if evhttpx can use the body as the query arguments, 0 otherwise.
 */
static int
_evhttpx_should_parse_query_body(evhttpx_request_t * req)
{
    const char * content_type;

    if (req == NULL) {
        return 0;
    }

    if (req->uri == NULL && req->uri->query != NULL) {
        return 0;
    }

    if (evhttpx_request_content_len(req) == 0) {
        return 0;
    }

    if (evhttpx_request_content_len(req) !=
        evbuffer_get_length(req->buffer_in)) {
        return 0;
    }

    content_type = evhttpx_kv_find(req->headers_in, "Content-Type");

    if (content_type == NULL) {
        return 0;
    }

    if (strcasecmp(content_type, "application/x-www-form-urlencoded")) {
        return 0;
    }

    return 1;
}

static int
_evhttpx_request_parser_fini(http_parser_t * p)
{
    evhttpx_connection_t * c = http_parser_get_userdata(p);

    /* check to see if we should use the body of the request as the query
     * arguments.
     */
    if (_evhttpx_should_parse_query_body(c->request) == 1) {
        const char  * body;
        size_t        body_len;
        evhttpx_uri_t * uri;
        evbuf_t     * buf_in;

        uri            = c->request->uri;
        buf_in         = c->request->buffer_in;

        body_len       = evbuffer_get_length(buf_in);
        body           = (const char *)evbuffer_pullup(buf_in, body_len);

        uri->query_raw = calloc(body_len + 1, 1);
        memcpy(uri->query_raw, body, body_len);

        uri->query     = evhttpx_parse_query(body, body_len);
    }


    /*
     * XXX c->request should never be NULL, but we have found some path of
     * execution where this actually happens. We will check for now, but the bug
     * path needs to be tracked down.
     *
     */
    if (c->request && c->request->cb) {
        (c->request->cb)(c->request, c->request->cbarg);
    }

    return 0;
}

static int
_evhttpx_create_headers(evhttpx_header_t * header, void * arg)
{
    evbuf_t * buf = arg;

    evbuffer_add(buf, header->key, header->klen);
    evbuffer_add(buf, ": ", 2);
    evbuffer_add(buf, header->val, header->vlen);
    evbuffer_add(buf, "\r\n", 2);
    return 0;
}

static evbuf_t *
_evhttpx_create_reply(evhttpx_request_t * request, evhttpx_res code)
{
    evbuf_t    * buf          = evbuffer_new();
    const char * content_type = evhttpx_header_find(request->headers_out, "Content-Type");

    if (http_parser_get_multipart(request->conn->parser) == 1) {
        goto check_proto;
    }

    if (evbuffer_get_length(request->buffer_out) && request->chunked == 0) {
        /* add extra headers (like content-length/type) if not already present */

        if (!evhttpx_header_find(request->headers_out, "Content-Length")) {
            char lstr[128];
            int  sres;

            sres = snprintf(lstr, sizeof(lstr), "%zu",
                            evbuffer_get_length(request->buffer_out));

            if (sres >= sizeof(lstr) || sres < 0) {
                /* overflow condition, this should never happen, but if it does,
                 * well lets just shut the connection down */
                request->keepalive = 0;
                goto check_proto;
            }

            evhttpx_headers_add_header(request->headers_out,
                                     evhttpx_header_new("Content-Length", lstr, 0, 1));
        }

        if (!content_type) {
            evhttpx_headers_add_header(request->headers_out,
                                     evhttpx_header_new("Content-Type", "text/plain", 0, 0));
        }
    } else {
        if (!evhttpx_header_find(request->headers_out, "Content-Length")) {
            const char * chunked = evhttpx_header_find(request->headers_out,
                                                     "transfer-encoding");

            if (!chunked || !strstr(chunked, "chunked")) {
                evhttpx_headers_add_header(request->headers_out,
                                         evhttpx_header_new("Content-Length", "0", 0, 0));
            }
        }
    }

check_proto:
    /* add the proper keep-alive type headers based on http version */
    switch (request->proto) {
        case evhttpx_PROTO_11:
            if (request->keepalive == 0) {
                /* protocol is HTTP/1.1 but client wanted to close */
                evhttpx_headers_add_header(request->headers_out,
                                         evhttpx_header_new("Connection", "close", 0, 0));
            }
            break;
        case evhttpx_PROTO_10:
            if (request->keepalive == 1) {
                /* protocol is HTTP/1.0 and clients wants to keep established */
                evhttpx_headers_add_header(request->headers_out,
                        evhttpx_header_new("Connection", "keep-alive", 0, 0));
            }
            break;
        default:
            /* this sometimes happens when a response is made but paused before
             * the method has been parsed */
            http_parser_set_major(request->conn->parser, 1);
            http_parser_set_minor(request->conn->parser, 0);
            break;
    } /* switch */

    /* add the status line */
    evbuffer_add_printf(buf, "HTTP/%d.%d %d %s\r\n",
                        http_parser_get_major(request->conn->parser),
                        http_parser_get_minor(request->conn->parser),
                        code, status_code_to_str(code));

    evhttpx_headers_for_each(request->headers_out, _evhttpx_create_headers, buf);
    evbuffer_add(buf, "\r\n", 2);

    if (evbuffer_get_length(request->buffer_out)) {
        evbuffer_add_buffer(buf, request->buffer_out);
    }

    return buf;
}     /* _evhttpx_create_reply */

static void
_evhttpx_connection_resumecb(int fd, short events, void * arg)
{
    evhttpx_connection_t * c = arg;

    if (c->request) {
        c->request->status = EVHTTPX_RES_OK;
    }

    _evhttpx_connection_readcb(c->bev, c);
}

static void
_evhttpx_connection_readcb(evbev_t * bev, void * arg)
{
    evhttpx_connection_t * c = arg;
    void               * buf;
    size_t               nread;
    size_t               avail;

    avail = evbuffer_get_length(bufferevent_get_input(bev));

    if (c->request) {
        c->request->status = EVHTTPX_RES_OK;
    }


    buf = evbuffer_pullup(bufferevent_get_input(bev), avail);

    bufferevent_disable(bev, EV_WRITE);
    {
        nread = http_parser_run(c->parser, &request_psets, (const char *)buf, avail);
    }
    bufferevent_enable(bev, EV_WRITE);

    if (c->owner != 1) {
        /*
         * someone has taken the ownership of this connection, we still need to
         * drain the input buffer that had been read up to this point.
         */
        evbuffer_drain(bufferevent_get_input(bev), nread);
        evhttpx_connection_free(c);
        return;
    }

    if (c->request) {
        switch (c->request->status) {
            case EVHTTPX_RES_DATA_TOO_LONG:
                if (c->request->hooks && c->request->hooks->on_error) {
                    (*c->request->hooks->on_error)(c->request, -1,
                            c->request->hooks->on_error_arg);
                }
                evhttpx_connection_free(c);
                return;
            default:
                break;
        }
    }

    if (avail != nread) {
        if (c->request && c->request->status == EVHTTPX_RES_PAUSE) {
            evhttpx_request_pause(c->request);
        } else {
            evhttpx_connection_free(c);
            return;
        }
    }

    evbuffer_drain(bufferevent_get_input(bev), nread);
} /* _evhttpx_connection_readcb */

static void
_evhttpx_connection_writecb(evbev_t * bev, void * arg)
{
    evhttpx_connection_t * c = arg;

    if (c->request == NULL) {
        return;
    }

    _evhttpx_connection_write_hook(c);

    if (c->request->finished == 0 || evbuffer_get_length(bufferevent_get_output(bev))) {
        return;
    }

    /*
     * if there is a set maximum number of keepalive requests configured, check
     * to make sure we are not over it. If we have gone over the max we set the
     * keepalive bit to 0, thus closing the connection.
     */
    if (c->httpx->max_keepalive_requests) {
        if (++c->num_requests >= c->httpx->max_keepalive_requests) {
            c->request->keepalive = 0;
        }
    }

    if (c->request->keepalive) {
        _evhttpx_request_free(c->request);

        c->request         = NULL;
        c->body_bytes_read = 0;

        if (c->httpx->parent && c->vhost_via_sni == 0) {
            /* this request was servied by a virtual host evhttpx_t structure
             * which was *NOT* found via SSL SNI lookup. In this case we want to
             * reset our connections evhttpx_t structure back to the original so
             * that subsequent requests can have a different Host: header.
             */
            evhttpx_t * orig_httpx = c->httpx->parent;

            c->httpx = orig_httpx;
        }

        http_parser_init(c->parser, httpx_type_request);


        http_parser_set_userdata(c->parser, c);
        return;
    } else {
        evhttpx_connection_free(c);
        return;
    }

    return;
} /* _evhttpx_connection_writecb */

static void
_evhttpx_connection_eventcb(evbev_t * bev, short events, void * arg)
{
    evhttpx_connection_t * c;

    if ((events & BEV_EVENT_CONNECTED)) {
        return;
    }

    c = arg;

    if (c->ssl && !(events & BEV_EVENT_EOF)) {
        /* XXX need to do better error handling for SSL specific errors */
        c->error = 1;

        if (c->request) {
            c->request->error = 1;
        }
    }

    c->error = 1;

    if (c->request && c->request->hooks && c->request->hooks->on_error) {
        (*c->request->hooks->on_error)(c->request, events,
                                       c->request->hooks->on_error_arg);
    }

    evhttpx_connection_free((evhttpx_connection_t *)arg);
}

static int
_evhttpx_run_pre_accept(evhttpx_t * httpx, evhttpx_connection_t * conn)
{
    void    * args;
    evhttpx_res res;

    if (httpx->defaults.pre_accept == NULL) {
        return 0;
    }

    args = httpx->defaults.pre_accept_cbarg;
    res  = httpx->defaults.pre_accept(conn, args);

    if (res != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
}

static int
_evhttpx_connection_accept(evbase_t * evbase, evhttpx_connection_t * connection)
{
    struct timeval * c_recv_timeo;
    struct timeval * c_send_timeo;

    if (_evhttpx_run_pre_accept(connection->httpx, connection) < 0) {
        evutil_closesocket(connection->sock);
        return -1;
    }

#ifndef EVHTTPX_DISABLE_SSL
    if (connection->httpx->ssl_ctx != NULL) {
        connection->ssl = SSL_new(connection->httpx->ssl_ctx);
        connection->bev = bufferevent_openssl_socket_new(evbase,
                                                         connection->sock,
                                                         connection->ssl,
                                                         BUFFEREVENT_SSL_ACCEPTING,
                                                         connection->httpx->bev_flags);
        SSL_set_app_data(connection->ssl, connection);
        goto end;
    }
#endif

    connection->bev = bufferevent_socket_new(evbase,
                                             connection->sock,
                                             connection->httpx->bev_flags);
#ifndef EVHTTPX_DISABLE_SSL
end:
#endif

    if (connection->recv_timeo.tv_sec || connection->recv_timeo.tv_usec) {
        c_recv_timeo = &connection->recv_timeo;
    } else if (connection->httpx->recv_timeo.tv_sec ||
               connection->httpx->recv_timeo.tv_usec) {
        c_recv_timeo = &connection->httpx->recv_timeo;
    } else {
        c_recv_timeo = NULL;
    }

    if (connection->send_timeo.tv_sec || connection->send_timeo.tv_usec) {
        c_send_timeo = &connection->send_timeo;
    } else if (connection->httpx->send_timeo.tv_sec ||
               connection->httpx->send_timeo.tv_usec) {
        c_send_timeo = &connection->httpx->send_timeo;
    } else {
        c_send_timeo = NULL;
    }

    evhttpx_connection_set_timeouts(connection, c_recv_timeo, c_send_timeo);

    connection->resume_ev = event_new(evbase, -1, EV_READ | EV_PERSIST,
                                      _evhttpx_connection_resumecb, connection);
    event_add(connection->resume_ev, NULL);

    bufferevent_enable(connection->bev, EV_READ);
    bufferevent_setcb(connection->bev,
                      _evhttpx_connection_readcb,
                      _evhttpx_connection_writecb,
                      _evhttpx_connection_eventcb, connection);

    return 0;
}     /* _evhttpx_connection_accept */

static void
_evhttpx_default_request_cb(evhttpx_request_t * request, void * arg)
{
    evhttpx_send_reply(request, EVHTTPX_RES_NOTFOUND);
}

static evhttpx_connection_t *
_evhttpx_connection_new(evhttpx_t * httpx, int sock)
{
    evhttpx_connection_t * connection;

    if (!(connection = calloc(sizeof(evhttpx_connection_t), 1))) {
        return NULL;
    }

    connection->error  = 0;
    connection->owner  = 1;
    connection->sock   = sock;
    connection->httpx    = httpx;
    connection->parser = http_parser_new();

    http_parser_init(connection->parser, httpx_type_request);
    http_parser_set_userdata(connection->parser, connection);

    return connection;
}

#ifdef LIBEVENT_HAS_SHUTDOWN
#ifndef EVHTTPX_DISABLE_SSL
static void
_evhttpx_shutdown_eventcb(evbev_t * bev, short events, void * arg)
{
}

#endif
#endif

static int
_evhttpx_run_post_accept(evhttpx_t * httpx, evhttpx_connection_t * connection)
{
    void    * args;
    evhttpx_res res;

    if (httpx->defaults.post_accept == NULL) {
        return 0;
    }

    args = httpx->defaults.post_accept_cbarg;
    res  = httpx->defaults.post_accept(connection, args);

    if (res != EVHTTPX_RES_OK) {
        return -1;
    }

    return 0;
}

#ifndef EVHTTPX_DISABLE_EVTHR
static void
_evhttpx_run_in_thread(evthr_t * thr, void * arg, void * shared)
{
    evhttpx_t            * httpx        = shared;
    evhttpx_connection_t * connection = arg;

    connection->evbase = evthr_get_base(thr);
    connection->thread = thr;

    evthr_inc_backlog(connection->thread);

    if (_evhttpx_connection_accept(connection->evbase, connection) < 0) {
        evhttpx_connection_free(connection);
        return;
    }

    if (_evhttpx_run_post_accept(httpx, connection) < 0) {
        evhttpx_connection_free(connection);
        return;
    }
}

#endif

static void
_evhttpx_accept_cb(evserv_t * serv, int fd, struct sockaddr * s, int sl, void * arg)
{
    evhttpx_t            * httpx = arg;
    evhttpx_connection_t * connection;

    if (!(connection = _evhttpx_connection_new(httpx, fd))) {
        return;
    }

    connection->saddr = malloc(sl);
    memcpy(connection->saddr, s, sl);

#ifndef EVHTTPX_DISABLE_EVTHR
    if (httpx->thr_pool != NULL) {
        if (evthr_pool_defer(httpx->thr_pool, _evhttpx_run_in_thread, connection) != EVTHR_RES_OK) {
            evutil_closesocket(connection->sock);
            evhttpx_connection_free(connection);
            return;
        }
        return;
    }
#endif
    connection->evbase = httpx->evbase;

    if (_evhttpx_connection_accept(httpx->evbase, connection) < 0) {
        evhttpx_connection_free(connection);
        return;
    }

    if (_evhttpx_run_post_accept(httpx, connection) < 0) {
        evhttpx_connection_free(connection);
        return;
    }
}

#ifndef EVHTTPX_DISABLE_SSL
#ifndef EVHTTPX_DISABLE_EVTHR
static unsigned long
_evhttpx_ssl_get_thread_id(void)
{
    return (unsigned long)pthread_self();
}

static void
_evhttpx_ssl_thread_lock(int mode, int type, const char * file, int line)
{
    if (type < ssl_num_locks) {
        if (mode & CRYPTO_LOCK) {
            pthread_mutex_lock(&(ssl_locks[type]));
        } else {
            pthread_mutex_unlock(&(ssl_locks[type]));
        }
    }
}

#endif
static void
_evhttpx_ssl_delete_scache_ent(evhttpx_ssl_ctx_t * ctx, evhttpx_ssl_sess_t * sess)
{
    evhttpx_t         * httpx;
    evhttpx_ssl_cfg_t * cfg;
    unsigned char   * sid;
    unsigned int      slen;

    httpx  = (evhttpx_t *)SSL_CTX_get_app_data(ctx);
    cfg  = httpx->ssl_cfg;

    sid  = sess->session_id;
    slen = sess->session_id_length;

    if (cfg->scache_del) {
        (cfg->scache_del)(httpx, sid, slen);
    }
}

static int
_evhttpx_ssl_add_scache_ent(evhttpx_ssl_t * ssl, evhttpx_ssl_sess_t * sess)
{
    evhttpx_connection_t * connection;
    evhttpx_ssl_cfg_t    * cfg;
    unsigned char      * sid;
    int                  slen;

    connection = (evhttpx_connection_t *)SSL_get_app_data(ssl);
    cfg        = connection->httpx->ssl_cfg;

    sid        = sess->session_id;
    slen       = sess->session_id_length;

    SSL_set_timeout(sess, cfg->scache_timeout);

    if (cfg->scache_add) {
        return (cfg->scache_add)(connection, sid, slen, sess);
    }

    return 0;
}

static evhttpx_ssl_sess_t *
_evhttpx_ssl_get_scache_ent(evhttpx_ssl_t * ssl,
        unsigned char * sid, int sid_len, int * copy)
{
    evhttpx_connection_t * connection;
    evhttpx_ssl_cfg_t    * cfg;
    evhttpx_ssl_sess_t   * sess;

    connection = (evhttpx_connection_t * )SSL_get_app_data(ssl);
    cfg        = connection->httpx->ssl_cfg;
    sess       = NULL;

    if (cfg->scache_get) {
        sess = (cfg->scache_get)(connection, sid, sid_len);
    }

    *copy = 0;

    return sess;
}

static int
_evhttpx_ssl_servername(evhttpx_ssl_t * ssl, int * unused, void * arg)
{
    const char         * sname;
    evhttpx_connection_t * connection;
    evhttpx_t            * evhttpx;
    evhttpx_t            * evhttpx_vhost;

    if (!(sname = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name))) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    if (!(connection = SSL_get_app_data(ssl))) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    if (!(evhttpx = connection->httpx)) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    if ((evhttpx_vhost = _evhttpx_request_find_vhost(evhttpx, sname))) {
        connection->httpx           = evhttpx_vhost;
        connection->vhost_via_sni = 1;

        SSL_set_SSL_CTX(ssl, evhttpx_vhost->ssl_ctx);
        SSL_set_options(ssl, SSL_CTX_get_options(ssl->ctx));

        if ((SSL_get_verify_mode(ssl) == SSL_VERIFY_NONE) ||
            (SSL_num_renegotiations(ssl) == 0)) {
            SSL_set_verify(ssl, SSL_CTX_get_verify_mode(ssl->ctx),
                           SSL_CTX_get_verify_callback(ssl->ctx));
        }

        return SSL_TLSEXT_ERR_OK;
    }

    return SSL_TLSEXT_ERR_NOACK;
} /* _evhttpx_ssl_servername */

#endif

/*
 * PUBLIC FUNCTIONS
 */

http_method_e
evhttpx_request_get_method(evhttpx_request_t * r)
{
    return http_parser_get_method(r->conn->parser);
}

/**
 * @brief pauses a connection (disables reading)
 *
 * @param c a evhttpx_connection_t * structure
 */
void
evhttpx_connection_pause(evhttpx_connection_t * c)
{
    if ((bufferevent_get_enabled(c->bev) & EV_READ)) {
        bufferevent_disable(c->bev, EV_READ);
    }
}

/**
 * @brief resumes a connection (enables reading) and activates resume event.
 *
 * @param c
 */
void
evhttpx_connection_resume(evhttpx_connection_t * c)
{
    if (!(bufferevent_get_enabled(c->bev) & EV_READ)) {
        bufferevent_enable(c->bev, EV_READ);
        event_active(c->resume_ev, EV_WRITE, 1);
    }
}

/**
 * @brief Wrapper around evhttpx_connection_pause
 *
 * @see evhttpx_connection_pause
 *
 * @param request
 */
void
evhttpx_request_pause(evhttpx_request_t * request)
{
    request->status = EVHTTPX_RES_PAUSE;
    evhttpx_connection_pause(request->conn);
}

/**
 * @brief Wrapper around evhttpx_connection_resume
 *
 * @see evhttpx_connection_resume
 *
 * @param request
 */
void
evhttpx_request_resume(evhttpx_request_t * request)
{
    evhttpx_connection_resume(request->conn);
}

evhttpx_header_t *
evhttpx_header_key_add(evhttpx_headers_t * headers, const char * key, char kalloc)
{
    evhttpx_header_t * header;

    if (!(header = evhttpx_header_new(key, NULL, kalloc, 0))) {
        return NULL;
    }

    evhttpx_headers_add_header(headers, header);

    return header;
}

evhttpx_header_t *
evhttpx_header_val_add(evhttpx_headers_t * headers, const char * val, char valloc)
{
    evhttpx_header_t * header = TAILQ_LAST(headers, evhttpx_headers_s);

    if (header == NULL) {
        return NULL;
    }

    header->vlen = strlen(val);

    if (valloc == 1) {
        header->val = malloc(header->vlen + 1);
        header->val[header->vlen] = '\0';
        memcpy(header->val, val, header->vlen);
    } else {
        header->val = (char *)val;
    }

    header->v_heaped = valloc;

    return header;
}

evhttpx_kvs_t *
evhttpx_kvs_new(void)
{
    evhttpx_kvs_t * kvs = malloc(sizeof(evhttpx_kvs_t));

    TAILQ_INIT(kvs);
    return kvs;
}

evhttpx_kv_t *
evhttpx_kvlen_new(const char * key, size_t key_len,
        const char * val, size_t val_len,
        char kalloc, char valloc)
{
    evhttpx_kv_t * kv;

    if (!(kv = malloc(sizeof(evhttpx_kv_t)))) {
        return NULL;
    }

    kv->k_heaped = kalloc;
    kv->v_heaped = valloc;
    kv->klen     = key_len;
    kv->vlen     = val_len;

    if (key != NULL) {

        if (kalloc == 1) {
            char * s = malloc(kv->klen + 1);

            s[kv->klen] = '\0';
            memcpy(s, key, kv->klen);
            kv->key     = s;
        } else {
            kv->key = (char *)key;
        }
    }

    if (val != NULL) {

        if (valloc == 1) {
            char * s = malloc(kv->vlen + 1);

            s[kv->vlen] = '\0';
            memcpy(s, val, kv->vlen);
            kv->val     = s;
        } else {
            kv->val = (char *)val;
        }
    }

    return kv;
}     /* evhttpx_kvlen_new */

evhttpx_kv_t *
evhttpx_kv_new(const char * key, const char * val, char kalloc, char valloc)
{
    evhttpx_kv_t * kv;

    if (!(kv = malloc(sizeof(evhttpx_kv_t)))) {
        return NULL;
    }

    kv->k_heaped = kalloc;
    kv->v_heaped = valloc;
    kv->klen     = 0;
    kv->vlen     = 0;

    if (key != NULL) {
        kv->klen = strlen(key);

        if (kalloc == 1) {
            char * s = malloc(kv->klen + 1);

            s[kv->klen] = '\0';
            memcpy(s, key, kv->klen);
            kv->key     = s;
        } else {
            kv->key = (char *)key;
        }
    }

    if (val != NULL) {
        kv->vlen = strlen(val);

        if (valloc == 1) {
            char * s = malloc(kv->vlen + 1);

            s[kv->vlen] = '\0';
            memcpy(s, val, kv->vlen);
            kv->val     = s;
        } else {
            kv->val = (char *)val;
        }
    }

    return kv;
}     /* evhttpx_kv_new */

void
evhttpx_kv_free(evhttpx_kv_t * kv)
{
    if (kv == NULL) {
        return;
    }

    if (kv->k_heaped) {
        free(kv->key);
    }

    if (kv->v_heaped) {
        free(kv->val);
    }

    free(kv);
}

void
evhttpx_kv_rm_and_free(evhttpx_kvs_t * kvs, evhttpx_kv_t * kv)
{
    if (kvs == NULL || kv == NULL) {
        return;
    }

    TAILQ_REMOVE(kvs, kv, next);

    evhttpx_kv_free(kv);
}

void
evhttpx_kvs_free(evhttpx_kvs_t * kvs)
{
    evhttpx_kv_t * kv;
    evhttpx_kv_t * save;

    if (kvs == NULL) {
        return;
    }

    for (kv = TAILQ_FIRST(kvs); kv != NULL; kv = save) {
        save = TAILQ_NEXT(kv, next);

        TAILQ_REMOVE(kvs, kv, next);

        evhttpx_kv_free(kv);
    }

    free(kvs);
}

int
evhttpx_kvs_for_each(evhttpx_kvs_t * kvs, evhttpx_kvs_iterator cb, void * arg)
{
    evhttpx_kv_t * kv;

    if (kvs == NULL || cb == NULL) {
        return -1;
    }

    TAILQ_FOREACH(kv, kvs, next) {
        int res;

        if ((res = cb(kv, arg))) {
            return res;
        }
    }

    return 0;
}

const char *
evhttpx_kv_find(evhttpx_kvs_t * kvs, const char * key)
{
    evhttpx_kv_t * kv;

    if (kvs == NULL || key == NULL) {
        return NULL;
    }

    TAILQ_FOREACH(kv, kvs, next) {
        if (strcasecmp(kv->key, key) == 0) {
            return kv->val;
        }
    }

    return NULL;
}

evhttpx_kv_t *
evhttpx_kvs_find_kv(evhttpx_kvs_t * kvs, const char * key)
{
    evhttpx_kv_t * kv;

    if (kvs == NULL || key == NULL) {
        return NULL;
    }

    TAILQ_FOREACH(kv, kvs, next) {
        if (strcasecmp(kv->key, key) == 0) {
            return kv;
        }
    }

    return NULL;
}

void
evhttpx_kvs_add_kv(evhttpx_kvs_t * kvs, evhttpx_kv_t * kv)
{
    if (kvs == NULL || kv == NULL) {
        return;
    }

    TAILQ_INSERT_TAIL(kvs, kv, next);
}

typedef enum {
    s_query_start = 0,
    s_query_question_mark,
    s_query_separator,
    s_query_key,
    s_query_val,
    s_query_key_hex_1,
    s_query_key_hex_2,
    s_query_val_hex_1,
    s_query_val_hex_2,
    s_query_done
} query_parser_state;

static inline int
evhttpx_is_hex_query_char(unsigned char ch)
{
    switch (ch) {
        case 'a': case 'A':
        case 'b': case 'B':
        case 'c': case 'C':
        case 'd': case 'D':
        case 'e': case 'E':
        case 'f': case 'F':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return 1;
        default:
            return 0;
    } /* switch */
}

enum unscape_state {
    unscape_state_start = 0,
    unscape_state_hex1,
    unscape_state_hex2
};

int
evhttpx_unescape_string(unsigned char ** out,
        unsigned char * str,
        size_t str_len)
{
    unsigned char    * optr;
    unsigned char    * sptr;
    unsigned char      d;
    unsigned char      ch;
    unsigned char      c;
    size_t             i;
    enum unscape_state state;

    if (out == NULL || *out == NULL) {
        return -1;
    }

    state = unscape_state_start;
    optr  = *out;
    sptr  = str;
    d     = 0;

    for (i = 0; i < str_len; i++) {
        ch = *sptr++;

        switch (state) {
            case unscape_state_start:
                if (ch == '%') {
                    state = unscape_state_hex1;
                    break;
                }

                *optr++ = ch;

                break;
            case unscape_state_hex1:
                if (ch >= '0' && ch <= '9') {
                    d     = (unsigned char)(ch - '0');
                    state = unscape_state_hex2;
                    break;
                }

                c = (unsigned char)(ch | 0x20);

                if (c >= 'a' && c <= 'f') {
                    d     = (unsigned char)(c - 'a' + 10);
                    state = unscape_state_hex2;
                    break;
                }

                state   = unscape_state_start;
                *optr++ = ch;
                break;
            case unscape_state_hex2:
                state   = unscape_state_start;

                if (ch >= '0' && ch <= '9') {
                    ch      = (unsigned char)((d << 4) + ch - '0');

                    *optr++ = ch;
                    break;
                }

                c = (unsigned char)(ch | 0x20);

                if (c >= 'a' && c <= 'f') {
                    ch      = (unsigned char)((d << 4) + c - 'a' + 10);
                    *optr++ = ch;
                    break;
                }

                break;
        } /* switch */
    }

    return 0;
}         /* evhttpx_unescape_string */

evhttpx_query_t *
evhttpx_parse_query(const char * query, size_t len)
{
    evhttpx_query_t    * query_args;
    query_parser_state state   = s_query_start;
    char             * key_buf = NULL;
    char             * val_buf = NULL;
    int                key_idx;
    int                val_idx;
    unsigned char      ch;
    size_t             i;

    query_args = evhttpx_query_new();

    if (!(key_buf = malloc(len + 1))) {
        return NULL;
    }

    if (!(val_buf = malloc(len + 1))) {
        free(key_buf);
        return NULL;
    }

    key_idx = 0;
    val_idx = 0;

    for (i = 0; i < len; i++) {
        ch  = query[i];

        if (key_idx >= len || val_idx >= len) {
            goto error;
        }

        switch (state) {
            case s_query_start:
                memset(key_buf, 0, len);
                memset(val_buf, 0, len);

                key_idx = 0;
                val_idx = 0;

                switch (ch) {
                    case '?':
                        state = s_query_key;
                        break;
                    case '/':
                        state = s_query_question_mark;
                        break;
                    default:
                        state = s_query_key;
                        goto query_key;
                }

                break;
            case s_query_question_mark:
                switch (ch) {
                    case '?':
                        state = s_query_key;
                        break;
                    case '/':
                        state = s_query_question_mark;
                        break;
                    default:
                        goto error;
                }
                break;
query_key:
            case s_query_key:
                switch (ch) {
                    case '=':
                        state = s_query_val;
                        break;
                    case '%':
                        key_buf[key_idx++] = ch;
                        key_buf[key_idx] = '\0';
                        state = s_query_key_hex_1;
                        break;
                    default:
                        key_buf[key_idx++] = ch;
                        key_buf[key_idx]   = '\0';
                        break;
                }
                break;
            case s_query_key_hex_1:
                if (!evhttpx_is_hex_query_char(ch)) {
                    /* not hex, so we treat as a normal key */
                    if ((key_idx + 2) >= len) {
                        /* we need to insert \%<ch>, but not enough space */
                        goto error;
                    }

                    key_buf[key_idx - 1] = '%';
                    key_buf[key_idx++]   = ch;
                    key_buf[key_idx]     = '\0';
                    state = s_query_key;
                    break;
                }

                key_buf[key_idx++] = ch;
                key_buf[key_idx]   = '\0';

                state = s_query_key_hex_2;
                break;
            case s_query_key_hex_2:
                if (!evhttpx_is_hex_query_char(ch)) {
                    goto error;
                }

                key_buf[key_idx++] = ch;
                key_buf[key_idx]   = '\0';

                state = s_query_key;
                break;
            case s_query_val:
                switch (ch) {
                    case ';':
                    case '&':
                        evhttpx_kvs_add_kv(query_args,
                                evhttpx_kv_new(key_buf, val_buf, 1, 1));

                        memset(key_buf, 0, len);
                        memset(val_buf, 0, len);

                        key_idx            = 0;
                        val_idx            = 0;

                        state              = s_query_key;

                        break;
                    case '%':
                        val_buf[val_idx++] = ch;
                        val_buf[val_idx]   = '\0';

                        state              = s_query_val_hex_1;
                        break;
                    default:
                        val_buf[val_idx++] = ch;
                        val_buf[val_idx]   = '\0';

                        break;
                }     /* switch */
                break;
            case s_query_val_hex_1:
                if (!evhttpx_is_hex_query_char(ch)) {
                    /* not really a hex val */
                    if ((val_idx + 2) >= len) {
                        /* we need to insert \%<ch>, but not enough space */
                        goto error;
                    }


                    val_buf[val_idx - 1] = '%';
                    val_buf[val_idx++]   = ch;
                    val_buf[val_idx]     = '\0';

                    state = s_query_val;
                    break;
                }

                val_buf[val_idx++] = ch;
                val_buf[val_idx]   = '\0';

                state = s_query_val_hex_2;
                break;
            case s_query_val_hex_2:
                if (!evhttpx_is_hex_query_char(ch)) {
                    goto error;
                }

                val_buf[val_idx++] = ch;
                val_buf[val_idx]   = '\0';

                state = s_query_val;
                break;
            default:
                /* bad state */
                goto error;
        }       /* switch */
    }

    if (key_idx && val_idx) {
        evhttpx_kvs_add_kv(query_args, evhttpx_kv_new(key_buf, val_buf, 1, 1));
    }

    free(key_buf);
    free(val_buf);

    return query_args;
error:
    free(key_buf);
    free(val_buf);

    return NULL;
}     /* evhttpx_parse_query */

void
evhttpx_send_reply_start(evhttpx_request_t * request, evhttpx_res code)
{
    evhttpx_connection_t * c;
    evbuf_t            * reply_buf;

    c = evhttpx_request_get_connection(request);

    if (!(reply_buf = _evhttpx_create_reply(request, code))) {
        evhttpx_connection_free(c);
        return;
    }

    bufferevent_write_buffer(c->bev, reply_buf);
    evbuffer_free(reply_buf);
}

void
evhttpx_send_reply_body(evhttpx_request_t * request, evbuf_t * buf)
{
    evhttpx_connection_t * c;

    c = request->conn;

    bufferevent_write_buffer(c->bev, buf);
}

void
evhttpx_send_reply_end(evhttpx_request_t * request)
{
    request->finished = 1;

    _evhttpx_connection_writecb(evhttpx_request_get_bev(request),
            evhttpx_request_get_connection(request));
}

void
evhttpx_send_reply(evhttpx_request_t * request,
        evhttpx_res code)
{
    evhttpx_connection_t * c;
    evbuf_t            * reply_buf;

    c = evhttpx_request_get_connection(request);
    request->finished = 1;

    if (!(reply_buf = _evhttpx_create_reply(request, code))) {
        evhttpx_connection_free(request->conn);
        return;
    }

    bufferevent_write_buffer(evhttpx_connection_get_bev(c), reply_buf);
    evbuffer_free(reply_buf);
}

int
evhttpx_response_needs_body(const evhttpx_res code,
        const http_method_e method)
{
    return code != EVHTTPX_RES_NOCONTENT &&
           code != EVHTTPX_RES_NOTMOD &&
           (code < 100 || code >= 200) &&
           method != http_method_HEAD;
}

void
evhttpx_send_reply_chunk_start(evhttpx_request_t * request,
        evhttpx_res code)
{
    evhttpx_header_t * content_len;

    if (evhttpx_response_needs_body(code, request->method)) {
        content_len = evhttpx_headers_find_header(request->headers_out,
                "Content-Length");

        switch (request->proto) {
            case evhttpx_PROTO_11:

                /*
                 * prefer HTTP/1.1 chunked encoding to closing the connection;
                 * note RFC 2616 section 4.4 forbids it with Content-Length:
                 * and it's not necessary then anyway.
                 */

                evhttpx_kv_rm_and_free(request->headers_out, content_len);
                request->chunked = 1;
                break;
            case evhttpx_PROTO_10:
                /*
                 * HTTP/1.0 can be chunked as long as the Content-Length header
                 * is set to 0
                 */
                evhttpx_kv_rm_and_free(request->headers_out, content_len);

                evhttpx_headers_add_header(request->headers_out,
                        evhttpx_header_new("Content-Length", "0", 0, 0));

                request->chunked = 1;
                break;
            default:
                request->chunked = 0;
                break;
        } /* switch */
    } else {
        request->chunked = 0;
    }

    if (request->chunked == 1) {
        evhttpx_headers_add_header(request->headers_out,
                evhttpx_header_new("Transfer-Encoding", "chunked", 0, 0));

        /*
         * if data already exists on the output buffer, we automagically convert
         * it to the first chunk.
         */
        if (evbuffer_get_length(request->buffer_out) > 0) {
            char lstr[128];
            int  sres;

            sres = snprintf(lstr, sizeof(lstr), "%x\r\n",
                            (unsigned)evbuffer_get_length(request->buffer_out));

            if (sres >= sizeof(lstr) || sres < 0) {
                /* overflow condition, shouldn't ever get here, but lets
                 * terminate the connection asap */
                goto end;
            }

            evbuffer_prepend(request->buffer_out, lstr, strlen(lstr));
            evbuffer_add(request->buffer_out, "\r\n", 2);
        }
    }

end:
    evhttpx_send_reply_start(request, code);
} /* evhttpx_send_reply_chunk_start */

void
evhttpx_send_reply_chunk(evhttpx_request_t * request, evbuf_t * buf)
{
    evbuf_t * output;

    output = bufferevent_get_output(request->conn->bev);

    if (evbuffer_get_length(buf) == 0) {
        return;
    }
    if (request->chunked) {
        evbuffer_add_printf(output, "%x\r\n",
                            (unsigned)evbuffer_get_length(buf));
    }
    evhttpx_send_reply_body(request, buf);
    if (request->chunked) {
        evbuffer_add(output, "\r\n", 2);
    }
    bufferevent_flush(request->conn->bev, EV_WRITE, BEV_FLUSH);
}

void
evhttpx_send_reply_chunk_end(evhttpx_request_t * request)
{
    if (request->chunked) {
        evbuffer_add(bufferevent_get_output(evhttpx_request_get_bev(request)),
                     "0\r\n\r\n", 5);
    }

    evhttpx_send_reply_end(request);
}

void
evhttpx_unbind_socket(evhttpx_t * httpx)
{
    evconnlistener_free(httpx->server);
    httpx->server = NULL;
}

int
evhttpx_bind_sockaddr(evhttpx_t * httpx,
        struct sockaddr * sa,
        size_t sin_len,
        int backlog)
{
    signal(SIGPIPE, SIG_IGN);

    httpx->server = evconnlistener_new_bind(httpx->evbase,
            _evhttpx_accept_cb, (void *)httpx,
            LEV_OPT_THREADSAFE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
            backlog, sa, sin_len);

#ifdef USE_DEFER_ACCEPT
    {
        evutil_socket_t sock;
        int             one = 1;

        sock = evconnlistener_get_fd(httpx->server);

        setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT,
                &one, (ev_socklen_t)sizeof(one));
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                &one, (ev_socklen_t)sizeof(one));
    }
#endif

#ifndef EVHTTPX_DISABLE_SSL
    if (httpx->ssl_ctx != NULL) {
        /* if ssl is enabled and we have virtual hosts, set our servername
         * callback. We do this here because we want to make sure that this gets
         * set after all potential virtualhosts have been set, not just after
         * ssl_init.
         */
        if (TAILQ_FIRST(&httpx->vhosts) != NULL) {
            SSL_CTX_set_tlsext_servername_callback(httpx->ssl_ctx,
                    _evhttpx_ssl_servername);
        }
    }
#endif

    return httpx->server ? 0 : -1;
}

int
evhttpx_bind_socket(evhttpx_t * httpx,
        const char * baddr,
        uint16_t port,
        int backlog)
{
    struct sockaddr_in  sin;
    struct sockaddr_in6 sin6;

#ifndef NO_SYS_UN
    struct sockaddr_un sun;
#endif
    struct sockaddr  * sa;
    size_t             sin_len;

    memset(&sin, 0, sizeof(sin));

    if (!strncmp(baddr, "ipv6:", 5)) {
        memset(&sin6, 0, sizeof(sin6));

        baddr           += 5;
        sin_len          = sizeof(struct sockaddr_in6);
        sin6.sin6_port   = htons(port);
        sin6.sin6_family = AF_INET6;

        evutil_inet_pton(AF_INET6, baddr, &sin6.sin6_addr);
        sa = (struct sockaddr *)&sin6;
    } else if (!strncmp(baddr, "unix:", 5)) {
#ifndef NO_SYS_UN
        baddr += 5;

        if (strlen(baddr) >= sizeof(sun.sun_path)) {
            return -1;
        }

        memset(&sun, 0, sizeof(sun));

        sin_len        = sizeof(struct sockaddr_un);
        sun.sun_family = AF_UNIX;

        strncpy(sun.sun_path, baddr, strlen(baddr));

        sa = (struct sockaddr *)&sun;
#else
        fprintf(stderr, "System does not support AF_UNIX sockets\n");
        return -1;
#endif
    } else {
        if (!strncmp(baddr, "ipv4:", 5)) {
            baddr += 5;
        }

        sin_len             = sizeof(struct sockaddr_in);

        sin.sin_family      = AF_INET;
        sin.sin_port        = htons(port);
        sin.sin_addr.s_addr = inet_addr(baddr);

        sa = (struct sockaddr *)&sin;
    }

    return evhttpx_bind_sockaddr(httpx, sa, sin_len, backlog);
} /* evhttpx_bind_socket */

void
evhttpx_callbacks_free(evhttpx_callbacks_t * callbacks)
{
    /* XXX TODO */
}

evhttpx_callback_t *
evhttpx_callback_new(const char * path,
        evhttpx_callback_type type,
        evhttpx_callback_cb cb,
        void * arg)
{
    evhttpx_callback_t * hcb;

    if (!(hcb = calloc(sizeof(evhttpx_callback_t), 1))) {
        return NULL;
    }

    hcb->type  = type;
    hcb->cb    = cb;
    hcb->cbarg = arg;

    switch (type) {
        case evhttpx_callback_type_hash:
            hcb->hash      = _evhttpx_quick_hash(path);
            hcb->val.path  = strdup(path);
            break;
        case evhttpx_callback_type_glob:
            hcb->val.glob = strdup(path);
            break;
        default:
            free(hcb);
            return NULL;
    } /* switch */

    return hcb;
}

void
evhttpx_callback_free(evhttpx_callback_t * callback)
{
    if (callback == NULL) {
        return;
    }

    switch (callback->type) {
        case evhttpx_callback_type_hash:
            free(callback->val.path);
            break;
        case evhttpx_callback_type_glob:
            free(callback->val.glob);
            break;
    }

    if (callback->hooks) {
        free(callback->hooks);
    }

    free(callback);

    return;
}

int
evhttpx_callbacks_add_callback(evhttpx_callbacks_t * cbs,
        evhttpx_callback_t * cb)
{
    TAILQ_INSERT_TAIL(cbs, cb, next);

    return 0;
}

int
evhttpx_set_hook(evhttpx_hooks_t ** hooks,
        evhttpx_hook_type_e type,
        evhttpx_hook cb,
        void * arg)
{
    if (*hooks == NULL) {
        if (!(*hooks = calloc(sizeof(evhttpx_hooks_t), 1))) {
            return -1;
        }
    }

    switch (type) {
        case evhttpx_hook_on_headers_start:
            (*hooks)->on_headers_start       = (evhttpx_hook_headers_start_cb)cb;
            (*hooks)->on_headers_start_arg   = arg;
            break;
        case evhttpx_hook_on_header:
            (*hooks)->on_header = (evhttpx_hook_header_cb)cb;
            (*hooks)->on_header_arg          = arg;
            break;
        case evhttpx_hook_on_headers:
            (*hooks)->on_headers             = (evhttpx_hook_headers_cb)cb;
            (*hooks)->on_headers_arg         = arg;
            break;
        case evhttpx_hook_on_path:
            (*hooks)->on_path = (evhttpx_hook_path_cb)cb;
            (*hooks)->on_path_arg            = arg;
            break;
        case evhttpx_hook_on_read:
            (*hooks)->on_read = (evhttpx_hook_read_cb)cb;
            (*hooks)->on_read_arg            = arg;
            break;
        case evhttpx_hook_on_request_fini:
            (*hooks)->on_request_fini        = (evhttpx_hook_request_fini_cb)cb;
            (*hooks)->on_request_fini_arg    = arg;
            break;
        case evhttpx_hook_on_connection_fini:
            (*hooks)->on_connection_fini     = (evhttpx_hook_connection_fini_cb)cb;
            (*hooks)->on_connection_fini_arg = arg;
            break;
        case evhttpx_hook_on_error:
            (*hooks)->on_error = (evhttpx_hook_err_cb)cb;
            (*hooks)->on_error_arg           = arg;
            break;
        case evhttpx_hook_on_new_chunk:
            (*hooks)->on_new_chunk           = (evhttpx_hook_chunk_new_cb)cb;
            (*hooks)->on_new_chunk_arg       = arg;
            break;
        case evhttpx_hook_on_chunk_complete:
            (*hooks)->on_chunk_fini          = (evhttpx_hook_chunk_fini_cb)cb;
            (*hooks)->on_chunk_fini_arg      = arg;
            break;
        case evhttpx_hook_on_chunks_complete:
            (*hooks)->on_chunks_fini         = (evhttpx_hook_chunks_fini_cb)cb;
            (*hooks)->on_chunks_fini_arg     = arg;
            break;
        case evhttpx_hook_on_hostname:
            (*hooks)->on_hostname            = (evhttpx_hook_hostname_cb)cb;
            (*hooks)->on_hostname_arg        = arg;
            break;
        case evhttpx_hook_on_write:
            (*hooks)->on_write = (evhttpx_hook_write_cb)cb;
            (*hooks)->on_write_arg           = arg;
            break;
        default:
            return -1;
    }     /* switch */

    return 0;
}         /* evhttpx_set_hook */

int
evhttpx_unset_hook(evhttpx_hooks_t ** hooks, evhttpx_hook_type_e type)
{
    return evhttpx_set_hook(hooks, type, NULL, NULL);
}

int
evhttpx_unset_all_hooks(evhttpx_hooks_t ** hooks)
{
    int res = 0;

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_headers_start)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_header)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_headers)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_path)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_read)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_request_fini)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_connection_fini)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_error)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_new_chunk)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_chunk_complete)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_chunks_complete)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_hostname)) {
        res -= 1;
    }

    if (evhttpx_unset_hook(hooks, evhttpx_hook_on_write)) {
        return -1;
    }

    return res;
} /* evhttpx_unset_all_hooks */

evhttpx_callback_t *
evhttpx_set_cb(evhttpx_t * httpx,
        const char * path,
        evhttpx_callback_cb cb,
        void * arg)
{
    evhttpx_callback_t * hcb;

    _evhttpx_lock(httpx);

    if (httpx->callbacks == NULL) {
        if (!(httpx->callbacks = calloc(sizeof(evhttpx_callbacks_t),
                        sizeof(char)))) {
            _evhttpx_unlock(httpx);
            return NULL;
        }

        TAILQ_INIT(httpx->callbacks);
    }

    if (!(hcb = evhttpx_callback_new(path,
                    evhttpx_callback_type_hash, cb, arg))) {
        _evhttpx_unlock(httpx);
        return NULL;
    }

    if (evhttpx_callbacks_add_callback(httpx->callbacks, hcb)) {
        evhttpx_callback_free(hcb);
        _evhttpx_unlock(httpx);
        return NULL;
    }

    _evhttpx_unlock(httpx);
    return hcb;
}

#ifndef EVHTTPX_DISABLE_EVTHR
static void
_evhttpx_thread_init(evthr_t * thr, void * arg)
{
    evhttpx_t * httpx = (evhttpx_t *)arg;

    if (httpx->thread_init_cb) {
        httpx->thread_init_cb(httpx, thr, httpx->thread_init_cbarg);
    }
}

int
evhttpx_use_threads(evhttpx_t * httpx,
        evhttpx_thread_init_cb init_cb,
        int nthreads,
        void * arg)
{
    httpx->thread_init_cb    = init_cb;
    httpx->thread_init_cbarg = arg;

#ifndef EVHTTPX_DISABLE_SSL
    evhttpx_ssl_use_threads();
#endif

    if (!(httpx->thr_pool = evthr_pool_new(nthreads,
                    _evhttpx_thread_init, httpx))) {
        return -1;
    }

    evthr_pool_start(httpx->thr_pool);
    return 0;
}

#endif

#ifndef EVHTTPX_DISABLE_EVTHR
int
evhttpx_use_callback_locks(evhttpx_t * httpx)
{
    if (httpx == NULL) {
        return -1;
    }

    if (!(httpx->lock = malloc(sizeof(pthread_mutex_t)))) {
        return -1;
    }

    return pthread_mutex_init(httpx->lock, NULL);
}

#endif

evhttpx_callback_t *
evhttpx_set_glob_cb(evhttpx_t * httpx,
        const char * pattern,
        evhttpx_callback_cb cb,
        void * arg)
{
    evhttpx_callback_t * hcb;

    _evhttpx_lock(httpx);

    if (httpx->callbacks == NULL) {
        if (!(httpx->callbacks = calloc(sizeof(evhttpx_callbacks_t),
                        sizeof(char)))) {
            _evhttpx_unlock(httpx);
            return NULL;
        }

        TAILQ_INIT(httpx->callbacks);
    }

    if (!(hcb = evhttpx_callback_new(pattern,
                    evhttpx_callback_type_glob, cb, arg))) {
        _evhttpx_unlock(httpx);
        return NULL;
    }

    if (evhttpx_callbacks_add_callback(httpx->callbacks, hcb)) {
        evhttpx_callback_free(hcb);
        _evhttpx_unlock(httpx);
        return NULL;
    }

    _evhttpx_unlock(httpx);
    return hcb;
}

void
evhttpx_set_gencb(evhttpx_t * httpx,
        evhttpx_callback_cb cb,
        void * arg)
{
    httpx->defaults.cb    = cb;
    httpx->defaults.cbarg = arg;
}

void
evhttpx_set_pre_accept_cb(evhttpx_t * httpx,
        evhttpx_pre_accept_cb cb,
        void * arg)
{
    httpx->defaults.pre_accept       = cb;
    httpx->defaults.pre_accept_cbarg = arg;
}

void
evhttpx_set_post_accept_cb(evhttpx_t * httpx,
        evhttpx_post_accept_cb cb,
        void * arg)
{
    httpx->defaults.post_accept       = cb;
    httpx->defaults.post_accept_cbarg = arg;
}

#ifndef EVHTTPX_DISABLE_SSL
#ifndef EVHTTPX_DISABLE_EVTHR
int
evhttpx_ssl_use_threads(void)
{
    int i;

    if (ssl_locks_initialized == 1) {
        return 0;
    }

    ssl_locks_initialized = 1;

    ssl_num_locks         = CRYPTO_num_locks();
    ssl_locks = malloc(ssl_num_locks * sizeof(evhttpx_mutex_t));

    for (i = 0; i < ssl_num_locks; i++) {
        pthread_mutex_init(&(ssl_locks[i]), NULL);
    }

    CRYPTO_set_id_callback(_evhttpx_ssl_get_thread_id);
    CRYPTO_set_locking_callback(_evhttpx_ssl_thread_lock);

    return 0;
}

#endif

int
evhttpx_ssl_init(evhttpx_t * httpx, evhttpx_ssl_cfg_t * cfg)
{
    long                  cache_mode;

    if (cfg == NULL || httpx == NULL || cfg->pemfile == NULL) {
        return -1;
    }

    SSL_library_init();
    SSL_load_error_strings();
    RAND_poll();

    STACK_OF(SSL_COMP) * comp_methods = SSL_COMP_get_compression_methods();
    sk_SSL_COMP_zero(comp_methods);

    httpx->ssl_cfg = cfg;
    httpx->ssl_ctx = SSL_CTX_new(SSLv23_server_method());

#if OPENSSL_VERSION_NUMBER >= 0x10000000L
    SSL_CTX_set_options(httpx->ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
    /* SSL_CTX_set_options(httpx->ssl_ctx, SSL_MODE_AUTO_RETRY); */
    SSL_CTX_set_timeout(httpx->ssl_ctx, cfg->ssl_ctx_timeout);
#endif

    SSL_CTX_set_options(httpx->ssl_ctx, cfg->ssl_opts);

#ifndef OPENSSL_NO_EC
    if (cfg->named_curve != NULL) {
        EC_KEY * ecdh = NULL;
        int      nid  = 0;

        nid  = OBJ_sn2nid(cfg->named_curve);
        if (nid == 0) {
            fprintf(stderr, "ECDH initialization failed: unknown curve %s\n",
                    cfg->named_curve);
        }
        ecdh = EC_KEY_new_by_curve_name(nid);
        if (ecdh == NULL) {
            fprintf(stderr, "ECDH initialization failed for curve %s\n",
                    cfg->named_curve);
        }
        SSL_CTX_set_tmp_ecdh(httpx->ssl_ctx, ecdh);
        EC_KEY_free(ecdh);
    }
#endif /* OPENSSL_NO_EC */

    if (cfg->ciphers != NULL) {
        SSL_CTX_set_cipher_list(httpx->ssl_ctx, cfg->ciphers);
    }

    SSL_CTX_load_verify_locations(httpx->ssl_ctx, cfg->cafile, cfg->capath);
    X509_STORE_set_flags(SSL_CTX_get_cert_store(httpx->ssl_ctx),
            cfg->store_flags);
    SSL_CTX_set_verify(httpx->ssl_ctx, cfg->verify_peer, cfg->x509_verify_cb);

    if (cfg->x509_chk_issued_cb != NULL) {
        httpx->ssl_ctx->cert_store->check_issued = cfg->x509_chk_issued_cb;
    }

    if (cfg->verify_depth) {
        SSL_CTX_set_verify_depth(httpx->ssl_ctx, cfg->verify_depth);
    }

    switch (cfg->scache_type) {
        case evhttpx_ssl_scache_type_disabled:
            cache_mode = SSL_SESS_CACHE_OFF;
            break;
        case evhttpx_ssl_scache_type_user:
            cache_mode = SSL_SESS_CACHE_SERVER |
                         SSL_SESS_CACHE_NO_INTERNAL |
                         SSL_SESS_CACHE_NO_INTERNAL_LOOKUP;
            break;
        case evhttpx_ssl_scache_type_builtin:
            cache_mode = SSL_SESS_CACHE_SERVER |
                         SSL_SESS_CACHE_NO_INTERNAL |
                         SSL_SESS_CACHE_NO_INTERNAL_LOOKUP;
            break;
        case evhttpx_ssl_scache_type_internal:
        default:
            cache_mode = SSL_SESS_CACHE_SERVER;
            break;
    }     /* switch */

    SSL_CTX_use_certificate_file(httpx->ssl_ctx, cfg->pemfile,
            SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(httpx->ssl_ctx,
            cfg->privfile ? cfg->privfile : cfg->pemfile,
            SSL_FILETYPE_PEM);

    SSL_CTX_set_session_id_context(httpx->ssl_ctx,
            (void *)&session_id_context,
            sizeof(session_id_context));

    SSL_CTX_set_app_data(httpx->ssl_ctx, httpx);
    SSL_CTX_set_session_cache_mode(httpx->ssl_ctx, cache_mode);

    if (cache_mode != SSL_SESS_CACHE_OFF) {
        SSL_CTX_sess_set_cache_size(httpx->ssl_ctx,
                cfg->scache_size ? cfg->scache_size : 1024);

        if (cfg->scache_type == evhttpx_ssl_scache_type_builtin ||
            cfg->scache_type == evhttpx_ssl_scache_type_user) {
            SSL_CTX_sess_set_new_cb(httpx->ssl_ctx, _evhttpx_ssl_add_scache_ent);
            SSL_CTX_sess_set_get_cb(httpx->ssl_ctx, _evhttpx_ssl_get_scache_ent);
            SSL_CTX_sess_set_remove_cb(httpx->ssl_ctx,
                    _evhttpx_ssl_delete_scache_ent);

            if (cfg->scache_init) {
                cfg->args = (cfg->scache_init)(httpx);
            }
        }
    }

    return 0;
}     /* evhttpx_use_ssl */

#endif

evbev_t *
evhttpx_connection_get_bev(evhttpx_connection_t * connection)
{
    return connection->bev;
}

evbev_t *
evhttpx_connection_take_ownership(evhttpx_connection_t * connection)
{
    evbev_t * bev = evhttpx_connection_get_bev(connection);

    if (connection->hooks) {
        evhttpx_unset_all_hooks(&connection->hooks);
    }

    if (connection->request && connection->request->hooks) {
        evhttpx_unset_all_hooks(&connection->request->hooks);
    }

    evhttpx_connection_set_bev(connection, NULL);

    connection->owner = 0;

    bufferevent_disable(bev, EV_READ);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

    return bev;
}

evbev_t *
evhttpx_request_get_bev(evhttpx_request_t * request)
{
    return evhttpx_connection_get_bev(request->conn);
}

evbev_t *
evhttpx_request_take_ownership(evhttpx_request_t * request)
{
    return evhttpx_connection_take_ownership(
            evhttpx_request_get_connection(request));
}

void
evhttpx_connection_set_bev(evhttpx_connection_t * conn, evbev_t * bev)
{
    conn->bev = bev;
}

void
evhttpx_request_set_bev(evhttpx_request_t * request, evbev_t * bev)
{
    evhttpx_connection_set_bev(request->conn, bev);
}

evhttpx_connection_t *
evhttpx_request_get_connection(evhttpx_request_t * request)
{
    return request->conn;
}

void
evhttpx_connection_set_timeouts(evhttpx_connection_t   * c,
                              const struct timeval * rtimeo,
                              const struct timeval * wtimeo)
{
    if (!c) {
        return;
    }

    bufferevent_set_timeouts(c->bev, rtimeo, wtimeo);
}

void
evhttpx_connection_set_max_body_size(evhttpx_connection_t * c, uint64_t len)
{
    if (len == 0) {
        c->max_body_size = c->httpx->max_body_size;
    } else {
        c->max_body_size = len;
    }
}

void
evhttpx_request_set_max_body_size(evhttpx_request_t * req, uint64_t len)
{
    evhttpx_connection_set_max_body_size(req->conn, len);
}

void
evhttpx_connection_free(evhttpx_connection_t * connection)
{
    if (connection == NULL) {
        return;
    }

    _evhttpx_request_free(connection->request);
    _evhttpx_connection_fini_hook(connection);

    free(connection->parser);
    free(connection->hooks);
    free(connection->saddr);

    if (connection->resume_ev) {
        event_free(connection->resume_ev);
    }

    if (connection->bev) {
#ifdef LIBEVENT_HAS_SHUTDOWN
        bufferevent_shutdown(connection->bev, _evhttpx_shutdown_eventcb);
#else
#ifndef EVHTTPX_DISABLE_SSL
        if (connection->ssl != NULL) {
            SSL_set_shutdown(connection->ssl, SSL_RECEIVED_SHUTDOWN);
            SSL_shutdown(connection->ssl);
        }
#endif
        bufferevent_free(connection->bev);
#endif
    }

#ifndef EVHTTPX_DISABLE_EVTHR
    if (connection->thread) {
        evthr_dec_backlog(connection->thread);
    }
#endif

    free(connection);
}     /* evhttpx_connection_free */

void
evhttpx_request_free(evhttpx_request_t * request)
{
    _evhttpx_request_free(request);
}

void
evhttpx_set_timeouts(evhttpx_t * httpx,
        const struct timeval * r_timeo,
        const struct timeval * w_timeo)
{
    if (r_timeo != NULL) {
        httpx->recv_timeo = *r_timeo;
    }

    if (w_timeo != NULL) {
        httpx->send_timeo = *w_timeo;
    }
}

void
evhttpx_set_max_keepalive_requests(evhttpx_t * httpx, uint64_t num)
{
    httpx->max_keepalive_requests = num;
}

/**
 * @brief set bufferevent flags, defaults to BEV_OPT_CLOSE_ON_FREE
 *
 * @param httpx
 * @param flags
 */
void
evhttpx_set_bev_flags(evhttpx_t * httpx, int flags)
{
    httpx->bev_flags = flags;
}

void
evhttpx_set_max_body_size(evhttpx_t * httpx, uint64_t len)
{
    httpx->max_body_size = len;
}

int
evhttpx_add_alias(evhttpx_t * evhttpx, const char * name)
{
    evhttpx_alias_t * alias;

    if (evhttpx == NULL || name == NULL) {
        return -1;
    }

    if (!(alias = calloc(sizeof(evhttpx_alias_t), 1))) {
        return -1;
    }

    alias->alias = strdup(name);

    TAILQ_INSERT_TAIL(&evhttpx->aliases, alias, next);

    return 0;
}

/**
 * @brief add a virtual host.
 *
 * NOTE: If SSL is being used and the vhost was found via SNI, the Host: header
 *       will *NOT* be used to find a matching vhost.
 *
 *       Also, any hooks which are set prior to finding a vhost that are hooks
 *       which are after the host hook, they are overwritten by the callbacks
 *       and hooks set for the vhost specific evhttpx_t structure.
 *
 * @param evhttpx
 * @param name
 * @param vhost
 *
 * @return
 */
int
evhttpx_add_vhost(evhttpx_t * evhttpx, const char * name, evhttpx_t * vhost)
{
    if (evhttpx == NULL || name == NULL || vhost == NULL) {
        return -1;
    }

    if (TAILQ_FIRST(&vhost->vhosts) != NULL) {
        /* vhosts cannot have secondary vhosts defined */
        return -1;
    }

    if (!(vhost->server_name = strdup(name))) {
        return -1;
    }

    /* set the parent of this vhost so when the request has been completely
     * serviced, the vhost can be reset to the original evhttpx structure.
     *
     * This allows for a keep-alive connection to make multiple requests with
     * different Host: values.
     */
    vhost->parent                 = evhttpx;

    /* inherit various flags from the parent evhttpx structure */
    vhost->bev_flags              = evhttpx->bev_flags;
    vhost->max_body_size          = evhttpx->max_body_size;
    vhost->max_keepalive_requests = evhttpx->max_keepalive_requests;
    vhost->recv_timeo             = evhttpx->recv_timeo;
    vhost->send_timeo             = evhttpx->send_timeo;

    TAILQ_INSERT_TAIL(&evhttpx->vhosts, vhost, next_vhost);

    return 0;
}

evhttpx_t *
evhttpx_new(evbase_t * evbase, void * arg)
{
    evhttpx_t * httpx;

    if (evbase == NULL) {
        return NULL;
    }

    if (!(httpx = calloc(sizeof(evhttpx_t), 1))) {
        return NULL;
    }

    status_code_init();

    httpx->arg       = arg;
    httpx->evbase    = evbase;
    httpx->bev_flags = BEV_OPT_CLOSE_ON_FREE;

    TAILQ_INIT(&httpx->vhosts);
    TAILQ_INIT(&httpx->aliases);

    evhttpx_set_gencb(httpx, _evhttpx_default_request_cb, (void *)httpx);

    return httpx;
}

void
evhttpx_free(evhttpx_t * evhttpx)
{
    evhttpx_alias_t * evhttpx_alias, * tmp;

    if (evhttpx == NULL) {
        return;
    }

    if (evhttpx->thr_pool) {
        evthr_pool_stop(evhttpx->thr_pool);
        evthr_pool_free(evhttpx->thr_pool);
    }

    if (evhttpx->callbacks) {
        free(evhttpx->callbacks);
    }

    if (evhttpx->server_name) {
        free(evhttpx->server_name);
    }

    TAILQ_FOREACH_SAFE(evhttpx_alias, &evhttpx->aliases, next, tmp) {
        if (evhttpx_alias->alias != NULL) {
            free(evhttpx_alias->alias);
        }
        TAILQ_REMOVE(&evhttpx->aliases, evhttpx_alias, next);
        free(evhttpx_alias);
    }

    free(evhttpx);
}


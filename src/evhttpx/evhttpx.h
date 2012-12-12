#ifndef _EVHTTPX_H_
#define _EVHTTPX_H_

#ifndef EVHTTPX_DISABLE_EVTHR
#include "evthr.h"
#endif

#include "htparse.h"

#include <sys/queue.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#ifndef EVHTTPX_DISABLE_SSL
#include <event2/bufferevent_ssl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EVHTTPX_DISABLE_SSL
typedef SSL_SESSION               evhttpx_ssl_sess_t;
typedef SSL                       evhttpx_ssl_t;
typedef SSL_CTX                   evhttpx_ssl_ctx_t;
typedef X509                      evhttpx_x509_t;
typedef X509_STORE_CTX            evhttpx_x509_store_ctx_t;
#else
typedef void                      evhttpx_ssl_sess_t;
typedef void                      evhttpx_ssl_t;
typedef void                      evhttpx_ssl_ctx_t;
typedef void                      evhttpx_x509_t;
typedef void                      evhttpx_x509_store_ctx_t;
#endif

typedef struct evbuffer           evbuf_t;
typedef struct event              event_t;
typedef struct evconnlistener     evserv_t;
typedef struct bufferevent        evbev_t;
#ifdef EVHTTPX_DISABLE_EVTHR
typedef struct event_base         evbase_t;
typedef void                      evthr_t;
typedef void                      evthr_pool_t;
typedef void                      evhttpx_mutex_t;
#else
typedef pthread_mutex_t           evhttpx_mutex_t;
#endif

typedef struct evhttpx_s            evhttpx_t;
typedef struct evhttpx_defaults_s   evhttpx_defaults_t;
typedef struct evhttpx_callbacks_s  evhttpx_callbacks_t;
typedef struct evhttpx_callback_s   evhttpx_callback_t;
typedef struct evhttpx_defaults_s   evhttpx_defaults_5;
typedef struct evhttpx_kv_s         evhttpx_kv_t;
typedef struct evhttpx_kvs_s        evhttpx_kvs_t;
typedef struct evhttpx_uri_s        evhttpx_uri_t;
typedef struct evhttpx_path_s       evhttpx_path_t;
typedef struct evhttpx_authority_s  evhttpx_authority_t;
typedef struct evhttpx_request_s    evhttpx_request_t;
typedef struct evhttpx_hooks_s      evhttpx_hooks_t;
typedef struct evhttpx_connection_s evhttpx_connection_t;
typedef struct evhttpx_ssl_cfg_s    evhttpx_ssl_cfg_t;
typedef struct evhttpx_alias_s      evhttpx_alias_t;
typedef uint16_t                  evhttpx_res;
typedef uint8_t                   evhttpx_error_flags;


#define evhttpx_header_s  evhttpx_kv_s
#define evhttpx_headers_s evhttpx_kvs_s
#define evhttpx_query_s   evhttpx_kvs_s

#define evhttpx_header_t  evhttpx_kv_t
#define evhttpx_headers_t evhttpx_kvs_t
#define evhttpx_query_t   evhttpx_kvs_t

enum evhttpx_ssl_scache_type {
    evhttpx_ssl_scache_type_disabled = 0,
    evhttpx_ssl_scache_type_internal,
    evhttpx_ssl_scache_type_user,
    evhttpx_ssl_scache_type_builtin
};

/**
 * @brief types associated with where a developer can hook into
 *        during the request processing cycle.
 */
enum evhttpx_hook_type {
    evhttpx_hook_on_header,       /**< type which defines to hook after one header has been parsed */
    evhttpx_hook_on_headers,      /**< type which defines to hook after all headers have been parsed */
    evhttpx_hook_on_path,         /**< type which defines to hook once a path has been parsed */
    evhttpx_hook_on_read,         /**< type which defines to hook whenever the parser recieves data in a body */
    evhttpx_hook_on_request_fini, /**< type which defines to hook before the request is free'd */
    evhttpx_hook_on_connection_fini,
    evhttpx_hook_on_new_chunk,
    evhttpx_hook_on_chunk_complete,
    evhttpx_hook_on_chunks_complete,
    evhttpx_hook_on_headers_start,
    evhttpx_hook_on_error,        /**< type which defines to hook whenever an error occurs */
    evhttpx_hook_on_hostname,
    evhttpx_hook_on_write
};

enum evhttpx_callback_type {
    evhttpx_callback_type_hash,
    evhttpx_callback_type_glob
};

enum evhttpx_proto {
    evhttpx_PROTO_INVALID,
    evhttpx_PROTO_10,
    evhttpx_PROTO_11
};

typedef enum evhttpx_hook_type       evhttpx_hook_type;
typedef enum evhttpx_callback_type   evhttpx_callback_type;
typedef enum evhttpx_proto           evhttpx_proto;
typedef enum evhttpx_ssl_scache_type evhttpx_ssl_scache_type;

typedef void (*evhttpx_thread_init_cb)(evhttpx_t * htp, evthr_t * thr, void * arg);
typedef void (*evhttpx_callback_cb)(evhttpx_request_t * req, void * arg);
typedef void (*evhttpx_hook_err_cb)(evhttpx_request_t * req, evhttpx_error_flags errtype, void * arg);

/* Generic hook for passing ISO tests */
typedef evhttpx_res (*evhttpx_hook)();

typedef evhttpx_res (*evhttpx_pre_accept_cb)(evhttpx_connection_t * conn, void * arg);
typedef evhttpx_res (*evhttpx_post_accept_cb)(evhttpx_connection_t * conn, void * arg);
typedef evhttpx_res (*evhttpx_hook_header_cb)(evhttpx_request_t * req, evhttpx_header_t * hdr, void * arg);
typedef evhttpx_res (*evhttpx_hook_headers_cb)(evhttpx_request_t * req, evhttpx_headers_t * hdr, void * arg);
typedef evhttpx_res (*evhttpx_hook_path_cb)(evhttpx_request_t * req, evhttpx_path_t * path, void * arg);
typedef evhttpx_res (*evhttpx_hook_read_cb)(evhttpx_request_t * req, evbuf_t * buf, void * arg);
typedef evhttpx_res (*evhttpx_hook_request_fini_cb)(evhttpx_request_t * req, void * arg);
typedef evhttpx_res (*evhttpx_hook_connection_fini_cb)(evhttpx_connection_t * connection, void * arg);
typedef evhttpx_res (*evhttpx_hook_chunk_new_cb)(evhttpx_request_t * r, uint64_t len, void * arg);
typedef evhttpx_res (*evhttpx_hook_chunk_fini_cb)(evhttpx_request_t * r, void * arg);
typedef evhttpx_res (*evhttpx_hook_chunks_fini_cb)(evhttpx_request_t * r, void * arg);
typedef evhttpx_res (*evhttpx_hook_headers_start_cb)(evhttpx_request_t * r, void * arg);
typedef evhttpx_res (*evhttpx_hook_hostname_cb)(evhttpx_request_t * r, const char * hostname, void * arg);
typedef evhttpx_res (*evhttpx_hook_write_cb)(evhttpx_connection_t * conn, void * arg);

typedef int (*evhttpx_kvs_iterator)(evhttpx_kv_t * kv, void * arg);
typedef int (*evhttpx_headers_iterator)(evhttpx_header_t * header, void * arg);

typedef int (*evhttpx_ssl_verify_cb)(int pre_verify, evhttpx_x509_store_ctx_t * ctx);
typedef int (*evhttpx_ssl_chk_issued_cb)(evhttpx_x509_store_ctx_t * ctx, evhttpx_x509_t * x, evhttpx_x509_t * issuer);

typedef int (*evhttpx_ssl_scache_add)(evhttpx_connection_t * connection, unsigned char * sid, int sid_len, evhttpx_ssl_sess_t * sess);
typedef void (*evhttpx_ssl_scache_del)(evhttpx_t * htp, unsigned char * sid, int sid_len);
typedef evhttpx_ssl_sess_t * (*evhttpx_ssl_scache_get)(evhttpx_connection_t * connection, unsigned char * sid, int sid_len);
typedef void * (*evhttpx_ssl_scache_init)(evhttpx_t *);

#define EVHTTPX_VERSION           "1.1.7"
#define EVHTTPX_VERSION_MAJOR     1
#define EVHTTPX_VERSION_MINOR     1
#define EVHTTPX_VERSION_PATCH     7

#define evhttpx_headers_iterator  evhttpx_kvs_iterator

#define EVHTTPX_RES_ERROR         0
#define EVHTTPX_RES_PAUSE         1
#define EVHTTPX_RES_FATAL         2
#define EVHTTPX_RES_USER          3
#define EVHTTPX_RES_DATA_TOO_LONG 4
#define EVHTTPX_RES_OK            200

#define EVHTTPX_RES_100           100
#define EVHTTPX_RES_CONTINUE      100
#define EVHTTPX_RES_SWITCH_PROTO  101
#define EVHTTPX_RES_PROCESSING    102
#define EVHTTPX_RES_URI_TOOLONG   122

#define EVHTTPX_RES_200           200
#define EVHTTPX_RES_CREATED       201
#define EVHTTPX_RES_ACCEPTED      202
#define EVHTTPX_RES_NAUTHINFO     203
#define EVHTTPX_RES_NOCONTENT     204
#define EVHTTPX_RES_RSTCONTENT    205
#define EVHTTPX_RES_PARTIAL       206
#define EVHTTPX_RES_MSTATUS       207
#define EVHTTPX_RES_IMUSED        226

#define EVHTTPX_RES_300           300
#define EVHTTPX_RES_MCHOICE       300
#define EVHTTPX_RES_MOVEDPERM     301
#define EVHTTPX_RES_FOUND         302
#define EVHTTPX_RES_SEEOTHER      303
#define EVHTTPX_RES_NOTMOD        304
#define EVHTTPX_RES_USEPROXY      305
#define EVHTTPX_RES_SWITCHPROXY   306
#define EVHTTPX_RES_TMPREDIR      307

#define EVHTTPX_RES_400           400
#define EVHTTPX_RES_BADREQ        400
#define EVHTTPX_RES_UNAUTH        401
#define EVHTTPX_RES_PAYREQ        402
#define EVHTTPX_RES_FORBIDDEN     403
#define EVHTTPX_RES_NOTFOUND      404
#define EVHTTPX_RES_METHNALLOWED  405
#define EVHTTPX_RES_NACCEPTABLE   406
#define EVHTTPX_RES_PROXYAUTHREQ  407
#define EVHTTPX_RES_TIMEOUT       408
#define EVHTTPX_RES_CONFLICT      409
#define EVHTTPX_RES_GONE          410
#define EVHTTPX_RES_LENREQ        411
#define EVHTTPX_RES_PRECONDFAIL   412
#define EVHTTPX_RES_ENTOOLARGE    413
#define EVHTTPX_RES_URITOOLARGE   414
#define EVHTTPX_RES_UNSUPPORTED   415
#define EVHTTPX_RES_RANGENOTSC    416
#define EVHTTPX_RES_EXPECTFAIL    417
#define EVHTTPX_RES_IAMATEAPOT    418

#define EVHTTPX_RES_500           500
#define EVHTTPX_RES_SERVERR       500
#define EVHTTPX_RES_NOTIMPL       501
#define EVHTTPX_RES_BADGATEWAY    502
#define EVHTTPX_RES_SERVUNAVAIL   503
#define EVHTTPX_RES_GWTIMEOUT     504
#define EVHTTPX_RES_VERNSUPPORT   505
#define EVHTTPX_RES_BWEXEED       509

struct evhttpx_defaults_s {
    evhttpx_callback_cb    cb;
    evhttpx_pre_accept_cb  pre_accept;
    evhttpx_post_accept_cb post_accept;
    void               * cbarg;
    void               * pre_accept_cbarg;
    void               * post_accept_cbarg;
};

struct evhttpx_alias_s {
    char * alias;

    TAILQ_ENTRY(evhttpx_alias_s) next;
};

/**
 * @brief main structure containing all configuration information
 */
struct evhttpx_s {
    evhttpx_t  * parent;         /**< only when this is a vhost */
    evbase_t * evbase;         /**< the initialized event_base */
    evserv_t * server;         /**< the libevent listener struct */
    char     * server_name;    /**< the name included in Host: responses */
    void     * arg;            /**< user-defined evhttpx_t specific arguments */
    int        bev_flags;      /**< bufferevent flags to use on bufferevent_*_socket_new() */
    uint64_t   max_body_size;
    uint64_t   max_keepalive_requests;

#ifndef DISABLE_SSL
    evhttpx_ssl_ctx_t * ssl_ctx; /**< if ssl enabled, this is the servers CTX */
    evhttpx_ssl_cfg_t * ssl_cfg;
#endif

#ifndef EVHTTPX_DISABLE_EVTHR
    evthr_pool_t * thr_pool;   /**< connection threadpool */
#endif

#ifndef EVHTTPX_DISABLE_EVTHR
    pthread_mutex_t    * lock; /**< parent lock for add/del cbs in threads */
    evhttpx_thread_init_cb thread_init_cb;
    void               * thread_init_cbarg;
#endif
    evhttpx_callbacks_t * callbacks;
    evhttpx_defaults_t    defaults;

    struct timeval recv_timeo;
    struct timeval send_timeo;

    TAILQ_HEAD(, evhttpx_alias_s) aliases;
    TAILQ_HEAD(, evhttpx_s) vhosts;
    TAILQ_ENTRY(evhttpx_s) next_vhost;
};

/**
 * @brief structure containing a single callback and configuration
 *
 * The definition structure which is used within the evhttpx_callbacks_t
 * structure. This holds information about what should execute for either
 * a single or regex path.
 *
 * For example, if you registered a callback to be executed on a request
 * for "/herp/derp", your defined callback will be executed.
 *
 * Optionally you can set callback-specific hooks just like per-connection
 * hooks using the same rules.
 *
 */
struct evhttpx_callback_s {
    evhttpx_callback_type type;           /**< the type of callback (regex|path) */
    evhttpx_callback_cb   cb;             /**< the actual callback function */
    unsigned int        hash;           /**< the full hash generated integer */
    void              * cbarg;          /**< user-defind arguments passed to the cb */
    evhttpx_hooks_t     * hooks;          /**< per-callback hooks */

    union {
        char * path;
        char * glob;
    } val;

    TAILQ_ENTRY(evhttpx_callback_s) next;
};

TAILQ_HEAD(evhttpx_callbacks_s, evhttpx_callback_s);

/**
 * @brief a generic key/value structure
 */
struct evhttpx_kv_s {
    char * key;
    char * val;

    size_t klen;
    size_t vlen;

    char k_heaped; /**< set to 1 if the key can be free()'d */
    char v_heaped; /**< set to 1 if the val can be free()'d */

    TAILQ_ENTRY(evhttpx_kv_s) next;
};

TAILQ_HEAD(evhttpx_kvs_s, evhttpx_kv_s);



/**
 * @brief a generic container representing an entire URI strucutre
 */
struct evhttpx_uri_s {
    evhttpx_authority_t * authority;
    evhttpx_path_t      * path;
    unsigned char     * fragment;     /**< data after '#' in uri */
    unsigned char     * query_raw;    /**< the unparsed query arguments */
    evhttpx_query_t     * query;        /**< list of k/v for query arguments */
    htp_scheme          scheme;       /**< set if a scheme is found */
};


/**
 * @brief structure which represents authority information in a URI
 */
struct evhttpx_authority_s {
    char   * username;                /**< the username in URI (scheme://USER:.. */
    char   * password;                /**< the password in URI (scheme://...:PASS.. */
    char   * hostname;                /**< hostname if present in URI */
    uint16_t port;                    /**< port if present in URI */
};


/**
 * @brief structure which represents a URI path and or file
 */
struct evhttpx_path_s {
    char       * full;                /**< the full path+file (/a/b/c.html) */
    char       * path;                /**< the path (/a/b/) */
    char       * file;                /**< the filename if present (c.html) */
    char       * match_start;
    char       * match_end;
    unsigned int matched_soff;        /**< offset of where the uri starts
                                       *   mainly used for regex matching
                                       */
    unsigned int matched_eoff;        /**< offset of where the uri ends
                                       *   mainly used for regex matching
                                       */
};


/**
 * @brief a structure containing all information for a http request.
 */
struct evhttpx_request_s {
    evhttpx_t            * httpx;         /**< the parent evhttpx_t structure */
    evhttpx_connection_t * conn;        /**< the associated connection */
    evhttpx_hooks_t      * hooks;       /**< request specific hooks */
    evhttpx_uri_t        * uri;         /**< request URI information */
    evbuf_t            * buffer_in;   /**< buffer containing data from client */
    evbuf_t            * buffer_out;  /**< buffer containing data to client */
    evhttpx_headers_t    * headers_in;  /**< headers from client */
    evhttpx_headers_t    * headers_out; /**< headers to client */
    evhttpx_proto          proto;       /**< HTTP protocol used */
    htp_method           method;      /**< HTTP method used */
    evhttpx_res            status;      /**< The HTTP response code or other error conditions */
    int                  keepalive;   /**< set to 1 if the connection is keep-alive */
    int                  finished;    /**< set to 1 if the request is fully processed */
    int                  chunked;     /**< set to 1 if the request is chunked */

    evhttpx_callback_cb cb;             /**< the function to call when fully processed */
    void            * cbarg;          /**< argument which is passed to the cb function */
    int               error;
};

#define evhttpx_request_content_len(r) htparser_get_content_length(r->conn->parser)

struct evhttpx_connection_s {
    evhttpx_t         * httpx;
    evbase_t        * evbase;
    evbev_t         * bev;
    evthr_t         * thread;
    evhttpx_ssl_t     * ssl;
    evhttpx_hooks_t   * hooks;
    htparser        * parser;
    event_t         * resume_ev;
    struct sockaddr * saddr;
    struct timeval    recv_timeo;    /**< conn read timeouts (overrides global) */
    struct timeval    send_timeo;    /**< conn write timeouts (overrides global) */
    int               sock;
    uint8_t           error;
    uint8_t           owner;         /**< set to 1 if this structure owns the bufferevent */
    uint8_t           vhost_via_sni; /**< set to 1 if the vhost was found via SSL SNI */
    evhttpx_request_t * request;       /**< the request currently being processed */
    uint64_t          max_body_size;
    uint64_t          body_bytes_read;
    uint64_t          num_requests;
};

struct evhttpx_hooks_s {
    evhttpx_hook_headers_start_cb   on_headers_start;
    evhttpx_hook_header_cb          on_header;
    evhttpx_hook_headers_cb         on_headers;
    evhttpx_hook_path_cb            on_path;
    evhttpx_hook_read_cb            on_read;
    evhttpx_hook_request_fini_cb    on_request_fini;
    evhttpx_hook_connection_fini_cb on_connection_fini;
    evhttpx_hook_err_cb             on_error;
    evhttpx_hook_chunk_new_cb       on_new_chunk;
    evhttpx_hook_chunk_fini_cb      on_chunk_fini;
    evhttpx_hook_chunks_fini_cb     on_chunks_fini;
    evhttpx_hook_hostname_cb        on_hostname;
    evhttpx_hook_write_cb           on_write;

    void * on_headers_start_arg;
    void * on_header_arg;
    void * on_headers_arg;
    void * on_path_arg;
    void * on_read_arg;
    void * on_request_fini_arg;
    void * on_connection_fini_arg;
    void * on_error_arg;
    void * on_new_chunk_arg;
    void * on_chunk_fini_arg;
    void * on_chunks_fini_arg;
    void * on_hostname_arg;
    void * on_write_arg;
};

struct evhttpx_ssl_cfg_s {
    char                  * pemfile;
    char                  * privfile;
    char                  * cafile;
    char                  * capath;
    char                  * ciphers;
    char                  * named_curve;
    long                    ssl_opts;
    long                    ssl_ctx_timeout;
    int                     verify_peer;
    int                     verify_depth;
    evhttpx_ssl_verify_cb     x509_verify_cb;
    evhttpx_ssl_chk_issued_cb x509_chk_issued_cb;
    long                    store_flags;
    evhttpx_ssl_scache_type   scache_type;
    long                    scache_timeout;
    long                    scache_size;
    evhttpx_ssl_scache_init   scache_init;
    evhttpx_ssl_scache_add    scache_add;
    evhttpx_ssl_scache_get    scache_get;
    evhttpx_ssl_scache_del    scache_del;
    void                  * args;
};

/**
 * @brief creates a new evhttpx_t instance
 *
 * @param evbase the initialized event base
 * @param arg user-defined argument which is evhttpx_t specific
 *
 * @return a new evhttpx_t structure or NULL on error
 */
evhttpx_t * evhttpx_new(evbase_t * evbase, void * arg);
void      evhttpx_free(evhttpx_t * evhtp);


/**
 * @brief set a read/write timeout on all things evhttpx_t. When the timeout
 *        expires your error hook will be called with the libevent supplied event
 *        flags.
 *
 * @param htp the base evhttpx_t struct
 * @param r read-timeout in timeval
 * @param w write-timeout in timeval.
 */
void evhttpx_set_timeouts(evhttpx_t * htp, const struct timeval * r, const struct timeval * w);
void evhttpx_set_bev_flags(evhttpx_t * htp, int flags);
int  evhttpx_ssl_use_threads(void);
int  evhttpx_ssl_init(evhttpx_t * htp, evhttpx_ssl_cfg_t * ssl_cfg);


/**
 * @brief creates a lock around callbacks and hooks, allowing for threaded
 * applications to add/remove/modify hooks & callbacks in a thread-safe manner.
 *
 * @param htp
 *
 * @return 0 on success, -1 on error
 */
int evhttpx_use_callback_locks(evhttpx_t * htp);

/**
 * @brief sets a callback which is called if no other callbacks are matched
 *
 * @param htp the initialized evhttpx_t
 * @param cb  the function to be executed
 * @param arg user-defined argument passed to the callback
 */
void evhttpx_set_gencb(evhttpx_t * htp, evhttpx_callback_cb cb, void * arg);
void evhttpx_set_pre_accept_cb(evhttpx_t * htp, evhttpx_pre_accept_cb, void * arg);
void evhttpx_set_post_accept_cb(evhttpx_t * htp, evhttpx_post_accept_cb, void * arg);


/**
 * @brief sets a callback to be executed on a specific path
 *
 * @param htp the initialized evhttpx_t
 * @param path the path to match
 * @param cb the function to be executed
 * @param arg user-defined argument passed to the callback
 *
 * @return evhttpx_callback_t * on success, NULL on error.
 */
evhttpx_callback_t * evhttpx_set_cb(evhttpx_t * htp, const char * path, evhttpx_callback_cb cb, void * arg);

/**
 * @brief sets a callback to to be executed on simple glob/wildcard patterns
 *        this is useful if the app does not care about what was matched, but
 *        just that it matched. This is technically faster than regex.
 *
 * @param htp
 * @param pattern wildcard pattern, the '*' can be set at either or both the front or end.
 * @param cb
 * @param arg
 *
 * @return
 */
evhttpx_callback_t * evhttpx_set_glob_cb(evhttpx_t * htp, const char * pattern, evhttpx_callback_cb cb, void * arg);

/**
 * @brief sets a callback hook for either a connection or a path/regex .
 *
 * A user may set a variety of hooks either per-connection, or per-callback.
 * This allows the developer to hook into various parts of the request processing
 * cycle.
 *
 * a per-connection hook can be set at any time, but it is recommended to set these
 * during either a pre-accept phase, or post-accept phase. This allows a developer
 * to set hooks before any other hooks are called.
 *
 * a per-callback hook works differently. In this mode a developer can setup a set
 * of hooks prior to starting the event loop for specific callbacks. For example
 * if you wanted to hook something ONLY for a callback set by evhttpx_set_cb or
 * evhttpx_set_regex_cb this is the method of doing so.
 *
 * per-callback example:
 *
 * evhttpx_callback_t * cb = evhttpx_set_regex_cb(htp, "/anything/(.*)", default_cb, NULL);
 *
 * evhttpx_set_hook(&cb->hooks, evhttpx_hook_on_headers, anything_headers_cb, NULL);
 *
 * evhttpx_set_hook(&cb->hooks, evhttpx_hook_on_fini, anything_fini_cb, NULL);
 *
 * With the above example, once libevhtp has determined that it has a user-defined
 * callback for /anything/.*; anything_headers_cb will be executed after all headers
 * have been parsed, and anything_fini_cb will be executed before the request is
 * free()'d.
 *
 * The same logic applies to per-connection hooks, but it should be noted that if
 * a per-callback hook is set, the per-connection hook will be ignored.
 *
 * @param hooks double pointer to the evhttpx_hooks_t structure
 * @param type the hook type
 * @param cb the callback to be executed.
 * @param arg optional argument which is passed when the callback is executed
 *
 * @return 0 on success, -1 on error (if hooks is NULL, it is allocated)
 */
int evhttpx_set_hook(evhttpx_hooks_t ** hooks, evhttpx_hook_type type, evhttpx_hook cb, void * arg);


/**
 * @brief remove a specific hook from being called.
 *
 * @param hooks
 * @param type
 *
 * @return
 */
int evhttpx_unset_hook(evhttpx_hooks_t ** hooks, evhttpx_hook_type type);


/**
 * @brief removes all hooks.
 *
 * @param hooks
 *
 * @return
 */
int evhttpx_unset_all_hooks(evhttpx_hooks_t ** hooks);


/**
 * @brief bind to a socket, optionally with specific protocol support
 *        formatting. The addr can be defined as one of the following:
 *          ipv6:<ipv6addr> for binding to an IPv6 address.
 *          unix:<named pipe> for binding to a unix named socket
 *          ipv4:<ipv4addr> for binding to an ipv4 address
 *        Otherwise the addr is assumed to be ipv4.
 *
 * @param htp
 * @param addr
 * @param port
 * @param backlog
 *
 * @return
 */
int evhttpx_bind_socket(evhttpx_t * htp, const char * addr, uint16_t port, int backlog);


/**
 * @brief stops the listening socket.
 *
 * @param htp
 */
void evhttpx_unbind_socket(evhttpx_t * htp);

/**
 * @brief bind to an already allocated sockaddr.
 *
 * @param htp
 * @param
 * @param sin_len
 * @param backlog
 *
 * @return
 */
int  evhttpx_bind_sockaddr(evhttpx_t * htp, struct sockaddr *, size_t sin_len, int backlog);

int  evhttpx_use_threads(evhttpx_t * htp, evhttpx_thread_init_cb init_cb, int nthreads, void * arg);
void evhttpx_send_reply(evhttpx_request_t * request, evhttpx_res code);
void evhttpx_send_reply_start(evhttpx_request_t * request, evhttpx_res code);
void evhttpx_send_reply_body(evhttpx_request_t * request, evbuf_t * buf);
void evhttpx_send_reply_end(evhttpx_request_t * request);

/**
 * @brief Determine if a response should have a body.
 * Follows the rules in RFC 2616 section 4.3.
 * @return 1 if the response MUST have a body; 0 if the response MUST NOT have
 *     a body.
 */
int evhttpx_response_needs_body(const evhttpx_res code, const htp_method method);


/**
 * @brief start a chunked response. If data already exists on the output buffer,
 *        this will be converted to the first chunk.
 *
 * @param request
 * @param code
 */
void evhttpx_send_reply_chunk_start(evhttpx_request_t * request, evhttpx_res code);


/**
 * @brief send a chunk reply.
 *
 * @param request
 * @param buf
 */
void evhttpx_send_reply_chunk(evhttpx_request_t * request, evbuf_t * buf);


/**
 * @brief call when all chunks have been sent and you wish to send the last
 *        bits. This will add the last 0CRLFCRCL and call send_reply_end().
 *
 * @param request
 */
void evhttpx_send_reply_chunk_end(evhttpx_request_t * request);

/**
 * @brief creates a new evhttpx_callback_t structure.
 *
 * All callbacks are stored in this structure
 * which define what the final function to be
 * called after all parsing is done. A callback
 * can be either a static string or a regular
 * expression.
 *
 * @param path can either be a static path (/path/to/resource/) or
 *        a POSIX compatible regular expression (^/resource/(.*))
 * @param type informs the function what type of of information is
 *        is contained within the path argument. This can either be
 *        callback_type_path, or callback_type_regex.
 * @param cb the callback function to be invoked
 * @param arg optional argument which is passed when the callback is executed.
 *
 * @return 0 on success, -1 on error.
 */
evhttpx_callback_t * evhttpx_callback_new(const char * path, evhttpx_callback_type type, evhttpx_callback_cb cb, void * arg);
void               evhttpx_callback_free(evhttpx_callback_t * callback);


/**
 * @brief Adds a evhttpx_callback_t to the evhttpx_callbacks_t list
 *
 * @param cbs an allocated evhttpx_callbacks_t structure
 * @param cb  an initialized evhttpx_callback_t structure
 *
 * @return 0 on success, -1 on error
 */
int evhttpx_callbacks_add_callback(evhttpx_callbacks_t * cbs, evhttpx_callback_t * cb);


/**
 * @brief add an evhttpx_t structure (with its own callbacks) to a base evhttpx_t
 *        structure for virtual hosts. It should be noted that if you enable SSL
 *        on the base evhttpx_t and your version of OpenSSL supports SNI, the SNI
 *        hostname will always take precedence over the Host header value.
 *
 * @param evhtp
 * @param name
 * @param vhost
 *
 * @return
 */
int evhttpx_add_vhost(evhttpx_t * evhtp, const char * name, evhttpx_t * vhost);


/**
 * @brief Add an alias hostname for a virtual-host specific evhttpx_t. This avoids
 *        having multiple evhttpx_t virtual hosts with the same callback for the same
 *        vhost.
 *
 * @param evhtp
 * @param name
 *
 * @return
 */
int evhttpx_add_alias(evhttpx_t * evhtp, const char * name);

/**
 * @brief Allocates a new key/value structure.
 *
 * @param key null terminated string
 * @param val null terminated string
 * @param kalloc if set to 1, the key will be copied, if 0 no copy is done.
 * @param valloc if set to 1, the val will be copied, if 0 no copy is done.
 *
 * @return evhttpx_kv_t * on success, NULL on error.
 */
evhttpx_kv_t  * evhttpx_kv_new(const char * key, const char * val, char kalloc, char valloc);
evhttpx_kvs_t * evhttpx_kvs_new(void);

void          evhttpx_kv_free(evhttpx_kv_t * kv);
void          evhttpx_kvs_free(evhttpx_kvs_t * kvs);
void          evhttpx_kv_rm_and_free(evhttpx_kvs_t * kvs, evhttpx_kv_t * kv);

const char  * evhttpx_kv_find(evhttpx_kvs_t * kvs, const char * key);
evhttpx_kv_t  * evhttpx_kvs_find_kv(evhttpx_kvs_t * kvs, const char * key);


/**
 * @brief appends a key/val structure to a evhttpx_kvs_t tailq
 *
 * @param kvs an evhttpx_kvs_t structure
 * @param kv  an evhttpx_kv_t structure
 */
void evhttpx_kvs_add_kv(evhttpx_kvs_t * kvs, evhttpx_kv_t * kv);

int  evhttpx_kvs_for_each(evhttpx_kvs_t * kvs, evhttpx_kvs_iterator cb, void * arg);

/**
 * @brief Parses the query portion of the uri into a set of key/values
 *
 * Parses query arguments like "?herp=derp&foo=bar;blah=baz"
 *
 * @param query data containing the uri query arguments
 * @param len size of the data
 *
 * @return evhttpx_query_t * on success, NULL on error
 */
evhttpx_query_t * evhttpx_parse_query(const char * query, size_t len);


/**
 * @brief Unescapes strings like '%7B1,%202,%203%7D' would become '{1, 2, 3}'
 *
 * @param out double pointer where output is stored. This is allocated by the user.
 * @param str the string to unescape
 * @param str_len the length of the string to unescape
 *
 * @return 0 on success, -1 on error
 */
int evhttpx_unescape_string(unsigned char ** out, unsigned char * str, size_t str_len);

/**
 * @brief creates a new evhttpx_header_t key/val structure
 *
 * @param key a null terminated string
 * @param val a null terminated string
 * @param kalloc if 1, key will be copied, otherwise no copy performed
 * @param valloc if 1, val will be copied, otehrwise no copy performed
 *
 * @return evhttpx_header_t * or NULL on error
 */
evhttpx_header_t * evhttpx_header_new(const char * key, const char * val, char kalloc, char valloc);

/**
 * @brief creates a new evhttpx_header_t, sets only the key, and adds to the
 *        evhttpx_headers TAILQ
 *
 * @param headers the evhttpx_headers_t TAILQ (evhttpx_kv_t)
 * @param key a null terminated string
 * @param kalloc if 1 the string will be copied, otherwise assigned
 *
 * @return an evhttpx_header_t pointer or NULL on error
 */
evhttpx_header_t * evhttpx_header_key_add(evhttpx_headers_t * headers, const char * key, char kalloc);


/**
 * @brief finds the last header in the headers tailq and adds the value
 *
 * @param headers the evhttpx_headers_t TAILQ (evhttpx_kv_t)
 * @param val a null terminated string
 * @param valloc if 1 the string will be copied, otherwise assigned
 *
 * @return an evhttpx_header_t pointer or NULL on error
 */
evhttpx_header_t * evhttpx_header_val_add(evhttpx_headers_t * headers, const char * val, char valloc);


/**
 * @brief adds an evhttpx_header_t to the end of the evhttpx_headers_t tailq
 *
 * @param headers
 * @param header
 */
void evhttpx_headers_add_header(evhttpx_headers_t * headers, evhttpx_header_t * header);

/**
 * @brief finds the value of a key in a evhttpx_headers_t structure
 *
 * @param headers the evhttpx_headers_t tailq
 * @param key the key to find
 *
 * @return the value of the header key if found, NULL if not found.
 */
const char * evhttpx_header_find(evhttpx_headers_t * headers, const char * key);

#define evhttpx_header_find         evhttpx_kv_find
#define evhttpx_headers_find_header evhttpx_kvs_find_kv
#define evhttpx_headers_for_each    evhttpx_kvs_for_each
#define evhttpx_header_new          evhttpx_kv_new
#define evhttpx_header_free         evhttpx_kv_free
#define evhttpx_headers_new         evhttpx_kvs_new
#define evhttpx_headers_free        evhttpx_kvs_free
#define evhttpx_header_rm_and_free  evhttpx_kv_rm_and_free
#define evhttpx_headers_add_header  evhttpx_kvs_add_kv
#define evhttpx_query_new           evhttpx_kvs_new
#define evhttpx_query_free          evhttpx_kvs_free


/**
 * @brief returns the htp_method enum version of the request method.
 *
 * @param r
 *
 * @return htp_method enum
 */
htp_method evhttpx_request_get_method(evhttpx_request_t * r);

void       evhttpx_connection_pause(evhttpx_connection_t * connection);
void       evhttpx_connection_resume(evhttpx_connection_t * connection);
void       evhttpx_request_pause(evhttpx_request_t * request);
void       evhttpx_request_resume(evhttpx_request_t * request);


/**
 * @brief returns the underlying evhttpx_connection_t structure from a request
 *
 * @param request
 *
 * @return evhttpx_connection_t on success, otherwise NULL
 */
evhttpx_connection_t * evhttpx_request_get_connection(evhttpx_request_t * request);

/**
 * @brief Sets the connections underlying bufferevent
 *
 * @param conn
 * @param bev
 */
void evhttpx_connection_set_bev(evhttpx_connection_t * conn, evbev_t * bev);

/**
 * @brief sets the underlying bufferevent for a evhttpx_request
 *
 * @param request
 * @param bev
 */
void evhttpx_request_set_bev(evhttpx_request_t * request, evbev_t * bev);


/**
 * @brief returns the underlying connections bufferevent
 *
 * @param conn
 *
 * @return bufferevent on success, otherwise NULL
 */
evbev_t * evhttpx_connection_get_bev(evhttpx_connection_t * conn);


/**
 * @brief sets a connection-specific read/write timeout which overrides the
 *        global read/write settings.
 *
 * @param conn
 * @param r timeval for read
 * @param w timeval for write
 */
void evhttpx_connection_set_timeouts(evhttpx_connection_t * conn, const struct timeval * r, const struct timeval * w);

/**
 * @brief returns the underlying requests bufferevent
 *
 * @param request
 *
 * @return bufferevent on success, otherwise NULL
 */
evbev_t * evhttpx_request_get_bev(evhttpx_request_t * request);


/**
 * @brief let a user take ownership of the underlying bufferevent and free
 *        all other underlying resources.
 *
 * Warning: this will free all evhttpx_connection/request structures, remove all
 * associated hooks and reset the bufferevent to defaults, i.e., disable
 * EV_READ, and set all callbacks to NULL.
 *
 * @param connection
 *
 * @return underlying connections bufferevent.
 */
evbev_t * evhttpx_connection_take_ownership(evhttpx_connection_t * connection);


/**
 * @brief free's all connection related resources, this will also call your
 *        request fini hook and request fini hook.
 *
 * @param connection
 */
void evhttpx_connection_free(evhttpx_connection_t * connection);

void evhttpx_request_free(evhttpx_request_t * request);


/**
 * @brief set a max body size to accept for an incoming request, this will
 *        default to unlimited.
 *
 * @param htp
 * @param len
 */
void evhttpx_set_max_body_size(evhttpx_t * htp, uint64_t len);


/**
 * @brief set a max body size for a specific connection, this will default to
 *        the size set by evhttpx_set_max_body_size
 *
 * @param conn
 * @param len
 */
void evhttpx_connection_set_max_body_size(evhttpx_connection_t * conn, uint64_t len);

/**
 * @brief just calls evhttpx_connection_set_max_body_size for the request.
 *
 * @param request
 * @param len
 */
void evhttpx_request_set_max_body_size(evhttpx_request_t * request, uint64_t len);

/**
 * @brief sets a maximum number of requests that a single connection can make.
 *
 * @param htp
 * @param num
 */
void evhttpx_set_max_keepalive_requests(evhttpx_t * htp, uint64_t num);

#ifdef __cplusplus
}
#endif

#endif /* _EVHTTPX_H_ */

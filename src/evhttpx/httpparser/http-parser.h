#ifndef _HTTP_PARSER_H_
#define _HTTP_PARSER_H_

struct http_parser;

enum httpx_type {
    httpx_type_request = 0,
    httpx_type_response
};

enum httpx_scheme {
    httpx_scheme_none = 0,
    httpx_scheme_ftp,
    httpx_scheme_http,
    httpx_scheme_https,
    httpx_scheme_nfs,
    httpx_scheme_unknown
};

enum httpx_method {
    http_method_GET = 0,
    http_method_HEAD,
    http_method_POST,
    http_method_PUT,
    http_method_DELETE,
    http_method_MKCOL,
    http_method_COPY,
    http_method_MOVE,
    http_method_OPTIONS,
    http_method_PROPFIND,
    http_method_PROPPATCH,
    http_method_LOCK,
    http_method_UNLOCK,
    http_method_TRACE,
    http_method_UNKNOWN
};

enum http_parse_error {
    http_parse_error_none = 0,
    http_parse_error_too_big,
    http_parse_error_inval_method,
    http_parse_error_inval_reqline,
    http_parse_error_inval_schema,
    http_parse_error_inval_proto,
    http_parse_error_inval_ver,
    http_parse_error_inval_hdr,
    http_parse_error_inval_chunk_sz,
    http_parse_error_inval_chunk,
    http_parse_error_inval_state,
    http_parse_error_user,
    http_parse_error_status,
    http_parse_error_generic
};

typedef struct http_parser      http_parser_t;
typedef struct http_parse_hooks http_parse_hooks_t;

typedef enum httpx_scheme      http_scheme_e;
typedef enum httpx_method      http_method_e;
typedef enum httpx_type        http_type_e;
typedef enum http_parse_error  http_parse_error_e;

typedef int (*http_parse_hook)(http_parser_t *);
typedef int (*http_parse_data_hook)(http_parser_t *, const char *, size_t);


struct http_parse_hooks {
    http_parse_hook      on_msg_begin;
    http_parse_data_hook method;
    http_parse_data_hook scheme;              /* called if scheme is found */
    http_parse_data_hook host;                /* called if a host was in the request scheme */
    http_parse_data_hook port;                /* called if a port was in the request scheme */
    http_parse_data_hook path;                /* only the path of the uri */
    http_parse_data_hook args;                /* only the arguments of the uri */
    http_parse_data_hook uri;                 /* the entire uri including path/args */
    http_parse_hook      on_hdrs_begin;
    http_parse_data_hook hdr_key;
    http_parse_data_hook hdr_val;
    http_parse_data_hook hostname;
    http_parse_hook      on_hdrs_complete;
    http_parse_hook      on_new_chunk;        /* called after parsed chunk octet */
    http_parse_hook      on_chunk_complete;   /* called after single parsed chunk */
    http_parse_hook      on_chunks_complete;  /* called after all parsed chunks processed */
    http_parse_data_hook body;
    http_parse_hook      on_msg_complete;
};


size_t         http_parser_run(http_parser_t *, http_parse_hooks_t *, const char *, size_t);
int            http_parser_should_keep_alive(http_parser_t * p);
http_scheme_e     http_parser_get_scheme(http_parser_t *);
http_method_e     http_parser_get_method(http_parser_t *);
const char   * http_parser_get_methodstr(http_parser_t *);
void           http_parser_set_major(http_parser_t *, unsigned char);
void           http_parser_set_minor(http_parser_t *, unsigned char);
unsigned char  http_parser_get_major(http_parser_t *);
unsigned char  http_parser_get_minor(http_parser_t *);
unsigned char  http_parser_get_multipart(http_parser_t *);
unsigned int   http_parser_get_status(http_parser_t *);
uint64_t       http_parser_get_content_length(http_parser_t *);
uint64_t       http_parser_get_total_bytes_read(http_parser_t *);
http_parse_error_e http_parser_get_error(http_parser_t *);
const char   * http_parser_get_strerror(http_parser_t *);
void         * http_parser_get_userdata(http_parser_t *);
void           http_parser_set_userdata(http_parser_t *, void *);
void           http_parser_init(http_parser_t *, http_type_e);
http_parser_t     * http_parser_new(void);

#endif

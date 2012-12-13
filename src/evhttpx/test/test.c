#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <inttypes.h>
#include <event2/event.h>

#include "evhttpx.h"

#ifndef EVHTP_DISABLE_EVTHR
int      use_threads    = 0;
int      num_threads    = 0;
#endif
char   * bind_addr      = "0.0.0.0";
uint16_t bind_port      = 8081;
char   * ext_body       = NULL;
char   * ssl_pem        = NULL;
char   * ssl_ca         = NULL;
char   * ssl_capath     = NULL;
size_t   bw_limit       = 0;
uint64_t max_keepalives = 0;

struct pauser {
    event_t         * timer_ev;
    evhttpx_request_t * request;
    struct timeval  * tv;
};

/* pause testing */
static void
resume_request_timer(int sock, short which, void * arg) {
    struct pauser * pause = (struct pauser *)arg;

    printf("resume_request_timer(%p) timer_ev = %p\n", pause->request->conn, pause->timer_ev);
    fflush(stdout);

    evhttpx_request_resume(pause->request);
}

static evhttpx_res
pause_cb(evhttpx_request_t * request, evhttpx_header_t * header, void * arg) {
    struct pauser * pause = (struct pauser *)arg;
    int             s     = rand() % 1000000;

    printf("pause_cb(%p) pause == %p, timer_ev = %p\n",
           request->conn, pause, pause->timer_ev);
    printf("pause_cb(%p) k=%s, v=%s timer_ev = %p\n", request->conn,
           header->key, header->val, pause->timer_ev);
    printf("pause_cb(%p) setting to %ld usec sleep timer_ev = %p\n",
           request->conn, (long int)s, pause->timer_ev);

    pause->tv->tv_sec  = 0;
    pause->tv->tv_usec = s;

    if (evtimer_pending(pause->timer_ev, NULL)) {
        evtimer_del(pause->timer_ev);
    }

    evtimer_add(pause->timer_ev, pause->tv);

    return EVHTTPX_RES_PAUSE;
}

static evhttpx_res
pause_connection_fini(evhttpx_connection_t * connection, void * arg) {
    printf("pause_connection_fini(%p)\n", connection);

    return EVHTTPX_RES_OK;
}

static evhttpx_res
pause_request_fini(evhttpx_request_t * request, void * arg) {
    struct pauser * pause = (struct pauser *)arg;

    printf("pause_request_fini() req=%p, c=%p\n", request, request->conn);
    event_free(pause->timer_ev);

    free(pause->tv);
    free(pause);

    return EVHTTPX_RES_OK;
}

static evhttpx_res
pause_init_cb(evhttpx_request_t * req, evhttpx_path_t * path, void * arg) {
    evbase_t      * evbase = req->conn->evbase;
    struct pauser * pause  = calloc(sizeof(struct pauser), 1);

    pause->tv       = calloc(sizeof(struct timeval), 1);

    pause->timer_ev = evtimer_new(evbase, resume_request_timer, pause);
    pause->request  = req;

    evhttpx_set_hook(&req->hooks, evhttpx_hook_on_header, pause_cb, pause);
    evhttpx_set_hook(&req->hooks, evhttpx_hook_on_request_fini, pause_request_fini, pause);
    evhttpx_set_hook(&req->conn->hooks, evhttpx_hook_on_connection_fini, pause_connection_fini, NULL);

    return EVHTTPX_RES_OK;
}

static void
test_pause_cb(evhttpx_request_t * request, void * arg) {
    printf("test_pause_cb(%p)\n", request->conn);
    evhttpx_send_reply(request, EVHTTPX_RES_OK);
}


static void
dynamic_cb(evhttpx_request_t * r, void * arg) {
    const char * name = arg;

    evbuffer_add_printf(r->buffer_out, "dynamic_cb = %s\n", name);
    evhttpx_send_reply(r, EVHTTPX_RES_OK);
}

static void
create_callback(evhttpx_request_t * r, void * arg) {
    char * uri;
    char * nuri;
    size_t urilen;

    uri    = r->uri->path->match_start;
    urilen = strlen(uri);

    if (urilen == 0) {
        return evhttpx_send_reply(r, EVHTTPX_RES_BADREQ);
    }

    nuri = calloc(urilen + 2, 1);

    snprintf(nuri, urilen + 2, "/%s", uri);
    evhttpx_set_cb(r->httpx, nuri, dynamic_cb, nuri);

    evhttpx_send_reply(r, EVHTTPX_RES_OK);
}

static void
test_foo_cb(evhttpx_request_t * req, void * arg ) {
    evbuffer_add_reference(req->buffer_out,
                           "test_foo_cb\n", 12, NULL, NULL);

    evhttpx_send_reply(req, EVHTTPX_RES_OK);
}

static void
test_500_cb(evhttpx_request_t * req, void * arg ) {
    evbuffer_add_reference(req->buffer_out,
                           "test_500_cb\n", 12, NULL, NULL);

    evhttpx_send_reply(req, EVHTTPX_RES_SERVERR);
}

static void
test_max_body(evhttpx_request_t * req, void * arg) {
    evbuffer_add_reference(req->buffer_out,
                           "test_max_body\n", 14, NULL, NULL);

    evhttpx_send_reply(req, EVHTTPX_RES_OK);
}

const char * chunk_strings[] = {
    "I give you the light of Eärendil,\n",
    "our most beloved star.\n",
    "May it be a light for you in dark places,\n",
    "when all other lights go out.\n",
    NULL
};

static void
test_chunking(evhttpx_request_t * req, void * arg) {
    const char * chunk_str;
    evbuf_t    * buf;
    int          i = 0;

    buf = evbuffer_new();

    evhttpx_send_reply_chunk_start(req, EVHTTPX_RES_OK);

    while ((chunk_str = chunk_strings[i++]) != NULL) {
        evbuffer_add(buf, chunk_str, strlen(chunk_str));

        evhttpx_send_reply_chunk(req, buf);

        evbuffer_drain(buf, -1);
    }

    evhttpx_send_reply_chunk_end(req);
    evbuffer_free(buf);
}

static void
test_bar_cb(evhttpx_request_t * req, void * arg) {
    evhttpx_send_reply(req, EVHTTPX_RES_OK);
}

static void
test_glob_cb(evhttpx_request_t * req, void * arg) {
    evbuffer_add(req->buffer_out, "test_glob_cb\n", 13);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);
}

static void
test_default_cb(evhttpx_request_t * req, void * arg) {
    evbuffer_add_reference(req->buffer_out,
                           "test_default_cb\n", 16, NULL, NULL);


    evhttpx_send_reply(req, EVHTTPX_RES_OK);
}

static evhttpx_res
print_kv(evhttpx_request_t * req, evhttpx_header_t * hdr, void * arg) {
    evbuffer_add_printf(req->buffer_out,
                        "print_kv() key = '%s', val = '%s'\n",
                        hdr->key, hdr->val);

    return EVHTTPX_RES_OK;
}

static int
output_header(evhttpx_header_t * header, void * arg) {
    evbuf_t * buf = arg;

    evbuffer_add_printf(buf, "print_kvs() key = '%s', val = '%s'\n",
                        header->key, header->val);
    return 0;
}

static evhttpx_res
print_kvs(evhttpx_request_t * req, evhttpx_headers_t * hdrs, void * arg ) {
    evhttpx_headers_for_each(hdrs, output_header, req->buffer_out);
    return EVHTTPX_RES_OK;
}

static evhttpx_res
print_path(evhttpx_request_t * req, evhttpx_path_t * path, void * arg) {
    if (ext_body) {
        evbuffer_add_printf(req->buffer_out, "ext_body: '%s'\n", ext_body);
    }

    evbuffer_add_printf(req->buffer_out,
                        "print_path() full        = '%s'\n"
                        "             path        = '%s'\n"
                        "             file        = '%s'\n"
                        "             match start = '%s'\n"
                        "             match_end   = '%s'\n"
                        "             methno      = '%d'\n",
                        path->full, path->path, path->file,
                        path->match_start, path->match_end,
                        evhttpx_request_get_method(req));

    return EVHTTPX_RES_OK;
}

static evhttpx_res
print_data(evhttpx_request_t * req, evbuf_t * buf, void * arg) {
#ifndef NDEBUG
    evbuffer_add_printf(req->buffer_out,
                        "got %zu bytes of data\n",
                        evbuffer_get_length(buf));
    printf("%.*s", (int)evbuffer_get_length(buf), (char *)evbuffer_pullup(buf, evbuffer_get_length(buf)));
#endif
    evbuffer_drain(buf, -1);
    return EVHTTPX_RES_OK;
}

static evhttpx_res
print_new_chunk_len(evhttpx_request_t * req, uint64_t len, void * arg) {
    evbuffer_add_printf(req->buffer_out, "started new chunk, %" PRId64 "u bytes\n", len);

    return EVHTTPX_RES_OK;
}

static evhttpx_res
print_chunk_complete(evhttpx_request_t * req, void * arg) {
    evbuffer_add_printf(req->buffer_out, "ended a single chunk\n");

    return EVHTTPX_RES_OK;
}

static evhttpx_res
print_chunks_complete(evhttpx_request_t * req, void * arg) {
    evbuffer_add_printf(req->buffer_out, "all chunks read\n");

    return EVHTTPX_RES_OK;
}

static evhttpx_res
set_max_body(evhttpx_request_t * req, evhttpx_headers_t * hdrs, void * arg) {
    evhttpx_request_set_max_body_size(req, 1024);

    return EVHTTPX_RES_OK;
}

static evhttpx_res
test_pre_accept(evhttpx_connection_t * c, void * arg) {
    uint16_t port = *(uint16_t *)arg;

    if (port > 10000) {
        return EVHTTPX_RES_ERROR;
    }

    return EVHTTPX_RES_OK;
}

static evhttpx_res
test_fini(evhttpx_request_t * r, void * arg) {
    struct ev_token_bucket_cfg * tcfg = arg;

    if (tcfg) {
        ev_token_bucket_cfg_free(tcfg);
    }

    return EVHTTPX_RES_OK;
}

#if 0
static evhttpx_res
print_hostname(evhttpx_request_t * r, const char * host, void * arg) {
    printf("%s\n", host);

    return EVHTTPX_RES_OK;
}

#endif

static evhttpx_res
set_my_connection_handlers(evhttpx_connection_t * conn, void * arg) {
    struct timeval               tick;
    struct ev_token_bucket_cfg * tcfg = NULL;

    evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_header, print_kv, "foo");
    evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_headers, print_kvs, "bar");
    evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_path, print_path, "baz");
    evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_read, print_data, "derp");
    evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_new_chunk, print_new_chunk_len, NULL);
    evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_chunk_complete, print_chunk_complete, NULL);
    evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_chunks_complete, print_chunks_complete, NULL);
    /* evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_hostname, print_hostname, NULL); */

    if (bw_limit > 0) {
        tick.tv_sec  = 0;
        tick.tv_usec = 500 * 100;

        tcfg         = ev_token_bucket_cfg_new(bw_limit, bw_limit, bw_limit, bw_limit, &tick);

        bufferevent_set_rate_limit(conn->bev, tcfg);
    }

    evhttpx_set_hook(&conn->hooks, evhttpx_hook_on_request_fini, test_fini, tcfg);

    return EVHTTPX_RES_OK;
}

#ifndef evhttpx_DISABLE_SSL
static int
dummy_ssl_verify_callback(int ok, X509_STORE_CTX * x509_store) {
    return 1;
}

static int
dummy_check_issued_cb(X509_STORE_CTX * ctx, X509 * x, X509 * issuer) {
    return 1;
}

#endif

const char * optstr = "htn:a:p:r:s:c:C:l:N:m:";

const char * help   =
    "Options: \n"
    "  -h       : This help text\n"
#ifndef EVHTTPX_DISABLE_EVTHR
    "  -t       : Run requests in a thread (default: off)\n"
    "  -n <int> : Number of threads        (default: 0 if -t is off, 4 if -t is on)\n"
#endif
#ifndef evhttpx_DISABLE_SSL
    "  -s <pem> : Enable SSL and PEM       (default: NULL)\n"
    "  -c <ca>  : CA cert file             (default: NULL)\n"
    "  -C <path>: CA Path                  (default: NULL)\n"
#endif
    "  -l <int> : Max bandwidth (in bytes) (default: NULL)\n"
    "  -r <str> : Document root            (default: .)\n"
    "  -N <str> : Add this string to body. (default: NULL)\n"
    "  -a <str> : Bind Address             (default: 0.0.0.0)\n"
    "  -p <int> : Bind Port                (default: 8081)\n"
    "  -m <int> : Max keepalive requests   (default: 0)\n";


int
parse_args(int argc, char ** argv) {
    extern char * optarg;
    extern int    optind;
    extern int    opterr;
    extern int    optopt;
    int           c;

    while ((c = getopt(argc, argv, optstr)) != -1) {
        switch (c) {
            case 'h':
                printf("Usage: %s [opts]\n%s", argv[0], help);
                return -1;
            case 'N':
                ext_body       = strdup(optarg);
                break;
            case 'a':
                bind_addr      = strdup(optarg);
                break;
            case 'p':
                bind_port      = atoi(optarg);
                break;
#ifndef EVHTTPX_DISABLE_EVTHR
            case 't':
                use_threads    = 1;
                break;
            case 'n':
                num_threads    = atoi(optarg);
                break;
#endif
#ifndef evhttpx_DISABLE_SSL
            case 's':
                ssl_pem        = strdup(optarg);
                break;
            case 'c':
                ssl_ca         = strdup(optarg);
                break;
            case 'C':
                ssl_capath     = strdup(optarg);
                break;
#endif
            case 'l':
                bw_limit       = atoll(optarg);
                break;
            case 'm':
                max_keepalives = atoll(optarg);
                break;
            default:
                printf("Unknown opt %s\n", optarg);
                return -1;
        } /* switch */
    }

#ifndef EVHTTPX_DISABLE_EVTHR
    if (use_threads && num_threads == 0) {
        num_threads = 4;
    }
#endif

    return 0;
} /* parse_args */

static void
sigint(int sig, short why, void * data) {
    event_base_loopexit(data, NULL);
}

int
main(int argc, char ** argv) {
    struct event     * ev_sigint;
    evbase_t         * evbase = NULL;
    evhttpx_t          * htp    = NULL;
    evhttpx_callback_t * cb_1   = NULL;
    evhttpx_callback_t * cb_2   = NULL;
    evhttpx_callback_t * cb_3   = NULL;
    evhttpx_callback_t * cb_4   = NULL;
    evhttpx_callback_t * cb_5   = NULL;
    evhttpx_callback_t * cb_9   = NULL;
    evhttpx_callback_t * cb_10  = NULL;
    evhttpx_callback_t * cb_11  = NULL;
    evhttpx_callback_t * cb_12  = NULL;

    if (parse_args(argc, argv) < 0) {
        exit(1);
    }

    srand((unsigned)time(NULL));

    evbase = event_base_new();
    htp    = evhttpx_new(evbase, NULL);

    evhttpx_set_max_keepalive_requests(htp, max_keepalives);

    cb_1   = evhttpx_set_cb(htp, "/ref", test_default_cb, "fjdkls");
    cb_2   = evhttpx_set_cb(htp, "/foo", test_foo_cb, "bar");
    cb_3   = evhttpx_set_cb(htp, "/foo/", test_foo_cb, "bar");
    cb_4   = evhttpx_set_cb(htp, "/bar", test_bar_cb, "baz");
    cb_5   = evhttpx_set_cb(htp, "/500", test_500_cb, "500");
    cb_9   = evhttpx_set_glob_cb(htp, "*/glob/*", test_glob_cb, NULL);
    cb_10  = evhttpx_set_cb(htp, "/max_body_size", test_max_body, NULL);

    /* set a callback to test out chunking API */
    cb_11  = evhttpx_set_cb(htp, "/chunkme", test_chunking, NULL);

    /* set a callback which takes ownership of the underlying bufferevent and
     * just starts echoing things
     */
    /* set a callback to set hooks specifically for the cb_6 callback */

    evhttpx_set_hook(&cb_10->hooks, evhttpx_hook_on_headers, set_max_body, NULL);

    /* set a default request handler */
    evhttpx_set_gencb(htp, test_default_cb, "foobarbaz");

    /* set a callback invoked before a connection is accepted */
    evhttpx_set_pre_accept_cb(htp, test_pre_accept, &bind_port);

    /* set a callback to set per-connection hooks (via a post_accept cb) */
    evhttpx_set_post_accept_cb(htp, set_my_connection_handlers, NULL);

#ifndef evhttpx_DISABLE_SSL
    if (ssl_pem != NULL) {
        evhttpx_ssl_cfg_t scfg = {
            .pemfile            = ssl_pem,
            .privfile           = ssl_pem,
            .cafile             = ssl_ca,
            .capath             = ssl_capath,
            .ciphers            = "RC4+RSA:HIGH:+MEDIUM:+LOW",
            .ssl_opts           = SSL_OP_NO_SSLv2,
            .ssl_ctx_timeout    = 60 * 60 * 48,
            .verify_peer        = SSL_VERIFY_PEER,
            .verify_depth       = 42,
            .x509_verify_cb     = dummy_ssl_verify_callback,
            .x509_chk_issued_cb = dummy_check_issued_cb,
            .scache_type        = evhttpx_ssl_scache_type_internal,
            .scache_size        = 1024,
            .scache_timeout     = 1024,
            .scache_init        = NULL,
            .scache_add         = NULL,
            .scache_get         = NULL,
            .scache_del         = NULL,
        };

        evhttpx_ssl_init(htp, &scfg);
#ifndef EVHTTPX_DISABLE_EVTHR
        if (use_threads) {
            #define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#if defined(OPENSSL_THREADS)
#else
            fprintf(stderr, "Your version of OpenSSL does not support threading!\n");
            exit(-1);
#endif
        }
#endif
    }
#endif

#ifndef EVHTTPX_DISABLE_EVTHR
    if (use_threads) {
        evhttpx_use_threads(htp, NULL, num_threads, NULL);
    }
#endif

    if (evhttpx_bind_socket(htp, bind_addr, bind_port, 128) < 0) {
        fprintf(stderr, "Could not bind socket: %s\n", strerror(errno));
        exit(-1);
    }

    ev_sigint = evsignal_new(evbase, SIGINT, sigint, evbase);
    evsignal_add(ev_sigint, NULL);

    event_base_loop(evbase, 0);

    event_free(ev_sigint);
    evhttpx_unbind_socket(htp);
    evhttpx_callback_free(cb_12);
    evhttpx_callback_free(cb_11);
    evhttpx_callback_free(cb_10);
    evhttpx_callback_free(cb_9);
    evhttpx_callback_free(cb_5);
    evhttpx_callback_free(cb_4);
    evhttpx_callback_free(cb_3);
    evhttpx_callback_free(cb_2);
    evhttpx_callback_free(cb_1);
    evhttpx_free(htp);
    event_base_free(evbase);

    return 0;
} /* main */


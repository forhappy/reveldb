#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "evhttpx.h"

void
testcb(evhttpx_request_t * req, void * a) {
    evbuffer_add_reference(req->buffer_out, "foobar", 6, NULL, NULL);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);
}

int
main(int argc, char ** argv) {
    evbase_t         * evbase = event_base_new();
    evhttpx_t          * evhtp  = evhttpx_new(evbase, NULL);
    evhttpx_t          * v1     = evhttpx_new(evbase, NULL);
    evhttpx_t          * v2     = evhttpx_new(evbase, NULL);
    evhttpx_callback_t * cb_1   = NULL;
    evhttpx_callback_t * cb_2   = NULL;

    cb_1 = evhttpx_set_cb(v1, "/host1", NULL, "host1.com");
    cb_2 = evhttpx_set_cb(v2, "/localhost", testcb, "localhost");

    evhttpx_add_vhost(evhtp, "host1.com", v1);
    evhttpx_add_vhost(evhtp, "localhost", v2);

    evhttpx_add_alias(v2, "127.0.0.1");
    evhttpx_add_alias(v2, "localhost");
    evhttpx_add_alias(v2, "localhost:8081");

#if 0
    scfg1.pemfile  = "./server.pem";
    scfg1.privfile = "./server.pem";
    scfg2.pemfile  = "./server1.pem";
    scfg2.pemfile  = "./server1.pem";

    evhttpx_ssl_init(evhtp, &scfg1);
    evhttpx_ssl_init(v1, &scfg2);
    evhttpx_ssl_init(v2, &scfg2);
#endif

    evhttpx_bind_socket(evhtp, "0.0.0.0", 8081, 1024);

    event_base_loop(evbase, 0);

    evhttpx_unbind_socket(evhtp);
    evhttpx_callback_free(cb_2);
    evhttpx_callback_free(cb_1);
    evhttpx_free(v2);
    evhttpx_free(v1);
    evhttpx_free(evhtp);
    event_base_free(evbase);

    return 0;
}


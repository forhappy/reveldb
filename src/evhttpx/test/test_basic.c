#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "evhttpx.h"

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

void
testcb(evhttpx_request_t * req, void * a) {
    const char * str = "{\"hello\": \"world\"}";
	int method= evhttpx_request_get_method(req);
	evhttpx_uri_t *uri = req->uri;
	evhttpx_query_t *uri_query = uri->query;

	print_kvs(req, uri_query, NULL);
    evbuffer_add_printf(req->buffer_out, "%s\n%d\n", str, method);
    evhttpx_send_reply(req, EVHTTPX_RES_OK);
}

int
main(int argc, char ** argv) {
    evbase_t         * evbase = event_base_new();
    evhttpx_t          * htp    = evhttpx_new(evbase, NULL);
    evhttpx_callback_t * cb   = NULL;

    cb = evhttpx_set_cb(htp, "/metasearch/attr_q", testcb, NULL);
#ifndef EVHTTPX_DISABLE_EVTHR
    evhttpx_use_threads(htp, NULL, 4, NULL);
#endif
    evhttpx_bind_socket(htp, "0.0.0.0", 8081, 1024);

    event_base_loop(evbase, 0);

    evhttpx_unbind_socket(htp);
    evhttpx_callback_free(cb);
    evhttpx_free(htp);
    event_base_free(evbase);

    return 0;
}


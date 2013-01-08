/*
 * =============================================================================
 *
 *       Filename:  rest.h
 *
 *    Description:  reveldb native rest api implementation.
 *
 *        Created:  01/08/2013 09:12:34 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef _REVELDB_REST_H_
#define _REVELDB_REST_H_

#include <reveldb/reveldb.h>

typedef struct reveldb_rest_callbacks_s_ reveldb_rest_callbacks_t;
typedef struct reveldb_rest_s_ reveldb_rest_t;

struct reveldb_rest_callbacks_s_ {

    /* admin operations. */
    evhttpx_callback_t  *rest_new_cb;

    /* set related operations. */
    evhttpx_callback_t  *rest_add_cb;
    evhttpx_callback_t  *rest_set_cb;
    evhttpx_callback_t  *rest_mset_cb;

    /* get related operations. */
    evhttpx_callback_t  *rest_get_cb;
    evhttpx_callback_t  *rest_mget_cb;
    evhttpx_callback_t  *rest_seize_cb;
    evhttpx_callback_t  *rest_mseize_cb;

    evhttpx_callback_t  *rest_cas_cb;
    evhttpx_callback_t  *rest_replace_cb;

    /* delete related operations. */
    evhttpx_callback_t  *rest_del_cb;
    evhttpx_callback_t  *rest_mdel_cb;

    evhttpx_callback_t  *rest_version_cb;
};

struct reveldb_rest_s_ {
    const char *host;
    uint32_t ports[REVELDB_REST_MAX_PORTS_LISTENING_ON];
    uint32_t num_ports;
    int backlog;

    evbase_t *evbase;
    evhttpx_t *httpx;
    evhttpx_ssl_cfg_t *sslcfg;

    reveldb_rest_callbacks_t *callbacks;
    reveldb_config_t *config;
};

extern reveldb_rest_t * reveldb_rest_init(reveldb_config_t *config);

extern void reveldb_rest_run(reveldb_rest_t *rest);

extern void reveldb_rest_stop(reveldb_rest_t *rest);

#endif // _REVELDB_REST_H_

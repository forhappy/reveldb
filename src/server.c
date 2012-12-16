/*
 * =============================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  reveldb main routine.
 *
 *        Created:  12/13/2012 05:04:08 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <leveldb/c.h>

#include <reveldb/reveldb.h>
#include <reveldb/rpc.h>

#include "log.h"

reveldb_log_t *reveldb_log = NULL;
struct rb_root reveldb = RB_ROOT;

int main(int argc, const char *argv[])
{

    LOG_DEBUG(("loading reveldb configuration..."));
    reveldb_config_t *config = reveldb_config_init("./conf/reveldb.json");
    LOG_DEBUG(("loading reveldb configuration done!"));

    LOG_DEBUG(("initializing reveldb log submodule..."));
    reveldb_log = reveldb_log_init(config->log_config->stream,
            config->log_config->level);
    LOG_DEBUG(("initializing reveldb log submodule done!"));

    LOG_DEBUG(("initializing default reveldb storage engine..."));
    reveldb_t * default_db = reveldb_init(config->db_config->dbname);
    reveldb_insert_db(&reveldb, default_db);
    LOG_DEBUG(("initializing default reveldb storage engine done!"));

    LOG_DEBUG(("initializing reveldb rpc server..."));
    reveldb_rpc_t *rpc = reveldb_rpc_init();
    LOG_DEBUG(("initializing reveldb rpc server done!"));

    LOG_DEBUG(("reveldb rpc server is running..."));
    reveldb_rpc_run(rpc);

    LOG_DEBUG(("reveldb rpc server is stopping..."));
    reveldb_rpc_stop(rpc);
    reveldb_log_free(reveldb_log);
    reveldb_config_fini(config);

    LOG_DEBUG(("reveldb rpc server is shutdown!"));
    return 0;
}

/*
 * =============================================================================
 *
 *       Filename:  server.c
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
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include <leveldb/c.h>

#include <reveldb/reveldb.h>
#include <reveldb/rpc.h>

#include "log.h"
#include "utility.h"

struct rb_root dbiter       = RB_ROOT;
struct rb_root dbsnapshot   = RB_ROOT;
struct rb_root dbwritebatch = RB_ROOT;

struct rb_root reveldb = RB_ROOT;

reveldb_log_t *reveldb_log       = NULL;
reveldb_config_t *reveldb_config = NULL;

static void
_server_save_pid(const char *pidfile) {
    FILE *fp = NULL;
    
    if (access(pidfile, F_OK) == 0) {
        if ((fp = fopen(pidfile, "r")) != NULL) {
            char buffer[1024];
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                unsigned int pid;
                if (safe_strtoul(buffer, &pid) && kill((pid_t)pid, 0) == 0) {
                    LOG_WARN(("the pid file contained the following (running) pid: %u\n", pid));
                }
            }
            fclose(fp);
        }
    }

    if ((fp = fopen(pidfile, "w")) == NULL) {
        LOG_ERROR(("could not open the pid file %s for writing", pidfile));
        return;
    }

    fprintf(fp,"%ld\n", (long)getpid());
    if (fclose(fp) == -1) {
        LOG_ERROR(("could not close the pid file %s", pidfile));
    }
}

static void
_server_remove_pidfile(const char *pidfile) {
  if (pidfile == NULL)
      return;

  if (unlink(pidfile) != 0) {
      LOG_ERROR(("could not remove the pid file %s", pidfile));
  }
}

int main(int argc, const char *argv[])
{
    reveldb_config = reveldb_config_init("./conf/reveldb.json");

    _server_save_pid(reveldb_config->server_config->pidfile);

    reveldb_log = reveldb_log_init(reveldb_config->log_config->stream,
            reveldb_config->log_config->level);

    LOG_DEBUG(("initializing default reveldb storage engine..."));
    reveldb_t * default_db = reveldb_init(reveldb_config->db_config->dbname,
            reveldb_config);
    reveldb_insert_db(&reveldb, default_db);
    LOG_DEBUG(("initializing default reveldb storage engine done!"));

    LOG_DEBUG(("initializing reveldb rpc server..."));
    reveldb_rpc_t *rpc = reveldb_rpc_init(reveldb_config);
    LOG_DEBUG(("initializing reveldb rpc server done!"));

    LOG_DEBUG(("reveldb rpc server is running..."));
    reveldb_rpc_run(rpc);

    LOG_DEBUG(("reveldb rpc server is stopping..."));
    reveldb_rpc_stop(rpc);
    _server_remove_pidfile(reveldb_config->server_config->pidfile);
    reveldb_log_free(reveldb_log);
    reveldb_config_fini(reveldb_config);

    LOG_DEBUG(("reveldb rpc server is shutdown!"));
    return 0;
}

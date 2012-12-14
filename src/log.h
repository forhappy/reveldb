/*
 * =============================================================================
 *
 *       Filename:  log.h
 *
 *    Description:  log utility.
 *
 *        Created:  10/20/2012 10:06:52 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#ifndef _REVELDB_LOG_H_
#define _REVELDB_LOG_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reveldb_log_s_ reveldb_log_t;
typedef enum reveldb_log_level_e_ reveldb_log_level_e;

struct reveldb_log_s_ {
    char *level; /* unused right now. */
    FILE *stream;
};

enum reveldb_log_level_e_ {
	// reveldb_log_LEVEL_QUIET = 0,
	REVELDB_LOG_LEVEL_ERROR = 1,
	REVELDB_LOG_LEVEL_WARN  = 2,
	REVELDB_LOG_LEVEL_INFO  = 3,
	REVELDB_LOG_LEVEL_DEBUG = 4
};

extern reveldb_log_level_e log_level;

#define LOGSTREAM reveldb_log_get_stream()

#define LOG_ERROR(x) if (log_level >= REVELDB_LOG_LEVEL_ERROR) \
    reveldb_log_message(REVELDB_LOG_LEVEL_ERROR, __LINE__, __func__, reveldb_log_format_message x)
#define LOG_WARN(x) if(log_level >= REVELDB_LOG_LEVEL_WARN) \
    reveldb_log_message(REVELDB_LOG_LEVEL_WARN, __LINE__, __func__, reveldb_log_format_message x)
#define LOG_INFO(x) if(log_level >= REVELDB_LOG_LEVEL_INFO) \
    reveldb_log_message(REVELDB_LOG_LEVEL_INFO, __LINE__, __func__, reveldb_log_format_message x)
#define LOG_DEBUG(x) if(log_level == REVELDB_LOG_LEVEL_DEBUG) \
    reveldb_log_message(REVELDB_LOG_LEVEL_DEBUG, __LINE__, __func__, reveldb_log_format_message x)

extern void reveldb_log_message(
		reveldb_log_level_e level, int line,
		const char* funcname,
		const char* message);

extern const char * reveldb_log_format_message(const char* format, ...);

extern FILE * reveldb_log_get_stream(void);

extern void reveldb_log_set_stream(FILE *stream);

extern void reveldb_log_set_debug_level(reveldb_log_level_e level);

extern reveldb_log_t* reveldb_log_init(const char *logfile, const char *level);

extern void reveldb_log_free(reveldb_log_t *log);

#ifdef __cplusplus
}
#endif

#endif /* _REVELDB_LOG_H_ */


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

#ifndef _LOG_H_
#define _LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

enum _log_level_e {
	// LOG_LEVEL_QUIET = 0,
	LOG_LEVEL_ERROR = 1,
	LOG_LEVEL_WARN  = 2,
	LOG_LEVEL_INFO  = 3,
	LOG_LEVEL_DEBUG = 4
};

typedef enum _log_level_e log_level_e;

extern log_level_e log_level;

#define LOGSTREAM log_get_stream()

#define LOG_ERROR(x) if (log_level >= LOG_LEVEL_ERROR) \
    log_message(LOG_LEVEL_ERROR, __LINE__, __func__, log_format_message x)
#define LOG_WARN(x) if(log_level >= LOG_LEVEL_WARN) \
    log_message(LOG_LEVEL_WARN, __LINE__, __func__, log_format_message x)
#define LOG_INFO(x) if(log_level >= LOG_LEVEL_INFO) \
    log_message(LOG_LEVEL_INFO, __LINE__, __func__, log_format_message x)
#define LOG_DEBUG(x) if(log_level == LOG_LEVEL_DEBUG) \
    log_message(LOG_LEVEL_DEBUG, __LINE__, __func__, log_format_message x)

extern void log_message(
		log_level_e level, int line,
		const char* funcname,
		const char* message);
extern const char * log_format_message(const char* format, ...);
extern FILE * log_get_stream(void);
extern void log_set_stream(FILE *stream);
extern void log_set_debug_level(log_level_e level);
extern FILE * log_init(const char *log_conf, const char *level);
extern void log_free(FILE *file);

#ifdef __cplusplus
}
#endif

#endif /*LOG_H*/


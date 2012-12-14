/*
 * =============================================================================
 *
 *       Filename:  log.c
 *
 *    Description:  log utility.
 *
 *        Created:  10/20/2012 10:06:58 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

#include "log.h"

#define TIME_NOW_BUFFER_SIZE 1024
#define FORMAT_LOG_BUFFER_SIZE 4096

static pthread_key_t time_now_buffer;
static pthread_key_t format_log_msg_buffer;

void reveldb_log_free_buffer(void* p)
{
    if (p != NULL) free(p);
}

__attribute__((constructor)) static void
reveldb_log_prepare_tsdkeys(void)
{
    pthread_key_create(&time_now_buffer, reveldb_log_free_buffer);
    pthread_key_create(&format_log_msg_buffer, reveldb_log_free_buffer);
}

static char *
reveldb_log_get_tsdata(pthread_key_t key, int size)
{
    char *p = (char *)pthread_getspecific(key);
    if (p == 0) {
        int res;
        p = (char *)calloc(1, size);
        res = pthread_setspecific(key, p);
        if (res != 0) {
            fprintf(stderr, "reveldb_log_get_tsdata() failed to set TSD key: %d", res);
        }
    }
    return p;
}

static char *
reveldb_log_get_time_buffer(void)
{
    return reveldb_log_get_tsdata(time_now_buffer, TIME_NOW_BUFFER_SIZE);
}

static char *
reveldb_log_get_format_buffer(void)
{  
    return reveldb_log_get_tsdata(format_log_msg_buffer, FORMAT_LOG_BUFFER_SIZE);
}

reveldb_log_level_e log_level = REVELDB_LOG_LEVEL_INFO;

static FILE *log_stream = 0;

static const char*
reveldb_log_time_now(char* now_str)
{
    struct timeval tv;
    struct tm lt;
    time_t now = 0;
    size_t len = 0;
    
    gettimeofday(&tv,0);

    now = tv.tv_sec;
    localtime_r(&now, &lt);

    // clone the format used by log4j ISO8601DateFormat
    // specifically: "yyyy-MM-dd HH:mm:ss,SSS"

    len = strftime(now_str, TIME_NOW_BUFFER_SIZE,
                          "%Y-%m-%d %H:%M:%S",
                          &lt);

    len += snprintf(now_str + len,
                    TIME_NOW_BUFFER_SIZE - len,
                    ",%03d",
                    (int)(tv.tv_usec/1000));

    return (const char *)now_str;
}

FILE *
reveldb_log_get_stream(void)
{
    if (log_stream == 0)
        log_stream = stderr;
    return log_stream;
}

void
reveldb_log_set_stream(FILE *stream)
{
    log_stream = stream;
}

void
reveldb_log_message(reveldb_log_level_e level, int line,
		const char* funcname, const char* message)
{
    static const char* level_readable_str[] = {
		"INVALID", "ERROR", "WARN", "INFO", "DEBUG"
	};
    static pid_t pid = 0;
    if(pid == 0) pid = getpid();
    fprintf(LOGSTREAM, "%s:%d(0x%lx):%s@%s@%d: %s\n",
			reveldb_log_time_now(reveldb_log_get_time_buffer()), pid,
            (unsigned long int) pthread_self(),
            level_readable_str[level], funcname, line, message);
	fflush(LOGSTREAM);
}

const char *
reveldb_log_format_message(const char* format, ...)
{
    va_list va;
    char *buf = reveldb_log_get_format_buffer();
    if(buf == NULL)
        return "reveldb_log_format_message(): unable to allocate memory buffer\n";
    
    va_start(va, format);
    vsnprintf(buf, FORMAT_LOG_BUFFER_SIZE - 1, format, va);
    va_end(va); 
    return buf;
}

void
reveldb_log_set_debug_level(reveldb_log_level_e level)
{
    if (level == 0) {
        log_level = (reveldb_log_level_e) 0;
        return;
    }
    if (level < REVELDB_LOG_LEVEL_ERROR) level = REVELDB_LOG_LEVEL_ERROR;
    if (level > REVELDB_LOG_LEVEL_DEBUG) level = REVELDB_LOG_LEVEL_DEBUG;

    log_level = level;
}

reveldb_log_t *
reveldb_log_init(const char *logfile, const char *level)
{
    reveldb_log_t *log = (reveldb_log_t *)malloc(sizeof(reveldb_log_t));
    if (log == NULL) {
        fprintf(stderr, "reveldb_log_init() failed to malloc.\n");
        return NULL;
    }
	FILE *file = fopen(logfile, "w+");
	if (file == NULL) {
		fprintf(stderr, "failed to open log conf.\n");
        free(log);
		return NULL;
	}

    log->stream = file;
	reveldb_log_set_stream(file);

	if (strcmp(level, "DEBUG") == 0) {
		reveldb_log_set_debug_level(REVELDB_LOG_LEVEL_DEBUG);
	} else if (strcmp(level, "INFO")) {
		reveldb_log_set_debug_level(REVELDB_LOG_LEVEL_INFO);
	} else if (strcmp(level, "WARN")) {
		reveldb_log_set_debug_level(REVELDB_LOG_LEVEL_WARN);
	} else if (strcmp(level, "ERROR")) {
		reveldb_log_set_debug_level(REVELDB_LOG_LEVEL_ERROR);
	} else {
		reveldb_log_set_debug_level(REVELDB_LOG_LEVEL_DEBUG);
	}

	return log;
}

void
reveldb_log_free(reveldb_log_t *log)
{
    assert(log != NULL);
    if (log->stream != NULL) {
        fclose(log->stream);
        log->stream = NULL;
    }

    free(log);
}

#if defined(_LOG_TEST_)

void * dummy(void *arg)
{
	LOG_DEBUG(("hello world, This is debug level.\n"));
	LOG_INFO(("hello world, This is info level.\n"));
	LOG_WARN(("hello world, This is warn level.\n"));
	LOG_ERROR(("hello world, This is error level.\n"));

}

int main(int argc, const char *argv[])
{
	pthread_t thread;
    reveldb_log_t *log = reveldb_log_init("reveldb.log", "DEBUG");
#if 0
	FILE *file = fopen("reveldb.log", "w+");
	reveldb_log_set_stream(file);
	reveldb_log_set_debug_level(REVELDB_LOG_LEVEL_DEBUG);
#endif
	LOG_DEBUG(("hello world, This is debug level."));
	LOG_INFO(("hello world, This is info level."));
	LOG_WARN(("hello world, This is warn level."));
	LOG_ERROR(("hello world, This is error level."));
	pthread_create(&thread, NULL, dummy, NULL);
	pthread_join(thread, NULL);
    reveldb_log_free(log);
#if 0
	fclose(file);
#endif
}

#endif

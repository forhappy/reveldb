
/*
 * =============================================================================
 *
 *       Filename:  utility.h
 *
 *    Description:  utility routines, copied from memcahced.
 *
 *        Created:  12/16/2012 11:58:42 AM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#ifndef _REVELDB_UTILITY_H_
#define _REVELDB_UTILITY_H_

#include <stdbool.h>

/*
 * Wrappers around strtoull/strtoll that are safer and easier to
 * use.  For tests and assumptions, see internal_tests.c.
 *
 * str   a NULL-terminated base decimal 10 unsigned integer
 * out   out parameter, if conversion succeeded
 *
 * returns true if conversion succeeded.
 */
bool safe_strtoull(const char *str, uint64_t * out);
bool safe_strtoll(const char *str, int64_t * out);
bool safe_strtoul(const char *str, uint32_t * out);
bool safe_strtol(const char *str, int32_t * out);

/*
 * Get GMT formatted time.
 */
char * gmttime_now(void);

#endif // _REVELDB_UTILITY_H_

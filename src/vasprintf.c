/*
 * =============================================================================
 *
 *       Filename:  vasprintf.c
 *
 *    Description:  vasprintf implementation.
 *
 *        Created:  12/16/2012 03:43:02 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include "vasprintf.h"

int reveldb_vasprintf(char **strp, const char *fmt, va_list ap)
{
    va_list args;
    int len;
    va_copy(args, ap);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *str = malloc(len + 1);
    if (str != NULL) {
        int len2 __attribute__((unused));
        va_copy(args, ap);
        len2 = vsprintf(str, fmt, args);
        assert(len2 == len);
        va_end(args);
    } else {
        len = -1;
    }
    *strp = str;
    return len;
}



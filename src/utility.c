
/*
 * =============================================================================
 *
 *       Filename:  utility.c
 *
 *    Description:  utility routines.
 *
 *        Created:  12/16/2012 11:58:51 AM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "utility.h"

bool
safe_strtoull(const char *str, uint64_t * out)
{
    assert(out != NULL);
    errno = 0;
    *out = 0;
    char *endptr;
    unsigned long long ull = strtoull(str, &endptr, 10);

    if ((errno == ERANGE) || (str == endptr)) {
        return false;
    }

    if (isspace(*endptr) || (*endptr == '\0' && endptr != str)) {
        if ((long long) ull < 0) {
            /* only check for negative signs in the uncommon case when
             * the unsigned number is so big that it's negative as a
             * signed number. 
             */
            if (strchr(str, '-') != NULL) {
                return false;
            }
        }
        *out = ull;
        return true;
    }
    return false;
}

bool
safe_strtoll(const char *str, int64_t * out)
{
    assert(out != NULL);
    errno = 0;
    *out = 0;
    char *endptr;
    long long ll = strtoll(str, &endptr, 10);

    if ((errno == ERANGE) || (str == endptr)) {
        return false;
    }

    if (isspace(*endptr) || (*endptr == '\0' && endptr != str)) {
        *out = ll;
        return true;
    }
    return false;
}

bool
safe_strntoll(const char *str, size_t len, int64_t * out)
{
    assert(out != NULL);
    char buf[64] = {0}; /* should be enough. */
    char *pstr = buf;
    errno = 0;
    *out = 0;
    char *endptr;
    snprintf(buf, len + 1, "%s", str);
    long long ll = strtoll(pstr, &endptr, 10);

    if ((errno == ERANGE) || (pstr == endptr)) {
        return false;
    }

    if (isspace(*endptr) || (*endptr == '\0' && endptr != pstr)) {
        *out = ll;
        return true;
    }
    return false;
}

bool
safe_strtoul(const char *str, uint32_t * out)
{
    char *endptr = NULL;
    unsigned long l = 0;

    assert(out);
    assert(str);
    *out = 0;
    errno = 0;

    l = strtoul(str, &endptr, 10);
    if ((errno == ERANGE) || (str == endptr)) {
        return false;
    }

    if (isspace(*endptr) || (*endptr == '\0' && endptr != str)) {
        if ((long) l < 0) {
            /* only check for negative signs in the uncommon case when
             * the unsigned number is so big that it's negative as a
             * signed number. 
             */
            if (strchr(str, '-') != NULL) {
                return false;
            }
        }
        *out = l;
        return true;
    }

    return false;
}

bool
safe_strtol(const char *str, int32_t * out)
{
    assert(out != NULL);
    errno = 0;
    *out = 0;
    char *endptr;
    long l = strtol(str, &endptr, 10);

    if ((errno == ERANGE) || (str == endptr)) {
        return false;
    }

    if (isspace(*endptr) || (*endptr == '\0' && endptr != str)) {
        *out = l;
        return true;
    }
    return false;
}

char *
safe_urldecode(const char *url)
{
	int s = 0, d = 0, url_len = 0;
	char c;
	char *dest = NULL;

	if (!url)
		return NULL;

	url_len = strlen(url) + 1;
	dest = (char *) malloc(sizeof(char) * url_len);

	if (!dest)
		return NULL;

	while (s < url_len) {
		c = url[s++];

		if (c == '%' && s + 2 < url_len) {
			char c2 = url[s++];
			char c3 = url[s++];

			if (isxdigit(c2) && isxdigit(c3)) {
				c2 = tolower(c2);
				c3 = tolower(c3);

				if (c2 <= '9')
					c2 = c2 - '0';
				else
					c2 = c2 - 'a' + 10;

				if (c3 <= '9')
					c3 = c3 - '0';
				else
					c3 = c3 - 'a' + 10;

				dest[d++] = 16 * c2 + c3;

			} else {			/* %zz or something other invalid */
				dest[d++] = c;
				dest[d++] = c2;
				dest[d++] = c3;
			}
		} else if (c == '+') {
			dest[d++] = ' ';
		} else {
			dest[d++] = c;
		}

	}

	return dest;
}

char *
gmttime_now()
{
	time_t now;
	struct tm *gmt;
	char *time_val;

	time(&now);
	gmt = gmtime(&now);

	time_val = (char *)malloc(sizeof(char) * 64);
	memset(time_val, '\0', 64);

	strftime(time_val, 64, "%a, %d %b %Y %H:%M:%S GMT", gmt);
	return time_val;
}

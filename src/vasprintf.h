/*
 * =============================================================================
 *
 *       Filename:  vasprintf.h
 *
 *    Description:  vasprintf implementation.
 *
 *        Created:  12/16/2012 03:44:35 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef _REVELDB_VASPRINTF_H_
#define _REVELDB_VASPRINTF_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

int reveldb_vasprintf(char **strp, const char *fmt, va_list ap);

#endif // _REVELDB_VASPRINTF_H_

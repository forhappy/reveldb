/*
 * =============================================================================
 *
 *       Filename:  tstring.h
 *
 *    Description:  tiny string utility implementation.
 *
 *        Version:  0.0.1
 *        Created:  01/26/2012 01:57:03 PM
 *       Revision:  r1
 *       Compiler:  gcc (Ubuntu/Linaro 4.4.4-14ubuntu5) 4.4.5
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef _TSTRING_H_
#define _TSTRING_H_

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define min(x, y) ({\
        __typeof(x) _min1 = (x);\
        __typeof(y) _min2 = (y);\
        (void) (&_min1 == &_min2);\
        _min1 < _min2 ? _min1 : _min2;})

#define max(x, y) ({\
        __typeof(x) _max1 = (x);\
        __typeof(y) _max2 = (y);\
        (void) (&_max1 == &_max2);\
        _max1 > _max2 ? _max1 : _max2;})

typedef struct _tstring_t tstring_t;

struct _tstring_t {
    char *str;
    unsigned int len;
    unsigned int allocated_len;
};

extern tstring_t * tstring_new(const char *init);
extern tstring_t * tstring_new_len(const char *init, int32_t len);
extern tstring_t * tstring_sized_new(uint32_t default_size);
extern void tstring_free(tstring_t *str);
extern int tstring_equal(const tstring_t *str1, const tstring_t *str2);
extern uint32_t tstring_hash(const tstring_t *str);
extern tstring_t * tstring_assign(tstring_t *str, const char *rval);
extern tstring_t * tstring_truncate(tstring_t *str, int32_t len);    
extern tstring_t * tstring_set_size(tstring_t *str, int32_t len);
extern tstring_t * tstring_insert_len(tstring_t *str, int32_t pos, const char *val, int32_t len);  
extern tstring_t * tstring_append(tstring_t *str, const char *val);
extern tstring_t * tstring_append_len(tstring_t *str, const char *val, int32_t len);  
extern tstring_t * tstring_append_c(tstring_t *str, char c);
extern tstring_t * tstring_append_unichar(tstring_t *str, unsigned int wc);
extern tstring_t * tstring_prepend(tstring_t *str, const char *val);
extern tstring_t * tstring_prepend_c(tstring_t *str, char c);
extern tstring_t * tstring_prepend_unichar(tstring_t *str, unsigned int wc);
extern tstring_t * tstring_prepend_len(tstring_t *str, const char *val, int32_t len);  
extern tstring_t * tstring_insert(tstring_t *str, int32_t pos, const char *val);
extern tstring_t * tstring_insert_c(tstring_t *str, int32_t pos, const char c);
extern tstring_t * tstring_insert_unichar(tstring_t *str, int32_t pos, unsigned int wc);
extern tstring_t * tstring_overwrite(tstring_t *str, int32_t pos, const char *val);
extern tstring_t * tstring_overwrite_len(tstring_t *str, int32_t pos, const char *val, int32_t len);
extern tstring_t * tstring_erase(tstring_t *str, int32_t pos, int32_t len);
extern tstring_t * tstring_ascii_down(tstring_t *str);
extern tstring_t * tstring_ascii_up(tstring_t *str);
extern void tstring_vprintf(tstring_t *str, const char *format, va_list args);
extern void tstring_printf(tstring_t *str, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern void tstring_append_vprintf(tstring_t *str, const char *format, va_list args);
extern void tstring_append_printf(tstring_t *str, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern tstring_t * tstring_down(tstring_t *str);
extern tstring_t * tstring_up(tstring_t *str);

/* optimize tstring_append_c */
static inline tstring_t *
tstring_append_c_inline(tstring_t *str, char c)
{
  if (str->len + 1 < str->allocated_len) {
      str->str[str->len++] = c;
      str->str[str->len] = 0;
  } else
    tstring_insert_c(str, -1, c);
  return str;
}
#define tstring_append_c_optimized(str,c)     tstring_append_c_inline(str, c)

static inline uint32_t tstring_size(const tstring_t *str)
{
    assert(str != NULL);

    return str->len;
}

static inline char * tstring_data(const tstring_t *str)
{
    assert(str != NULL);

    return str->str;
}

#ifdef __cplusplus
}
#endif

#endif // _TSTRING_H_

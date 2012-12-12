/*
 * =============================================================================
 *
 *       Filename:  tstring.c
 *
 *    Description:  tiny string utility implementation.
 *
 *        Version:  0.0.1
 *        Created:  01/26/2012 03:29:54 PM
 *       Revision:  r1
 *       Compiler:  gcc (Ubuntu/Linaro 4.4.4-14ubuntu5) 4.4.5
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "tstring.h"

#ifdef HAVE_STDBOOL_H
  #include <stdbool.h>
  #define TRUE true
  #define FALSE false
  typedef   bool bool_t;
#else
  typedef int bool_t;
  #define TRUE  (1)
  #define FALSE (!TRUE)
#endif

/*
 * _tstring_equal:
 * @v1: a key
 * @v2: a key to compare with @v1
 *
 * Compares two strings for byte-by-byte equality and returns %TRUE
 * if they are equal. 
 *
 * Returns: %TRUE if the two keys match
 */

static int 
_tstring_equal(const void *v1, const void *v2)
{
    const char *str1 = v1;
    const char *str2 = v2;
    return strcmp(str1, str2) == 0;
}

/*
 * _strcmp:
 * @str1: a C string or %NULL
 * @str2: another C string or %NULL
 *
 * Compares @str1 and @str2 like strcmp(). Handles %NULL 
 * gracefully by sorting it before non-%NULL strings.
 * Comparing two %NULL pointers returns 0.
 *
 * Returns: -1, 0 or 1, if @str1 is <, == or > than @str2.
 */
static int
_tstring_strcmp (const char *str1, const char *str2)
{
    if (!str1)
        return -(str1 != str2);
    if (!str2)
        return str1 != str2;
    return strcmp(str1, str2);
}

#define MY_MAXSIZE ((unsigned int)-1)

static unsigned int 
_nearest_power(unsigned int base, unsigned int num)    
{
  if (num > MY_MAXSIZE / 2) {
      return MY_MAXSIZE;
  } else {
      unsigned int n = base;
      while (n < num)
          n <<= 1;
      return n;
  }
}

static void
_tstring_maybe_expand(tstring_t *str, int32_t len) 
{
    assert(len > 0);
    assert(str != NULL);
    if (str->len + len >= str->allocated_len) {
        str->allocated_len = _nearest_power(1, str->len + len + 1);
        str->str = realloc(str->str, str->allocated_len);
        assert(str->str != NULL);
    }
}

tstring_t *
tstring_sized_new(unsigned int default_size)
{
    tstring_t *str = (tstring_t *)malloc(sizeof(tstring_t) * 1);
    assert(str != NULL);
    str->allocated_len = 0;
    str->len = 0;
    str->str = NULL;
    _tstring_maybe_expand(str, max(default_size, (unsigned int)2));
    str->str[0] = 0;
    return str;
}

tstring_t *
tstring_new(const char *init)
{
    tstring_t *str;
    if (init == NULL || *init == '\0')
        str = tstring_sized_new(2);
    else {
      int len = strlen(init);
      str = tstring_sized_new(len + 2);

      tstring_append_len(str, init, len);
    }
  return str;
}

/*
 * tstring_new_len:
 * @init: initial contents of the string
 * @len: length of @init to use
 *
 * Creates a new #tstring_t with @len bytes of the @init buffer.  
 * Because a length is provided, @init need not be nul-terminated,
 * and can contain embedded null bytes.
 *
 * Since this function does not stop at null bytes, it is the caller's
 * responsibility to ensure that @init has at least @len addressable 
 * bytes.
 *
 * Returns: a new #tstring_t
 */
tstring_t *
tstring_new_len(const char *init, int32_t len) 
{
    tstring_t *str;
    if (len < 0) 
      return tstring_new(init);
    else {
        str = tstring_sized_new(len);
        if (init != NULL)
            tstring_append_len(str, init, len);
        return str;
    }
}

/*
 * tstring_free:
 * @str: a #tstring_t
 * @free_segment: if %TRUE the actual character data is freed as well
 *
 * Frees the memory allocated for the #tstring_t.
 * If @free_segment is %TRUE it also frees the character data.
 *
 * Returns: the character data of @str 
 *          (i.e. %NULL if @free_segment is %TRUE)
 */
void
tstring_free(tstring_t *str)
{
    if (str != NULL) {
        if (str->str != NULL) {
            free(str->str);
            str->str = NULL;
        }
        free(str);
        str = NULL;
    }
}

/*
 * tstring_equal:
 * @v: a #tstring_t
 * @v2: another #tstring_t
 *
 * Compares two strings for equality, returning %TRUE if they are equal. 
 *
 * Returns: %TRUE if they strings are the same length and contain the 
 *   same bytes
 */
int
tstring_equal(const tstring_t *v, const tstring_t *v2)
{
    char *p, *q;
    tstring_t *str1 = (tstring_t *)v;
    tstring_t *str2 = (tstring_t *)v2;
    unsigned int i = str1->len;
    
    if (i != str2->len)
        return FALSE;
    
    p = str1->str;
    q = str2->str;
    while (i) {
        if (*p != *q)
            return FALSE;
        p++;
        q++;
        i--;
    }
    return TRUE;
}

/*
 * tstring_hash:
 * @str: a string to hash
 *
 * Creates a hash code for @str; 
 *
 * Returns: hash code for @str
 */
/* 31 bit hash function */
unsigned int
tstring_hash(const tstring_t *str)
{
    const char *p = str->str;
    unsigned int n = str->len;
    unsigned int h = 0;
    while (n--) {
        h = (h << 5) - h + *p;
        p++;
    }
    return h;
}

/**
 * tstring_assign:
 * @str: the destination #tstring_t. Its current contents 
 *          are destroyed.
 * @rval: the string to copy into @str
 *
 * Copies the bytes from a string into a #tstring_t, 
 * destroying any previous contents. It is rather like 
 * the standard strcpy() function, except that you do not 
 * have to worry about having enough space to copy the string.
 *
 * Returns: @str
 */
tstring_t *
tstring_assign(tstring_t *str, const char *rval)
{
    assert(rval != NULL);
    assert(str != NULL);
    /* Make sure assigning to itself doesn't corrupt the string.  */
    if (str->str != rval) {
      tstring_truncate(str, 0);
      tstring_append(str, rval);
    }
    return str;
}

/*
 * tstring_truncate:
 * @str: a #tstring_t
 * @len: the new size of @str
 *
 * Cuts off the end of the tstring_t, leaving the first @len bytes. 
 *
 * Returns: @str
 */
tstring_t *
tstring_truncate(tstring_t *str, int32_t len)    
{
    assert(str != NULL);
    str->len = min(len, (int32_t)(str->len));
    str->str[str->len] = 0;
    return str;
}

/*
 * tstring_set_size:
 * @str: a #tstring_t
 * @len: the new length
 * 
 * Sets the length of a #tstring_t. If the length is less than
 * the current length, the string will be truncated. If the
 * length is greater than the current length, the contents
 * of the newly added area are undefined. (However, as
 * always, str->str[str->len] will be a null byte.) 
 * 
 * Return value: @str
 **/
tstring_t *
tstring_set_size(tstring_t *str, int32_t len)    
{
    assert(str != NULL);
    assert(len >= 0);
    if (len >= str->allocated_len)
        _tstring_maybe_expand(str, len - str->len);
    
    str->len = len;
    str->str[len] = 0;
    return str;
}

/**
 * tstring_insert_len:
 * @str: a #tstring_t
 * @pos: position in @str where insertion should
 *       happen, or -1 for at the end
 * @val: bytes to insert
 * @len: number of bytes of @val to insert
 *
 * Inserts @len bytes of @val into @str at @pos.
 * Because @len is provided, @val may contain embedded
 * nuls and need not be nul-terminated. If @pos is -1,
 * bytes are inserted at the end of the string.
 *
 * Since this function does not stop at nul bytes, it is
 * the caller's responsibility to ensure that @val has at
 * least @len addressable bytes.
 *
 * Returns: @str
 */
tstring_t *
tstring_insert_len(tstring_t *str, int32_t pos, const char *val, int32_t len)
{
    assert(str != NULL);
    assert(val != NULL);
    if (len == 0)
        return str;
    if (len < 0)
        len = strlen(val);
    if (pos < 0)
        pos = str->len;
    else
        assert(pos <= str->len);
    /* Check whether val represents a substring of string.  This test
     * probably violates chapter and verse of the C standards, since
     * ">=" and "<=" are only valid when val really is a substring.
     * In practice, it will work on modern archs.  
     */
    if (val >= str->str && val <= str->str + str->len) {
        unsigned int offset = val - str->str;
        unsigned int precount = 0;
        _tstring_maybe_expand (str, len);
        val = str->str + offset;
        /* At this point, val is valid again.  */
        /* Open up space where we are going to insert.  */
        if (pos < str->len)
        memmove(str->str + pos + len, str->str + pos, str->len - pos);
        /* Move the source part before the gap, if any.  */
        if (offset < pos) {
            precount = min((unsigned int)len, pos - offset);
            memcpy(str->str + pos, val, precount);
        }
        /* Move the source part after the gap, if any.  */
        if (len > precount)
            memcpy(str->str + pos + precount,
                    val + /* Already moved: */ precount + /* Space opened up: */ len,
                    len - precount);
    } else {
        _tstring_maybe_expand (str, len);
        /* If we aren't appending at the end, move a hunk
         * of the old string to the end, opening up space
         */
        if (pos < str->len)
            memmove(str->str + pos + len, str->str + pos, str->len - pos);
        /* insert the new string */
        if (len == 1)
            str->str[pos] = *val;
        else
            memcpy(str->str + pos, val, len);
    }
    str->len += len;
    str->str[str->len] = 0;
    return str;
}

#define SUB_DELIM_CHARS  "!$&'()*+,;="

__attribute__((unused)) static int
is_valid(char c, const char *reserved_chars_allowed)
{
    if (isalnum (c) ||
            c == '-' ||
            c == '.' ||
            c == '_' ||
            c == '~')
        return TRUE;
    if (reserved_chars_allowed &&
            strchr (reserved_chars_allowed, c) != NULL)
        return TRUE;
    return FALSE;
}

__attribute__((unused)) static int
unichar_ok(unsigned int c)
{
  return
      (c != (unsigned char) -2) &&
      (c != (unsigned char) -1);
}

/**
 * tstring_append:
 * @str: a #tstring_t
 * @val: the string to append onto the end of @str
 * 
 * Adds a string onto the end of a #tstring_t, expanding 
 * it if necessary.
 *
 * Returns: @str
 */
tstring_t *
tstring_append(tstring_t *str, const char *val)
{
    assert(str != NULL);
    assert(val != NULL);
    return tstring_insert_len(str, -1, val, -1);
}

/**
 * tstring_append_len:
 * @str: a #tstring_t
 * @val: bytes to append
 * @len: number of bytes of @val to use
 * 
 * Appends @len bytes of @val to @str. Because @len is 
 * provided, @val may contain embedded nuls and need not 
 * be nul-terminated.
 * 
 * Since this function does not stop at nul bytes, it is 
 * the caller's responsibility to ensure that @val has at 
 * least @len addressable bytes.
 *
 * Returns: @str
 */
tstring_t *
tstring_append_len(tstring_t *str, const char *val, int32_t len)    
{
    assert(str != NULL);
    assert(val != NULL);
    return tstring_insert_len(str, -1, val, len);
}

/**
 * tstring_append_c:
 * @str: a #tstring_t
 * @c: the byte to append onto the end of @str
 *
 * Adds a byte onto the end of a #tstring_t, expanding 
 * it if necessary.
 * 
 * Returns: @str
 */
tstring_t *
tstring_append_c(tstring_t *str, char c)
{
    assert(str != NULL);
    return tstring_insert_c(str, -1, c);
}

/**
 * tstring_append_unichar:
 * @str: a #tstring_t
 * @wc: a Unicode character
 * 
 * Converts a Unicode character into UTF-8, and appends it
 * to the string.
 * 
 * Return value: @str
 **/
tstring_t *
tstring_append_unichar(tstring_t  *str, unsigned int wc)
{
    assert(str != NULL);
    return tstring_insert_unichar(str, -1, wc);
}

/**
 * tstring_prepend:
 * @str: a #tstring_t
 * @val: the string to prepend on the start of @str
 *
 * Adds a string on to the start of a #tstring_t, 
 * expanding it if necessary.
 *
 * Returns: @str
 */
tstring_t *
tstring_prepend(tstring_t *str, const char *val)
{
    assert(str != NULL);
    assert(val != NULL);
    return tstring_insert_len(str, 0, val, -1);
}

/**
 * tstring_prepend_len:
 * @str: a #tstring_t
 * @val: bytes to prepend
 * @len: number of bytes in @val to prepend
 *
 * Prepends @len bytes of @val to @str. 
 * Because @len is provided, @val may contain 
 * embedded nuls and need not be nul-terminated.
 *
 * Since this function does not stop at nul bytes, 
 * it is the caller's responsibility to ensure that 
 * @val has at least @len addressable bytes.
 *
 * Returns: @str
 */
tstring_t *
tstring_prepend_len(tstring_t *str, const char *val, int32_t len)    
{
    assert(str != NULL);
    assert(val != NULL);
    return tstring_insert_len(str, 0, val, len);
}

/**
 * tstring_prepend_c:
 * @str: a #tstring_t
 * @c: the byte to prepend on the start of the #tstring_t
 *
 * Adds a byte onto the start of a #tstring_t, 
 * expanding it if necessary.
 *
 * Returns: @str
 */
tstring_t *
tstring_prepend_c(tstring_t *str, char c)
{  
    assert(str != NULL);
    return tstring_insert_c(str, 0, c);
}

/**
 * tstring_prepend_unichar:
 * @str: a #tstring_t
 * @wc: a Unicode character
 * 
 * Converts a Unicode character into UTF-8, and prepends it
 * to the string.
 * 
 * Return value: @str
 **/
tstring_t *
tstring_prepend_unichar(tstring_t *str, unsigned int wc)
{ 
    assert(str != NULL);
    return tstring_insert_unichar(str, 0, wc);
}

/**
 * tstring_insert:
 * @str: a #tstring_t
 * @pos: the position to insert the copy of the string
 * @val: the string to insert
 *
 * Inserts a copy of a string into a #tstring_t, 
 * expanding it if necessary.
 *
 * Returns: @str
 */
tstring_t*
tstring_insert(tstring_t *str, int32_t pos, const char *val)
{
    assert(str != NULL);
    assert(pos >= 0);
    assert(val != NULL);
    if (pos >= 0)
        assert(pos <= str->len);
    return tstring_insert_len(str, pos, val, -1);
}

/**
 * tstring_insert_c:
 * @str: a #tstring_t
 * @pos: the position to insert the byte
 * @c: the byte to insert
 *
 * Inserts a byte into a #tstring_t, expanding it if necessary.
 * 
 * Returns: @str
 */
tstring_t *
tstring_insert_c(tstring_t *str, int32_t pos, const char c)
{
    assert(str != NULL);
    _tstring_maybe_expand (str, 1);
    if (pos < 0)
        pos = str->len;
    else
        assert(pos <= str->len);
    /* If not just an append, move the old stuff */
    if (pos < str->len)
        memmove(str->str + pos + 1, str->str + pos, str->len - pos);
    str->str[pos] = c;
    str->len += 1;
    
    str->str[str->len] = 0;
    return str;
}

/**
 * tstring_insert_unichar:
 * @str: a #tstring_t
 * @pos: the position at which to insert character, or -1 to
 *       append at the end of the string
 * @wc: a Unicode character
 * 
 * Converts a Unicode character into UTF-8, and insert it
 * into the string at the given position.
 * 
 * Return value: @str
 **/
tstring_t *
tstring_insert_unichar(tstring_t *str, int32_t pos, unsigned int wc)
{
    int charlen, first, i;
    char *dest;
    
    assert(str != NULL);
    /* Code copied from g_unichar_to_utf() */
    if (wc < 0x80) {
        first = 0;
        charlen = 1;
    } else if (wc < 0x800) {
        first = 0xc0;
        charlen = 2;
    } else if (wc < 0x10000) {
        first = 0xe0;
        charlen = 3;
    } else if (wc < 0x200000) {
        first = 0xf0;
        charlen = 4;
    } else if (wc < 0x4000000) {
        first = 0xf8;
        charlen = 5;
    } else {
        first = 0xfc;
        charlen = 6;
    }
    /* End of copied code */
    _tstring_maybe_expand(str, charlen);
    if (pos < 0)
        pos = str->len;
    else
        assert(pos <= str->len);
    /* If not just an append, move the old stuff */
    if (pos < str->len)
        memmove(str->str + pos + charlen, str->str + pos, str->len - pos);
    dest = str->str + pos;
    for (i = charlen - 1; i > 0; --i) {
        dest[i] = (wc & 0x3f) | 0x80;
        wc >>= 6;
    }
    dest[0] = wc | first;
    str->len += charlen;
    
    str->str[str->len] = 0;
    return str;
}

/**
 * tstring_overwrite:
 * @str: a #tstring_t
 * @pos: the position at which to start overwriting
 * @val: the string that will overwrite the @string starting at @pos
 * 
 * Overwrites part of a string, lengthening it if necessary.
 * 
 * Return value: @str
 *
 **/
tstring_t *
tstring_overwrite(tstring_t *str, int32_t pos, const char *val)
{
    assert(val != NULL);
    return tstring_overwrite_len(str, pos, val, strlen(val));
}

/*
 * tstring_overwrite_len:
 * @str: a #tstring_t
 * @pos: the position at which to start overwriting
 * @val: the str that will overwrite the @str starting at @pos
 * @len: the number of bytes to write from @val
 * 
 * Overwrites part of a string, lengthening it if necessary. 
 * This function will work with embedded nuls.
 * 
 * Return value: @str
 *
 */
tstring_t *
tstring_overwrite_len(tstring_t *str, int32_t pos, const char *val, int32_t len)
{
    int end;
    assert(str != NULL);
    if (!len) return str;
    assert(val != NULL);
    assert(pos <= str->len);
    if (len < 0)
        len = strlen(val);
    end = pos + len;
    if (end > str->len)
        _tstring_maybe_expand(str, end - str->len);
    memcpy(str->str + pos, val, len);
    if (end > str->len) {
        str->str[end] = '\0';
        str->len = end;
    }
    return str;
}

/**
 * tstring_erase:
 * @str: a #tstring_t
 * @pos: the position of the content to remove
 * @len: the number of bytes to remove, or -1 to remove all
 *       following bytes
 *
 * Removes @len bytes from a #tstring_t, starting at position @pos.
 * The rest of the #tstring_t is shifted down to fill the gap.
 *
 * Returns: @str
 */
tstring_t *
tstring_erase(tstring_t *str, int32_t pos, int32_t len)
{
    assert(str != NULL);
    assert(pos >= 0);
    assert(pos <= str->len);
    if (len < 0)
        len = str->len - pos;
    else {
        assert(pos + len <= str->len);
        if (pos + len < str->len)
            memmove(str->str + pos, str->str + pos + len, str->len - (pos + len));
    }
    str->len -= len;
    
    str->str[str->len] = 0;
    
    return str;
}

/**
 * tstring_ascii_down:
 * @str: a tstring_t
 * 
 * Converts all upper case ASCII letters to lower case ASCII letters.
 * 
 * Return value: passed-in @str pointer, with all the upper case
 *               characters converted to lower case in place, with
 *               semantics that exactly match g_ascii_tolower().
 **/
tstring_t *
tstring_ascii_down(tstring_t *str)
{
    char *s;
    int n;
    assert(str != NULL);
    n = str->len;
    s = str->str;
    while (n) {
        *s = tolower(*s);
        s++;
        n--;
    }
    return str;
}

/**
 * tstring_ascii_up:
 * @str: a tstring_t
 * 
 * Converts all lower case ASCII letters to upper case ASCII letters.
 * 
 * Return value: passed-in @str pointer, with all the lower case
 *               characters converted to upper case in place, with
 *               semantics that exactly match toupper().
 **/
tstring_t*
tstring_ascii_up(tstring_t *str)
{
    char *s;
    int n;
    assert(str != NULL);
    n = str->len;
    s = str->str;
    while (n) {
        *s = toupper(*s);
        s++;
        n--;
    }
    return str;
}

/**
 * tstring_down:
 * @str: a #tstring_t
 *  
 * Converts a #tstring_t to lowercase.
 *
 * Returns: the #tstring_t.
 *
 * Deprecated:2.2: This function uses the locale-specific 
 *   tolower() function, which is almost never the right thing. 
 *   Use tstring_ascii_down() or g_utf8_strdown() instead.
 */
tstring_t *
tstring_down(tstring_t *str)
{
    unsigned char *s;
    long n;
    assert(str != NULL);
    n = str->len;
    s = (unsigned char *)str->str;
    while (n) {
        if (isupper(*s))
            *s = tolower(*s);
        s++;
        n--;
    }
    return str;
}

/**
 * tstring_up:
 * @str: a #tstring_t 
 * 
 * Converts a #tstring_t to uppercase.
 * 
 * Return value: @str
 *
 * Deprecated:2.2: This function uses the locale-specific 
 *   toupper() function, which is almost never the right thing. 
 *   Use tstring_ascii_up() or g_utf8_strup() instead.
 **/
tstring_t *
tstring_up(tstring_t *str)
{
    unsigned char *s;
    long n;
    assert(str != NULL);
    n = str->len;
    s = (unsigned char *)str->str;
    while (n) {
        if (islower (*s))
            *s = toupper(*s);
        s++;
        n--;
    }
    
    return str;
}

/**
 * tstring_append_vprintf:
 * @str: a #tstring_t
 * @format: the str format. See the printf() documentation
 * @args: the list of arguments to insert in the output
 *
 * Appends a formatted string onto the end of a #tstring_t.
 * This function is similar to tstring_append_printf()
 * except that the arguments to the format string are passed
 * as a va_list.
 *
 */
void
tstring_append_vprintf(tstring_t *str, const char *format, va_list args)
{
    char *buf;
    int len;
    assert(str != NULL);
    assert(format != NULL);
    len = vasprintf(&buf, format, args);
    if (len >= 0) {
        _tstring_maybe_expand(str, len);
        memcpy(str->str + str->len, buf, len + 1);
        str->len += len;
        free(buf);
    }
}

/**
 * tstring_vprintf:
 * @str: a #tstring_t
 * @format: the string format. See the printf() documentation
 * @args: the parameters to insert into the format string
 *
 * Writes a formatted string into a #tstring_t. 
 * This function is similar to tstring_printf() except that 
 * the arguments to the format string are passed as a va_list.
 *
 */
void
tstring_vprintf(tstring_t *str, const char *format, va_list args)
{
    tstring_truncate(str, 0);
    tstring_append_vprintf(str, format, args);
}

/**
 * tstring_printf:
 * @str: a #tstring_t
 * @format: the string format. See the printf() documentation
 * @Varargs: the parameters to insert into the format string
 *
 * Writes a formatted string into a #tstring_t.
 * This is similar to the standard sprintf() function,
 * except that the #tstring_t buffer automatically expands 
 * to contain the results. The previous contents of the 
 * #tstring_t are destroyed.
 */
void
tstring_printf(tstring_t *str, const char *format, ...)
{
    va_list args;
    tstring_truncate(str, 0);
    va_start(args, format);
    tstring_append_vprintf(str, format, args);
    va_end(args);
}

/**
 * tstring_append_printf:
 * @str: a #tstring_t
 * @format: the string format. See the printf() documentation
 * @Varargs: the parameters to insert into the format string
 *
 * Appends a formatted string onto the end of a #tstring_t.
 * This function is similar to tstring_printf() except 
 * that the text is appended to the #tstring_t.
 */
void
tstring_append_printf(tstring_t *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    tstring_append_vprintf(str, format, args);
    va_end(args);
}

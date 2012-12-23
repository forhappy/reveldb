/*
 * =============================================================================
 *
 *       Filename:  iter.h
 *
 *    Description:  leveldb iterator wrapper.
 *
 *        Created:  12/23/2012 02:45:42 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef _ENGINE_ITER_H_
#define _ENGINE_ITER_H_
#include <reveldb/reveldb.h>

struct rb_node;

typedef struct xleveldb_iter_s_ xleveldb_iter_t;

struct xleveldb_iter_s_ {
    char *uuid;
    leveldb_iterator_t *iter;
    reveldb_t *reveldb;
    struct rb_node node;
};

extern xleveldb_iter_t * xleveldb_init_iter(
        const char *uuid,
        reveldb_t *reveldb);

extern xleveldb_iter_t * xleveldb_search_iter(
        struct rb_root *root,
        const char *uuid);

extern int xleveldb_insert_iter(
        struct rb_root *root,
        xleveldb_iter_t *iter);

extern void xleveldb_free_iter(
        xleveldb_iter_t *iter);

/* leveldb iterator wrapper. */
extern unsigned char xleveldb_iter_valid(const xleveldb_iter_t*);
extern void xleveldb_iter_seek_to_first(xleveldb_iter_t*);
extern void xleveldb_iter_seek_to_last(xleveldb_iter_t*);
extern void xleveldb_iter_seek(xleveldb_iter_t*, const char* k, size_t klen);
extern void xleveldb_iter_next(xleveldb_iter_t*);
extern void xleveldb_iter_prev(xleveldb_iter_t*);
extern void xleveldb_iter_forward(xleveldb_iter_t*, unsigned int step);
extern void xleveldb_iter_backward(xleveldb_iter_t*, unsigned int step);
extern const char* xleveldb_iter_key(const xleveldb_iter_t*, size_t* klen);
extern const char* xleveldb_iter_value(const xleveldb_iter_t*, size_t* vlen);
extern void xleveldb_iter_kv(const xleveldb_iter_t*,
        const char **key, size_t *klen,
        const char **value, size_t *vlen);
#endif /* _ENGINE_ITER_H_ */

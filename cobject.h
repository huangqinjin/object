#ifndef COBJECT_H
#define COBJECT_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef cobject_handle_copy
#define cobject_handle_copy(p) (assert(!(p) && "cobject_handle_copy must be defined!"), (p))
#endif
#ifndef cobject_handle_clear
#define cobject_handle_clear(p) assert(!(p) && "cobject_handle_clear must be defined!")
#endif


typedef enum cobject_t
{
    cobject_null,
    cobject_uint,
    cobject_sint,
    cobject_float,
    cobject_ptr,
    cobject_lit,
    cobject_pod,
    cobject_str,
    cobject_handle
} cobject_t;

typedef struct cobject
{
    union
    {
        unsigned long long u;
        long long i;
        double f;
        char b[8];
        void* p;
    };

    unsigned int size;
    cobject_t type;
} cobject;

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

static void cobject_init(cobject* o)
{
    memset(o, 0, sizeof(cobject));
}

static void cobject_clear(cobject* o)
{
    if ((o->type == cobject_pod || o->type == cobject_str) && o->size > sizeof(o->b))
    {
        unsigned c;
        char* p = (char*)o->p + o->size;
        memcpy(&c, p, sizeof(c));
        if (--c) memcpy(p, &c, sizeof(c));
        else free(o->p);
    }
    else if (o->type == cobject_handle)
    {
        cobject_handle_clear(o->p);
    }
    cobject_init(o);
}

static void cobject_copy(const cobject* src, cobject* dst)
{
    cobject_clear(dst);
    memcpy(dst, src, sizeof(cobject));

    if ((src->type == cobject_pod || src->type == cobject_str) && src->size > sizeof(src->b))
    {
        unsigned c;
        char* p = (char*)dst->p + dst->size;
        memcpy(&c, p, sizeof(c));
        ++c;
        memcpy(p, &c, sizeof(c));
    }
    else if (src->type == cobject_handle)
    {
        dst->p = cobject_handle_copy(src->p);
    }
}

static cobject_t cobject_type(const cobject* o)
{
    return o->type;
}

static void cobject_set_uint(cobject* o, unsigned long long u)
{
    cobject_clear(o);
    o->type = cobject_uint;
    o->u = u;
}

static unsigned long long cobject_get_unsafe_uint(const cobject* o)
{
    return o->u;
}

static bool cobject_get_uint(const cobject* o, unsigned long long* u)
{
    if (o->type != cobject_uint) return false;
    *u = o->u;
    return true;
}

static void cobject_set_sint(cobject* o, long long i)
{
    cobject_clear(o);
    o->type = cobject_sint;
    o->i = i;
}

static long long cobject_get_unsafe_sint(const cobject* o)
{
    return o->i;
}

static bool cobject_get_sint(const cobject* o, long long* i)
{
    if (o->type != cobject_sint) return false;
    *i = o->i;
    return true;
}

static void cobject_set_float(cobject* o, double f)
{
    cobject_clear(o);
    o->type = cobject_float;
    o->f = f;
}

static double cobject_get_unsafe_float(const cobject* o)
{
    return o->f;
}

static bool cobject_get_float(const cobject* o, double* f)
{
    if (o->type != cobject_float) return false;
    *f = o->f;
    return true;
}

static void cobject_set_ptr(cobject* o, void* p)
{
    cobject_clear(o);
    o->type = cobject_ptr;
    o->p = p;
}

static void* cobject_get_unsafe_ptr(const cobject* o)
{
    return o->p;
}

static bool cobject_get_ptr(const cobject* o, void** p)
{
    if (o->type != cobject_ptr) return false;
    *p = o->p;
    return true;
}

static void cobject_set_lit(cobject* o, const char* s, size_t n)
{
    cobject_clear(o);
    o->type = cobject_lit;
    o->p = (void*)s;
    o->size = (unsigned)(n + 1);
}

static const char* cobject_get_unsafe_lit(const cobject* o, size_t* n)
{
    if (n) { (*n) = o->size; --*n; }
    return (const char*)o->p;
}

static bool cobject_get_lit(const cobject* o, const char** s, size_t* n)
{
    if (o->type != cobject_lit) return false;
    *s = cobject_get_unsafe_lit(o, n);
    return true;
}

static void cobject_set_pod(cobject* o, const void* p, size_t n)
{
    cobject_clear(o);
    o->type = cobject_pod;
    o->size = (unsigned)n;
    if (n > sizeof(o->b))
    {
        unsigned c = 1;
        o->p = malloc(n + sizeof(c));   //TODO alignment
        memcpy(o->p, p, n);
        memcpy((char*)o->p + n, &c, sizeof(c));
    }
    else
    {
        memcpy(o->b, p, n);
    }
}

static const void* cobject_get_unsafe_pod(const cobject* o, size_t* n)
{
    if (n) *n = o->size;
    return o->size > sizeof(o->b) ? o->p : (void*)o->b;
}

static bool cobject_get_pod(const cobject* o, const void** p, size_t* n)
{
    if (o->type != cobject_pod) return false;
    *p = cobject_get_unsafe_pod(o, n);
    return true;
}

static void cobject_set_str(cobject* o, const char* s, size_t n)
{
    if (n == (size_t)-1) n = strlen(s);
    cobject_set_pod(o, s, n + 1);
    o->type = cobject_str;
}

static const char* cobject_get_unsafe_str(const cobject* o, size_t* n)
{
    const void* p = cobject_get_unsafe_pod(o, n);
    if (n) --*n;
    return (const char*)p;
}

static bool cobject_get_str(const cobject* o, const char** s, size_t* n)
{
    if (o->type != cobject_str) return false;
    *s = cobject_get_unsafe_str(o, n);
    return true;
}

static void cobject_set_handle(cobject* o, void* p)
{
    cobject_clear(o);
    o->type = cobject_handle;
    o->p = p;
}

static void* cobject_get_unsafe_handle(const cobject* o)
{
    return o->p;
}

static bool cobject_get_handle(const cobject* o, void** p)
{
    if (o->type != cobject_handle) return false;
    *p = o->p;
    return true;
}

static const char* cobject_get_lit_or_str(const cobject* o, size_t* n)
{
    if (o->type == cobject_lit) return cobject_get_unsafe_lit(o, n);
    if (o->type == cobject_str) return cobject_get_unsafe_str(o, n);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif //COBJECT_H

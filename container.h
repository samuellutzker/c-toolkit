/*
 * vim: fileencoding=utf8 expandtab shiftwidth=4 tabstop=4
 */
/**
 * @file container.h
 * @brief @ref container.h A collection of dynamic container types (vector, dynamic ringbuffer, unrolled linked list, ...)
 */
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef con_out
#define con_out printf
#endif
#ifdef CON_DEBUG
#include <stdio.h>
#define con_debug(fmt, ...) con_out("%s(): [DEBUG] " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define con_debug(...)
#endif

/**
 * @brief Result code
 * @todo Add error types
 */
enum con_result {
    CON_OK = 0,
    CON_FAIL,
};

#ifndef CON_NOTHING // "empty" may be defined to be anything you want it to be
#define CON_NOTHING NULL
#endif

// ------ helpers ------

inline static bool filter_nothing(void *e, size_t i, void *cbdata)
{
    return true;
}

inline static bool filter_delete(void *e, size_t i, void *cbdata)
{
    return e != cbdata;
}

// ------- dynamic vector -------

#define CON_STD_VEC_CAP 64

/**
 * @brief Dynamic vector
 */
typedef struct {
    void **data;
    size_t cap;
    size_t len;
    size_t min_cap;
} vec;

/**
 *  @def vec_at(v, i)
 *  @brief Retrieves the container element at the specified index.
 *
 *  @param[in] v container
 *  @param[in] i index (must be >= 0)
 *  @return container element, or CON_NOTHING if out of bounds
 */
#define vec_at(v, i) ((i) < (v).len ? (v).data[i] : CON_NOTHING)

/**
 *  @def vec_free(v, freefn)
 *  @brief Frees reserved memory of a container, must be called before it goes out of scope.
 *
 *  @param[in] v pointer to a container
 *  @param[in] freefn (optional, may be NULL) void (*)(void*) for freeing elements
 */
#define vec_free(v, freefn) { vec_clear(v, freefn); free((v)->data); (v)->data = NULL; }

/**
 *  @def vec_foreach(v, type, as)
 *  @brief Iterates over all container elements. Allows break and continue.
 *
 *  @param[in] v container
 *  @param[in] type the type to cast the elements to
 *  @param[in] as a variable name which will hold the casted element
 */
#define vec_foreach(v, type, as) \
    for (size_t _i_##as = 0, _n_##as = (v).len; _i_##as < _n_##as; ++_i_##as) \
        for (type as = (v).data[_n_##as = 0, _i_##as]; !_n_##as; _n_##as = (v).len)

/**
 *  @brief Initializes a container.
 *
 *  @param[in] v pointer to a container
 *  @param[in] min_cap minimum capacity beyond which the container is not shrunk
 *  @return CON_OK on success, CON_FAIL on error
 */
enum con_result vec_alloc(vec *v, size_t min_cap);

/**
 *  @brief Clears all elements of a container.
 *
 *  @param[in] v pointer to a container
 *  @param[in] freefn (optional, may be NULL) function to free elements
 */
void vec_clear(vec *v, void (*freefn)(void *));

/**
 *  @brief Pushes an element to the front of a container.
 *
 *  @param[in] v pointer to a container
 *  @param[in] e data
 *  @return CON_OK on success, CON_FAIL on error
 */
enum con_result vec_prepend(vec *v, void *e);

/**
 *  @brief Pushes an element to the back of a container.
 *
 *  @param[in] v pointer to a container
 *  @param[in] e data
 *  @return CON_OK on success, CON_FAIL on error
 */
enum con_result vec_append(vec *v, void *e);

/**
 *  @brief Retrieves and removes an element from a certain position.
 *  @note The container is shrunk, but the elements remain in order.
 *
 *  @param[in] v pointer to a container
 *  @param[in] i index
 *  @return container element, CON_NOTHING if i is out of bounds
 */
void *vec_pop(vec *v, size_t i);

/**
 *  @brief Deletes all container elements that carry the specified value.
 *  @note The container is shrunk, but the elements remain in order
 *
 *  @param[in] v pointer to a container
 *  @param[in] e value to match
 *  @param[in] freefn (optional, may be NULL) function to free elements
 */
void vec_delete(vec *v, void *e, void (*freefn)(void *e));

/**
 *  @brief Deletes all container elements for which filterfn(..) returns false.
 *  @note The container is shrunk, but the elements remain in order.
 *
 *  @param[in] v pointer to a container
 *  @param[in] filterfn filter-function which receives the value and index plus optional callback data
 *  @param[in] cbdata optional callback data to pass to filterfn
 *  @param[in] freefn (optional, may be NULL) function to free elements
 */
void vec_filter(vec *v, bool (*filterfn)(void *e, size_t i, void *cbdata), void *cbdata, void (*freefn)(void *e));

/**
 *  @brief Retrieves and removes the last element of the container.
 *
 *  @param[in] v pointer to a container
 *  @return container element, CON_NOTHING if container is empty
 */
void *vec_popback(vec *v);

/**
 *  @brief Retrieves and removes the first element of the container.
 *
 *  @param[in] v pointer to a container
 *  @return container element, CON_NOTHING if container is empty
 */
void *vec_popfront(vec *v);

/**
 *  @brief Quick removal of a specified container element, by replacing it with the tail.
 *  @note This changes the order of the remaining elements.
 *
 *  @param[in] v pointer to a container
 *  @param[in] i container index
 *  @return container element, CON_NOTHING if i is out of bounds
 */
void *vec_swap_popback(vec *v, size_t i);

/**
 *  @brief Finds the first occurrence of a given element in a container.
 *
 *  @param[in] v pointer to a container
 *  @param[in] e data to match
 *  @param[out] pos container index for the first occurrence of e
 *  @return CON_OK if the element is found, CON_FAIL if not
 */
enum con_result vec_find(vec *v, void *e, size_t *pos);

/* !!! define this in ONE source file: !!! */
#ifdef CON_IMPLEMENT

static bool vec_capfix(vec *v)
{
    if (!v->cap) {
        return false;
    }
    size_t new_cap = v->cap;
    while (1) {
        if (v->len + 1 >= new_cap) {
            new_cap *= 2;
        } else if (new_cap > v->min_cap && new_cap > v->len * 4) {
            new_cap /= 2;
        } else {
            break;
        }
    }
    if (new_cap == v->cap) {
        return true;
    }
    con_debug("resizing capacity to %zu", new_cap);
    void **new_data = (void**)realloc(v->data, new_cap * sizeof(void*));
    if (!new_data) {
        return false;
    }
    v->cap = new_cap;
    v->data = new_data;
    return true;
}

enum con_result vec_append(vec *v, void *e)
{
    if (!vec_capfix(v)) return CON_FAIL;
    v->data[v->len++] = e;
    return CON_OK;
}

enum con_result vec_prepend(vec *v, void *e)
{
    if (!vec_capfix(v)) return CON_FAIL;
    memmove(v->data + 1, v->data, sizeof(void *) * v->len++);
    v->data[0] = e;
    return CON_OK;
}

void *vec_pop(vec *v, size_t i)
{
    if (i >= v->len) return CON_NOTHING;
    void *out = v->data[i];
    memmove(v->data + i, v->data + i + 1, (--v->len - i) * sizeof(void*));
    return out;
}

void vec_filter(vec *v, bool (*filterfn)(void *e, size_t i, void *cbdata), void *cbdata, void (*freefn)(void *e))
{
    if (!filterfn) return;
    size_t j = 0;
    for (size_t i = 0; i < v->len; ++i) {
        if (filterfn(v->data[i], i, cbdata)) {
            v->data[j++] = v->data[i];
        } else {
            if (freefn) freefn(v->data[i]);
        }
    }
    v->len = j;
}

void vec_delete(vec *v, void *e, void (*freefn)(void *e))
{
    vec_filter(v, filter_delete, e, freefn);
    (void)vec_capfix(v);
}

void *vec_swap_popback(vec *v, size_t i)
{
    if (i >= v->len) return CON_NOTHING;
    void *out = v->data[i];
    v->data[i] = v->data[--v->len];
    (void)vec_capfix(v);
    return out;
}

void *vec_popback(vec *v)
{
    if (!v->len) return CON_NOTHING;
    void *out = v->len ? v->data[--v->len] : CON_NOTHING;
    (void)vec_capfix(v);
    return out;
}

void *vec_popfront(vec *v)
{
    if (!v->len) return CON_NOTHING;
    void *out = v->data[0];
    vec_pop(v, 0);
    return out;
}

enum con_result vec_find(vec *v, void *e, size_t *pos)
{
    for (size_t i = 0; i < v->len; ++i) {
        if (v->data[i] == e) {
            if (pos) *pos = i;
            return CON_OK;
        }
    }
    return CON_FAIL;
}

enum con_result vec_alloc(vec *v, size_t min_cap)
{
    min_cap = min_cap ? min_cap : CON_STD_VEC_CAP;
    v->len = 0;
    v->cap = min_cap;
    v->min_cap = min_cap;
    v->data = (void**)malloc(min_cap * sizeof(void*));
    return v->data != NULL ? CON_OK : CON_FAIL;
}

void vec_clear(vec *v, void (*freefn)(void *))
{
    if (freefn) {
        vec_foreach(*v, void*, e) {
            freefn(e);
        }
    }
    v->len = 0;
    (void)vec_capfix(v);
}
#endif

// ------- dynamic ringbuffer ------

#define CON_STD_RBUF_CAP 64

/**
 * @brief Dynamic ringbuffer
 */
typedef struct {
    void **data;
    size_t cap;
    size_t min_cap;
    size_t len;
    size_t head;
    size_t tail;
} rbuf;

/**
 * @def rbuf_at(v, i)
 * @copydoc vec_at(v, i)
 */
#define rbuf_at(v, i) \
    ((i) < (v).len ? (v).data[((i) + (v).head) & ((v).cap - 1)] : CON_NOTHING)

/**
 * @def rbuf_free(v, freefn)
 * @copydoc vec_free(v, freefn)
 */
#define rbuf_free(v, freefn) \
    { rbuf_clear(v, freefn); free((v)->data); memset(v, 0, sizeof(rbuf)); }

/**
 * @def rbuf_foreach(v, type, as)
 * @copydoc vec_foreach(v, type, as)
 */
#define rbuf_foreach(v, type, as) \
    for (size_t _i_##as = 0, _n_##as = (v).len; _i_##as < _n_##as; ++_i_##as) \
        for (type as = (v).data[_n_##as = 0, ((v).head + _i_##as) & ((v).cap - 1)]; !_n_##as; _n_##as = (v).len)

/**
 * @copydoc vec_alloc()
 */
enum con_result rbuf_alloc(rbuf *v, size_t min_cap);

/**
 * @copydoc vec_clear()
 */
void rbuf_clear(rbuf *v, void (*freefn)(void *));

/**
 * @copydoc vec_append()
 */
enum con_result rbuf_append(rbuf *v, void *e);

/**
 * @copydoc vec_prepend()
 */
enum con_result rbuf_prepend(rbuf *v, void *e);

/**
 * @copydoc vec_pop()
 */
void *rbuf_pop(rbuf *v, size_t i);

/**
 * @copydoc vec_filter()
 */
void rbuf_filter(rbuf *v, bool (*filterfn)(void *e, size_t i, void *cbdata), void *cbdata, void (*freefn)(void *e));

/**
 * @copydoc vec_delete()
 */
void rbuf_delete(rbuf *v, void *e, void (*freefn)(void *e));

/**
 * @copydoc vec_popback()
 */
void *rbuf_popback(rbuf *v);

/**
 * @copydoc vec_popfront()
 */
void *rbuf_popfront(rbuf *v);

#ifdef CON_IMPLEMENT

static bool rbuf_capfix(rbuf *v)
{
    if (!v->cap) {
        return false;
    }
    size_t new_cap = v->cap;
    while (1) {
        if (v->len + 1 >= new_cap) {
            new_cap <<= 1;
        } else if (new_cap > v->min_cap && new_cap > v->len * 4) {
            new_cap >>= 1;
        } else {
            break;
        }
    }
    if (new_cap == v->cap) {
        return true;
    }
    if (new_cap > v->cap) {
        void **new_data = (void**)realloc(v->data, new_cap * sizeof(void*));
        if (!new_data) {
            return false;
        }
        v->data = new_data;
    }
    size_t new_head = v->head;
    size_t new_tail = v->tail;
    con_debug("resizing capacity to %zu", new_cap);
    if (v->head > v->tail) {
        size_t head_len = v->cap - v->head;
        new_head = new_cap - head_len;
        memcpy(v->data + new_head, v->data + v->head, head_len * sizeof(void*));
    } else if (v->head >= new_cap) {
        memcpy(v->data, v->data + v->head, v->len * sizeof(void*));
        new_head = 0;
        new_tail = v->len - 1;
    } else if (v->tail >= new_cap) {
        size_t tail_len = v->tail + 1 - new_cap;
        memcpy(v->data, v->data + v->cap, tail_len * sizeof(void*));
        new_tail = tail_len - 1;
    }
    if (new_cap < v->cap) {
        void **new_data = (void**)realloc(v->data, new_cap * sizeof(void*));
        if (!new_data) {
            return false;
        }
        v->data = new_data;
    }
    v->cap = new_cap;
    v->tail = new_tail;
    v->head = new_head;
    return true;
}

enum con_result rbuf_append(rbuf *v, void *e)
{
    if (!rbuf_capfix(v)) return CON_FAIL;
    if (v->len++) v->tail = (v->tail + 1) & (v->cap - 1);
    v->data[v->tail] = e;
    return CON_OK;
}

enum con_result rbuf_prepend(rbuf *v, void *e)
{
    if (!rbuf_capfix(v)) return CON_FAIL;
    if (v->len++) v->head = (v->head + v->cap - 1) & (v->cap - 1);
    v->data[v->head] = e;
    return CON_OK;
}

void *rbuf_pop(rbuf *v, size_t i)
{
    if (i >= v->len) return CON_NOTHING;
    void *out = rbuf_at(*v, i);
    size_t p = (i + v->head) & (v->cap - 1);
    if (i < v->len / 2) {
        // shrink from beginning
        if (p < v->head) {
            // wrap around
            memmove(v->data + 1, v->data, p * sizeof(void*));
            v->data[0] = v->data[v->cap - 1];
            memmove(v->data + v->head + 1, v->data + v->head, (v->cap - v->head - 1) * sizeof(void*));
        } else {
            memmove(v->data + v->head + 1, v->data + v->head, i * sizeof(void*));
        }
        if (--v->len) v->head = (v->head + 1) & (v->cap - 1);
    } else {
        // shrink from end
        if (p > v->tail) {
            // wrap around
            memmove(v->data + p, v->data + p + 1, (v->cap - 1 - p) * sizeof(void*));
            v->data[v->cap - 1] = v->data[0];
            memmove(v->data, v->data + 1, (v->tail) * sizeof(void*));
        } else {
            memmove(v->data + p, v->data + p + 1, (v->tail - p) * sizeof(void*));
        }
        if (--v->len) v->tail = (v->cap + v->tail - 1) & (v->cap - 1);
    }
    (void)rbuf_capfix(v);
    return out;
}

void rbuf_filter(rbuf *v, bool (*filterfn)(void *e, size_t i, void *cbdata), void *cbdata, void (*freefn)(void *e))
{
    if (!filterfn) return;
    size_t src = v->head;
    size_t dst = v->head;
    size_t cnt = 0;
    for (size_t i = 0; i < v->len; ++i) {
        if (filterfn(v->data[src], i, cbdata)) {
            v->data[dst] = v->data[src];
            dst = (dst + 1) & (v->cap - 1);
        } else {
            if (freefn) freefn(v->data[src]);
            ++cnt;
        }
        src = (src + 1) & (v->cap - 1);
    }
    v->len -= cnt;
    v->tail = (v->tail + v->cap - cnt) & (v->cap - 1);
    (void)rbuf_capfix(v);
}

void rbuf_delete(rbuf *v, void *e, void (*freefn)(void *e))
{
    rbuf_filter(v, filter_delete, e, freefn);
}

void *rbuf_popback(rbuf *v)
{
    if (!v->len) return CON_NOTHING;
    void *e = v->data[v->tail];
    if (--v->len) v->tail = (v->tail + v->cap - 1) & (v->cap - 1);
    (void)rbuf_capfix(v);
    return e;
}

void *rbuf_popfront(rbuf *v)
{
    if (!v->len) return CON_NOTHING;
    void *e = v->data[v->head];
    if (--v->len) v->head = (v->head + 1) & (v->cap - 1);
    (void)rbuf_capfix(v);
    return e;
}

enum con_result rbuf_alloc(rbuf *v, size_t min_cap)
{
    for (v->min_cap = 1; v->min_cap < min_cap; v->min_cap <<= 1)
        ;
    if (!min_cap) v->min_cap = CON_STD_RBUF_CAP;
    v->len = 0;
    v->head = 0;
    v->tail = 0;
    v->cap = v->min_cap;
    v->data = (void**)malloc(v->cap * sizeof(void*));
    return v->data != NULL ? CON_OK : CON_FAIL;
}

void rbuf_clear(rbuf *v, void (*freefn)(void *))
{
    if (freefn) {
        rbuf_foreach(*v, void*, e) {
            freefn(e);
        }
    }
    v->len = 0;
    v->head = 0;
    v->tail = 0;
    (void)rbuf_capfix(v);
}
#endif

// ------- unrolled linked list -------

#define CON_ULIST_ARRSIZE 256

typedef uint16_t ulsize_t;

/**
 * @brief Unrolled linked list
 */
typedef struct ulist {
    void *data[CON_ULIST_ARRSIZE];
    ulsize_t len;
    struct ulist *next; /* NULL at the tail */
    struct ulist *prev; /* this one is circular for quick tail access */
} ulist;

/**
 *  @def ulist_foreach(v, type, as)
 *  @brief Iterates over all list elements. Allows break and continue.
 *
 *  @param[in] l list
 *  @param[in] type the type to cast the elements to
 *  @param[in] as a variable name which will hold the casted element
 */
#define ulist_foreach(l, type, as) \
    for (ulist *_l_##as = l, *_ok_##as = (void*)1; _ok_##as && _l_##as; _l_##as = _l_##as->next) \
        for (ulsize_t _i_##as = 0; _ok_##as && _i_##as < _l_##as->len; ++_i_##as) \
            for (type as = _l_##as->data[_ok_##as = NULL, _i_##as]; !_ok_##as; _ok_##as = (void*)1)

/**
 *  @brief Pushes an element to the back of a list.
 *
 *  @param[in] h pointer to the list head
 *  @param[in] e data
 *  @return CON_OK on success, CON_FAIL on error
 */
#define ulist_append(h, e) ulist_insert(h, 0, true, e)

/**
 *  @brief Pushes an element to the front of a list.
 *
 *  @param[in] h pointer to the list head
 *  @param[in] e data
 *  @return CON_OK on success, CON_FAIL on error
 */
#define ulist_prepend(h, e) ulist_insert(h, 0, false, e)

/**
 *  @brief Deletes a list, releasing all memory.
 *
 *  @param[in] h pointer to the list head
 *  @param[in] freefn (optional, may be NULL) function to free elements
 */
void ulist_free(ulist **h, void (*freefn)(void *));

/**
 *  @brief Retrieves the list element at the specified index.
 *
 *  @param[in] l list head
 *  @param[in] i index
 *  @param[in] back search backwards (this is O(n), so worth the effort)
 *  @return container element, or CON_NOTHING if out of bounds
 */
void *ulist_at(ulist *l, size_t i, bool back);

/**
 *  @brief Retrieves and removes the last element of the list.
 *
 *  @param[in] h pointer to the list head
 *  @return list element, or CON_NOTHING if list is empty (then l == NULL)
 */
void *ulist_popback(ulist **h);

/**
 *  @brief Retrieves and removes the first element of the list.
 *
 *  @param[in] h pointer to the list head
 *  @return list element, or CON_NOTHING if list is empty (then l == NULL)
 */
void *ulist_popfront(ulist **h);

/**
 *  @brief Checks if given data exists in the list.
 *
 *  @param[in] l list head
 *  @param[in] e data
 *  @retval true e exists in l
 *  @retval false e does not occur in l
 */
bool ulist_contains(ulist *l, void *e);

/**
 *  @brief Checks two given lists for equality.
 *
 *  @param[in] l1 list (head)
 *  @param[in] l2 other list (head)
 *  @retval true lists have the same elements
 *  @retval false lists differ
 */
bool ulist_equal(ulist *l1, ulist *l2);

/**
 *  @brief Inserts data into a list at the given spot.
 *
 *  @param[in] h pointer to the list head
 *  @param[in] i index where the data should be located
 *  @param[in] back search backwards (this is O(n), so worth the effort)
 *  @param[in] e data
 *  @return CON_OK if successful, CON_FAIL otherwise
 */
enum con_result ulist_insert(ulist **h, size_t i, bool back, void *e);

/**
 *  @brief Determines the length of a given list.
 *
 *  @param[in] l list head
 *  @return length of the list
 */
size_t ulist_len(ulist *l);

/**
 *  @brief Removes and retrieves data at the given index.
 *
 *  @param[in] h pointer to the list head
 *  @param[in] i index of the element
 *  @param[in] back search backwards (this is O(n), so worth the effort)
 *  @return list element, or CON_NOTHING if list is empty (then l == NULL)
 */
void *ulist_pop(ulist **h, size_t i, bool back);

/**
 *  @brief Deletes all list element that match the given data.
 *
 *  @param[in] h pointer to the list head
 *  @param[in] e data to match
 *  @param[in] freefn (optional, may be NULL) function to free elements
 */
void ulist_delete(ulist **h, void *e, void (*freefn)(void *e));

/**
 *  @brief Deletes all list elements for which filterfn(..) returns false.
 *
 *  @param[in] h pointer to the list head
 *  @param[in] filterfn filter-function which receives the value and index plus optional callback data
 *  @param[in] cbdata optional callback data to pass to filterfn
 *  @param[in] freefn (optional, may be NULL) function to free elements
 */
void ulist_filter(ulist **h, bool (*filterfn)(void *e, size_t i, void *cbdata), void *cbdata, void (*freefn)(void *e));

#ifdef CON_IMPLEMENT

#define SPLIT_AT    CON_ULIST_ARRSIZE
#define FILLUP_TO   CON_ULIST_ARRSIZE/2
#define MERGE_AT    CON_ULIST_ARRSIZE/4

bool ulist_contains(ulist *l, void *e)
{
    ulist_foreach(l, void*, v) {
        if (v == e) return true;
    }
    return false;
}

size_t ulist_len(ulist *l)
{
    size_t len = 0;
    for (; l; l = l->next) {
        len += l->len;
    }
    return len;

}

void *ulist_at(ulist *l, size_t i, bool back)
{
    if (!l) return CON_NOTHING;
    if (back) {
        for (l = l->prev; l && l->prev->next && i >= l->len; i -= l->len, l = l->prev)
            ;
        return i < l->len ? l->data[l->len - i - 1] : CON_NOTHING;
    }
    for (; l && i >= l->len; i -= l->len, l = l->next)
        ;
    return l && i < l->len ? l->data[i] : CON_NOTHING;
}

bool ulist_equal(ulist *l1, ulist *l2)
{
    if (!l1 || !l2) return !l1 && !l2;
    for (; l1 && l2; l1 = l1->next, l2 = l2->next) {
        if (l1->len != l2->len) return false;
        for (size_t i = 0; i < l1->len; ++i) {
            if (l1->data[i] != l2->data[i]) return false;
        }
    }
    return !l1 && !l2;
}

void ulist_free(ulist **h, void (*freefn)(void *))
{
    if (!h) return;
    for (ulist *l = *h; l; l = *h) {
        *h = l->next;
        if (freefn) {
            for (ulsize_t i = 0; i < l->len; ++i) {
                freefn(l->data[i]);
            }
        }
        free(l);
    }
}

static ulist *ul_free_getnext(ulist **h, ulist *l)
{
    con_debug("freeing");
    ulist *n = l->next;
    if (n) {
        n->prev = l->prev;
    } else {
        (*h)->prev = l->prev;
    }
    if (l != *h) {
        l->prev->next = n;
    } else {
        *h = n;
    }
    free(l);
    return n;
}

void *ulist_popback(ulist **h)
{
    if (!h || !*h) return CON_NOTHING;
    ulist *l = *h;
    void *e = ulist_at(l, 0, true);
    if (--l->prev->len == 0) {
        (void)ul_free_getnext(h, l->prev);
    }
    return e;
}

void *ulist_popfront(ulist **h)
{
    if (!h || !*h) return CON_NOTHING;
    ulist *l = *h;
    void *e = ulist_at(l, 0, false);
    if (--l->len == 0) {
        (void)ul_free_getnext(h, l);
    } else {
        memmove(l->data, l->data + 1, l->len * sizeof(void*));
    }
    return e;
}

static size_t ulist_maybe_merge(ulist **h, ulist *l, size_t pos, bool (*filterfn)(void *e, size_t i, void *cbdata), void *cbdata, void (*freefn)(void *e))
{
    size_t shift = 0; // the shift of the next segment
    if (l->len && l->len < MERGE_AT && l->next) {
        con_debug("merging");
        ulist *n = l->next;
        while (n && l->len < FILLUP_TO) {
            if (!filterfn(n->data[shift], pos++, cbdata)) {
                if (freefn) {
                    freefn(n->data[shift]);
                }
            } else {
                l->data[l->len++] = n->data[shift];
            }
            if (++shift >= n->len) {
                n = ul_free_getnext(h, n);
                shift = 0;
            }
        }
    }
    return shift;
}

void ulist_filter(ulist **h, bool (*filterfn)(void *e, size_t i, void *cbdata), void *cbdata, void (*freefn)(void *e))
{
    if (!h || !*h || !filterfn) return;

    size_t j = 0;
    size_t pos = 0;

    for (ulist *l = *h; l; l = l->len ? l->next : ul_free_getnext(h, l)) {
        for (size_t i = j; i < l->len; ++i) {
            if (!filterfn(l->data[i], pos++, cbdata)) {
                if (freefn) {
                    freefn(l->data[i]);
                }
                ++j;
            } else if (j) {
                l->data[i - j] = l->data[i];
            }
        }
        l->len -= j;
        j = ulist_maybe_merge(h, l, pos, filterfn, cbdata, freefn);
    }
}

void *ulist_pop(ulist **h, size_t i, bool back)
{
    if (!h || !*h) return CON_NOTHING;
    ulist *l = *h;
    if (back) {
        for (l = l->prev; l != *h && i >= l->len; i -= l->len, l = l->prev)
            ;
        if (i >= l->len) return CON_NOTHING;
        i = l->len - 1 - i;
    } else {
        for (; l && i >= l->len; i -= l->len, l = l->next)
            ;
        if (!l || i >= l->len) return CON_NOTHING;
    }
    void *out = l->data[i];
    memmove(l->data + i, l->data + i + 1, (l->len - i - 1) * sizeof(void*));
    if (--l->len == 0) {
        (void)ul_free_getnext(h, l);
        return out;
    }
    size_t shift = ulist_maybe_merge(h, l, l->len, filter_nothing, NULL, NULL);
    l = l->next;
    if (l && shift) {
        l->len -= shift;
        memmove(l->data, l->data + shift, l->len * sizeof(void*));
    }
    return out;
}

void ulist_delete(ulist **h, void *e, void (*freefn)(void *e))
{
    ulist_filter(h, filter_delete, e, freefn);
}

enum con_result ulist_insert(ulist **h, size_t i, bool back, void *e)
{
    if (!h) return CON_FAIL;
    if (!*h) {
        if (i) return CON_FAIL;
        *h = (ulist *)calloc(1, sizeof(ulist));
        if (!*h) return CON_FAIL;
        (*h)->prev = *h;
    }
    ulist *l = back ? (*h)->prev : *h;
    while (i > l->len) {
        i -= l->len;
        l = back ? l->prev : l->next;
        if (!l || (back && !l->next)) {
            return CON_FAIL; /* out of range */
        }
    }
    if (back) {
        i = l->len - i;
    }

    if (l->len >= SPLIT_AT) {
        con_debug("splitting");
        ulist *p = (ulist *)calloc(1, sizeof(ulist));
        if (!p) return CON_FAIL;
        p->next = l->next;
        if (!p->next) {
            (*h)->prev = p;
        } else {
            p->next->prev = p;
        }
        l->next = p;
        p->prev = l;
        ulsize_t s = l->len / 2;
        if (i >= s) {
            memcpy(p->data, l->data + s, sizeof(*p->data) * (i - s));
            p->data[i - s] = e;
            memcpy(p->data + i - s + 1, l->data + i, sizeof(*p->data) * (l->len - i));
            p->len = l->len - s + 1;
            l->len = s;
        } else {
            memcpy(p->data, l->data + s, sizeof(*p->data) * (l->len - s));
            memmove(l->data + i + 1, l->data + i, sizeof(*p->data) * (s - i));
            l->data[i] = e;
            p->len = l->len - s;
            l->len = s + 1;
        }
    } else {
        memmove(l->data + i + 1, l->data + i, sizeof(*l->data) * (l->len++ - i));
        l->data[i] = e;
    }
    return CON_OK;
}
#endif

#ifdef CON_UNITTEST
#include <stdio.h>
#include <time.h>

static void *test_action(vec *v, rbuf *rb, ulist **ul, int dtype, int rand1, int rand2)
{
    const int rnd_range = 10000;
    const int action_space = 8;

    void *e = (void*)(uintptr_t)(rand2 % rnd_range);
    int action = rand1 % action_space;

    if (dtype == 0) {
        switch (action) {
            case 0: e = vec_pop(v, rand2 % (1 + v->len)); break;
            case 1: vec_delete(v, e, NULL); break;
            case 2: e = vec_popback(v); break;
            case 3: e = vec_popfront(v); break;
            case 4 ... 5: vec_append(v, e); break;
            case 6 ... 7: vec_prepend(v, e); break;
        }
    } else if (dtype == 1) {
        switch (action) {
            case 0: e = rbuf_pop(rb, rand2 % (1 + rb->len)); break;
            case 1: rbuf_delete(rb, e, NULL); break;
            case 2: e = rbuf_popback(rb); break;
            case 3: e = rbuf_popfront(rb); break;
            case 4 ... 5: rbuf_append(rb, e); break;
            case 6 ... 7: rbuf_prepend(rb, e); break;
        }
    } else {
        switch (action) {
            case 0: e = ulist_pop(ul, rand2 % (1 + ulist_len(*ul)), false); break;
            case 1: ulist_delete(ul, e, NULL); break;
            case 2: e = ulist_popback(ul); break;
            case 3: e = ulist_popfront(ul); break;
            case 4 ... 5: ulist_append(ul, e); break;
            case 6 ... 7: ulist_prepend(ul, e); break;
        }
    }
    return e;
}
static bool test_consistency(void)
{
    const int test_iters = 10000;
    const int test_compare_after = 1;
    bool ok = false;
    ulist *ul = NULL;
    rbuf rb;
    vec v;

    vec_alloc(&v, 0);
    rbuf_alloc(&rb, 0);

    for (size_t i = 0; i < test_iters; ++i) {
        int rand1 = rand();
        int rand2 = rand();
        void *r0 = test_action(&v, &rb, &ul, 0, rand1, rand2);
        void *r1 = test_action(&v, &rb, &ul, 1, rand1, rand2);
        void *r2 = test_action(&v, &rb, &ul, 2, rand1, rand2);
        size_t j = 0;
        if (r0 != r1 || r1 != r2 || v.len != rb.len || v.len != ulist_len(ul)) {
            con_out("FAIL: return of action %d or length inconsistent (vec[len=%zu]=%p, rbuf[len=%zu]=%p, ulist[len=%zu]=%p).\n", rand1, v.len, r0, rb.len, r1, ulist_len(ul), r2);
            goto end;
        }
        if (0 == i % test_compare_after) {
            vec_foreach(v, void*, el) {
                if (rbuf_at(rb, j) != el) {
                    con_out("FAIL: rbuf != vec at %zu. %p != %p.\n", j, rbuf_at(rb, j), el);
                    goto end;
                }
                ++j;
            }
            j = 0;
            rbuf_foreach(rb, void*, el) {
                if (ulist_at(ul, j, false) != el) {
                    con_out("FAIL: ulist != rbuf at %zu. %p != %p\n", j, ulist_at(ul, j, false), el);
                    goto end;
                }
                ++j;
            }
            j = 0;
            ulist_foreach(ul, void*, el) {
                if (vec_at(v, j) != el) {
                    con_out("FAIL: vec != ulist at %zu. %p != %p\n", j, vec_at(v, j), el);
                    goto end;
                }
                ++j;
            }
        }
    }
    ok = true;
end:
    ulist_free(&ul, NULL);
    vec_free(&v, NULL);
    rbuf_free(&rb, NULL);
    return ok;
}

static void wait_fullsec(void)
{
    time_t t = time(0);
    while (t == time(0))
        ;
}

static bool test_speed(void)
{
    const time_t test_duration = 5;

    ulist *ul = NULL;
    rbuf rb;
    vec v;

    vec_alloc(&v, 0);
    rbuf_alloc(&rb, 0);

    for (int i = 0; i < 3; ++i) {
        wait_fullsec();
        time_t t = time(0);
        size_t count = 0;
        while (time(0) - t < test_duration) {
            int rand1 = rand();
            int rand2 = rand();
            void *r0 = test_action(&v, &rb, &ul, i, rand1, rand2);
            ++count;
        }
        con_out("%s:\t%zu actions.\n", (const char *[]){"vec", "rbuf", "ulist"}[i], count);
    }
    size_t ul_bytes = 0;
    for (ulist *i = ul; i; i = i->next) ul_bytes += sizeof(ulist);
    con_out("num_elements: vec=%zu, rbuf=%zu, ulist=%zu.\n", v.len, rb.len, ulist_len(ul));
    con_out("num_bytes: vec=%zu, rbuf=%zu, ulist=%zu.\n", sizeof(vec) + v.cap * sizeof(*v.data), sizeof(rbuf) + rb.cap * sizeof(*rb.data), ul_bytes);
    ulist_free(&ul, NULL);
    vec_free(&v, NULL);
    rbuf_free(&rb, NULL);

    return true;
}

static const struct {
    const char *name;
    bool (*test)(void);
} tests[] = {
    { "consistency",  test_consistency },
    { "speed",        test_speed },
};

void test_main(void)
{
    srand(time(0));
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        con_out("[%zu] Running Test: %s...\n", i, tests[i].name);
        bool rc = tests[i].test();
        con_out("Test %s finished ....  %s\n\n", tests[i].name, rc ? "[  OK  ]" : "[ FAIL ]");
    }
}
#endif
#undef con_debug
#undef con_out

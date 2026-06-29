/**
 *  @file yr.h
 *  @brief @ref yr.h YR >--< tinY serializeR v1.0 (Sam 2026)
 *
 *  @par Example usage
 *
 *  @code
 *  struct example {
 *      int32_t some_i32;
 *      yr_enum_t union_selector;
 *      union example_union {
 *          struct example_innerstruct {
 *              const unsigned char *buffer;
 *              yr_size_t buffer_len;
 *              uint16_t bonus_u16;
 *          } another_struct;
 *          const char *a_string;
 *          double some_f64;
 *      } the_union;
 *  };
 *
 *  // ------- example protocol --------
 *
 *  static const yr_proto_t example_innerstruct_p = YR_PROTO(
 *      // order does not need to match the struct order
 *      { YR_UINT16,  offsetof(struct example_innerstruct, bonus_u16) },
 *      // mandatory buffer length (yr_size_t) must be included in buffer protocol
 *      { YR_BUFFER,  offsetof(struct example_innerstruct, buffer),
 *                    offsetof(struct example_innerstruct, buffer_len) },
 *  );
 *
 *  static const yr_proto_t example_union_p = YR_PROTO(
 *      // structs and unions require the .proto parameter to point to their protocol
 *      { YR_STRUCT,  offsetof(union example_union, another_struct),
 *                    .proto = &example_innerstruct_p },
 *      // 0-terminated string does not have a length parameter
 *      { YR_STRING,  offsetof(union example_union, a_string) },
 *      { YR_FLOAT64, offsetof(union example_union, some_f64) },
 *  );
 *
 *  static const yr_proto_t example_p = YR_PROTO(
 *      { YR_INT32,   offsetof(struct example, some_i32) },
 *      // mandatory union selector enum (yr_enum_t) must be included in union protocol
 *      { YR_UNION,   offsetof(struct example, the_union),
 *                    offsetof(struct example, union_selector),
 *                    .proto = &example_union_p },
 *  );
 *
 *  // ------- example serialization --------
 *
 *  const unsigned char *serialized_out;
 *  size_t buffer_len;
 *  struct example src = {
 *      -1234, 0,
 *      .the_union.another_struct = {
 *          (const unsigned char[]){ 3,1,4,1,5,9,2,6,5,3,5 }, 11, 4711
 *      }
 *  };
 *
 *  struct yr_ctx *ctx = yr_serialize(&example_p, &src);
 *  while (!yr_serialize_getbuf(ctx, &serialized_out, &buffer_len)) {
 *      // send buffer_len bytes of serialized_out. src must remain valid and accessible.
 *  }
 *  yr_finish(&ctx);
 *
 *  // ------- example deserialization --------
 *
 *  unsigned char *serialized_in;
 *  struct example dst;
 *
 *  ctx = yr_deserialize(&example_p, &dst);
 *  while (!yr_deserialize_getbuf(ctx, &serialized_in, &buffer_len, 0)) {
 *      // receive buffer_len bytes of serialized data into serialized_in (memory is allocated)
 *  }
 *  // do something with dst
 *  yr_finish(&ctx); // afterwards! invalidates buffers in dst and frees allocated memory.
 *  @endcode
 *
 *  @par Brief explanation of YR_STREAM
 *
 *  One unsigned char* buffer may be of type YR_STREAM. Basically this is a length-changing buffer.
 *  It receives portions of its content as data arrives and adjusts its auxiliary length parameter accordingly.
 *  Current use requires new data to be consumed, so the old buffer is replaced, but future uses may make it convenient
 *  to add an option to concatenate new data to the already received contents.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// ------------ types ------------

struct yr_protocol;
typedef struct yr_protocol yr_proto_t;
typedef int16_t yr_enum_t;
typedef uint32_t yr_size_t;

enum yr_dtype {
    YR_INT8,
    YR_INT16,
    YR_INT32,
    YR_INT64,
    YR_UINT8,
    YR_UINT16,
    YR_UINT32,
    YR_UINT64,
    YR_FLOAT32,
    YR_FLOAT64,
    YR_ENUM,
    YR_SIZE,
    YR_STRING,
    YR_BUFFER,
    YR_STREAM,
    YR_STRUCT,
    YR_UNION,
};

/**
 *  @brief Describes a single member of a serializable struct or union.
 */
struct yr_member {
    enum yr_dtype type;     /**< Data type of this member. */
    size_t offs;            /**< Offset of this member within the parent struct or union. */
    size_t offs_aux;        /**< Auxiliary offset. For YR_BUFFER: offset of the buffer
                             *   size field. For YR_UNION: offset of the union type
                             *   selector field. */
    const yr_proto_t *proto; /**< Pointer to the protocol definition for nested structs or
                              *   unions. NULL for primitive types. */
};

/**
 *  @brief Describes the serialization protocol for a struct or union.
 *
 *  Contains the number of members and a flexible array of @ref yr_member
 *  descriptors. Typically constructed using the @ref YR_PROTO macro rather
 *  than being initialized directly.
 */
struct yr_protocol {
    size_t len;                      /**< Number of entries in the @p member array. */
    const struct yr_member member[]; /**< Flexible array of member descriptors. */
};


struct yr_buflist {
    void *buf;
    size_t len;
    struct yr_buflist *next;
};

/**
 *  @brief (De-)serialization context.
 */
struct yr_ctx {
    union {
        unsigned char *obj_out;
        const unsigned char *obj_in;
    };
    unsigned char *curser;
    size_t ser_struct_len;
    size_t ser_buffers_len;
    bool container_ready;
    void *streamfield;
    void *streamfield_len;
    void *streambuf;
    struct yr_member root;
    struct yr_buflist *todo;
    struct yr_buflist *trash;
    struct yr_buflist *members;
};

// ------------ api ------------
/**
 *  @brief Starts serialization by creating a context
 *
 *  @param[in] proto        Protocol defining the structure to be serialized
 *  @param[in] input        Points to the actual structure
 *
 *  @return     a serialization context or NULL on error
 */
struct yr_ctx *yr_serialize(const yr_proto_t *proto, void *input);

/**
 *  @brief Retrieves the next (serialized) buffer for sending.
 *
 *  This does not copy any user-owned buffers from the source structure,
 *  so they must remain available until all data is sent. Reserved memory is freed in yr_finish(..).
 *
 *  @param[in]  ctx         The serialization context
 *  @param[out] buf         The data
 *  @param[out] len         Length of data
 *
 *  @retval >0  we are finished, all buffers have been provided
 *  @retval  0  buf points to the next buffer
 *  @retval <0  error
 */
int yr_serialize_getbuf(struct yr_ctx *ctx, /*OUT*/ const unsigned char **buf, /*OUT*/ size_t *len);

/**
 *  @brief Retrieves the entire serialized buffer by copying it from chunks. It must be freed by the user.
 *
 *  @param[in]  ctx         The serialization context
 *  @param[out] buf         The data (must be freed by user)
 *  @param[out] len         Length of data
 *
 *  @retval  0  ok
 *  @retval <0  error
 */
int yr_serialize_copyall(struct yr_ctx *ctx, /*OUT*/ unsigned char **buf, /*OUT*/ size_t *len);

/**
 *  @brief Starts deserialization by creating a context
 *
 *  @param[in] proto        Protocol defining the structure to be deserialized
 *  @param[in] output       Points to the structure that should be filled
 *
 *  @return     a deserialization context or NULL on error
 */
struct yr_ctx *yr_deserialize(const yr_proto_t *proto, void *output);

/**
 *  @brief Retrieves the next buffer to hold a chunk of incoming serialized data.
 *
 *  The deserializer takes ownership of the incoming data buffers, and releases them in yr_finish(..).
 *  The destination structure only contains valid pointers until yr_finish(..) is called.
 *
 *  @param[in]  ctx         The deserialization context
 *  @param[out] buf         The buffer
 *  @param[out] len         Length of buffer
 *  @param[out] streambuf_len  Requested buffer size, in case next payload is for a field of type YR_STREAM
 *
 *  @retval  2  finished, destination structure is valid, a buffer for YR_STREAM field was reserved
 *  @retval  1  finished, destination structure is valid, no buffer was returned
 *  @retval  0  buf points to the next buffer
 *  @retval <0  error
 */
int yr_deserialize_getbuf(struct yr_ctx *ctx, /*OUT*/ unsigned char **buf, /*OUT*/ size_t *len, size_t streambuf_len);

/**
 *  @brief Process a buffer of serialized data.
 *
 *  It will be copied into buffers allocated via yr_deserialize_getbuf(..),
 *  so the passed data does not need to remain available upon using the result.
 *  Return value is what the next yr_deserialize_getbuf(..) would return.
 *
 *  @param[in]  ctx         The deserialization context
 *  @param[out] buf         The buffer
 *  @param[out] len         Length of buffer
 *
 *  @retval  2  finished, destination structure is valid, more data may be provided for a field of type YR_STREAM
 *  @retval  1  finished, destination structure is valid
 *  @retval  0  more data is needed
 *  @retval <0  error
 */
int yr_deserialize_copyall(struct yr_ctx *ctx, const unsigned char *buf, size_t len);

/**
 *  @brief Cleanup all reserved space. This invalidates any results.
 *
 *  @param[in,out]  pctx    The serialization context (set to NULL)
 */
void yr_finish(struct yr_ctx **pctx);

/**
 *  @def YR_PROTO(...)
 *  @brief Wrapper to initialize a yr_proto_t, setting up the correct length
 *
 *  @param[in]  ...         A list of members
 */
#define YR_PROTO(...) { \
    .len = sizeof((struct yr_member[]){ __VA_ARGS__ }) / sizeof(struct yr_member), \
    .member = { __VA_ARGS__ } \
}

/* !!! Define this before including the header if you need the api: !!! */
#ifdef YR_IMPLEMENT

// ------------ implementation ------------

static inline uint16_t yr_hton16(uint16_t x) { unsigned char b[2]={(unsigned char)(x>>8),(unsigned char)x}; uint16_t r; memcpy(&r,b,sizeof r); return r; }
static inline uint32_t yr_hton32(uint32_t x) { unsigned char b[4]={(unsigned char)(x>>24),(unsigned char)(x>>16),(unsigned char)(x>>8),(unsigned char)x}; uint32_t r; memcpy(&r,b,sizeof r); return r; }
static inline uint64_t yr_hton64(uint64_t x) { unsigned char b[8]={(unsigned char)(x>>56),(unsigned char)(x>>48),(unsigned char)(x>>40),(unsigned char)(x>>32),(unsigned char)(x>>24),(unsigned char)(x>>16),(unsigned char)(x>>8),(unsigned char)x}; uint64_t r; memcpy(&r,b,sizeof r); return r; }
#define yr_ntoh16 yr_hton16
#define yr_ntoh32 yr_hton32
#define yr_ntoh64 yr_hton64
#define noop(y) y
#define error(...) fprintf(stderr, __VA_ARGS__)
#define yr_enum_fn yr_hton16
#define yr_size_fn yr_hton32

#define YR_IO(type,fn,dst,src,inc)  \
{                                   \
    type v;                         \
    memcpy(&v, src, sizeof v);      \
    v = fn(v);                      \
    memcpy(dst, &v, sizeof v);      \
    inc += sizeof v;                \
}

static const size_t yr_dtsize[] = {
    [YR_INT8]       = sizeof(int8_t),
    [YR_INT16]      = sizeof(int16_t),
    [YR_INT32]      = sizeof(int32_t),
    [YR_INT64]      = sizeof(int64_t),
    [YR_UINT8]      = sizeof(uint8_t),
    [YR_UINT16]     = sizeof(uint16_t),
    [YR_UINT32]     = sizeof(uint32_t),
    [YR_UINT64]     = sizeof(uint64_t),
    [YR_ENUM]       = sizeof(yr_enum_t),
    [YR_SIZE]       = sizeof(yr_size_t),
    [YR_FLOAT32]    = sizeof(float),
    [YR_FLOAT64]    = sizeof(double),
};

static int yr_write_container(struct yr_ctx *ctx, const yr_proto_t *proto, size_t base_offs, bool is_union, yr_enum_t sel);
static int yr_write_member(struct yr_ctx *ctx, const struct yr_member *member, size_t base_offs);
static int yr_read_container(struct yr_ctx *ctx, const yr_proto_t *proto, size_t base_offs, bool is_union, yr_enum_t sel);
static int yr_read_member(struct yr_ctx *ctx, const struct yr_member *member, size_t base_offs);
static size_t yr_size_container(const yr_proto_t *proto, bool is_union, bool max_size);

// ------------ helpers ------------

static int yr_buflist_pushback(struct yr_buflist **list, void *buf, size_t len)
{
    for (; *list; list = &(*list)->next)
        ;
    *list = (struct yr_buflist *)calloc(1, sizeof **list);
    if (!*list) {
        return -1;
    }
    (*list)->buf = buf;
    (*list)->len = len;
    return 0;
}

static int yr_buflist_pushfront(struct yr_buflist **list, void *buf, size_t len)
{
    struct yr_buflist *front = (struct yr_buflist *)calloc(1, sizeof *front);
    if (!front) {
        return -1;
    }
    front->next = *list;
    front->buf = buf;
    front->len = len;
    *list = front;
    return 0;
}

static int yr_buflist_popfront(struct yr_buflist **list, void **buf, size_t *len)
{
    if (!list || !buf || !len) {
        return -1;
    }
    if (!*list) {
        *buf = NULL;
        *len = 0;
        return 1;
    }
    struct yr_buflist *front = *list;
    *list = front->next;
    *buf = front->buf;
    *len = front->len;
    free(front);
    return 0;
}

static void yr_buflist_free(struct yr_buflist **list, bool free_content)
{
    if (!list) {
        return;
    }
    struct yr_buflist *front = *list;
    while (front) {
        *list = front->next;
        if (free_content) {
            free(front->buf);
        }
        free(front);
        front = *list;
    }
}

static void *yr_mkbuffer(struct yr_ctx *ctx, struct yr_buflist **dst, size_t len, bool front)
{
    unsigned char *buf = malloc(len + 1);
    if (!buf
        || (!front && yr_buflist_pushback(dst, buf, len) < 0)
        || (front && yr_buflist_pushfront(dst, buf, len) < 0)
        || yr_buflist_pushfront(&ctx->trash, buf, len) < 0
    ) {
        free(buf);
        return NULL;
    }
    buf[len] = 0; // for strings
    return buf;
}

static size_t yr_size_member(const struct yr_member *member, bool max_size)
{
    switch (member->type) {
        case YR_UNION:  return yr_dtsize[YR_ENUM] + (max_size ? yr_size_container(member->proto, true, max_size) : 0);
        case YR_STRUCT: return yr_size_container(member->proto, false, max_size);
        case YR_BUFFER:
        case YR_STRING: return yr_dtsize[YR_SIZE];
        case YR_STREAM: return 0;
        default:        return yr_dtsize[member->type];
    }
}

static size_t yr_size_container(const yr_proto_t *proto, bool is_union, bool max_size) {
    size_t len = 0;
    for (size_t i = 0; i < proto->len; ++i) {
        size_t nextlen = yr_size_member(&proto->member[i], max_size);
        len = is_union ? (len > nextlen ? len : nextlen) : len + nextlen;
    }
    return len;
}

// ------------ serializer ------------

static int yr_write_member(struct yr_ctx *ctx, const struct yr_member *member, size_t base_offs)
{
    yr_size_t len;
    yr_enum_t sel;
    const char *str;
    const unsigned char *mem;
    /* source: */
    const unsigned char *field      = ctx->obj_in + base_offs + member->offs;
    const unsigned char *field_aux  = ctx->obj_in + base_offs + member->offs_aux;

    switch (member->type) {
        case YR_INT8:
        case YR_UINT8:
            YR_IO(uint8_t, noop, ctx->curser, field, ctx->curser);
            break;
        case YR_INT16:
        case YR_UINT16:
            YR_IO(uint16_t, yr_hton16, ctx->curser, field, ctx->curser);
            break;
        case YR_INT32:
        case YR_UINT32:
        case YR_FLOAT32:
            YR_IO(uint32_t, yr_hton32, ctx->curser, field, ctx->curser);
            break;
        case YR_INT64:
        case YR_UINT64:
        case YR_FLOAT64:
            YR_IO(uint64_t, yr_hton64, ctx->curser, field, ctx->curser);
            break;
        case YR_ENUM:
            YR_IO(yr_enum_t, yr_enum_fn, ctx->curser, field, ctx->curser);
            break;
        case YR_SIZE:
            YR_IO(yr_size_t, yr_size_fn, ctx->curser, field, ctx->curser);
            break;
        case YR_BUFFER:
            memcpy(&mem, field, sizeof mem);
            memcpy(&len, field_aux, sizeof len);
            YR_IO(yr_size_t, yr_size_fn, ctx->curser, &len, ctx->curser);
            ctx->ser_buffers_len += len;
            if (yr_buflist_pushback(&ctx->todo, (void *)mem, len) < 0) {
                return -1;
            }
            break;
        case YR_STRING:
            memcpy(&str, field, sizeof str);
            len = str ? strlen(str) : 0;
            YR_IO(yr_size_t, yr_size_fn, ctx->curser, &len, ctx->curser);
            ctx->ser_buffers_len += len;
            if (yr_buflist_pushback(&ctx->todo, (void *)str, len) < 0) {
                return -2;
            }
            break;
        case YR_STRUCT:
            if (yr_write_container(ctx, member->proto, base_offs + member->offs, false, 0) < 0) {
                return -3;
            }
            break;
        case YR_UNION:
            memcpy(&sel, field_aux, sizeof sel);
            YR_IO(yr_enum_t, yr_enum_fn, ctx->curser, &sel, ctx->curser);
            if (yr_write_container(ctx, member->proto, base_offs + member->offs, true, sel) < 0) {
                return -4;
            }
            break;
        case YR_STREAM:
            if (ctx->streambuf) {
                error("%s(): illegal protocol, more than one stream field.", __func__);
                return -6;
            }
            ctx->streamfield = (void *)field;
            ctx->streamfield_len = (void *)field_aux;
            break;
        default:
            error("%s(): type %i not supported (yet).", __func__, (int)member->type);
            return -5;
    }
    return 0;
}

static int yr_write_container(struct yr_ctx *ctx, const yr_proto_t *proto, size_t base_offs, bool is_union, yr_enum_t sel)
{
    if (is_union) {
        if (sel < 0 || sel >= proto->len) {
            return 0; /* enum out of bounds => no union element selected */
        }
        if (yr_size_member(&proto->member[sel], false) == 0) {
            return yr_write_member(ctx, &proto->member[sel], base_offs) < 0 ? -2 : 0;
        }
        /* stash union members after struct content */
        if (yr_buflist_pushback(&ctx->members, (void *)&proto->member[sel], base_offs) < 0) {
            return -1;
        }
    } else {
        /* struct */
        for (size_t i = 0; i < proto->len; ++i) {
            if (yr_write_member(ctx, &proto->member[i], base_offs) < 0) {
                return -2;
            }
        }
    }
    return 0;
}

struct yr_ctx *yr_serialize(const yr_proto_t *proto, void *input)
{
    size_t offs;
    void *vbuf;
    struct yr_buflist *list = NULL;
    struct yr_buflist **plast;
    struct yr_ctx *ctx = (struct yr_ctx *)calloc(1, sizeof *ctx);
    if (!ctx) {
        return NULL;
    }
    ctx->obj_in = input;
    ctx->root.type = YR_STRUCT;
    ctx->root.proto = proto;
    if (yr_buflist_pushfront(&ctx->members, &ctx->root, 0) < 0) {
        goto err;
    }
    while (!yr_buflist_popfront(&ctx->members, &vbuf, &offs)) {
        const struct yr_member *member = vbuf;
        size_t len = yr_size_member(member, false);
        ctx->curser = yr_mkbuffer(ctx, &list, len, false);
        ctx->ser_struct_len += len;
        if (!ctx->curser || yr_write_member(ctx, member, offs) < 0) {
            goto err;
        }
    }
    for (plast = &list; *plast; plast = &(*plast)->next)
        ;
    *plast = ctx->todo;
    ctx->todo = list;
    return ctx;
err:
    yr_finish(&ctx);
    return NULL;
}

int yr_serialize_getbuf(struct yr_ctx *ctx, /*OUT*/ const unsigned char **buf, /*OUT*/ size_t *len)
{
    if (!ctx) {
        return -1;
    }
    void *vbuf = NULL;
    int rc;
    do {
        rc = yr_buflist_popfront(&ctx->todo, &vbuf, len);
    } while(!rc && !*len);
    *buf = vbuf;
    if (rc > 0 && ctx->streamfield) {
        yr_size_t len_on_yr;
        memcpy(buf, ctx->streamfield, sizeof *buf);
        memcpy(&len_on_yr, ctx->streamfield_len, sizeof len_on_yr);
        *len = len_on_yr;
    }
    return rc;
}

int yr_serialize_copyall(struct yr_ctx *ctx, /*OUT*/ unsigned char **buf, /*OUT*/ size_t *len)
{
    if (!ctx || !len) {
        return -1;
    }
    size_t outsize = ctx->ser_struct_len + ctx->ser_buffers_len;
    if (ctx->streamfield) {
        yr_size_t len_on_yr;
        memcpy(&len_on_yr, ctx->streamfield_len, sizeof len_on_yr);
        outsize += len_on_yr;
    }
    unsigned char *dest = outsize ? (unsigned char *)malloc(outsize) : NULL;
    if (outsize && !dest) {
        return -2;
    }
    size_t step;
    size_t pos = 0;
    while (pos < outsize) {
        const unsigned char *chunk;
        if (yr_serialize_getbuf(ctx, &chunk, &step) < 0) {
            free(dest);
            return -3;
        }
        memcpy(dest + pos, chunk, step);
        pos += step;
    }
    *buf = dest;
    *len = outsize;
    return 0;
}

// ------------ deserializer ------------

static int yr_read_member(struct yr_ctx *ctx, const struct yr_member *member, size_t base_offs)
{
    yr_size_t len;
    yr_enum_t sel;
    const char *str;
    const unsigned char *mem;
    /* destination: */
    unsigned char *field        = ctx->obj_out + base_offs + member->offs;
    unsigned char *field_aux    = ctx->obj_out + base_offs + member->offs_aux;

    switch (member->type) {
        case YR_INT8:
        case YR_UINT8:
            YR_IO(uint8_t, noop, field, ctx->curser, ctx->curser);
            break;
        case YR_INT16:
        case YR_UINT16:
            YR_IO(uint16_t, yr_ntoh16, field, ctx->curser, ctx->curser);
            break;
        case YR_INT32:
        case YR_UINT32:
        case YR_FLOAT32:
            YR_IO(uint32_t, yr_ntoh32, field, ctx->curser, ctx->curser);
            break;
        case YR_INT64:
        case YR_UINT64:
        case YR_FLOAT64:
            YR_IO(uint64_t, yr_ntoh64, field, ctx->curser, ctx->curser);
            break;
        case YR_ENUM:
            YR_IO(yr_enum_t, yr_enum_fn, field, ctx->curser, ctx->curser);
            break;
        case YR_SIZE:
            YR_IO(yr_size_t, yr_size_fn, field, ctx->curser, ctx->curser);
            break;
        case YR_BUFFER:
            YR_IO(yr_size_t, yr_size_fn, &len, ctx->curser, ctx->curser);
            mem = (const unsigned char *)yr_mkbuffer(ctx, &ctx->todo, len, false);
            if (!mem) {
                return -1;
            }
            memcpy(field, &mem, sizeof mem);
            memcpy(field_aux, &len, sizeof len);
            break;
        case YR_STRING:
            YR_IO(yr_size_t, yr_size_fn, &len, ctx->curser, ctx->curser);
            str = (const char *)yr_mkbuffer(ctx, &ctx->todo, len, false);
            if (!str) {
                return -2;
            }
            memcpy(field, &str, sizeof str);
            break;
        case YR_STRUCT:
            if (yr_read_container(ctx, member->proto, base_offs + member->offs, false, 0) < 0) {
                return -3;
            }
            break;
        case YR_UNION:
            YR_IO(yr_enum_t, yr_enum_fn, &sel, ctx->curser, ctx->curser);
            memcpy(field_aux, &sel, sizeof sel);
            if (yr_read_container(ctx, member->proto, base_offs + member->offs, true, sel) < 0) {
                return -4;
            }
            break;
        case YR_STREAM:
            if (ctx->streamfield) {
                error("%s(): illegal protocol, more than one stream field.", __func__);
                return -6;
            }
            ctx->streamfield = field;
            ctx->streamfield_len = field_aux;
            memset(ctx->streamfield, 0, sizeof mem);
            memset(ctx->streamfield_len, 0, sizeof len);
            break;
        default:
            error("%s(): type not supported (yet).", __func__);
            return -5;
    }
    return 0;
}

static int yr_read_container(struct yr_ctx *ctx, const yr_proto_t *proto, size_t base_offs, bool is_union, yr_enum_t sel)
{
    if (is_union) {
        if (sel < 0 || sel >= proto->len) {
            return 0;
        }
        if (yr_size_member(&proto->member[sel], false) == 0) {
            return yr_read_member(ctx, &proto->member[sel], base_offs) < 0 ? -2 : 0;
        }
        /* stash union members after struct content */
        yr_buflist_pushback(&ctx->members, (void *)&proto->member[sel], base_offs);
    } else {
        /* struct */
        for (size_t i = 0; i < proto->len; ++i) {
            if (yr_read_member(ctx, &proto->member[i], base_offs) < 0) {
                return -2;
            }
        }
    }
    return 0;
}

struct yr_ctx *yr_deserialize(const yr_proto_t *proto, void *output)
{
    struct yr_ctx *ctx = (struct yr_ctx *)calloc(1, sizeof *ctx);
    if (!ctx) {
        return NULL;
    }
    ctx->root.type = YR_STRUCT;
    ctx->root.offs = 0;
    ctx->root.proto = proto;
    ctx->obj_out = output;
    return ctx;
}

int yr_deserialize_getbuf(struct yr_ctx *ctx, /*OUT*/ unsigned char **buf, /*OUT*/ size_t *len, size_t streambuf_len)
{
    if (!ctx || !buf) {
        return -1;
    }
    void *vbuf = NULL;
    if (!ctx->todo && !ctx->container_ready) {
        size_t offs;
        if (!yr_buflist_popfront(&ctx->members, &vbuf, &offs)) {
            const struct yr_member *member = vbuf;
            if (yr_read_member(ctx, member, offs) < 0) {
                return -2;
            }
        } else {
            yr_buflist_pushfront(&ctx->members, &ctx->root, 0);
        }
        if (!ctx->members) {
            ctx->container_ready = true;
        } else {
            ctx->curser = yr_mkbuffer(ctx, &ctx->todo, yr_size_member((const struct yr_member *)ctx->members->buf, false), true);
        }
    }
    int rc;
    do {
        rc = yr_buflist_popfront(&ctx->todo, &vbuf, len);
    } while (!rc && !*len);
    if (rc > 0 && !ctx->container_ready) {
        /* to-do list cleared, but the outer container hasn't been fully populated */
        return yr_deserialize_getbuf(ctx, buf, len, streambuf_len);
    }

    *buf = vbuf;
    if (rc > 0 && ctx->streamfield) {
        free(ctx->streambuf);
        ctx->streambuf = streambuf_len ? (unsigned char *)malloc(streambuf_len) : NULL;
        if (streambuf_len && !ctx->streambuf) {
            return -3;
        }
        yr_size_t len_on_yr = streambuf_len;
        memcpy(ctx->streamfield, &ctx->streambuf, sizeof ctx->streambuf);
        memcpy(ctx->streamfield_len, &len_on_yr, sizeof len_on_yr);
        *buf = ctx->streambuf;
        *len = streambuf_len;
        return 2;
    }
    return rc;
}

int yr_deserialize_copyall(struct yr_ctx *ctx, const unsigned char *buf, size_t len)
{
    if (!ctx || (len && !buf)) {
        return -10;
    }
    unsigned char *dest;
    size_t step;
    while (len > 0) {
        int ret = yr_deserialize_getbuf(ctx, &dest, &step, len);
        if (ret != 0) {
            if (ret == 2) {
                /* we are in "streaming mode" */
                memcpy(dest, buf, step);
            }
            return ret;
        }
        if (len < step) {
            if (yr_buflist_pushfront(&ctx->todo, dest + len, step - len) < 0) {
                return -11;
            }
            memcpy(dest, buf, len);
            return 0;
        }
        memcpy(dest, buf, step);
        len -= step;
        buf += step;
    }
    /* peek, for proper return code */
    int rc = yr_deserialize_getbuf(ctx, &dest, &step, 0);
    if (!rc) {
        return yr_buflist_pushfront(&ctx->todo, dest, step) ? -12 : 0;
    }
    return rc;
}

// ----- cleanup -------

void yr_finish(struct yr_ctx **pctx)
{
    if (!pctx || !*pctx) return;
    /* take out the trash */
    yr_buflist_free(&(*pctx)->trash, true);
    yr_buflist_free(&(*pctx)->todo, false);
    yr_buflist_free(&(*pctx)->members, false);
    free((*pctx)->streambuf);
    free(*pctx);
    *pctx = NULL;
}

#endif

#ifdef __cplusplus
}
#endif

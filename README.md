# c-toolkit

Collection of header-only C implementations of some core functionality. Each file is self-contained and dependency-free.

## Contents

| Header | What it provides |
| --- | --- |
| [`container.h`](container.h) | Three different types of dynamic containers using an almost uniform API: a vector (`vec`), a power-of-two dynamic ring buffer (`rbuf`), and an unrolled linked list (`ulist`). All store `void*`. |
| [`yr.h`](yr.h) | **yr — tinY serializeR**: Reflection-style serialization driven by a protocol table of struct member offsets, with network-byte-order output and a chunked, zero-copy streaming API. |

## Usage

Include the header anywhere you need the declarations, and define the implementation macro `*_IMPLEMENT` in **exactly one** `.c` file:

```c
/* in exactly one source file */
#define CON_IMPLEMENT      /* for container.h */
#include "container.h"

#define YR_IMPLEMENT       /* for yr.h */
#include "yr.h"
```

---

## container.h

Three containers share the same vocabulary: `*_append`, `*_prepend`, `*_pop`, `*_popback`, `*_popfront`, `*_delete`, `*_filter`, plus `*_at` / `*_foreach` macros. `vec` and `rbuf` are allocated on the stack and initialized with `*_alloc`; `ulist` is a heap-linked structure addressed through a head pointer.

### Vector / ring buffer example

```c
#define CON_IMPLEMENT
#include "container.h"

vec v;
vec_alloc(&v, 0);              /* 0 => default capacity */

vec_append(&v, (void *)"a");
vec_prepend(&v, (void *)"b");

vec_foreach(v, char *, s) {
    printf("%s\n", s);         /* break / continue are allowed */
}

void *first = vec_at(v, 0);    /* CON_NOTHING (NULL) if out of bounds */
void *back  = vec_popback(&v);

vec_free(&v, NULL);            /* pass a void(*)(void*) to also free elements */
```

`rbuf` mirrors this API (`rbuf_alloc`, `rbuf_append`, `rbuf_at`, `rbuf_foreach`, …).

> **`clear` vs `free`:** `*_clear` empties the container and may shrink it, but keeps it usable. `vec_free` / `rbuf_free` release the backing memory and must be called before the container goes out of scope.

### Unrolled linked list example

```c
ulist *l = NULL;               /* the list is its own head pointer */

ulist_append(&l, (void *)"x");
ulist_prepend(&l, (void *)"y");

ulist_foreach(l, char *, s) {
    printf("%s\n", s);
}

void *e = ulist_at(l, 0, false);   /* `back = true` searches from the tail, O(n) */
ulist_clear(l, NULL);              /* frees the whole list */
```

### Compile-time options

| Macro | Effect |
| --- | --- |
| `CON_IMPLEMENT` | Emit the implementation (define in one TU). |
| `CON_DEBUG` | Enable `con_debug(...)` resize/merge tracing. |
| `con_out` | Output function for debug/test messages (default `printf`). |
| `CON_NOTHING` | Sentinel returned for out-of-bounds / empty (default `NULL`). |
| `CON_STD_VEC_CAP` / `CON_STD_RBUF_CAP` | Default initial capacity (default `64`). |
| `CON_ULIST_ARRSIZE` | Elements per unrolled-list node (default `256`). |
| `CON_UNITTEST` | Compile the built-in test suite (`test_main()`). |

> **Note:** `*_at` and `*_foreach` are macros that evaluate their arguments more than once — avoid passing expressions with side effects.

---

## yr.h

YR serializes a C struct by walking a **protocol**: a table that maps each serializable member to its type and its `offsetof` within the struct. The same protocol drives both serialization and deserialization, so there is one source of truth per type.

Supported member types: fixed-width integers (`YR_INT8…64`, `YR_UINT8…64`), `YR_FLOAT32/64`, `YR_ENUM`, `YR_SIZE`, `YR_STRING` (NUL-terminated), `YR_BUFFER` (length-prefixed bytes), `YR_STRUCT`, `YR_UNION`, and `YR_STREAM` (a single growable trailing buffer). Output is big-endian on the wire.

### Defining a protocol

```c
#define YR_IMPLEMENT
#include "yr.h"

struct msg {
    int32_t   id;
    uint16_t  len;
    const unsigned char *payload;
};

static const yr_proto_t msg_p = YR_PROTO(
    { YR_INT32,  offsetof(struct msg, id) },
    /* a buffer needs the offset of its length field as the auxiliary offset */
    { YR_BUFFER, offsetof(struct msg, payload),
                 offsetof(struct msg, len) },
);
```

Nested `YR_STRUCT` / `YR_UNION` members point `.proto` at the child protocol; a `YR_UNION` also takes the offset of its selector (`yr_enum_t`) as the auxiliary offset.

### Serialize

```c
struct msg src = { 7, 3, (const unsigned char[]){ 1, 2, 3 } };

struct yr_ctx *ctx = yr_serialize(&msg_p, &src);

/* Streaming: emits chunks without copying your buffers,
   so `src` must stay valid until you are done. */
const unsigned char *chunk;
size_t n;
while (yr_serialize_getbuf(ctx, &chunk, &n) == 0) {
    /* send n bytes of chunk */
}
yr_finish(&ctx);

/* …or grab one contiguous, caller-owned buffer: */
ctx = yr_serialize(&msg_p, &src);
unsigned char *blob; size_t blob_len;
yr_serialize_copyall(ctx, &blob, &blob_len);
yr_finish(&ctx);
/* free(blob) when done */
```

### Deserialize

```c
struct msg dst;
struct yr_ctx *ctx = yr_deserialize(&msg_p, &dst);

/* feed received bytes; works even one byte at a time */
int rc = yr_deserialize_copyall(ctx, blob, blob_len);
/* rc == 1 => dst is fully populated */

/* dst's buffers/strings stay valid only until yr_finish() */
yr_finish(&ctx);   /* call this AFTER you are done reading dst */
```

A fuller worked example (nested struct + union) lives in the comment block at the top of [`yr.h`](yr.h).

### Notes & limits

- Lengths use `yr_size_t` (`uint32_t`), so individual buffers/strings are capped at 4 GiB; `YR_STRING` is `strlen`-based, so embedded NULs truncate.
- At most one `YR_STREAM` field per protocol.
- The deserializer allocates based on length fields read from the input, so treat incoming data as **trusted** (or add your own bounds check) when reading from a network.

---

## Tests

`container.h` ships a self-test that runs an identical random sequence of operations against all three containers and asserts they stay equivalent:

```sh
cat > t.c <<'EOF'
#define CON_IMPLEMENT
#define CON_UNITTEST
#include "container.h"
int main(void) { test_main(); return 0; }
EOF
gcc -std=c11 -O2 t.c -o t && ./t
```

## License

MIT — see [LICENSE](LICENSE).

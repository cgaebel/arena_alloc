#include "arena.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * saucetenuto (reddit):
 *
 * How would you implement an arena capable of holding 1024 64-byte objects,
 * with O(1) allocate and free and with only one bit of overhead per object?
 *
 * Clark:
 *
 * What about 0 bits of overhead per object, with O(1) reset, alloc, and free?
 */

/*** BEGIN CUSTOMIZATION ***/

// Default implementation - just use standard malloc + free. Feel free to
// redefine ALLOC and FREE to fit your needs. Obviously, their prototypes must
// be identical to those of malloc and free.
#include <stdlib.h>
#define ALLOC malloc
#define FREE  free

// By default, turn on heap checking only in DEBUG builds.
// If HEAP_CHECK is defined, arena_free and arena_reset become O(n), but you get
// double-free protection.
#ifdef DEBUG
    #define HEAP_CHECK
#endif

#include <stdio.h>
// Set this to your own custom handling. Once an error has been triggered,
// heap state is undefined. I suggest changing this to a function which either
// dumps core or breaks into a debugger. A backtrace is extremely useful when
// double-frees are detected.
static void error(const char* message)
{
    fputs(message, stderr);
    fflush(stdout);
    exit(314);
}

/*** END CUSTOMIZATION ***/

// This is actually a shadow structure in that we allocate AT LEAST enough
// space for a node. An unallocated node uses the structure to store a pointer
// to the next free node. When the node is allocated, the whole structure
// (including the space for the `next' pointer) is used for user data.
struct node {
    uint64_t     guard; // Protects the node from corruption.
    struct node* next;  // Points to the next free object in the buffer.
};

// A fibonacci pattern of bits, to detect corruption. Hopefully unlikely to
// appear in user code.
// 1011000111110000000011111111111110000000000000000000001111111111
// NOTE: The nibbles have been reversed so that on little endian machines the
// most entropy is in the LSBs, where most manipulation takes place.
#define GUARD_BITS 0xFF30000811100F1B

/**
 * TODO
 *   - Debug arena. Double-free and overflow protection.
 *   - Document arena internals.
 *       - lazy initializaiton
 *       - user data and arena data sharing space -> 0 overhead
 *       - Efficiency
 *       - Dangers.
 */
struct arena {
    size_t size;    // The size of each node.
    size_t count;   // The number of nodes in the buffer.
    bool lazy_init; // True if we're still initializing the buffer.

    struct node* free_list; // Points to the first node in the free list.
    struct node* bufstart;  // Points to the first non-initialized element of
                            // the buffer. If lazy_init is false, is undefined.
    struct node* buffer;    // Points to the raw arena buffer.
    struct node* bufend;    // Points one past the last element in the buffer.
};

// Returns true if n is in the interval [low, high)
static inline bool in_range(const void* low, const void* n, const void* high)
{
    return low <= n && n < high;
}

#ifdef HEAP_CHECK
    // Return true if the heap is ok. O(n).
    static void check_heap(struct arena* a)
    {
        size_t i = 0;
        struct node* p = a->free_list;

        // Ensure each pointer is in [buffer, bufend)
        for(; p != NULL; p = p->next, ++i)
        {
            if(i >= a->count)
                error("Either more elements have been freed than physically "
                      "possible or the heap has become cyclic due to a "
                      "double-free bug.");
        }
    }

    static void detect_double_free(struct node* n, struct node* free_list)
    {
        for(struct node* c = free_list; c != NULL; c = c->next)
            if(c == n)
                error("Double-free detected.");
    }
#else
    #define check_heap(x)
    #define detect_double_free(x, y)
#endif

static inline size_t max(size_t a, size_t b)
{
    return a <= b ? b : a;
}

struct arena* arena_init(size_t size, size_t count)
{
    // Make sure we have enough room for underlying heap data.
    size = max(size, sizeof(struct node*));

    size_t allocated = sizeof(struct arena) + count*size;

    void* buf = ALLOC(allocated);
    if(buf == NULL) return NULL;

    return arena_init_(size, count, buf, allocated);
}

struct arena* arena_init_(size_t size, size_t count, void* mem, size_t len)
{
    // Make sure we have enough room for underlying heap data.
    size = max(size, sizeof(struct node*));

    if(len < sizeof(struct arena) + size*count)
        return NULL;

    struct arena* a   = mem;
    struct node*  buf = (struct node*)((char*)mem + sizeof(struct arena));

    *a = (struct arena) {
        .size      = size,
        .count     = count,
        .lazy_init = true,
        .free_list = NULL,
        .bufstart  = buf,
        .buffer    = buf,
        .bufend    = (struct node*)((char*)buf + count*size)
    };

    return a;
}

void arena_reset(struct arena* a)
{
    check_heap(a);

    a->lazy_init = true;
    a->bufstart = a->buffer;
    a->free_list = NULL;
    // a->bufend never changes. Leave it alone.
}

// Returns the value, then sets it to something else.
static inline struct node* ret_and_set(struct node** n, void* v)
{
    struct node* r = *n;
    *n = v;
    return r;
}

// Unlinks a node from a free list and returns it.
static void* recycle(struct node** free_list)
{
    // Oh no we're out of recyclable elements!
    if(*free_list == NULL) return NULL;

    if((*free_list)->guard != GUARD_BITS)
        error("Use of previously-freed pointer detected.");

    return ret_and_set(free_list, (*free_list)->next);
}

static inline void* lazy_alloc(struct arena* a)
{
    return a->bufstart == a->bufend ? ((a->lazy_init = false), recycle(&a->free_list))
                                    : ret_and_set(&a->bufstart, (char*)a->bufstart + a->size);
    //                                ^--                 a->bufstart++                   --^
}

void* arena_alloc(struct arena* a)
{
    return a->lazy_init ? lazy_alloc(a)
                        : recycle(&a->free_list);
}

void arena_free(struct arena* a, void* p)
{
    struct node* n = p;

    if(n == NULL) return;

    if(!in_range(a->buffer, p, a->bufend))
        error("Trying to free a pointer which was not allocated in this arena.");

    check_heap(a);
    detect_double_free(p, a->free_list);

    n->guard = GUARD_BITS;
    n->next = a->free_list;
    a->free_list = n;
}

void arena_destroy(struct arena* a)
{
    check_heap(a);
    FREE(a);
}

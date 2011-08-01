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
 *
 * HOW IT WORKS:
 *
 * The arena consists of two parts - a raw buffer which allocations are made
 * out of and a linked list of unallocated elements.
 *
 * When the arena is first initialized, the linked list of free elements is
 * empty since no elements have yet been freed. bufstart is then set to the
 * beginning of the buffer so for the next little while, allocations can just
 * move the pointer forward to get the next chunk.
 *
 * When arena_free gets called on previously allocated memory, it just prepends
 * the newly freed chunk to the free list. In this implementation, the first few
 * bytes of the memory given to the user get used for this purpose.
 *
 * When we've given out the whole buffer (with alloc_free) possibly being
 * called throughout this, we set lazy_init to false since all elements are now
 * either in the free list or have been allocated to the user.
 *
 * Now all allocations just consist of lopping off the head of the free list
 * and reusing it.
 *
 * Finally, if an arena reset happens, we just set lazy_init to true, move
 * bufstart back where it belongs, and empty the free list. Note that we don't
 * actually have to walk the free list, since the contents of each node is
 * undefined anyhow, and the memory won't leak since it's all in the buffer.
 *
 * And voila: O(1) allocation, deallocation, and resetting, in ~100LOC
 *
 * Now a word about debugging and safety.
 *
 * Heap corruption is detected by placing 64 guard bits in the beginning of the
 * allocation handed to the user. If they are modified while the data has been
 * freed, signal an error. This helps guard against dangling pointers and
 *
 * We also use check_heap, an O(n) function which detects cycles in the free
 * list; and detect_double_free, an O(n) function which walks the free list
 * looking for the element we're about to free. Note that by using these,
 * arena_reset and arena_free become O(n), kind of defeating the purpose of
 * this arena. But at least it's safe, dammit.
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
    a->bufstart  = a->buffer;
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

#include "arena.h"

/**
 * saucetenuto (reddit):
 *
 * How would you implement an arena capable of holding 1024 64-byte objects,
 * with O(1) allocate and free and with only one bit of overhead per object?
 *
 * Clark:
 *
 * What about 0 bits of overhead per object, with 3 instruction allocation? =)
 *
 * Of course, this comes with tradeoffs:
 *   1) Resetting and allocating the arena is O(n).
 *   2) Double free, dangling pointers, etc. will wreak havok with the arena.
 *      Currently, there's no support for debugging this. Not even valgrind
 *      will help. You'll just get crazy memory corruption.
 */

// Default implementation - just use standard malloc + free. Feel free to
// change this to suit your needs.
#include <stdlib.h>
#define ALLOC malloc
#define FREE  free

// This is actually a shadow structure in that we allocate AT LEAST enough
// space for a node. An unallocated node uses the structure to store a pointer
// to the next free node. When the node is allocated, the whole structure
// (including the space for the `next' pointer) is used for user data.
union node {
    union node* next; // Points to the next free object in the buffer.
};

struct arena {
    size_t size;  // the size of each node.
    size_t count; // the number of nodes in the buffer.

    union node* first_free; // points to the first free object in the buffer.
    union node buffer[];
};

// Rounds `n' up to the next `alignment' where `alignment' is a power of two.
static inline size_t align(size_t n, size_t alignment)
{
    size_t align_down = n & ~(alignment - 1); // aligned, but rounded down.
    if(n == align_down) return n;
    else                return align_down + alignment;
}

static inline union node* index_buffer(union node* buffer,
                                       size_t node_size,
                                       size_t index)
{
    return (union node*)((char*)buffer + node_size*index);
}

struct arena* arena_init(size_t size, size_t count)
{
    // Align the size to the node next pointer, to allow more efficient aligned
    // array access, and also the hack of storing the next pointer inside free
    // nodes. This will be the size of each node.
    size = align(size, sizeof(union node*));

    // Allocate space for both the arena and the buffer in one go.
    struct arena* a = ALLOC(sizeof(a->first_free) +
                            sizeof(a->size) +
                            sizeof(a->count) +
                            count * size);

    a->size = size;
    a->count = count;

    arena_reset(a);

    return a;
}

void arena_reset(struct arena* a)
{
    size_t size  = a->size,
           count = a->count;

    if(count == 0) return;

    // Set up every element but the last to point to the node next to it.
    for(size_t i = 1; i <= count-1; ++i)
    {
        union node* current = index_buffer(a->buffer, size, i-1);
        union node* next    = index_buffer(a->buffer, size, i  );

        current->next = next;
    }

    // For the last element, have the next pointer going to NULL, creating an
    // array representing a linked list. =)
    index_buffer(a->buffer, size, count-1)->next = NULL;

    a->first_free = a->buffer;
}

void* arena_alloc(struct arena* a)
{
    if(a->first_free == NULL) return NULL;

    union node* e = a->first_free;
    a->first_free = e->next;
    return e;
}

void arena_free(struct arena* a, void* p)
{
    union node* n = p;
    if(n == NULL) return;

    n->next = a->first_free;
    a->first_free = n;
}

void arena_destroy(struct arena* a)
{
    FREE(a);
}

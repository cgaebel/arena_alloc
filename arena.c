#include "arena.h"

#include <stdlib.h>

/**
 * saucetenuto (reddit):
 *
 * How would you implement an arena capable of holding 1024 64-byte objects,
 * with O(1) allocate and free and with only one bit of overhead per object?
 *
 * Clark:
 *
 * What about 0 bits of overhead per object? =)
 */

// This is actually a shadow structure in that we allocate AT LEAST enough
// space for a node. An unallocated node uses the structure to store a pointer
// to the next free node. When the node is allocated, the whole structure
// (including the space for the `next' pointer) is used for user data.
union node {
    union node* next; // Points to the next free object in the buffer.
};

struct arena {
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
    struct arena* a = malloc(sizeof(a->first_free) + count * size);

    // Creates a simple linear linked list.
    for(size_t i = 1; i <= count; ++i)
    {
        union node* current = index_buffer(a->buffer, size, i-1);
        union node* next    = index_buffer(a->buffer, size, i  );

        current->next = i != count ? next : NULL;
    }

    a->first_free = a->buffer;

    return a;
}

void* arena_alloc(struct arena* a)
{
    if(a->first_free == NULL)
        return NULL;

    void* ret = a->first_free;

    a->first_free = a->first_free->next;

    return ret;
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
    free(a);
}

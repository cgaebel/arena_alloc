#pragma once
#include <stddef.h>

/**
 * saucetenuto (reddit):
 *
 * How would you implement an arena capable of holding 1024 64-byte objects,
 * with O(1) allocate and free and with only one bit of overhead per object?
 *
 * Clark:
 *
 * Hell if I know. ;)
 */

struct arena;

struct arena* arena_init(size_t size, size_t count);

/* Frees everything allocated with the arena. */
void arena_reset(struct arena*);

void* arena_alloc(struct arena*);
void arena_free(struct arena*, void*);

void arena_destroy(struct arena*);

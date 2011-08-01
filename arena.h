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

/* Note: All operations on the arena are O(1) if DEBUG is not defined. */
struct arena;

/*
 * arena_init - Initializes an arena with memory from the heap. The arena will
 *              contain `count' elements, all of size `size'. All allocations
 *              made by the arena will be `size' bytes long. When all of the
 *              arena's elements have been allocated, arena_alloc returns NULL.
 *
 * arena_init_ - Initializes an arena using pre-allocated memory at `mem', and
 *               of length `len'. The first two parameters are the same as
 *               arena_init. This version lets you allocate an arena wherever
 *               you want; whether it be inside another arena, on the stack, in
 *               a memory map, or where ever your heart desires.
 *
 *               If you use this version of arena_init, DO NOT call
 *               arena_destroy. It will attempt to deallocate the underlying
 *               memory. You must do that manually, by whatever method it was
 *               originally allocated.
 *
 *               When using arena_init_, len must be at least
 *               max(size, 2*sizeof(size_t))*count + 7*sizeof(size_t) so
 *               that the arena header can be placed directly next to the raw
 *               memory, instead of being heap allocated.
 */
struct arena* arena_init(size_t size, size_t count);
struct arena* arena_init_(size_t size, size_t count, void* mem, size_t len);

void arena_reset(struct arena*);

void* arena_alloc(struct arena*);
void arena_free(struct arena*, void*);

void arena_destroy(struct arena*);

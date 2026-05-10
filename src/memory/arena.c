#include "memory/arena.h"

#include <stdint.h>

static size_t rtc_align_forward(size_t value, size_t alignment)
{
    size_t mask;

    if (alignment <= 1) {
        return value;
    }

    mask = alignment - 1;
    if ((alignment & mask) != 0) {
        return value;
    }

    return (value + mask) & ~mask;
}

void rtc_arena_init(rtc_arena_view_t *arena, rtc_arena_t backing)
{
    if (arena == 0) {
        return;
    }

    arena->data = (unsigned char *)backing.data;
    arena->size = backing.size;
    arena->offset = 0;
}

void *rtc_arena_alloc(rtc_arena_view_t *arena, size_t size, size_t alignment,
                      rtc_capacity_resource_t resource,
                      rtc_capacity_diagnostics_t *diag)
{
    size_t base;
    size_t aligned;
    size_t next;
    uintptr_t address;

    if (arena == 0 || arena->data == 0 || size == 0) {
        if (diag != 0) {
            diag->resource = resource;
            diag->required = size;
            diag->used = arena != 0 ? arena->offset : 0;
        }
        return 0;
    }

    address = (uintptr_t)(arena->data + arena->offset);
    base = (size_t)(address - (uintptr_t)arena->data);
    aligned = rtc_align_forward(base, alignment);
    next = aligned + size;

    if (aligned < base || next < aligned || next > arena->size) {
        if (diag != 0) {
            diag->resource = resource;
            diag->required = size;
            diag->used = arena->offset;
        }
        return 0;
    }

    arena->offset = next;
    return arena->data + aligned;
}

size_t rtc_arena_used(const rtc_arena_view_t *arena)
{
    return arena != 0 ? arena->offset : 0;
}

size_t rtc_arena_remaining(const rtc_arena_view_t *arena)
{
    if (arena == 0 || arena->offset > arena->size) {
        return 0;
    }

    return arena->size - arena->offset;
}

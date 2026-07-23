#include "heap.h"
#include "string.h"

#define HEAP_SIZE (64 * 1024)

static uint8_t heap_area[HEAP_SIZE];
static size_t heap_offset;
static size_t last_alloc_size;

void heap_init(void)
{
    heap_offset = 0;
    last_alloc_size = 0;
    memset(heap_area, 0, HEAP_SIZE);
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return 0;

    /* 8-byte align */
    size = (size + 7u) & ~7u;
    if (heap_offset + size > HEAP_SIZE)
        return 0;

    void *ptr = &heap_area[heap_offset];
    heap_offset += size;
    last_alloc_size = size;
    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr || last_alloc_size == 0)
        return;
    uint8_t *p = ptr;
    if (p + last_alloc_size == &heap_area[heap_offset]) {
        heap_offset -= last_alloc_size;
        last_alloc_size = 0;
    }
}

size_t heap_used(void)
{
    return heap_offset;
}

size_t heap_free_bytes(void)
{
    return HEAP_SIZE - heap_offset;
}

size_t heap_total(void)
{
    return HEAP_SIZE;
}

#include "heap.h"
#include "string.h"
#include "panic.h"
#include <stdint.h>

#define HEAP_SIZE (64 * 1024)
#define HEAP_MAGIC 0xHEApu /* will use numeric */
#undef HEAP_MAGIC
#define HEAP_MAGIC 0x48454150u /* 'HEAP' */

struct heap_hdr {
    uint32_t magic;
    uint32_t size;
};

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
    if (size == 0 || size > HEAP_SIZE / 2)
        return 0;

    size = (size + 7u) & ~7u;
    size_t total = size + sizeof(struct heap_hdr);
    if (heap_offset + total > HEAP_SIZE)
        return 0;

    struct heap_hdr *hdr = (struct heap_hdr *)&heap_area[heap_offset];
    hdr->magic = HEAP_MAGIC;
    hdr->size = (uint32_t)size;
    heap_offset += total;
    last_alloc_size = total;
    return (void *)(hdr + 1);
}

void kfree(void *ptr)
{
    if (!ptr || last_alloc_size == 0)
        return;

    struct heap_hdr *hdr = ((struct heap_hdr *)ptr) - 1;
    if (hdr->magic != HEAP_MAGIC)
        panic("heap: bad free magic");

    uint8_t *p = (uint8_t *)hdr;
    if (p + last_alloc_size == &heap_area[heap_offset]) {
        heap_offset -= last_alloc_size;
        last_alloc_size = 0;
        hdr->magic = 0;
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

#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr); /* bump allocator: only frees if last block */
size_t heap_used(void);
size_t heap_free_bytes(void);
size_t heap_total(void);

#endif

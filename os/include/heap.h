#ifndef NEBULA_HEAP_H
#define NEBULA_HEAP_H

#include "types.h"

/*
 * The kernel heap lives in its own slice of virtual address space, well
 * clear of the identity-mapped region, and grows by mapping fresh frames
 * as demand requires.
 */
#define KHEAP_START    0xD0000000u
#define KHEAP_MAX_SIZE 0x01000000u   /* 16 MiB ceiling */

void  heap_init(void);

void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t size);
void  kfree(void *ptr);

void  heap_stats(size_t *total, size_t *used, size_t *blocks);

#endif /* NEBULA_HEAP_H */

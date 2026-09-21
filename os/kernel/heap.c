/*
 * heap.c -- kernel heap: a first-fit free list with splitting and
 * coalescing.
 *
 * Every block carries an 8-byte header holding its size and a link to
 * the next block. Blocks are kept in address order, which is what makes
 * coalescing a neighbour on free an O(1) check rather than a search.
 *
 * A magic word in the header catches the two mistakes that otherwise
 * corrupt a kernel silently: freeing a pointer the heap never issued,
 * and writing past the end of an allocation.
 */

#include "heap.h"
#include "paging.h"
#include "pmm.h"
#include "kprintf.h"
#include "string.h"

#define BLOCK_MAGIC 0x4E424C4Bu   /* "NBLK" */
#define MIN_SPLIT   32            /* don't bother splitting off scraps */

struct block {
    uint32_t      magic;
    size_t        size;           /* payload bytes, excluding this header */
    struct block *next;
    bool          free;
};

static struct block *heap_head;
static virtaddr_t    heap_break;  /* first virtual address not yet mapped */
static size_t        heap_total;

/* Round up so every payload starts 8-byte aligned. */
static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

/*
 * Extend the heap by mapping more pages at the current break.
 * Returns the newly created free block, or NULL if we cannot grow.
 */
static struct block *heap_grow(size_t needed)
{
    size_t bytes = align_up(needed + sizeof(struct block), PAGE_SIZE);

    if (heap_break + bytes > KHEAP_START + KHEAP_MAX_SIZE)
        return NULL;

    virtaddr_t start = heap_break;

    for (size_t offset = 0; offset < bytes; offset += PAGE_SIZE) {
        physaddr_t frame = pmm_alloc_frame();

        if (!frame || !paging_map(start + offset, frame, PTE_WRITABLE)) {
            /* Undo the partial growth so the heap stays consistent. */
            for (size_t undo = 0; undo < offset; undo += PAGE_SIZE) {
                physaddr_t mapped = paging_resolve(start + undo);
                paging_unmap(start + undo);
                if (mapped)
                    pmm_free_frame(mapped & ~0xFFFu);
            }
            if (frame)
                pmm_free_frame(frame);
            return NULL;
        }
    }

    heap_break += bytes;
    heap_total += bytes;

    struct block *block = (struct block *)start;
    block->magic = BLOCK_MAGIC;
    block->size  = bytes - sizeof(struct block);
    block->next  = NULL;
    block->free  = true;

    /* Link it on at the tail; the list is kept in address order. */
    if (!heap_head) {
        heap_head = block;
    } else {
        struct block *tail = heap_head;
        while (tail->next)
            tail = tail->next;
        tail->next = block;

        /* The new region is contiguous with the old break, so if the
         * previous tail is free the two are really one block. */
        if (tail->free) {
            tail->size += sizeof(struct block) + block->size;
            tail->next = NULL;
            return tail;
        }
    }

    return block;
}

void heap_init(void)
{
    heap_head  = NULL;
    heap_break = KHEAP_START;
    heap_total = 0;

    /* Start with 64 KiB so early allocations do not each fault in a page. */
    if (!heap_grow(64 * 1024))
        panic("heap: could not create the initial kernel heap");

    kprintf("[heap] %u KiB at %p\n", (uint32_t)(heap_total / 1024),
            (void *)KHEAP_START);
}

/* Split block so it holds exactly `size` bytes, if the remainder is
 * large enough to be worth tracking. */
static void split_block(struct block *block, size_t size)
{
    if (block->size < size + sizeof(struct block) + MIN_SPLIT)
        return;

    struct block *rest =
        (struct block *)((uint8_t *)block + sizeof(struct block) + size);

    rest->magic = BLOCK_MAGIC;
    rest->size  = block->size - size - sizeof(struct block);
    rest->next  = block->next;
    rest->free  = true;

    block->size = size;
    block->next = rest;
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;

    size = align_up(size, 8);

    for (struct block *block = heap_head; block; block = block->next) {
        if (!block->free || block->size < size)
            continue;

        split_block(block, size);
        block->free = false;
        return (uint8_t *)block + sizeof(struct block);
    }

    /* Nothing suitable: ask for more address space and retry once. */
    struct block *fresh = heap_grow(size);
    if (!fresh)
        return NULL;

    split_block(fresh, size);
    fresh->free = false;
    return (uint8_t *)fresh + sizeof(struct block);
}

void *kcalloc(size_t count, size_t size)
{
    /* Reject multiplications that would wrap rather than returning a
     * buffer smaller than the caller asked for. */
    if (count && size > (size_t)0xFFFFFFFFu / count)
        return NULL;

    size_t bytes = count * size;
    void  *ptr   = kmalloc(bytes);

    if (ptr)
        memset(ptr, 0, bytes);

    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    struct block *block =
        (struct block *)((uint8_t *)ptr - sizeof(struct block));

    if (block->magic != BLOCK_MAGIC) {
        kprintf("[heap] kfree(%p): bad magic 0x%08x -- not a heap pointer, "
                "or the header was overwritten\n", ptr, block->magic);
        return;
    }

    if (block->free) {
        kprintf("[heap] kfree(%p): double free ignored\n", ptr);
        return;
    }

    block->free = true;

    /* Merge forward. Merging backwards needs the predecessor, which the
     * singly linked list gives us only by walking, so do that below. */
    while (block->next && block->next->free) {
        block->size += sizeof(struct block) + block->next->size;
        block->next  = block->next->next;
    }

    for (struct block *prev = heap_head; prev; prev = prev->next) {
        if (prev->next == block && prev->free) {
            prev->size += sizeof(struct block) + block->size;
            prev->next  = block->next;
            break;
        }
    }
}

void *krealloc(void *ptr, size_t size)
{
    if (!ptr)
        return kmalloc(size);

    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    struct block *block =
        (struct block *)((uint8_t *)ptr - sizeof(struct block));

    if (block->magic != BLOCK_MAGIC) {
        kprintf("[heap] krealloc(%p): not a heap pointer\n", ptr);
        return NULL;
    }

    if (block->size >= size)
        return ptr;

    void *fresh = kmalloc(size);
    if (!fresh)
        return NULL;

    memcpy(fresh, ptr, block->size);
    kfree(ptr);

    return fresh;
}

void heap_stats(size_t *total, size_t *used, size_t *blocks)
{
    size_t used_bytes = 0;
    size_t count = 0;

    for (struct block *block = heap_head; block; block = block->next) {
        count++;
        if (!block->free)
            used_bytes += block->size + sizeof(struct block);
    }

    if (total)  *total  = heap_total;
    if (used)   *used   = used_bytes;
    if (blocks) *blocks = count;
}

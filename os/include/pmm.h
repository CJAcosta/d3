#ifndef NEBULA_PMM_H
#define NEBULA_PMM_H

#include "types.h"

#define PAGE_SIZE  4096u
#define PAGE_SHIFT 12

#define PAGE_ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x)   (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/*
 * Physical frames above this line are left alone. Everything the kernel
 * hands out is identity mapped, and mapping a full 4 GiB would cost 4 MiB
 * of page tables for memory a hobby kernel has no use for.
 */
#define PMM_MAX_MANAGED 0x40000000u   /* 1 GiB */

void       pmm_init(void);

/* Returns a zeroed-out physical frame, or 0 when memory is exhausted. */
physaddr_t pmm_alloc_frame(void);
void       pmm_free_frame(physaddr_t frame);

/* Mark a physical range as unavailable. Used for MMIO and firmware areas. */
void       pmm_reserve_range(physaddr_t start, physaddr_t end);

uint32_t   pmm_total_frames(void);
uint32_t   pmm_used_frames(void);
uint32_t   pmm_free_frames(void);
physaddr_t pmm_highest_address(void);

void       pmm_dump_map(void);

#endif /* NEBULA_PMM_H */

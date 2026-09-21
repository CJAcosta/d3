#ifndef NEBULA_PAGING_H
#define NEBULA_PAGING_H

#include "types.h"

/* Page table / directory entry flags. */
#define PTE_PRESENT   0x001
#define PTE_WRITABLE  0x002
#define PTE_USER      0x004
#define PTE_WRITETHRU 0x008
#define PTE_NOCACHE   0x010
#define PTE_ACCESSED  0x020
#define PTE_DIRTY     0x040

void paging_init(void);

/* Create or replace the mapping for one page. Allocates a page table if
 * the directory entry is still empty. Returns false if out of memory. */
bool paging_map(virtaddr_t virt, physaddr_t phys, uint32_t flags);

void paging_unmap(virtaddr_t virt);

/* Returns the physical address backing virt, or 0 when unmapped. */
physaddr_t paging_resolve(virtaddr_t virt);

bool paging_is_enabled(void);

#endif /* NEBULA_PAGING_H */

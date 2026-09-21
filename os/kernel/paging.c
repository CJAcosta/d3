/*
 * paging.c -- 32-bit two-level paging.
 *
 * A page directory of 1024 entries, each pointing at a page table of
 * 1024 entries, each describing one 4 KiB page: 4 GiB of address space.
 *
 * All physical RAM the PMM manages is identity mapped, so physical and
 * virtual addresses agree for anything the kernel allocates. That keeps
 * page-table edits simple -- a table's physical address is also a usable
 * pointer -- while still leaving the upper address space free for
 * mappings such as the kernel heap.
 */

#include "paging.h"
#include "pmm.h"
#include "idt.h"
#include "kprintf.h"
#include "string.h"

#define ENTRIES_PER_TABLE 1024
#define PDE_INDEX(addr)   (((addr) >> 22) & 0x3FF)
#define PTE_INDEX(addr)   (((addr) >> 12) & 0x3FF)

static uint32_t *page_directory;
static bool      paging_enabled;

static inline void invlpg(virtaddr_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/*
 * Find the page table for virt, optionally creating it.
 *
 * Because every table comes from identity-mapped physical memory, the
 * entry's physical address can be dereferenced directly.
 */
static uint32_t *get_table(virtaddr_t virt, bool create)
{
    uint32_t pde_index = PDE_INDEX(virt);
    uint32_t entry     = page_directory[pde_index];

    if (entry & PTE_PRESENT)
        return (uint32_t *)(entry & ~0xFFFu);

    if (!create)
        return NULL;

    physaddr_t table = pmm_alloc_frame();   /* arrives zeroed */
    if (!table)
        return NULL;

    /*
     * Permissions are enforced by the page table entry as well as the
     * directory entry, and the CPU ANDs the two. Keeping the directory
     * entry permissive lets each page decide for itself.
     */
    page_directory[pde_index] = table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;

    return (uint32_t *)table;
}

bool paging_map(virtaddr_t virt, physaddr_t phys, uint32_t flags)
{
    uint32_t *table = get_table(virt, true);

    if (!table)
        return false;

    table[PTE_INDEX(virt)] = (phys & ~0xFFFu) | (flags & 0xFFF) | PTE_PRESENT;

    if (paging_enabled)
        invlpg(virt);

    return true;
}

void paging_unmap(virtaddr_t virt)
{
    uint32_t *table = get_table(virt, false);

    if (!table)
        return;

    table[PTE_INDEX(virt)] = 0;

    if (paging_enabled)
        invlpg(virt);
}

physaddr_t paging_resolve(virtaddr_t virt)
{
    uint32_t *table = get_table(virt, false);

    if (!table)
        return 0;

    uint32_t entry = table[PTE_INDEX(virt)];
    if (!(entry & PTE_PRESENT))
        return 0;

    return (entry & ~0xFFFu) | (virt & 0xFFFu);
}

bool paging_is_enabled(void)
{
    return paging_enabled;
}

static void page_fault_handler(struct registers *regs)
{
    uint32_t cr2;
    __asm__ volatile("movl %%cr2, %0" : "=r"(cr2));

    panic("page fault %s %p from %s mode\n"
          "  cause: %s%s\n"
          "  eip=%p  eflags=0x%08x",
          (regs->err_code & 2) ? "writing" : "reading",
          (void *)cr2,
          (regs->err_code & 4) ? "user" : "kernel",
          (regs->err_code & 1) ? "protection violation" : "page not present",
          (regs->err_code & 16) ? ", instruction fetch" : "",
          (void *)regs->eip, regs->eflags);
}

void paging_init(void)
{
    page_directory = (uint32_t *)pmm_alloc_frame();
    if (!page_directory)
        panic("paging: could not allocate the page directory");

    /*
     * Identity map every frame the PMM knows about. This covers the
     * kernel, the frame bitmap, video memory and every frame we might
     * later hand out, so a physical address is always dereferenceable.
     */
    physaddr_t limit = pmm_highest_address();

    for (physaddr_t addr = 0; addr < limit; addr += PAGE_SIZE) {
        if (!paging_map(addr, addr, PTE_WRITABLE))
            panic("paging: ran out of memory identity mapping %p", (void *)addr);
    }

    /* The VGA text buffer lives below 1 MiB and is therefore already
     * covered, but say so explicitly in case the map ever shrinks. */
    if (!paging_resolve(0xB8000))
        paging_map(0xB8000, 0xB8000, PTE_WRITABLE);

    isr_register(14, page_fault_handler);

    __asm__ volatile("movl %0, %%cr3" : : "r"((uint32_t)page_directory));

    uint32_t cr0;
    __asm__ volatile("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;              /* CR0.PG */
    __asm__ volatile("movl %0, %%cr0" : : "r"(cr0));

    paging_enabled = true;

    kprintf("[paging] identity mapped 0-%u MiB, directory at %p\n",
            limit / (1024 * 1024), page_directory);
}

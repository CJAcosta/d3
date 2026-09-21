/*
 * gdt.c -- the kernel's own Global Descriptor Table.
 *
 * The boot sector built a throwaway GDT in the boot sector's memory,
 * which we are free to overwrite once the kernel is running. This one
 * lives in kernel .bss and adds ring-3 segments plus a TSS, so user mode
 * has somewhere to land.
 *
 * All four code/data segments cover the whole 4 GiB address space: on
 * x86 we get memory protection from paging, not segmentation, so the
 * segments are deliberately flat and overlapping.
 */

#include "gdt.h"
#include "string.h"

/* Two-step stringification, so the selector constants can be pasted
 * straight into the inline assembly below. */
#define STR_(x) #x
#define STR(x)  STR_(x)

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;   /* high limit nibble + flags */
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* Task State Segment. We only care about ss0/esp0, which the CPU reads
 * when an interrupt promotes us from ring 3 to ring 0. */
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0, ss0;
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

#define GDT_ENTRIES 6

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdt_pointer;
static struct tss_entry tss;

static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    gdt[index].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[index].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[index].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt[index].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[index].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (flags & 0xF0));

    gdt[index].access      = access;
}

/*
 * Load the GDT, then reload every segment register. CS can only be
 * changed by a far transfer, so we far-jump to the next instruction.
 */
static void gdt_flush(void)
{
    __asm__ volatile(
        "lgdt %0\n\t"
        "ljmp $" STR(SEL_KERNEL_CODE) ", $1f\n\t"
        "1:\n\t"
        "movw $" STR(SEL_KERNEL_DATA) ", %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        : : "m"(gdt_pointer) : "eax", "memory");
}

void tss_set_kernel_stack(uint32_t esp0)
{
    tss.esp0 = esp0;
}

void gdt_init(void)
{
    uint32_t tss_base  = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;

    /* access byte: P | DPL | S | type    granularity: 4K pages | 32-bit */
    gdt_set_entry(0, 0, 0x00000, 0x00, 0x00);          /* null */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);          /* ring 0 code */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);          /* ring 0 data */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);          /* ring 3 code */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);          /* ring 3 data */
    /* 0x89 = present, ring 0, 32-bit available TSS. Byte granularity. */
    gdt_set_entry(5, tss_base, tss_limit, 0x89, 0x00);

    gdt_pointer.limit = sizeof(gdt) - 1;
    gdt_pointer.base  = (uint32_t)&gdt;

    memset(&tss, 0, sizeof(tss));
    tss.ss0 = SEL_KERNEL_DATA;
    tss.esp0 = 0;               /* filled in once we have a task stack */
    /* Point the I/O permission bitmap past the end of the TSS, which
     * means "no port access from ring 3". */
    tss.iomap_base = sizeof(tss);

    gdt_flush();

    __asm__ volatile("ltr %0" : : "r"((uint16_t)SEL_TSS));
}

/*
 * pmm.c -- physical memory manager.
 *
 * A flat bitmap with one bit per 4 KiB frame: set means allocated. The
 * bitmap is small (32 KiB per gigabyte of RAM) and makes "is this frame
 * free" a single test, which is all the allocator needs.
 *
 * The usable regions come from the E820 map the boot sector collected
 * before leaving real mode; the BIOS is the only thing that knows which
 * parts of the address space are real RAM rather than firmware or MMIO.
 */

#include "pmm.h"
#include "kprintf.h"
#include "string.h"

/* Where boot.S left the memory map. */
#define E820_COUNT_ADDR   0x4FF0
#define E820_ENTRIES_ADDR 0x5000
#define E820_MAX_ENTRIES  32

#define E820_TYPE_USABLE 1

struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attrs;
} __attribute__((packed));

/* Provided by the linker script. */
extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];

static uint8_t   *frame_bitmap;
static uint32_t   total_frames;
static uint32_t   used_count;
static physaddr_t highest_addr;

/* Start searching here rather than from frame 0 on every allocation. */
static uint32_t   search_hint;

static const char *e820_type_name(uint32_t type)
{
    switch (type) {
    case 1:  return "usable";
    case 2:  return "reserved";
    case 3:  return "ACPI reclaimable";
    case 4:  return "ACPI NVS";
    case 5:  return "bad memory";
    default: return "unknown";
    }
}

static inline void bitmap_set(uint32_t frame)
{
    frame_bitmap[frame / 8] |= (uint8_t)(1 << (frame % 8));
}

static inline void bitmap_clear(uint32_t frame)
{
    frame_bitmap[frame / 8] &= (uint8_t)~(1 << (frame % 8));
}

static inline bool bitmap_test(uint32_t frame)
{
    return (frame_bitmap[frame / 8] & (1 << (frame % 8))) != 0;
}

static void mark_used(uint32_t frame)
{
    if (frame >= total_frames || bitmap_test(frame))
        return;

    bitmap_set(frame);
    used_count++;
}

static void mark_free(uint32_t frame)
{
    if (frame >= total_frames || !bitmap_test(frame))
        return;

    bitmap_clear(frame);
    used_count--;
}

void pmm_reserve_range(physaddr_t start, physaddr_t end)
{
    uint32_t first = PAGE_ALIGN_DOWN(start) >> PAGE_SHIFT;
    uint32_t last  = PAGE_ALIGN_UP(end) >> PAGE_SHIFT;

    for (uint32_t frame = first; frame < last; frame++)
        mark_used(frame);
}

static void free_range(physaddr_t start, physaddr_t end)
{
    /* Only whole frames fully inside the region are safe to hand out. */
    uint32_t first = PAGE_ALIGN_UP(start) >> PAGE_SHIFT;
    uint32_t last  = PAGE_ALIGN_DOWN(end) >> PAGE_SHIFT;

    for (uint32_t frame = first; frame < last; frame++)
        mark_free(frame);
}

void pmm_init(void)
{
    const struct e820_entry *entries =
        (const struct e820_entry *)E820_ENTRIES_ADDR;
    uint16_t count = *(volatile uint16_t *)E820_COUNT_ADDR;

    if (count > E820_MAX_ENTRIES)
        count = E820_MAX_ENTRIES;

    /* First pass: how much physical address space do we need to cover? */
    highest_addr = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (entries[i].type != E820_TYPE_USABLE)
            continue;

        uint64_t end = entries[i].base + entries[i].length;
        if (end > PMM_MAX_MANAGED)
            end = PMM_MAX_MANAGED;
        if ((physaddr_t)end > highest_addr)
            highest_addr = (physaddr_t)end;
    }

    if (highest_addr == 0) {
        /* No usable E820 map. Assume the 16 MiB every PC has had since
         * the 386 and carry on rather than failing to boot. */
        kprintf("[pmm] no E820 map; assuming 16 MiB of RAM\n");
        highest_addr = 16 * 1024 * 1024;
    }

    total_frames = highest_addr >> PAGE_SHIFT;

    /* Park the bitmap immediately after the kernel image. */
    frame_bitmap = (uint8_t *)PAGE_ALIGN_UP((uint32_t)__kernel_end);
    uint32_t bitmap_bytes = (total_frames + 7) / 8;

    /* Start with everything allocated, then punch out the usable holes:
     * anything the BIOS did not explicitly call usable stays reserved. */
    memset(frame_bitmap, 0xFF, bitmap_bytes);
    used_count = total_frames;

    for (uint16_t i = 0; i < count; i++) {
        if (entries[i].type != E820_TYPE_USABLE)
            continue;

        uint64_t base = entries[i].base;
        uint64_t end  = base + entries[i].length;

        if (base >= PMM_MAX_MANAGED)
            continue;
        if (end > PMM_MAX_MANAGED)
            end = PMM_MAX_MANAGED;

        free_range((physaddr_t)base, (physaddr_t)end);
    }

    /*
     * Now claw back the regions that are usable RAM but already spoken
     * for: the real-mode IVT and BIOS data area, the boot sector, our
     * E820 scratch area, video memory, the kernel image itself and the
     * bitmap we just built.
     */
    pmm_reserve_range(0, 0x100000);                       /* low 1 MiB */
    pmm_reserve_range((physaddr_t)__kernel_start,
                      (physaddr_t)__kernel_end);
    pmm_reserve_range((physaddr_t)frame_bitmap,
                      (physaddr_t)frame_bitmap + bitmap_bytes);

    kprintf("[pmm] %u MiB usable, %u frames (%u free), bitmap at %p\n",
            highest_addr / (1024 * 1024), total_frames,
            pmm_free_frames(), frame_bitmap);
}

physaddr_t pmm_alloc_frame(void)
{
    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t start = (pass == 0) ? search_hint : 0;
        uint32_t stop  = (pass == 0) ? total_frames : search_hint;

        for (uint32_t frame = start; frame < stop; frame++) {
            /* Skip eight frames at a time when a whole byte is taken. */
            if (frame_bitmap[frame / 8] == 0xFF) {
                frame |= 7;     /* the loop's ++ moves to the next byte */
                continue;
            }

            if (bitmap_test(frame))
                continue;

            bitmap_set(frame);
            used_count++;
            search_hint = frame + 1;

            physaddr_t addr = (physaddr_t)frame << PAGE_SHIFT;

            /* Callers rely on fresh frames being zeroed -- page tables
             * in particular must not start life full of garbage. */
            memset((void *)addr, 0, PAGE_SIZE);
            return addr;
        }
    }

    return 0;
}

void pmm_free_frame(physaddr_t frame_addr)
{
    uint32_t frame = frame_addr >> PAGE_SHIFT;

    if (frame >= total_frames)
        return;

    if (!bitmap_test(frame)) {
        kprintf("[pmm] warning: double free of frame %p\n", (void *)frame_addr);
        return;
    }

    mark_free(frame);

    if (frame < search_hint)
        search_hint = frame;
}

uint32_t pmm_total_frames(void) { return total_frames; }
uint32_t pmm_used_frames(void)  { return used_count; }
uint32_t pmm_free_frames(void)  { return total_frames - used_count; }
physaddr_t pmm_highest_address(void) { return highest_addr; }

void pmm_dump_map(void)
{
    const struct e820_entry *entries =
        (const struct e820_entry *)E820_ENTRIES_ADDR;
    uint16_t count = *(volatile uint16_t *)E820_COUNT_ADDR;

    if (count > E820_MAX_ENTRIES)
        count = E820_MAX_ENTRIES;

    kprintf("BIOS-provided physical memory map (%u entries):\n", count);

    for (uint16_t i = 0; i < count; i++) {
        uint64_t base = entries[i].base;
        uint64_t end  = base + entries[i].length;

        /* Addresses beyond 4 GiB cannot be reached in 32-bit mode; show
         * the low half rather than printing nothing at all. */
        kprintf("  [%2u] %08x-%08x  %s\n", i,
                (uint32_t)base, (uint32_t)end, e820_type_name(entries[i].type));
    }
}

#include "types.h"
#include "vga.h"
#include "serial.h"
#include "kprintf.h"
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "sched.h"
#include "string.h"

static void selftest_task(void *arg)
{
    const char *label = (const char *)arg;

    for (int i = 0; i < 3; i++) {
        kprintf("[%s] iteration %d at %u ms\n", label, i, timer_uptime_ms());
        sched_sleep(150);
    }

    kprintf("[%s] done\n", label);
}

static void report(const char *name, bool ok)
{
    vga_set_color(ok ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
    kprintf("  [%s] ", ok ? "pass" : "FAIL");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("%s\n", name);
}

static void run_selftests(void)
{
    kprintf("\nself-tests:\n");

    /* Heap: allocate, write, free, and confirm the space comes back. */
    size_t before, after, unused;
    heap_stats(&unused, &before, &unused);

    void *a = kmalloc(1024);
    void *b = kmalloc(4096);
    report("kmalloc returns distinct non-null blocks", a && b && a != b);

    memset(a, 0xAB, 1024);
    memset(b, 0xCD, 4096);
    report("allocated memory is writable", ((uint8_t *)a)[1023] == 0xAB &&
                                           ((uint8_t *)b)[4095] == 0xCD);

    kfree(a);
    kfree(b);
    heap_stats(&unused, &after, &unused);
    report("kfree returns memory to the heap", after == before);

    /* Paging: the heap lives above the identity map, so its virtual
     * addresses must resolve to some other physical frame. */
    void *heap_ptr = kmalloc(16);
    physaddr_t phys = paging_resolve((virtaddr_t)heap_ptr);
    report("heap virtual addresses are really mapped", phys != 0);
    report("heap is not identity mapped", phys != (physaddr_t)heap_ptr);
    kfree(heap_ptr);

    /* Formatting. */
    char buf[64];
    ksnprintf(buf, sizeof(buf), "%d|%u|%x|%05d|%-6s|%c", -42, 42u, 0xBEEF, 7, "ab", 'Z');
    report("kprintf formatting", strcmp(buf, "-42|42|beef|00007|ab    |Z") == 0);

    ksnprintf(buf, 8, "%s", "truncate me");
    report("ksnprintf truncates safely", strcmp(buf, "truncat") == 0);

    /* Interrupts are live if the timer is counting. */
    uint32_t t0 = timer_ticks();
    timer_sleep(50);
    report("timer interrupt is firing", timer_ticks() > t0);
}

void kmain(uint32_t boot_drive)
{
    vga_init();
    serial_init();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("Nebula OS\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    kprintf("a small 32-bit x86 kernel\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    kprintf("[boot] booted from drive 0x%02x\n", boot_drive);

    gdt_init();
    kprintf("[gdt] descriptors loaded, TSS installed\n");

    idt_init();
    pic_init();
    kprintf("[idt] 256 vectors installed, PIC remapped to 0x20-0x2F\n");

    pmm_init();
    paging_init();
    heap_init();

    timer_init();
    keyboard_init();

    sti();
    kprintf("[cpu] interrupts enabled\n");

    sched_init();

    run_selftests();

    kprintf("\nscheduler demo:\n");
    sched_spawn("alpha", selftest_task, (void *)"alpha");
    sched_spawn("beta", selftest_task, (void *)"beta");

    /* Let the demo tasks interleave, then show the run queue. */
    sched_sleep(700);

    kprintf("\n");
    sched_dump();

    kprintf("\n[boot] reached the end of kmain; idling\n");

    for (;;) {
        sched_yield();
        hlt();
    }
}

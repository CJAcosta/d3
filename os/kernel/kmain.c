/*
 * kmain.c -- kernel entry point.
 *
 * Brings up the subsystems in dependency order, runs a set of boot-time
 * self-tests, then hands the machine to the shell.
 */

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
#include "fs.h"
#include "shell.h"
#include "string.h"

static uint32_t tests_run;
static uint32_t tests_failed;

static void check(const char *name, bool ok)
{
    tests_run++;

    if (ok) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        kprintf("  [pass] ");
    } else {
        tests_failed++;
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kprintf("  [FAIL] ");
    }

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("%s\n", name);
}

/*
 * Boot-time self-tests.
 *
 * These are not a substitute for the host-side unit tests, but they
 * exercise the things that can only be checked on real hardware: that
 * paging really translates, that interrupts really fire, that a context
 * switch really returns.
 */
static void run_selftests(void)
{
    kprintf("\nself-tests:\n");

    /* --- heap --- */
    size_t before, after, ignored;
    heap_stats(&ignored, &before, &ignored);

    void *a = kmalloc(1024);
    void *b = kmalloc(4096);
    check("kmalloc returns distinct non-null blocks", a && b && a != b);

    memset(a, 0xAB, 1024);
    memset(b, 0xCD, 4096);
    check("allocated memory holds what was written",
          ((uint8_t *)a)[1023] == 0xAB && ((uint8_t *)b)[4095] == 0xCD);

    kfree(a);
    kfree(b);
    heap_stats(&ignored, &after, &ignored);
    check("kfree coalesces the heap back to its prior state", after == before);

    void *grown = kmalloc(200 * 1024);
    check("heap grows past its initial size on demand", grown != NULL);
    kfree(grown);

    /* --- paging --- */
    void *heap_ptr = kmalloc(16);
    physaddr_t phys = paging_resolve((virtaddr_t)heap_ptr);
    check("heap addresses resolve to a physical frame", phys != 0);
    check("the heap is mapped, not identity mapped",
          phys != (physaddr_t)heap_ptr);
    check("an unmapped address resolves to nothing",
          paging_resolve(0x40000000) == 0);
    kfree(heap_ptr);

    /* --- formatting --- */
    char buf[64];
    ksnprintf(buf, sizeof(buf), "%d|%u|%x|%05d|%-6s|%c",
              -42, 42u, 0xBEEF, 7, "ab", 'Z');
    check("kprintf conversions and padding",
          strcmp(buf, "-42|42|beef|00007|ab    |Z") == 0);

    ksnprintf(buf, 8, "%s", "truncate me");
    check("ksnprintf truncates instead of overflowing",
          strcmp(buf, "truncat") == 0);

    /* --- filesystem --- */
    struct fs_node *dir = fs_create(fs_root(), "/tmp/selftest", FS_DIR);
    check("fs_create makes a directory", dir != NULL);

    struct fs_node *file = fs_create(fs_root(), "/tmp/selftest/a.txt", FS_FILE);
    check("fs_create makes a file in it", file != NULL);

    fs_write(file, "hello", 5);
    fs_append(file, " world", 6);
    check("fs_write then fs_append concatenate",
          file->size == 11 && memcmp(file->data, "hello world", 11) == 0);

    check("fs_lookup finds it by absolute path",
          fs_lookup(fs_root(), "/tmp/selftest/a.txt") == file);
    check("fs_lookup understands . and ..",
          fs_lookup(fs_root(), "/tmp/selftest/../selftest/./a.txt") == file);
    check("fs_lookup rejects a missing path",
          fs_lookup(fs_root(), "/tmp/selftest/nope") == NULL);

    char path[FS_PATH_MAX];
    fs_path(file, path, sizeof(path));
    check("fs_path rebuilds the absolute path",
          strcmp(path, "/tmp/selftest/a.txt") == 0);

    check("a non-empty directory cannot be removed", !fs_unlink(dir));
    check("a file can be removed", fs_unlink(file));
    check("the directory can be removed once empty", fs_unlink(dir));

    /* --- interrupts --- */
    uint32_t t0 = timer_ticks();
    timer_sleep(50);
    check("the timer interrupt advances the tick counter", timer_ticks() > t0);

    kprintf("\n%u tests, %u failed\n", tests_run, tests_failed);

    if (tests_failed) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kprintf("SELFTEST FAILURE\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    }
}

void kmain(uint32_t boot_drive)
{
    vga_init();
    serial_init();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("Nebula OS 0.1.0\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    kprintf("a small 32-bit x86 kernel\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    kprintf("[boot] loaded from drive 0x%02x\n", boot_drive);

    /* Descriptor tables first: nothing else is safe until faults have
     * somewhere to land. */
    gdt_init();
    kprintf("[gdt] flat descriptors loaded, TSS installed\n");

    idt_init();
    pic_init();
    kprintf("[idt] 256 vectors installed, PIC remapped to 0x20-0x2f\n");

    /* Memory, in dependency order: frames, then the MMU, then the heap. */
    pmm_init();
    paging_init();
    heap_init();

    /* Devices. */
    timer_init();
    keyboard_init();
    serial_enable_input();

    sti();
    kprintf("[cpu] interrupts enabled\n");

    sched_init();
    fs_init();

    run_selftests();

    if (!sched_spawn("shell", shell_task, NULL))
        panic("could not start the shell");

    /*
     * The boot context becomes the idle task. hlt parks the CPU until the
     * next interrupt, which keeps a virtual machine from spinning a host
     * core at 100%.
     */
    for (;;) {
        sched_yield();
        hlt();
    }
}

#include "types.h"
#include "vga.h"
#include "serial.h"
#include "kprintf.h"
#include "io.h"

void kmain(uint32_t boot_drive)
{
    vga_init();
    serial_init();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("Nebula OS\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("booted from drive 0x%02x\n", boot_drive);

    for (;;)
        hlt();
}

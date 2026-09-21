#ifndef NEBULA_VGA_H
#define NEBULA_VGA_H

#include "types.h"

/* Standard CGA/VGA text-mode palette. */
enum vga_color {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
};

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_write(const char *s);
void vga_set_color(enum vga_color fg, enum vga_color bg);
void vga_get_cursor(uint8_t *row, uint8_t *col);

#endif /* NEBULA_VGA_H */

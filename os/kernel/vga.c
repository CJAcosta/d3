/*
 * vga.c -- 80x25 text-mode console.
 *
 * The VGA text buffer is memory-mapped at 0xB8000. Each cell is two
 * bytes: the code page 437 character, then an attribute byte holding the
 * background colour in the high nibble and the foreground in the low one.
 */

#include "vga.h"
#include "io.h"
#include "string.h"

#define VGA_MEMORY    ((volatile uint16_t *)0xB8000)
#define VGA_CRTC_IDX  0x3D4
#define VGA_CRTC_DATA 0x3D5
#define TAB_WIDTH     4

static uint8_t cursor_row;
static uint8_t cursor_col;
static uint8_t attribute;

static uint16_t vga_entry(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}

/* Tell the CRT controller where to draw the blinking hardware cursor. */
static void vga_update_cursor(void)
{
    uint16_t pos = (uint16_t)cursor_row * VGA_WIDTH + cursor_col;

    outb(VGA_CRTC_IDX, 0x0F);
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(VGA_CRTC_IDX, 0x0E);
    outb(VGA_CRTC_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    attribute = (uint8_t)(fg | (bg << 4));
}

void vga_clear(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = vga_entry(' ', attribute);

    cursor_row = 0;
    cursor_col = 0;
    vga_update_cursor();
}

void vga_init(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

/* Shift every row up by one and blank the bottom line. */
static void vga_scroll(void)
{
    for (size_t row = 1; row < VGA_HEIGHT; row++) {
        for (size_t col = 0; col < VGA_WIDTH; col++)
            VGA_MEMORY[(row - 1) * VGA_WIDTH + col] =
                VGA_MEMORY[row * VGA_WIDTH + col];
    }

    for (size_t col = 0; col < VGA_WIDTH; col++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] =
            vga_entry(' ', attribute);

    cursor_row = VGA_HEIGHT - 1;
}

static void vga_newline(void)
{
    cursor_col = 0;
    if (++cursor_row >= VGA_HEIGHT)
        vga_scroll();
}

void vga_putchar(char c)
{
    switch (c) {
    case '\n':
        vga_newline();
        break;

    case '\r':
        cursor_col = 0;
        break;

    case '\t':
        /* Advance to the next tab stop, wrapping if we run off the edge. */
        cursor_col = (uint8_t)((cursor_col + TAB_WIDTH) & ~(TAB_WIDTH - 1));
        if (cursor_col >= VGA_WIDTH)
            vga_newline();
        break;

    case '\b':
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        }
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
            vga_entry(' ', attribute);
        break;

    default:
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
            vga_entry(c, attribute);
        if (++cursor_col >= VGA_WIDTH)
            vga_newline();
        break;
    }

    vga_update_cursor();
}

void vga_write(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}

void vga_get_cursor(uint8_t *row, uint8_t *col)
{
    if (row)
        *row = cursor_row;
    if (col)
        *col = cursor_col;
}

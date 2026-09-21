/*
 * keyboard.c -- PS/2 keyboard on IRQ1.
 *
 * The controller hands us scan code set 1: one byte per event, with bit
 * 7 set on release. Keys such as the arrows and the right-hand control
 * are prefixed with 0xE0. We translate to ASCII and queue the result in
 * a ring buffer, so the interrupt handler never blocks.
 */

#include "keyboard.h"
#include "input.h"
#include "idt.h"
#include "io.h"
#include "kprintf.h"

#define KBD_DATA   0x60
#define KBD_STATUS 0x64

#define SC_RELEASE  0x80
#define SC_EXTENDED 0xE0

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CTRL   0x1D
#define SC_ALT    0x38
#define SC_CAPS   0x3A

/* Scan code -> ASCII, unshifted. Index is the scan code itself. */
static const char keymap[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,    '*',  0,  ' ',
};

static const char keymap_shift[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,    '*',  0,  ' ',
};

static bool shift_down;
static bool ctrl_down;
static bool caps_lock;
static bool extended;

static void keyboard_irq(struct registers *regs)
{
    (void)regs;

    uint8_t scancode = inb(KBD_DATA);

    if (scancode == SC_EXTENDED) {
        extended = true;
        return;
    }

    bool released = (scancode & SC_RELEASE) != 0;
    uint8_t code  = (uint8_t)(scancode & 0x7F);

    if (extended) {
        /* We only care about the extended modifier keys for now. */
        extended = false;
        if (code == SC_CTRL)
            ctrl_down = !released;
        return;
    }

    switch (code) {
    case SC_LSHIFT:
    case SC_RSHIFT:
        shift_down = !released;
        return;
    case SC_CTRL:
        ctrl_down = !released;
        return;
    case SC_ALT:
        return;
    case SC_CAPS:
        if (!released)
            caps_lock = !caps_lock;
        return;
    default:
        break;
    }

    if (released || code >= 128)
        return;

    char c = shift_down ? keymap_shift[code] : keymap[code];
    if (c == 0)
        return;

    /* Caps lock affects letters only, and inverts whatever shift did. */
    if (caps_lock) {
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        else if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
    }

    /* Ctrl+letter produces the classic control character. */
    if (ctrl_down) {
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 1);
        else if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 1);
    }

    input_push(c);
}

void keyboard_init(void)
{
    /* Drain any byte the BIOS left in the controller's output buffer,
     * otherwise the first IRQ never arrives. */
    while (inb(KBD_STATUS) & 1)
        (void)inb(KBD_DATA);

    shift_down = ctrl_down = caps_lock = extended = false;

    irq_register(IRQ_KEYBOARD, keyboard_irq);
}

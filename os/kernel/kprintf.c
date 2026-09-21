/*
 * kprintf.c -- formatted output for the kernel.
 *
 * Everything funnels through a "sink" so the same formatting code can
 * write to the console or into a caller's buffer. Console output goes to
 * both the VGA text screen and the serial port.
 */

#include "kprintf.h"
#include "vga.h"
#include "serial.h"
#include "string.h"
#include "io.h"

struct sink {
    char  *buf;     /* NULL means "write to the console" */
    size_t size;    /* capacity of buf, including the NUL */
    size_t written; /* characters that would have been written */
};

static void sink_putc(struct sink *s, char c)
{
    if (s->buf) {
        /* Reserve the last byte for the terminator, printf-style. */
        if (s->written + 1 < s->size)
            s->buf[s->written] = c;
    } else {
        vga_putchar(c);
        serial_putchar(c);
    }

    s->written++;
}

static void sink_puts(struct sink *s, const char *str)
{
    while (*str)
        sink_putc(s, *str++);
}

/*
 * Render an unsigned value in the given base. Digits come out backwards,
 * so we build into a scratch buffer and emit it in reverse.
 */
static void sink_number(struct sink *s, uint32_t value, uint32_t base,
                        bool uppercase, int width, bool zero_pad,
                        bool left_align)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char        tmp[33];
    int         len = 0;

    if (value == 0) {
        tmp[len++] = '0';
    } else {
        while (value > 0) {
            tmp[len++] = digits[value % base];
            value /= base;
        }
    }

    if (!left_align) {
        for (int pad = len; pad < width; pad++)
            sink_putc(s, zero_pad ? '0' : ' ');
    }

    int emitted = len;
    while (len > 0)
        sink_putc(s, tmp[--len]);

    if (left_align) {
        for (int pad = emitted; pad < width; pad++)
            sink_putc(s, ' ');
    }
}

static void sink_signed(struct sink *s, int32_t value, int width,
                        bool zero_pad, bool left_align)
{
    uint32_t magnitude;

    if (value < 0) {
        sink_putc(s, '-');
        if (width > 0)
            width--;
        /* Negating INT32_MIN overflows, so cast first and negate in
         * unsigned arithmetic where the result is well defined. */
        magnitude = (uint32_t)0 - (uint32_t)value;
    } else {
        magnitude = (uint32_t)value;
    }

    sink_number(s, magnitude, 10, false, width, zero_pad, left_align);
}

static void format(struct sink *s, const char *fmt, va_list args)
{
    while (*fmt) {
        if (*fmt != '%') {
            sink_putc(s, *fmt++);
            continue;
        }

        fmt++;  /* consume '%' */

        bool left_align = false;
        bool zero_pad    = false;

        for (;;) {
            if (*fmt == '-') {
                left_align = true;
                fmt++;
            } else if (*fmt == '0') {
                zero_pad = true;
                fmt++;
            } else {
                break;
            }
        }

        /* '-' wins over '0': you cannot zero-pad on the right. */
        if (left_align)
            zero_pad = false;

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Accept and ignore length modifiers: our ints are already 32-bit. */
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z')
            fmt++;

        switch (*fmt) {
        case 'd':
        case 'i':
            sink_signed(s, va_arg(args, int32_t), width, zero_pad, left_align);
            break;

        case 'u':
            sink_number(s, va_arg(args, uint32_t), 10, false, width, zero_pad, left_align);
            break;

        case 'x':
            sink_number(s, va_arg(args, uint32_t), 16, false, width, zero_pad, left_align);
            break;

        case 'X':
            sink_number(s, va_arg(args, uint32_t), 16, true, width, zero_pad, left_align);
            break;

        case 'p':
            sink_puts(s, "0x");
            sink_number(s, (uint32_t)va_arg(args, void *), 16, false, 8, true, false);
            break;

        case 'c':
            sink_putc(s, (char)va_arg(args, int));
            break;

        case 's': {
            const char *str = va_arg(args, const char *);
            if (!str)
                str = "(null)";

            int len = (int)strlen(str);

            if (!left_align) {
                for (int pad = len; pad < width; pad++)
                    sink_putc(s, ' ');
            }

            sink_puts(s, str);

            if (left_align) {
                for (int pad = len; pad < width; pad++)
                    sink_putc(s, ' ');
            }
            break;
        }

        case '%':
            sink_putc(s, '%');
            break;

        case '\0':
            /* Trailing '%' with nothing after it: emit it literally. */
            sink_putc(s, '%');
            return;

        default:
            /* Unknown conversion: show it verbatim so bugs are visible. */
            sink_putc(s, '%');
            sink_putc(s, *fmt);
            break;
        }

        fmt++;
    }
}

void kputchar(char c)
{
    vga_putchar(c);
    serial_putchar(c);
}

void kvprintf(const char *fmt, va_list args)
{
    struct sink s = { NULL, 0, 0 };
    format(&s, fmt, args);
}

void kprintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    struct sink s = { buf, size, 0 };

    format(&s, fmt, args);

    if (size > 0) {
        size_t end = (s.written < size) ? s.written : size - 1;
        buf[end] = '\0';
    }

    return (int)s.written;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int     n;

    va_start(args, fmt);
    n = kvsnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}

void panic(const char *fmt, ...)
{
    va_list args;

    cli();

    vga_set_color(VGA_WHITE, VGA_RED);
    kprintf("\n*** KERNEL PANIC ***\n");

    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);

    kprintf("\nSystem halted.\n");

    for (;;)
        hlt();
}

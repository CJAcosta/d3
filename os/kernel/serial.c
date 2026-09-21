/*
 * serial.c -- 16550 UART driver for COM1, used as a debug console.
 */

#include "serial.h"
#include "io.h"

#define COM1 0x3F8

/* Register offsets from the port base. */
#define UART_DATA        0   /* also divisor low when DLAB is set  */
#define UART_IER         1   /* also divisor high when DLAB is set */
#define UART_FIFO_CTRL   2
#define UART_LINE_CTRL   3
#define UART_MODEM_CTRL  4
#define UART_LINE_STATUS 5

#define LSR_TX_EMPTY 0x20

static bool serial_ready;

void serial_init(void)
{
    outb(COM1 + UART_IER, 0x00);        /* no interrupts: we poll */
    outb(COM1 + UART_LINE_CTRL, 0x80);  /* DLAB on, so 0/1 become the divisor */
    outb(COM1 + UART_DATA, 0x03);       /* divisor 3 -> 38400 baud */
    outb(COM1 + UART_IER, 0x00);
    outb(COM1 + UART_LINE_CTRL, 0x03);  /* DLAB off, 8 data bits, no parity, 1 stop */
    outb(COM1 + UART_FIFO_CTRL, 0xC7);  /* enable and clear FIFOs, 14-byte trigger */
    outb(COM1 + UART_MODEM_CTRL, 0x0B); /* DTR + RTS + OUT2 */

    /*
     * Loopback test: if what we write comes back, a UART is really there.
     * Without this we would spin forever on a machine that has no COM1.
     */
    outb(COM1 + UART_MODEM_CTRL, 0x1E);
    outb(COM1 + UART_DATA, 0xAE);
    serial_ready = (inb(COM1 + UART_DATA) == 0xAE);

    outb(COM1 + UART_MODEM_CTRL, 0x0B);
}

bool serial_available(void)
{
    return serial_ready;
}

void serial_putchar(char c)
{
    if (!serial_ready)
        return;

    /* Bounded wait: a wedged UART must not hang the kernel. */
    for (uint32_t spin = 0; spin < 100000; spin++) {
        if (inb(COM1 + UART_LINE_STATUS) & LSR_TX_EMPTY)
            break;
    }

    if (c == '\n')
        outb(COM1 + UART_DATA, '\r');

    outb(COM1 + UART_DATA, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s)
        serial_putchar(*s++);
}

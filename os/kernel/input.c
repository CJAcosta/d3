/*
 * input.c -- the shared console input queue and line editor.
 *
 * A single-producer-per-source ring buffer. Producers are interrupt
 * handlers (keyboard IRQ1, serial IRQ4) and the consumer is whichever
 * task is reading, so pushes must never block and must never overwrite
 * data the reader has not taken yet.
 */

#include "input.h"
#include "kprintf.h"
#include "sched.h"
#include "io.h"

#define BUFFER_SIZE 256

static volatile char   buffer[BUFFER_SIZE];
static volatile size_t head;    /* next write position */
static volatile size_t tail;    /* next read position  */

void input_push(char c)
{
    size_t next = (head + 1) % BUFFER_SIZE;

    /* Full: drop the newest character rather than clobber unread input. */
    if (next == tail)
        return;

    buffer[head] = c;
    head = next;
}

char input_poll(void)
{
    char c;

    if (head == tail)
        return 0;

    /*
     * Interrupts are masked for the update so a keystroke arriving
     * between the read and the index bump cannot be lost.
     */
    bool enabled = irq_save();
    c = buffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    irq_restore(enabled);

    return c;
}

char input_getchar(void)
{
    for (;;) {
        char c = input_poll();
        if (c)
            return c;

        /* Nothing queued: let other tasks run instead of spinning. */
        sched_yield();
    }
}

size_t input_readline(char *buf, size_t size)
{
    size_t len = 0;

    if (size == 0)
        return 0;

    for (;;) {
        char c = input_getchar();

        if (c == '\n' || c == '\r') {
            kputchar('\n');
            break;
        }

        /* Terminals send DEL for backspace; accept both. */
        if (c == '\b' || c == 0x7F) {
            if (len > 0) {
                len--;
                kprintf("\b \b");   /* erase on the screen and on a terminal */
            }
            continue;
        }

        /* Ctrl+C abandons the line. */
        if (c == 3) {
            kprintf("^C\n");
            len = 0;
            break;
        }

        /* Ignore anything unprintable, and keep room for the NUL. */
        if (c < ' ' || c > '~' || len + 1 >= size)
            continue;

        buf[len++] = c;
        kputchar(c);
    }

    buf[len] = '\0';
    return len;
}

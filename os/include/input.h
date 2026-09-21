#ifndef NEBULA_INPUT_H
#define NEBULA_INPUT_H

#include "types.h"

/*
 * The console input queue.
 *
 * Both the PS/2 keyboard and the serial port feed characters in here, so
 * the machine can be driven interactively from a real keyboard or over a
 * serial line without the shell knowing the difference.
 */

/* Queue a character. Safe to call from an interrupt handler. */
void input_push(char c);

/* Non-blocking: returns 0 when the queue is empty. */
char input_poll(void);

/* Blocks, yielding to other tasks, until a character arrives. */
char input_getchar(void);

/*
 * Read one line with echo and backspace editing. Returns the length,
 * excluding the terminating NUL.
 */
size_t input_readline(char *buf, size_t size);

#endif /* NEBULA_INPUT_H */

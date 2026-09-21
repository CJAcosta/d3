#ifndef NEBULA_KEYBOARD_H
#define NEBULA_KEYBOARD_H

#include "types.h"

void keyboard_init(void);

/* Returns the next character typed, blocking until one is available. */
char keyboard_getchar(void);

/* Non-blocking: returns 0 when the buffer is empty. */
char keyboard_poll(void);

/* Reads a line into buf with echo and backspace editing. Returns the
 * length, excluding the terminating NUL. */
size_t keyboard_readline(char *buf, size_t size);

#endif /* NEBULA_KEYBOARD_H */

#ifndef NEBULA_SERIAL_H
#define NEBULA_SERIAL_H

#include "types.h"

/*
 * COM1 mirrors everything the console prints. Under QEMU this gives us a
 * text transcript of the boot on the host's stdout, which is how the
 * automated tests observe the kernel.
 */
void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *s);
bool serial_available(void);

/*
 * Start delivering received bytes into the console input queue, so the
 * machine can be driven over a serial line as well as from the keyboard.
 * Requires the IDT and PIC to be up.
 */
void serial_enable_input(void);

#endif /* NEBULA_SERIAL_H */

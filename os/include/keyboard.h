#ifndef NEBULA_KEYBOARD_H
#define NEBULA_KEYBOARD_H

#include "types.h"

/*
 * PS/2 keyboard driver. Decoded characters go to the shared console
 * input queue in input.h; this header only starts the driver.
 */
void keyboard_init(void);

#endif /* NEBULA_KEYBOARD_H */

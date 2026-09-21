#ifndef NEBULA_GDT_H
#define NEBULA_GDT_H

#include "types.h"

/* Selectors, i.e. byte offsets into the GDT, with the RPL in the low bits. */
#define SEL_KERNEL_CODE 0x08
#define SEL_KERNEL_DATA 0x10
#define SEL_USER_CODE   (0x18 | 3)
#define SEL_USER_DATA   (0x20 | 3)
#define SEL_TSS         0x28

void gdt_init(void);

/* Point the TSS at the stack to use when entering ring 0 from ring 3. */
void tss_set_kernel_stack(uint32_t esp0);

#endif /* NEBULA_GDT_H */

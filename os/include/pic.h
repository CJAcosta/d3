#ifndef NEBULA_PIC_H
#define NEBULA_PIC_H

#include "types.h"

/*
 * The two cascaded 8259A interrupt controllers. Out of reset they
 * deliver IRQ0-7 as vectors 8-15, which collide with the CPU's own
 * exception vectors, so remapping them is mandatory.
 */
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

/* True if the PIC reports this IRQ as in-service (used to spot spurious
 * interrupts on lines 7 and 15). */
bool pic_irq_is_real(uint8_t irq);

#endif /* NEBULA_PIC_H */

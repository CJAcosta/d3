#ifndef NEBULA_IDT_H
#define NEBULA_IDT_H

#include "types.h"
#include "regs.h"

#define IDT_ENTRIES 256

/* Vector numbers the PIC's IRQ lines are remapped to. */
#define IRQ_BASE       32
#define IRQ_TIMER      0
#define IRQ_KEYBOARD   1
#define IRQ_CASCADE    2

#define SYSCALL_VECTOR 128

typedef void (*interrupt_handler_t)(struct registers *regs);

void idt_init(void);

/* Register a handler for a hardware IRQ line (0-15). */
void irq_register(uint8_t irq, interrupt_handler_t handler);

/* Register a handler for a raw interrupt vector (0-255). */
void isr_register(uint8_t vector, interrupt_handler_t handler);

#endif /* NEBULA_IDT_H */

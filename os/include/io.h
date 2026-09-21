#ifndef NEBULA_IO_H
#define NEBULA_IO_H

#include "types.h"

/* Port I/O primitives. These are the only way to reach legacy hardware. */

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*
 * Some devices need a moment between back-to-back accesses. Writing to
 * the unused POST diagnostic port burns roughly one bus cycle without
 * side effects.
 */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/* Returns true if IF was set, so callers can restore the prior state. */
static inline bool irq_save(void)
{
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0" : "=r"(flags));
    cli();
    return (flags & 0x200) != 0;
}

static inline void irq_restore(bool enabled)
{
    if (enabled)
        sti();
}

#endif /* NEBULA_IO_H */

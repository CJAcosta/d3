/*
 * timer.c -- the 8253/8254 programmable interval timer on IRQ0.
 *
 * This is the kernel's heartbeat: it drives uptime accounting and the
 * preemption point for the scheduler.
 */

#include "timer.h"
#include "idt.h"
#include "io.h"
#include "sched.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

/* The PIT is driven by a fixed 1.193182 MHz oscillator. */
#define PIT_BASE_FREQUENCY 1193182u

static volatile uint32_t ticks;

static void timer_tick(struct registers *regs)
{
    (void)regs;
    ticks++;

    /* Preempt the running task; a no-op until the scheduler is up. */
    sched_tick();
}

uint32_t timer_ticks(void)
{
    return ticks;
}

uint32_t timer_uptime_ms(void)
{
    return ticks * (1000u / TIMER_HZ);
}

void timer_sleep(uint32_t ms)
{
    uint32_t target = ticks + (ms * TIMER_HZ) / 1000u;

    /* Compare with a signed difference so the wait still terminates
     * correctly when the tick counter wraps at 2^32. */
    while ((int32_t)(ticks - target) < 0)
        sched_yield();
}

void timer_init(void)
{
    uint32_t divisor = PIT_BASE_FREQUENCY / TIMER_HZ;

    /* 0x36 = channel 0, access low byte then high byte, mode 3 (square
     * wave), binary counting. */
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_register(IRQ_TIMER, timer_tick);
}

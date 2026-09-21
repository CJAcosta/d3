#ifndef NEBULA_TIMER_H
#define NEBULA_TIMER_H

#include "types.h"

#define TIMER_HZ 100

void     timer_init(void);
uint32_t timer_ticks(void);
uint32_t timer_uptime_ms(void);

/* Busy-wait, yielding to the scheduler, for at least ms milliseconds. */
void     timer_sleep(uint32_t ms);

#endif /* NEBULA_TIMER_H */

#ifndef NEBULA_SCHED_H
#define NEBULA_SCHED_H

#include "types.h"

#define TASK_NAME_MAX  24
#define TASK_STACK_SIZE 16384

enum task_state {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_ZOMBIE
};

struct task {
    /* Must stay first: context_switch writes the saved stack pointer
     * through a pointer to this field. */
    uint32_t         esp;

    uint32_t         id;
    char             name[TASK_NAME_MAX];
    enum task_state  state;

    uint8_t         *stack;       /* base of the allocation, for freeing */
    uint32_t         wake_tick;   /* when a sleeping task becomes runnable */
    uint32_t         cpu_ticks;   /* time slices consumed, for `ps` */

    struct task     *next;        /* circular run queue */
};

typedef void (*task_entry_t)(void *arg);

void sched_init(void);

/* Create a runnable task. Returns its id, or 0 on failure. */
uint32_t sched_spawn(const char *name, task_entry_t entry, void *arg);

/* Give up the rest of the current time slice. */
void sched_yield(void);

/* Called from the timer interrupt; drives preemption and wakeups. */
void sched_tick(void);

/* Terminate the calling task. Never returns. */
void sched_exit(void) __attribute__((noreturn));

/* Block the caller for at least ms milliseconds. */
void sched_sleep(uint32_t ms);

struct task *sched_current(void);
void         sched_dump(void);
uint32_t     sched_task_count(void);

#endif /* NEBULA_SCHED_H */

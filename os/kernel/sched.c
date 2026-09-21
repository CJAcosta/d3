/*
 * sched.c -- preemptive round-robin scheduling.
 *
 * Tasks are kernel threads: they share the kernel's address space but
 * each owns a stack. A circular list is the run queue; the timer
 * interrupt calls sched_tick(), which preempts the running task when its
 * slice expires.
 *
 * Switching from inside the timer interrupt works because each task's
 * interrupt frame lives on that task's own stack. When a task is resumed
 * it returns through context_switch into the handler it was preempted
 * in, finishes it, and irets back to whatever it was doing.
 */

#include "sched.h"
#include "heap.h"
#include "timer.h"
#include "kprintf.h"
#include "string.h"
#include "io.h"

/* How many timer ticks a task runs before it is preempted. */
#define TIME_SLICE_TICKS 5

extern void context_switch(uint32_t *save_esp, uint32_t new_esp);

static struct task *current;
static struct task *task_list;      /* circular; task_list == current's ring */
static uint32_t     next_task_id = 1;
static uint32_t     slice_remaining;
static bool         scheduler_ready;
static uint32_t     task_count;

/*
 * Where a task lands when its entry function returns. Tasks have no
 * caller to return to, so this must not fall through.
 */
static void task_trampoline_exit(void)
{
    sched_exit();
}

struct task *sched_current(void)
{
    return current;
}

uint32_t sched_task_count(void)
{
    return task_count;
}

/* Free a finished task's stack and unlink it from the run queue. */
static void reap_zombies(void)
{
    struct task *prev = current;

    /* current is never a zombie here, so starting from it means we never
     * try to unlink the node we are standing on. */
    for (;;) {
        struct task *victim = prev->next;

        if (victim == current)
            break;

        if (victim->state != TASK_ZOMBIE) {
            prev = victim;
            continue;
        }

        prev->next = victim->next;
        if (task_list == victim)
            task_list = victim->next;

        kfree(victim->stack);
        kfree(victim);
        task_count--;
    }
}

/*
 * Pick the next runnable task and switch to it. Returns true if a switch
 * actually happened. Must be called with interrupts disabled.
 *
 * Never enables interrupts itself: it can run inside the timer IRQ, and
 * an sti there would let the timer re-enter the handler on the same
 * stack. Callers that need to wait do their idling outside this function.
 */
static bool schedule(void)
{
    if (!scheduler_ready || !current)
        return false;

    uint32_t now = timer_ticks();

    /* Wake anyone whose sleep has elapsed. The signed comparison keeps
     * this correct across the tick counter's 32-bit wrap. */
    struct task *scan = current;
    do {
        if (scan->state == TASK_SLEEPING &&
            (int32_t)(now - scan->wake_tick) >= 0)
            scan->state = TASK_READY;
        scan = scan->next;
    } while (scan != current);

    reap_zombies();

    /* Round robin: start after the current task so everyone gets a turn. */
    struct task *next = current->next;
    while (next != current && next->state != TASK_READY)
        next = next->next;

    if (next == current) {
        /*
         * Nobody else is runnable. If the current task can carry on it
         * simply keeps the CPU; if it cannot (it is sleeping or exiting)
         * we report that no switch happened and let the caller idle.
         */
        if (current->state == TASK_READY)
            current->state = TASK_RUNNING;

        slice_remaining = TIME_SLICE_TICKS;
        return false;
    }

    struct task *previous = current;

    if (previous->state == TASK_RUNNING)
        previous->state = TASK_READY;

    next->state = TASK_RUNNING;
    current = next;
    slice_remaining = TIME_SLICE_TICKS;

    context_switch(&previous->esp, next->esp);

    /* Execution resumes here when this task is scheduled again. */
    return true;
}

/*
 * Wait for an interrupt with interrupts briefly enabled. Only safe from
 * task context -- never from inside an interrupt handler.
 */
static void idle_once(void)
{
    sti();
    hlt();
    cli();
}

void sched_yield(void)
{
    if (!scheduler_ready)
        return;

    bool enabled = irq_save();
    schedule();
    irq_restore(enabled);
}

void sched_tick(void)
{
    if (!scheduler_ready || !current)
        return;

    current->cpu_ticks++;

    if (slice_remaining > 0)
        slice_remaining--;

    /* Reschedule when the slice runs out, and also whenever someone is
     * due to wake, so sleepers are not delayed by a full slice. */
    if (slice_remaining == 0)
        schedule();
}

void sched_sleep(uint32_t ms)
{
    if (!scheduler_ready) {
        timer_sleep(ms);
        return;
    }

    bool enabled = irq_save();

    current->wake_tick = timer_ticks() + (ms * TIMER_HZ) / 1000u;
    current->state = TASK_SLEEPING;

    /*
     * Loop rather than switching once: schedule() returns immediately
     * when no other task is runnable, and a spurious wakeup must not be
     * mistaken for the sleep having elapsed.
     */
    while (current->state == TASK_SLEEPING) {
        if (!schedule())
            idle_once();
    }

    irq_restore(enabled);
}

void sched_exit(void)
{
    cli();

    if (task_count == 1)
        panic("sched: the last task (%s) exited", current->name);

    current->state = TASK_ZOMBIE;

    /*
     * Never returns: a zombie is never chosen again, so the only way out
     * is a switch to another task. If every other task is asleep we idle
     * until a timer interrupt wakes one of them. Our stack is reclaimed
     * by whichever task next runs the reaper.
     */
    for (;;) {
        if (!schedule())
            idle_once();
    }
}

uint32_t sched_spawn(const char *name, task_entry_t entry, void *arg)
{
    struct task *task = kcalloc(1, sizeof(struct task));
    if (!task)
        return 0;

    task->stack = kmalloc(TASK_STACK_SIZE);
    if (!task->stack) {
        kfree(task);
        return 0;
    }

    strlcpy(task->name, name, TASK_NAME_MAX);
    task->id    = next_task_id++;
    task->state = TASK_READY;

    /*
     * Build a stack that context_switch can "resume". Laid out from the
     * top down, so that after its final `ret` the CPU is executing entry
     * with a normal-looking call frame beneath it:
     *
     *     arg                  <- entry's first argument
     *     task_trampoline_exit <- entry's return address
     *     entry                <- consumed by context_switch's ret
     *     ebp, ebx, esi, edi   <- restored by the pops
     *     eflags               <- restored by popfl (esp points here)
     */
    uint32_t *sp = (uint32_t *)(task->stack + TASK_STACK_SIZE);

    /* Keep the stack 16-byte aligned at the entry boundary. */
    sp = (uint32_t *)((uint32_t)sp & ~0xFu);

    *--sp = (uint32_t)arg;
    *--sp = (uint32_t)task_trampoline_exit;
    *--sp = (uint32_t)entry;
    *--sp = 0;              /* ebp */
    *--sp = 0;              /* ebx */
    *--sp = 0;              /* esi */
    *--sp = 0;              /* edi */
    *--sp = 0x202;          /* eflags: reserved bit 1, IF set */

    task->esp = (uint32_t)sp;

    bool enabled = irq_save();

    /* Splice into the run queue right after the current task. */
    task->next = current->next;
    current->next = task;
    task_count++;

    irq_restore(enabled);

    return task->id;
}

void sched_init(void)
{
    /*
     * The context we are already running in becomes task 1. Its stack
     * pointer is filled in the first time it is switched away from, so
     * there is nothing to build here.
     */
    struct task *boot = kcalloc(1, sizeof(struct task));
    if (!boot)
        panic("sched: could not allocate the boot task");

    strlcpy(boot->name, "kernel", TASK_NAME_MAX);
    boot->id    = next_task_id++;
    boot->state = TASK_RUNNING;
    boot->stack = NULL;         /* the boot stack lives in .bss */
    boot->next  = boot;

    current   = boot;
    task_list = boot;
    task_count = 1;

    slice_remaining = TIME_SLICE_TICKS;
    scheduler_ready = true;

    kprintf("[sched] round-robin scheduler up, slice %u ticks\n",
            TIME_SLICE_TICKS);
}

static const char *state_name(enum task_state state)
{
    switch (state) {
    case TASK_READY:    return "ready";
    case TASK_RUNNING:  return "running";
    case TASK_SLEEPING: return "sleeping";
    case TASK_ZOMBIE:   return "zombie";
    default:            return "?";
    }
}

void sched_dump(void)
{
    bool enabled = irq_save();

    kprintf("%4s  %-16s %-9s %8s\n", "PID", "NAME", "STATE", "TICKS");

    struct task *task = current;
    do {
        kprintf("%4u  %-16s %-9s %8u%s\n",
                task->id, task->name, state_name(task->state),
                task->cpu_ticks, (task == current) ? "  <-- current" : "");
        task = task->next;
    } while (task != current);

    irq_restore(enabled);
}

/*
 * switch.S -- the context switch.
 *
 * void context_switch(uint32_t *save_esp, uint32_t new_esp);
 *
 * Saves the callee-saved registers and flags of the current task onto
 * its own stack, records the resulting stack pointer through save_esp,
 * then adopts new_esp and unwinds the mirror image for the incoming
 * task. Everything else -- the caller-saved registers -- is already
 * dead across a call by the C ABI, so it does not need preserving.
 *
 * The switch is symmetric: a task resumed here returns from the same
 * context_switch call that suspended it, with the stack exactly as it
 * left it. A brand new task instead has a synthetic stack built by
 * sched_spawn that makes the final `ret` land on its entry point.
 */

.section .text
.globl context_switch

context_switch:
    movl    4(%esp), %eax       /* save_esp */
    movl    8(%esp), %edx       /* new_esp  */

    pushl   %ebp
    pushl   %ebx
    pushl   %esi
    pushl   %edi
    pushfl

    movl    %esp, (%eax)        /* remember where the outgoing task stopped */
    movl    %edx, %esp          /* ... and stand on the incoming task's stack */

    popfl
    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp
    ret

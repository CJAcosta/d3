/*
 * isr.S -- interrupt stubs.
 *
 * The CPU gives every handler a slightly different stack: some
 * exceptions push an error code, most do not, and none of them record
 * which vector fired. These stubs normalise all of that into one layout
 * (see struct registers) before calling into C.
 */

.section .text

/* Exception vectors that do NOT push an error code: push a dummy zero so
 * every frame has the same shape. */
.macro ISR_NOERR num
    .globl isr\num
isr\num:
    cli
    pushl   $0
    pushl   $\num
    jmp     isr_common_stub
.endm

/* Exception vectors where the CPU already pushed an error code. */
.macro ISR_ERR num
    .globl isr\num
isr\num:
    cli
    pushl   $\num
    jmp     isr_common_stub
.endm

/* Hardware interrupts. \num is the IDT vector, \irq the PIC line. */
.macro IRQ num, irq
    .globl irq\irq
irq\irq:
    cli
    pushl   $0
    pushl   $\num
    jmp     irq_common_stub
.endm

/* CPU exceptions 0-31. 8, 10-14 and 17 deliver an error code. */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

/* The PIC is remapped so IRQ0-15 arrive as vectors 32-47. */
IRQ 32, 0
IRQ 33, 1
IRQ 34, 2
IRQ 35, 3
IRQ 36, 4
IRQ 37, 5
IRQ 38, 6
IRQ 39, 7
IRQ 40, 8
IRQ 41, 9
IRQ 42, 10
IRQ 43, 11
IRQ 44, 12
IRQ 45, 13
IRQ 46, 14
IRQ 47, 15

/* The syscall gate. Reachable from ring 3, hence a DPL-3 IDT entry. */
.globl isr128
isr128:
    cli
    pushl   $0
    pushl   $128
    jmp     isr_common_stub

/*
 * Save the rest of the CPU state, switch to kernel data segments, and
 * call the C dispatcher with a pointer to the frame.
 */
.macro COMMON_STUB handler
    pusha                       /* edi, esi, ebp, esp, ebx, edx, ecx, eax */

    xorl    %eax, %eax
    movw    %ds, %ax
    pushl   %eax                /* remember the interrupted data segment */

    movw    $0x10, %ax          /* kernel data selector */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    pushl   %esp                /* struct registers * argument */
    call    \handler
    addl    $4, %esp

    popl    %eax                /* restore the interrupted data segment */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    popa
    addl    $8, %esp            /* discard int_no and err_code */
    iret                        /* also restores EFLAGS, re-enabling IF */
.endm

isr_common_stub:
    COMMON_STUB isr_handler

irq_common_stub:
    COMMON_STUB irq_handler

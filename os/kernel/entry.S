/*
 * entry.S -- the first kernel code to run in protected mode.
 *
 * The boot sector jumps here at 0x10000 with a flat GDT already loaded,
 * segment registers pointing at the flat data descriptor, and the BIOS
 * boot drive in EDX. We are still running on the loader's scratch stack,
 * and .bss is full of whatever the BIOS left in RAM, so before touching
 * any C code we have to zero .bss and move onto a stack we own.
 */

.section .text.boot
.globl _start
.extern kmain

_start:
    cli
    movl    %edx, boot_drive_save   /* stash the boot drive before we clobber EDX */

    /* Zero .bss: C code assumes uninitialised globals start at zero. */
    movl    $__bss_start, %edi
    movl    $__bss_end, %ecx
    subl    %edi, %ecx
    xorl    %eax, %eax
    cld
    rep stosb

    /* Switch to the kernel's own stack. */
    movl    $stack_top, %esp
    movl    %esp, %ebp

    /* A zero return address terminates any stack walk cleanly. */
    pushl   $0
    pushl   $0
    movl    %esp, %ebp

    movl    boot_drive_save, %eax
    pushl   %eax
    call    kmain
    addl    $4, %esp

    /* kmain is not expected to return, but if it does, park the CPU. */
.halt:
    cli
    hlt
    jmp     .halt

.section .data
boot_drive_save:
    .long 0

.section .bss
.align 16
stack_bottom:
    .skip 32768             /* 32 KiB kernel stack */
stack_top:

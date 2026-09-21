/*
 * boot.S -- Stage 1 boot sector for Nebula OS.
 *
 * The BIOS loads this 512-byte sector at 0x7C00 in 16-bit real mode and
 * jumps to it with DL set to the drive we were booted from. Our job:
 *
 *   1. Set up a flat real-mode environment and a stack.
 *   2. Ask the BIOS for the physical memory map (E820) and stash it at
 *      a fixed address the kernel knows about.
 *   3. Read the kernel off the boot media into memory at 0x10000.
 *   4. Enable the A20 gate so we can address more than 1 MiB.
 *   5. Install a flat GDT, switch to 32-bit protected mode and jump to
 *      the kernel.
 *
 * KERNEL_SECTORS is supplied by the Makefile once the kernel binary has
 * been built and its size is known.
 */

#ifndef KERNEL_SECTORS
#define KERNEL_SECTORS 64
#endif

/* Where the kernel image is loaded: segment 0x1000 -> linear 0x10000. */
#define KERNEL_SEG   0x1000
#define KERNEL_ADDR  0x10000

/* The E820 memory map is deposited here for the kernel to consume. */
#define E820_COUNT_ADDR   0x4FF0
#define E820_ENTRIES_ADDR 0x5000

/* Floppy geometry used for the LBA -> CHS conversion. */
#define SECTORS_PER_TRACK 18
#define NUM_HEADS         2

.code16
.section .text
.globl _start

_start:
    /* BIOS may enter with arbitrary segment registers; normalise them. */
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0x7C00, %sp        /* stack grows down, away from our code */
    sti

    /* Remember which drive we came from -- we need it for every read. */
    movb    %dl, boot_drive

    movw    $msg_boot, %si
    call    print_string

    call    detect_memory
    call    load_kernel
    call    enable_a20

    movw    $msg_pmode, %si
    call    print_string

    cli
    lgdt    gdt_descriptor

    movl    %cr0, %eax
    orl     $1, %eax            /* CR0.PE -- protected mode enable */
    movl    %eax, %cr0

    /* Far jump flushes the prefetch queue and loads CS with our selector. */
    ljmp    $0x08, $protected_mode

/* ------------------------------------------------------------------ */
/* Real-mode helpers                                                   */
/* ------------------------------------------------------------------ */

/* print_string: writes the NUL-terminated string at DS:SI via BIOS. */
print_string:
    pusha
    movb    $0x0E, %ah          /* teletype output */
    xorw    %bx, %bx
.print_next:
    lodsb
    testb   %al, %al
    jz      .print_done
    int     $0x10
    jmp     .print_next
.print_done:
    popa
    ret

/*
 * detect_memory: walk the BIOS E820 map into E820_ENTRIES_ADDR.
 *
 * Each entry is 24 bytes (base, length, type, ACPI attrs). The number of
 * entries we managed to collect is written to E820_COUNT_ADDR so the
 * kernel does not have to guess.
 */
detect_memory:
    pusha
    xorw    %ax, %ax
    movw    %ax, %es
    movw    $E820_ENTRIES_ADDR, %di
    xorl    %ebx, %ebx          /* continuation value, 0 = start of walk */
    xorw    %bp, %bp            /* entry counter */

.e820_loop:
    movl    $0xE820, %eax
    movl    $0x534D4150, %edx   /* 'SMAP' */
    movl    $24, %ecx
    /* Force a valid ACPI 3.0 attribute word for BIOSes that ignore it. */
    movl    $1, %es:20(%di)
    int     $0x15
    jc      .e820_done          /* carry on the first call means no E820 */
    cmpl    $0x534D4150, %eax
    jne     .e820_done

    jcxz    .e820_skip          /* zero-length entry, ignore it */
    cmpl    $20, %ecx
    jbe     .e820_keep
    /* ACPI 3.0 returned an "ignore this entry" bit. */
    testb   $1, %es:20(%di)
    jz      .e820_skip

.e820_keep:
    /* Drop entries with a zero length -- they carry no information. */
    movl    8(%di), %eax
    orl     12(%di), %eax
    jz      .e820_skip
    incw    %bp
    addw    $24, %di

.e820_skip:
    testl   %ebx, %ebx          /* EBX == 0 means the walk is complete */
    jz      .e820_done
    cmpw    $32, %bp            /* don't overrun the area we reserved */
    jae     .e820_done
    jmp     .e820_loop

.e820_done:
    movw    %bp, E820_COUNT_ADDR
    popa
    ret

/*
 * load_kernel: read KERNEL_SECTORS sectors starting at LBA 1 to 0x10000.
 *
 * We advance ES by 0x20 (512 bytes) per sector and keep BX at zero, which
 * sidesteps the classic 64 KiB segment-wrap bug that bites when you simply
 * add 512 to BX for a large read.
 */
load_kernel:
    pusha
    movw    $msg_load, %si
    call    print_string

    movw    $KERNEL_SEG, %ax
    movw    %ax, %es
    xorw    %bx, %bx
    movw    $1, %ax             /* LBA 1: the sector right after us */
    movw    $KERNEL_SECTORS, %cx

.load_next:
    pushw   %cx
    pushw   %ax
    call    read_sector
    popw    %ax
    popw    %cx

    incw    %ax                 /* next LBA */
    movw    %es, %dx
    addw    $0x20, %dx          /* next 512-byte window */
    movw    %dx, %es

    decw    %cx
    jnz     .load_next

    movw    $msg_ok, %si
    call    print_string
    popa
    ret

/*
 * read_sector: read the single LBA in AX into ES:BX.
 *
 * Retries up to five times, resetting the disk controller between
 * attempts, because floppy reads (real or emulated) are allowed to fail
 * spuriously on the first try.
 */
read_sector:
    pusha
    movw    $5, %di             /* retry budget */

.rs_attempt:
    pushw   %di
    pushw   %ax

    /* LBA -> CHS: sector = LBA % SPT + 1, head = (LBA / SPT) % heads,
     *             cylinder = (LBA / SPT) / heads                      */
    xorw    %dx, %dx
    movw    $SECTORS_PER_TRACK, %cx
    divw    %cx                 /* AX = LBA/SPT, DX = LBA%SPT */
    incw    %dx
    movb    %dl, %cl            /* CL = sector (1-based) */

    xorw    %dx, %dx
    movw    $NUM_HEADS, %si
    divw    %si                 /* AX = cylinder, DX = head */
    movb    %al, %ch            /* CH = cylinder (low 8 bits) */
    movb    %dl, %dh            /* DH = head */
    movb    boot_drive, %dl

    movw    $0x0201, %ax        /* AH=02 read sectors, AL=1 sector */
    int     $0x13
    jc      .rs_failed

    popw    %ax
    popw    %di
    popa
    ret

.rs_failed:
    xorw    %ax, %ax            /* AH=00: reset disk system */
    movb    boot_drive, %dl
    int     $0x13

    popw    %ax
    popw    %di
    decw    %di
    jnz     .rs_attempt

    movw    $msg_disk_err, %si
    call    print_string
    jmp     halt16

/*
 * enable_a20: without A20 every odd megabyte of physical memory is
 * aliased onto the one below it. Try the fast path first, then fall back
 * to driving the keyboard controller the old-fashioned way.
 */
enable_a20:
    pusha
    /* Fast A20 via the System Control Port. */
    inb     $0x92, %al
    testb   $2, %al
    jnz     .a20_done           /* already on; writing again can lock up */
    orb     $2, %al
    andb    $0xFE, %al          /* never set bit 0: that is a fast reset */
    outb    %al, $0x92

.a20_done:
    popa
    ret

halt16:
    cli
    hlt
    jmp     halt16

/* ------------------------------------------------------------------ */
/* 32-bit protected mode                                               */
/* ------------------------------------------------------------------ */

.code32
protected_mode:
    movw    $0x10, %ax          /* flat data selector */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss
    movl    $0x90000, %esp      /* temporary stack; the kernel installs its own */

    /* Hand the boot drive to the kernel in EDX, then transfer control. */
    xorl    %edx, %edx
    movb    boot_drive, %dl
    ljmp    $0x08, $KERNEL_ADDR

/* ------------------------------------------------------------------ */
/* Data                                                                */
/* ------------------------------------------------------------------ */

boot_drive:
    .byte 0

msg_boot:
    .asciz "Nebula: booting\r\n"
msg_load:
    .asciz "Nebula: loading kernel"
msg_ok:
    .asciz " ok\r\n"
msg_pmode:
    .asciz "Nebula: entering protected mode\r\n"
msg_disk_err:
    .asciz "\r\nNebula: disk read error\r\n"

.align 8
gdt_start:
    /* Null descriptor. */
    .quad 0
gdt_code:
    /* base=0, limit=4GiB, ring 0, code, readable, 32-bit, 4K granularity */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x9A, 0xCF, 0x00
gdt_data:
    /* base=0, limit=4GiB, ring 0, data, writable, 32-bit, 4K granularity */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .long gdt_start

/* The BIOS only treats us as bootable if the last two bytes are 0xAA55. */
.org 510
.word 0xAA55

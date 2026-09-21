/*
 * idt.c -- Interrupt Descriptor Table and the C-side interrupt dispatch.
 */

#include "idt.h"
#include "gdt.h"
#include "pic.h"
#include "io.h"
#include "kprintf.h"
#include "string.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idt_pointer;

static interrupt_handler_t irq_handlers[16];
static interrupt_handler_t isr_handlers[IDT_ENTRIES];

/*
 * A 32-bit interrupt gate: present, given privilege level, type 0xE.
 * Interrupt gates (unlike trap gates) clear IF on entry, so a handler
 * is not immediately re-entered by the interrupt it is servicing.
 */
#define GATE_INTERRUPT 0x0E
#define GATE_PRESENT   0x80
#define GATE_DPL(x)    (((x) & 3) << 5)

static void idt_set_gate(int vector, uint32_t handler, uint8_t dpl)
{
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].selector    = SEL_KERNEL_CODE;
    idt[vector].zero        = 0;
    idt[vector].type_attr   = GATE_PRESENT | GATE_DPL(dpl) | GATE_INTERRUPT;
}

/* Declared in isr.S. */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void); extern void isr128(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static const char *const exception_names[32] = {
    "divide by zero",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid TSS",
    "segment not present",
    "stack-segment fault",
    "general protection fault",
    "page fault",
    "reserved",
    "x87 floating-point exception",
    "alignment check",
    "machine check",
    "SIMD floating-point exception",
    "virtualization exception",
    "control protection exception",
    "reserved", "reserved", "reserved", "reserved",
    "reserved", "reserved",
    "hypervisor injection exception",
    "VMM communication exception",
    "security exception",
    "reserved",
};

void isr_register(uint8_t vector, interrupt_handler_t handler)
{
    isr_handlers[vector] = handler;
}

void irq_register(uint8_t irq, interrupt_handler_t handler)
{
    if (irq >= 16)
        return;

    irq_handlers[irq] = handler;
    pic_unmask_irq(irq);
}

/* Called from isr_common_stub for CPU exceptions and software interrupts. */
void isr_handler(struct registers *regs)
{
    interrupt_handler_t handler = isr_handlers[regs->int_no];

    if (handler) {
        handler(regs);
        return;
    }

    if (regs->int_no < 32) {
        const char *name = exception_names[regs->int_no];

        /* A page fault records the offending address in CR2. */
        if (regs->int_no == 14) {
            uint32_t cr2;
            __asm__ volatile("movl %%cr2, %0" : "=r"(cr2));
            panic("%s at %p\n"
                  "  error=0x%x (%s, %s, %s)\n"
                  "  eip=%p eflags=0x%08x",
                  name, (void *)cr2, regs->err_code,
                  (regs->err_code & 1) ? "protection violation" : "page not present",
                  (regs->err_code & 2) ? "write" : "read",
                  (regs->err_code & 4) ? "user mode" : "kernel mode",
                  (void *)regs->eip, regs->eflags);
        }

        panic("unhandled exception %u: %s\n"
              "  error=0x%x eip=%p eflags=0x%08x\n"
              "  eax=%p ebx=%p ecx=%p edx=%p\n"
              "  esi=%p edi=%p ebp=%p",
              regs->int_no, name, regs->err_code,
              (void *)regs->eip, regs->eflags,
              (void *)regs->eax, (void *)regs->ebx,
              (void *)regs->ecx, (void *)regs->edx,
              (void *)regs->esi, (void *)regs->edi, (void *)regs->ebp);
    }

    kprintf("[idt] ignoring unhandled interrupt vector %u\n", regs->int_no);
}

/* Called from irq_common_stub for hardware interrupts. */
void irq_handler(struct registers *regs)
{
    uint8_t irq = (uint8_t)(regs->int_no - IRQ_BASE);

    /*
     * Lines 7 and 15 double as the PIC's "spurious interrupt" report. If
     * the controller does not actually have the line in service, there
     * is nothing to handle -- and the master must not be sent an EOI for
     * an interrupt it never raised.
     */
    if ((irq == 7 || irq == 15) && !pic_irq_is_real(irq)) {
        if (irq == 15)
            outb(0x20, 0x20);   /* the master still needs its EOI */
        return;
    }

    /*
     * Acknowledge the controller *before* running the handler. The timer
     * handler can context-switch away and not come back for a while; if
     * the EOI waited until afterwards, the PIC would keep IRQ0 masked
     * in-service and no further timer interrupts would ever arrive,
     * freezing preemption. Re-entry is not a concern: an interrupt gate
     * has already cleared IF.
     */
    pic_send_eoi(irq);

    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq](regs);
}

void idt_init(void)
{
    memset(idt, 0, sizeof(idt));
    memset(irq_handlers, 0, sizeof(irq_handlers));
    memset(isr_handlers, 0, sizeof(isr_handlers));

    idt_set_gate(0,  (uint32_t)isr0,  0);
    idt_set_gate(1,  (uint32_t)isr1,  0);
    idt_set_gate(2,  (uint32_t)isr2,  0);
    idt_set_gate(3,  (uint32_t)isr3,  0);
    idt_set_gate(4,  (uint32_t)isr4,  0);
    idt_set_gate(5,  (uint32_t)isr5,  0);
    idt_set_gate(6,  (uint32_t)isr6,  0);
    idt_set_gate(7,  (uint32_t)isr7,  0);
    idt_set_gate(8,  (uint32_t)isr8,  0);
    idt_set_gate(9,  (uint32_t)isr9,  0);
    idt_set_gate(10, (uint32_t)isr10, 0);
    idt_set_gate(11, (uint32_t)isr11, 0);
    idt_set_gate(12, (uint32_t)isr12, 0);
    idt_set_gate(13, (uint32_t)isr13, 0);
    idt_set_gate(14, (uint32_t)isr14, 0);
    idt_set_gate(15, (uint32_t)isr15, 0);
    idt_set_gate(16, (uint32_t)isr16, 0);
    idt_set_gate(17, (uint32_t)isr17, 0);
    idt_set_gate(18, (uint32_t)isr18, 0);
    idt_set_gate(19, (uint32_t)isr19, 0);
    idt_set_gate(20, (uint32_t)isr20, 0);
    idt_set_gate(21, (uint32_t)isr21, 0);
    idt_set_gate(22, (uint32_t)isr22, 0);
    idt_set_gate(23, (uint32_t)isr23, 0);
    idt_set_gate(24, (uint32_t)isr24, 0);
    idt_set_gate(25, (uint32_t)isr25, 0);
    idt_set_gate(26, (uint32_t)isr26, 0);
    idt_set_gate(27, (uint32_t)isr27, 0);
    idt_set_gate(28, (uint32_t)isr28, 0);
    idt_set_gate(29, (uint32_t)isr29, 0);
    idt_set_gate(30, (uint32_t)isr30, 0);
    idt_set_gate(31, (uint32_t)isr31, 0);

    idt_set_gate(IRQ_BASE + 0,  (uint32_t)irq0,  0);
    idt_set_gate(IRQ_BASE + 1,  (uint32_t)irq1,  0);
    idt_set_gate(IRQ_BASE + 2,  (uint32_t)irq2,  0);
    idt_set_gate(IRQ_BASE + 3,  (uint32_t)irq3,  0);
    idt_set_gate(IRQ_BASE + 4,  (uint32_t)irq4,  0);
    idt_set_gate(IRQ_BASE + 5,  (uint32_t)irq5,  0);
    idt_set_gate(IRQ_BASE + 6,  (uint32_t)irq6,  0);
    idt_set_gate(IRQ_BASE + 7,  (uint32_t)irq7,  0);
    idt_set_gate(IRQ_BASE + 8,  (uint32_t)irq8,  0);
    idt_set_gate(IRQ_BASE + 9,  (uint32_t)irq9,  0);
    idt_set_gate(IRQ_BASE + 10, (uint32_t)irq10, 0);
    idt_set_gate(IRQ_BASE + 11, (uint32_t)irq11, 0);
    idt_set_gate(IRQ_BASE + 12, (uint32_t)irq12, 0);
    idt_set_gate(IRQ_BASE + 13, (uint32_t)irq13, 0);
    idt_set_gate(IRQ_BASE + 14, (uint32_t)irq14, 0);
    idt_set_gate(IRQ_BASE + 15, (uint32_t)irq15, 0);

    /* DPL 3: user code must be allowed to issue `int $0x80`. */
    idt_set_gate(SYSCALL_VECTOR, (uint32_t)isr128, 3);

    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base  = (uint32_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idt_pointer));
}

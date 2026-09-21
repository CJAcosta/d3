/*
 * pic.c -- the 8259A programmable interrupt controllers.
 */

#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x11  /* edge triggered, cascade mode, expect ICW4 */
#define ICW4_8086 0x01  /* 8086/88 mode rather than MCS-80/85 */

#define PIC_EOI   0x20
#define PIC_READ_ISR 0x0B

#define PIC1_VECTOR_OFFSET 0x20
#define PIC2_VECTOR_OFFSET 0x28

void pic_init(void)
{
    /* Preserve whatever masks the BIOS left in place. */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT);
    io_wait();

    /* ICW2: the vector each controller's IRQ0 maps to. */
    outb(PIC1_DATA, PIC1_VECTOR_OFFSET);
    io_wait();
    outb(PIC2_DATA, PIC2_VECTOR_OFFSET);
    io_wait();

    /* ICW3: how the two are wired together -- slave on master's IRQ2. */
    outb(PIC1_DATA, 1 << 2);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq)
{
    /* An IRQ from the slave was relayed through the master, so both
     * controllers need to be told the interrupt has been handled. */
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);

    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t)(irq & 7);

    outb(port, (uint8_t)(inb(port) | (1 << bit)));
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t)(irq & 7);

    outb(port, (uint8_t)(inb(port) & ~(1 << bit)));
}

bool pic_irq_is_real(uint8_t irq)
{
    uint16_t cmd = (irq < 8) ? PIC1_CMD : PIC2_CMD;
    uint8_t  bit = (uint8_t)(irq & 7);

    outb(cmd, PIC_READ_ISR);
    return (inb(cmd) & (1 << bit)) != 0;
}

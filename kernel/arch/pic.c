#include "../include/kernel.h"

VOID
INIT_PIC(VOID)
{
    UINT8 Mask1 = INB(PIC1_DATA);
    UINT8 Mask2 = INB(PIC2_DATA);

    OUTB(PIC1_CMD, ICW1_INIT);
    IO_WAIT();
    OUTB(PIC2_CMD, ICW1_INIT);
    IO_WAIT();

    OUTB(PIC1_DATA, IRQ_BASE);
    IO_WAIT();
    OUTB(PIC2_DATA, IRQ_BASE + 8);
    IO_WAIT();

    OUTB(PIC1_DATA, 0x04);
    IO_WAIT();
    OUTB(PIC2_DATA, 0x02);
    IO_WAIT();

    OUTB(PIC1_DATA, ICW4_8086);
    IO_WAIT();
    OUTB(PIC2_DATA, ICW4_8086);
    IO_WAIT();

    OUTB(PIC1_DATA, 0xF8);  /* unmask IRQ0, IRQ1, IRQ2 (cascade) */
    OUTB(PIC2_DATA, 0xEF);  /* unmask IRQ12 (mouse) */

    (void)Mask1;
    (void)Mask2;
}

VOID
PIC_SEND_EOI(
    UINT8 Irq
    )
{
    if (Irq >= 8)
    {
        OUTB(PIC2_CMD, 0x20);
    }
    OUTB(PIC1_CMD, 0x20);
}

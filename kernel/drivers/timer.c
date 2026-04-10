#include "../include/kernel.h"

static volatile UINT64 TickCount = 0;

VOID
INIT_TIMER(VOID)
{
    UINT16 Divisor = (UINT16)(PIT_FREQ / TIMER_HZ);

    OUTB(PIT_CMD, 0x34);
    OUTB(PIT_CH0_DATA, (UINT8)(Divisor & 0xFF));
    OUTB(PIT_CH0_DATA, (UINT8)((Divisor >> 8) & 0xFF));
}

UINT64
GET_TICK_COUNT(VOID)
{
    return TickCount;
}

VOID
TIMER_TICK(VOID)
{
    TickCount++;
}

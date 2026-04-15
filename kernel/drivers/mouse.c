#include "mouse.h"

extern UINT32 FbWidth;
extern UINT32 FbHeight;

static volatile INT64  MouseX = 0;
static volatile INT64  MouseY = 0;
static volatile UINT8  MouseButtons = 0;
static volatile INT64  MouseDx = 0;
static volatile INT64  MouseDy = 0;
static volatile UINT8  MouseDirty = 0;

static UINT8  PacketBuf[3];
static UINT8  PacketIdx = 0;

static VOID
MOUSE_WAIT_INPUT(VOID)
{
    UINT32 Timeout = 100000;
    while (Timeout--)
    {
        if (INB(MOUSE_STATUS_PORT) & 0x02) continue;
        return;
    }
}

static VOID
MOUSE_WAIT_OUTPUT(VOID)
{
    UINT32 Timeout = 100000;
    while (Timeout--)
    {
        if (INB(MOUSE_STATUS_PORT) & 0x01) return;
    }
}

static VOID
MOUSE_WRITE_CMD(
    UINT8 Cmd
    )
{
    MOUSE_WAIT_INPUT();
    OUTB(MOUSE_CMD_PORT, 0xD4);
    MOUSE_WAIT_INPUT();
    OUTB(MOUSE_DATA_PORT, Cmd);
}

static UINT8
MOUSE_READ(VOID)
{
    MOUSE_WAIT_OUTPUT();
    return INB(MOUSE_DATA_PORT);
}

VOID
INIT_MOUSE(VOID)
{
    MouseX = (INT64)FbWidth / 2;
    MouseY = (INT64)FbHeight / 2;
    PacketIdx = 0;

    /* enable auxiliary device (mouse) on PS/2 controller */
    MOUSE_WAIT_INPUT();
    OUTB(MOUSE_CMD_PORT, 0xA8);

    /* get compaq status byte, enable IRQ12 */
    MOUSE_WAIT_INPUT();
    OUTB(MOUSE_CMD_PORT, 0x20);
    MOUSE_WAIT_OUTPUT();
    UINT8 Status = INB(MOUSE_DATA_PORT);
    Status |= 0x02;    /* enable IRQ12 */
    Status &= ~0x20;   /* clear disable mouse clock */
    MOUSE_WAIT_INPUT();
    OUTB(MOUSE_CMD_PORT, 0x60);
    MOUSE_WAIT_INPUT();
    OUTB(MOUSE_DATA_PORT, Status);

    /* set defaults */
    MOUSE_WRITE_CMD(0xF6);
    MOUSE_READ();

    /* set sample rate 100 */
    MOUSE_WRITE_CMD(0xF3);
    MOUSE_READ();
    MOUSE_WRITE_CMD(100);
    MOUSE_READ();

    /* set resolution 8 count/mm */
    MOUSE_WRITE_CMD(0xE8);
    MOUSE_READ();
    MOUSE_WRITE_CMD(0x03);
    MOUSE_READ();

    /* enable data reporting */
    MOUSE_WRITE_CMD(0xF4);
    MOUSE_READ();
}

VOID
MOUSE_IRQ_HANDLER(VOID)
{
    UINT8 Status = INB(MOUSE_STATUS_PORT);
    if (!(Status & 0x20))
    {
        return;
    }

    UINT8 Data = INB(MOUSE_DATA_PORT);
    PacketBuf[PacketIdx] = Data;
    PacketIdx++;

    if (PacketIdx == 1)
    {
        /* first byte must have bit 3 set (always-1 bit) */
        if (!(Data & 0x08))
        {
            PacketIdx = 0;
            return;
        }
    }

    if (PacketIdx < 3)
    {
        return;
    }

    PacketIdx = 0;

    UINT8 Flags = PacketBuf[0];
    INT64 Dx = (INT64)PacketBuf[1];
    INT64 Dy = (INT64)PacketBuf[2];

    if (Flags & 0x10) Dx |= 0xFFFFFFFFFFFFFF00ULL;
    if (Flags & 0x20) Dy |= 0xFFFFFFFFFFFFFF00ULL;

    /* discard overflow packets */
    if (Flags & 0xC0)
    {
        return;
    }

    MouseButtons = Flags & 0x07;
    MouseDx = Dx;
    MouseDy = -Dy;  /* PS/2 Y is inverted relative to screen coords */

    MouseX += Dx;
    MouseY += -Dy;

    if (MouseX < 0) MouseX = 0;
    if (MouseY < 0) MouseY = 0;
    if (MouseX >= (INT64)FbWidth)  MouseX = (INT64)FbWidth - 1;
    if (MouseY >= (INT64)FbHeight) MouseY = (INT64)FbHeight - 1;

    MouseDirty = 1;
}

MOUSE_STATE
MOUSE_GET_STATE(VOID)
{
    MOUSE_STATE S;
    S.X       = MouseX;
    S.Y       = MouseY;
    S.Buttons = MouseButtons;
    S.DeltaX  = MouseDx;
    S.DeltaY  = MouseDy;
    return S;
}

UINT8
MOUSE_HAS_EVENT(VOID)
{
    return MouseDirty;
}

MOUSE_STATE
MOUSE_POLL(VOID)
{
    while (!MouseDirty)
    {
        __asm__ volatile("hlt");
    }
    MouseDirty = 0;
    return MOUSE_GET_STATE();
}

MOUSE_STATE
MOUSE_CONSUME(VOID)
{
    MouseDirty = 0;
    return MOUSE_GET_STATE();
}

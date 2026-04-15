#include "keyboard.h"

static const char SCANCODE_TABLE[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char SCANCODE_TABLE_SHIFT[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static KEY_EVENT KeyBuffer[KB_BUFFER_SIZE];
static volatile UINT32 BufferHead = 0;
static volatile UINT32 BufferTail = 0;

static volatile UINT8 ShiftHeld  = 0;
static volatile UINT8 CapsActive = 0;

static VOID
BUFFER_PUSH(
    KEY_EVENT *Event
    )
{
    UINT32 Next = (BufferHead + 1) % KB_BUFFER_SIZE;
    if (Next == BufferTail)
    {
        return;
    }

    KeyBuffer[BufferHead] = *Event;
    BufferHead = Next;
}

VOID
INIT_KEYBOARD(VOID)
{
    BufferHead = 0;
    BufferTail = 0;
    ShiftHeld  = 0;
    CapsActive = 0;
}

char
KB_SCANCODE_TO_ASCII(
    UINT8 Scancode,
    UINT8 Shift,
    UINT8 CapsLock
    )
{
    if (Scancode >= 128)
    {
        return 0;
    }

    char C;
    if (Shift)
    {
        C = SCANCODE_TABLE_SHIFT[Scancode];
    }
    else
    {
        C = SCANCODE_TABLE[Scancode];
    }

    if (CapsLock && C >= 'a' && C <= 'z')
    {
        C -= 32;
    }
    else if (CapsLock && C >= 'A' && C <= 'Z' && !Shift)
    {
        /* caps + no shift = uppercase already handled above for lowercase */
    }
    else if (CapsLock && Shift && C >= 'A' && C <= 'Z')
    {
        C += 32;
    }

    return C;
}

VOID
KB_IRQ_HANDLER(VOID)
{
    UINT8 Scancode = INB(KB_DATA_PORT);
    UINT8 Released = Scancode & 0x80;
    UINT8 KeyCode  = Scancode & 0x7F;

    if (Scancode == KEY_LSHIFT || Scancode == KEY_RSHIFT)
    {
        ShiftHeld = 1;
        return;
    }
    if (Scancode == KEY_LSHIFT_REL || Scancode == KEY_RSHIFT_REL)
    {
        ShiftHeld = 0;
        return;
    }

    if (!Released && KeyCode == KEY_CAPS_LOCK)
    {
        CapsActive = !CapsActive;
        return;
    }

    KEY_EVENT Event;
    Event.Scancode = KeyCode;
    Event.Pressed  = !Released;
    Event.Shift    = ShiftHeld;
    Event.CapsLock = CapsActive;
    Event.Ascii    = KB_SCANCODE_TO_ASCII(KeyCode, ShiftHeld, CapsActive);

    BUFFER_PUSH(&Event);
}

UINT8
KB_HAS_KEY(VOID)
{
    return BufferHead != BufferTail;
}

KEY_EVENT
KB_POLL(VOID)
{
    while (!KB_HAS_KEY())
    {
        __asm__ volatile("hlt");
    }

    KEY_EVENT Event = KeyBuffer[BufferTail];
    BufferTail = (BufferTail + 1) % KB_BUFFER_SIZE;
    return Event;
}

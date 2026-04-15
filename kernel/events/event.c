#include "event.h"

static EVENT EventBuffer[EVENT_BUFFER_SIZE];
static volatile UINT32 EvHead = 0;
static volatile UINT32 EvTail = 0;

VOID
INIT_EVENTS(VOID)
{
    EvHead = 0;
    EvTail = 0;
}

static VOID
EVENT_PUSH(
    EVENT *Ev
    )
{
    UINT32 Next = (EvHead + 1) % EVENT_BUFFER_SIZE;
    if (Next == EvTail)
    {
        return;
    }
    EventBuffer[EvHead] = *Ev;
    EvHead = Next;
}

VOID
EVENT_PUSH_KEY(
    KEY_EVENT *Ev
    )
{
    EVENT E;
    E.Type = EVENT_KEY;
    E.Key  = *Ev;
    EVENT_PUSH(&E);
}

VOID
EVENT_PUSH_MOUSE(
    MOUSE_STATE *Ev
    )
{
    EVENT E;
    E.Type  = EVENT_MOUSE;
    E.Mouse = *Ev;
    EVENT_PUSH(&E);
}

UINT8
EVENT_HAS_EVENT(VOID)
{
    return EvHead != EvTail;
}

EVENT
EVENT_POLL(VOID)
{
    while (!EVENT_HAS_EVENT())
    {
        __asm__ volatile("hlt");
    }

    EVENT Ev = EventBuffer[EvTail];
    EvTail = (EvTail + 1) % EVENT_BUFFER_SIZE;
    return Ev;
}

EVENT
EVENT_PEEK(VOID)
{
    EVENT Ev;
    if (!EVENT_HAS_EVENT())
    {
        Ev.Type = EVENT_NONE;
        return Ev;
    }
    return EventBuffer[EvTail];
}

#ifndef EVENT_H
#define EVENT_H

#include "../include/kernel.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"

#define EVENT_BUFFER_SIZE 512

#define EVENT_NONE   0
#define EVENT_KEY    1
#define EVENT_MOUSE  2

typedef struct _EVENT
{
    UINT8 Type;
    union
    {
        KEY_EVENT   Key;
        MOUSE_STATE Mouse;
    };
} EVENT, *PEVENT;

VOID
INIT_EVENTS(VOID);

VOID
EVENT_PUSH_KEY(
    KEY_EVENT *Ev
    );

VOID
EVENT_PUSH_MOUSE(
    MOUSE_STATE *Ev
    );

UINT8
EVENT_HAS_EVENT(VOID);

EVENT
EVENT_POLL(VOID);

EVENT
EVENT_PEEK(VOID);

#endif

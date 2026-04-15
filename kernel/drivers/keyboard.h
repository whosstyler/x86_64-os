#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../include/kernel.h"

#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64
#define KB_BUFFER_SIZE  256

#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_LSHIFT_REL  0xAA
#define KEY_RSHIFT_REL  0xB6
#define KEY_CAPS_LOCK   0x3A
#define KEY_BACKSPACE   0x0E
#define KEY_ENTER       0x1C
#define KEY_TAB         0x0F
#define KEY_ESCAPE      0x01

typedef struct _KEY_EVENT
{
    UINT8 Scancode;
    char  Ascii;
    UINT8 Pressed;
    UINT8 Shift;
    UINT8 CapsLock;
} KEY_EVENT, *PKEY_EVENT;

VOID
INIT_KEYBOARD(VOID);

VOID
KB_IRQ_HANDLER(VOID);

UINT8
KB_HAS_KEY(VOID);

KEY_EVENT
KB_POLL(VOID);

char
KB_SCANCODE_TO_ASCII(
    UINT8 Scancode,
    UINT8 Shift,
    UINT8 CapsLock
    );

#endif

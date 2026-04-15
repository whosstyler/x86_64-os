#ifndef WIDGET_H
#define WIDGET_H

#include "../include/kernel.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"

#define WIDGET_LABEL    1
#define WIDGET_BUTTON   2
#define WIDGET_TEXTBOX  3

#define TEXTBOX_MAX_LEN 256

typedef struct _WINDOW WINDOW;
typedef WINDOW *PWINDOW;
typedef struct _WIDGET WIDGET;
typedef WIDGET *PWIDGET;

typedef VOID (*WIDGET_CLICK_FN)(PWINDOW Win, PWIDGET W);
typedef VOID (*WIDGET_TEXT_FN)(PWINDOW Win, PWIDGET W, const char *Text);

struct _WIDGET
{
    UINT8   Type;
    UINT32  RelX;
    UINT32  RelY;
    UINT32  W;
    UINT32  H;
    UINT8   Focused;

    union
    {
        struct
        {
            const char *Text;
            UINT32      Color;
        } Label;

        struct
        {
            const char     *Text;
            UINT32          BgColor;
            UINT32          FgColor;
            UINT8           Hovered;
            UINT8           Pressed;
            WIDGET_CLICK_FN OnClick;
        } Button;

        struct
        {
            char            Buf[TEXTBOX_MAX_LEN];
            UINT32          Len;
            UINT32          CursorPos;
            UINT32          BgColor;
            UINT32          FgColor;
            WIDGET_TEXT_FN  OnSubmit;
        } Textbox;
    };

    struct _WIDGET *Next;
};

PWIDGET
CREATE_LABEL(
    struct _WINDOW *Win,
    UINT32 RelX,
    UINT32 RelY,
    const char *Text,
    UINT32 Color
    );

PWIDGET
CREATE_BUTTON(
    struct _WINDOW *Win,
    UINT32 RelX,
    UINT32 RelY,
    UINT32 W,
    UINT32 H,
    const char *Text,
    UINT32 BgColor,
    UINT32 FgColor,
    WIDGET_CLICK_FN OnClick
    );

PWIDGET
CREATE_TEXTBOX(
    struct _WINDOW *Win,
    UINT32 RelX,
    UINT32 RelY,
    UINT32 W,
    UINT32 H,
    WIDGET_TEXT_FN OnSubmit
    );

VOID
WIDGET_DRAW_ALL(
    struct _WINDOW *Win
    );

VOID
WIDGET_DISPATCH_KEY(
    struct _WINDOW *Win,
    KEY_EVENT *Ev
    );

VOID
WIDGET_DISPATCH_MOUSE(
    struct _WINDOW *Win,
    MOUSE_STATE *Ms
    );

#endif

#ifndef WINDOW_H
#define WINDOW_H

#include "../include/kernel.h"
#include "../events/event.h"

#define MAX_WINDOWS       32
#define WIN_TITLE_MAX     64
#define TITLEBAR_HEIGHT   24
#define CLOSE_BTN_SIZE    16
#define WIN_BORDER        2

#define WIN_FLAG_VISIBLE  0x01
#define WIN_FLAG_FOCUSED  0x02
#define WIN_FLAG_DRAGGING 0x04
#define WIN_FLAG_CLOSABLE 0x08

typedef struct _WINDOW WINDOW;
typedef WINDOW *PWINDOW;
struct _WIDGET;

typedef VOID (*WIN_EVENT_FN)(PWINDOW Win, EVENT *Ev);
typedef VOID (*WIN_DRAW_FN)(PWINDOW Win);

struct _WINDOW
{
    UINT32  X;
    UINT32  Y;
    UINT32  W;
    UINT32  H;
    char    Title[WIN_TITLE_MAX];
    UINT8   Flags;
    UINT8   ZOrder;
    UINT32  Id;

    INT64   DragOffX;
    INT64   DragOffY;

    WIN_EVENT_FN OnEvent;
    WIN_DRAW_FN  OnDraw;

    struct _WIDGET *Widgets;

    UINT32  ContentX;
    UINT32  ContentY;
    UINT32  ContentW;
    UINT32  ContentH;
};

PWINDOW
CREATE_WINDOW(
    UINT32 X,
    UINT32 Y,
    UINT32 W,
    UINT32 H,
    const char *Title,
    UINT8 Flags
    );

VOID
DESTROY_WINDOW(
    PWINDOW Win
    );

VOID
FOCUS_WINDOW(
    PWINDOW Win
    );

PWINDOW
WINDOW_AT_POINT(
    INT64 X,
    INT64 Y
    );

UINT8
WINDOW_TITLEBAR_HIT(
    PWINDOW Win,
    INT64 X,
    INT64 Y
    );

UINT8
WINDOW_CLOSE_BTN_HIT(
    PWINDOW Win,
    INT64 X,
    INT64 Y
    );

PWINDOW
GET_FOCUSED_WINDOW(VOID);

UINT32
GET_WINDOW_COUNT(VOID);

PWINDOW
GET_WINDOW_BY_INDEX(
    UINT32 Idx
    );

PWINDOW
GET_WINDOW_BY_ID(
    UINT32 Id
    );

#endif

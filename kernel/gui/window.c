#include "window.h"
#include "../mm/heap.h"

static WINDOW Windows[MAX_WINDOWS];
static UINT8  WindowUsed[MAX_WINDOWS];
static UINT32 NextWindowId = 1;

static UINT32
KSTRLEN(
    const char *S
    )
{
    UINT32 Len = 0;
    while (S[Len]) Len++;
    return Len;
}

static VOID
KSTRCPY(
    char *Dst,
    const char *Src,
    UINT32 Max
    )
{
    UINT32 I;
    for (I = 0; I < Max - 1 && Src[I]; I++)
    {
        Dst[I] = Src[I];
    }
    Dst[I] = '\0';

    (void)KSTRLEN;
}

PWINDOW
CREATE_WINDOW(
    UINT32 X,
    UINT32 Y,
    UINT32 W,
    UINT32 H,
    const char *Title,
    UINT8 Flags
    )
{
    UINT32 I;
    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (!WindowUsed[I])
        {
            WindowUsed[I] = 1;
            PWINDOW Win = &Windows[I];

            Win->X = X;
            Win->Y = Y;
            Win->W = W;
            Win->H = H;
            KSTRCPY(Win->Title, Title, WIN_TITLE_MAX);
            Win->Flags   = Flags | WIN_FLAG_VISIBLE;
            Win->ZOrder  = 0;
            Win->Id      = NextWindowId++;
            Win->DragOffX = 0;
            Win->DragOffY = 0;
            Win->OnEvent = 0;
            Win->OnDraw  = 0;
            Win->Widgets = 0;

            Win->ContentX = X + WIN_BORDER;
            Win->ContentY = Y + TITLEBAR_HEIGHT;
            Win->ContentW = W - 2 * WIN_BORDER;
            Win->ContentH = H - TITLEBAR_HEIGHT - WIN_BORDER;

            FOCUS_WINDOW(Win);
            return Win;
        }
    }
    return 0;
}

VOID
DESTROY_WINDOW(
    PWINDOW Win
    )
{
    UINT32 I;
    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (WindowUsed[I] && &Windows[I] == Win)
        {
            WindowUsed[I] = 0;
            Win->Flags = 0;
            return;
        }
    }
}

VOID
FOCUS_WINDOW(
    PWINDOW Win
    )
{
    UINT32 I;
    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (WindowUsed[I])
        {
            Windows[I].Flags &= ~WIN_FLAG_FOCUSED;
        }
    }
    Win->Flags |= WIN_FLAG_FOCUSED;

    UINT8 MaxZ = 0;
    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (WindowUsed[I] && Windows[I].ZOrder > MaxZ)
        {
            MaxZ = Windows[I].ZOrder;
        }
    }
    Win->ZOrder = MaxZ + 1;
}

PWINDOW
WINDOW_AT_POINT(
    INT64 X,
    INT64 Y
    )
{
    PWINDOW TopWin = 0;
    UINT8 TopZ = 0;
    UINT32 I;

    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (!WindowUsed[I]) continue;
        if (!(Windows[I].Flags & WIN_FLAG_VISIBLE)) continue;

        PWINDOW W = &Windows[I];
        if (X >= (INT64)W->X && X < (INT64)(W->X + W->W) &&
            Y >= (INT64)W->Y && Y < (INT64)(W->Y + W->H))
        {
            if (!TopWin || W->ZOrder > TopZ)
            {
                TopWin = W;
                TopZ   = W->ZOrder;
            }
        }
    }
    return TopWin;
}

UINT8
WINDOW_TITLEBAR_HIT(
    PWINDOW Win,
    INT64 X,
    INT64 Y
    )
{
    return (X >= (INT64)Win->X &&
            X < (INT64)(Win->X + Win->W) &&
            Y >= (INT64)Win->Y &&
            Y < (INT64)(Win->Y + TITLEBAR_HEIGHT));
}

UINT8
WINDOW_CLOSE_BTN_HIT(
    PWINDOW Win,
    INT64 X,
    INT64 Y
    )
{
    if (!(Win->Flags & WIN_FLAG_CLOSABLE)) return 0;

    INT64 BtnX = (INT64)(Win->X + Win->W - CLOSE_BTN_SIZE - 4);
    INT64 BtnY = (INT64)(Win->Y + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2);

    return (X >= BtnX && X < BtnX + CLOSE_BTN_SIZE &&
            Y >= BtnY && Y < BtnY + CLOSE_BTN_SIZE);
}

PWINDOW
GET_FOCUSED_WINDOW(VOID)
{
    UINT32 I;
    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (WindowUsed[I] && (Windows[I].Flags & WIN_FLAG_FOCUSED))
        {
            return &Windows[I];
        }
    }
    return 0;
}

UINT32
GET_WINDOW_COUNT(VOID)
{
    UINT32 Count = 0;
    UINT32 I;
    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (WindowUsed[I]) Count++;
    }
    return Count;
}

PWINDOW
GET_WINDOW_BY_INDEX(
    UINT32 Idx
    )
{
    UINT32 Count = 0;
    UINT32 I;
    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (WindowUsed[I])
        {
            if (Count == Idx) return &Windows[I];
            Count++;
        }
    }
    return 0;
}

PWINDOW
GET_WINDOW_BY_ID(
    UINT32 Id
    )
{
    UINT32 I;
    for (I = 0; I < MAX_WINDOWS; I++)
    {
        if (WindowUsed[I] && Windows[I].Id == Id)
        {
            return &Windows[I];
        }
    }
    return 0;
}

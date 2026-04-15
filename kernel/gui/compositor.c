#include "compositor.h"
#include "window.h"
#include "widget.h"
#include "../gfx/draw.h"
#include "../gfx/font.h"
#include "../events/event.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../mm/heap.h"

extern UINT32 *Framebuffer;
extern UINT32  FbWidth;
extern UINT32  FbHeight;
extern UINT32  FbPitch;
extern VOID FB_PRINT(const char *S);
extern VOID FB_PRINT_HEX(UINT64 Value);
extern UINT64 GET_TICK_COUNT(VOID);

static VOID
DRAW_TEXT(
    UINT32 X,
    UINT32 Y,
    const char *Text,
    UINT32 Fg,
    UINT32 Bg
    )
{
    UINT32 I = 0;
    while (Text[I])
    {
        UINT32 Glyph = (UINT32)(unsigned char)Text[I];
        if (Glyph >= 128) Glyph = '?';
        const uint8_t *Bitmap = FONT_8X16[Glyph];
        UINT32 Row, Col;
        for (Row = 0; Row < FONT_HEIGHT; Row++)
        {
            for (Col = 0; Col < FONT_WIDTH; Col++)
            {
                if (Bitmap[Row] & (0x80 >> Col))
                {
                    UINT32 Px = X + I * FONT_WIDTH + Col;
                    UINT32 Py = Y + Row;
                    if (Px < FbWidth && Py < FbHeight)
                    {
                        UINT32 *RowPtr = (UINT32 *)((UINT8 *)Framebuffer + Py * FbPitch);
                        RowPtr[Px] = Fg;
                    }
                }
                else if (Bg != 0xFF000000)
                {
                    UINT32 Px = X + I * FONT_WIDTH + Col;
                    UINT32 Py = Y + Row;
                    if (Px < FbWidth && Py < FbHeight)
                    {
                        UINT32 *RowPtr = (UINT32 *)((UINT8 *)Framebuffer + Py * FbPitch);
                        RowPtr[Px] = Bg;
                    }
                }
            }
        }
        I++;
    }
}

static UINT32
TEXT_LEN(
    const char *S
    )
{
    UINT32 Len = 0;
    while (S[Len]) Len++;
    return Len;
}

VOID
COMPOSITOR_DRAW_WINDOW(
    PWINDOW Win
    )
{
    if (!(Win->Flags & WIN_FLAG_VISIBLE)) return;

    UINT8 Focused = (Win->Flags & WIN_FLAG_FOCUSED) ? 1 : 0;
    UINT32 TitleColor = Focused ? COLOR_TITLEBAR_ACT : COLOR_TITLEBAR;

    /* border */
    FB_DRAW_RECT(Win->X, Win->Y, Win->W, Win->H, COLOR_WIN_BORDER);
    if (Focused)
    {
        FB_DRAW_RECT(Win->X + 1, Win->Y + 1, Win->W - 2, Win->H - 2, COLOR_TITLEBAR_ACT);
    }

    /* title bar */
    FB_FILL_RECT(Win->X + WIN_BORDER, Win->Y + WIN_BORDER,
                 Win->W - 2 * WIN_BORDER, TITLEBAR_HEIGHT - WIN_BORDER,
                 TitleColor);

    /* title text (centered in titlebar) */
    UINT32 TitleLen = TEXT_LEN(Win->Title);
    UINT32 TextW = TitleLen * FONT_WIDTH;
    UINT32 TitleX = Win->X + (Win->W - TextW) / 2;
    UINT32 TitleY = Win->Y + (TITLEBAR_HEIGHT - FONT_HEIGHT) / 2;
    DRAW_TEXT(TitleX, TitleY, Win->Title, COLOR_TEXT, TitleColor);

    /* close button */
    if (Win->Flags & WIN_FLAG_CLOSABLE)
    {
        UINT32 BtnX = Win->X + Win->W - CLOSE_BTN_SIZE - 6;
        UINT32 BtnY = Win->Y + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
        FB_FILL_RECT(BtnX, BtnY, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE, COLOR_CLOSE_BG);
        /* draw X */
        FB_DRAW_LINE(BtnX + 3, BtnY + 3,
                     BtnX + CLOSE_BTN_SIZE - 4, BtnY + CLOSE_BTN_SIZE - 4,
                     COLOR_CLOSE_FG);
        FB_DRAW_LINE(BtnX + CLOSE_BTN_SIZE - 4, BtnY + 3,
                     BtnX + 3, BtnY + CLOSE_BTN_SIZE - 4,
                     COLOR_CLOSE_FG);
    }

    /* content area */
    Win->ContentX = Win->X + WIN_BORDER;
    Win->ContentY = Win->Y + TITLEBAR_HEIGHT;
    Win->ContentW = Win->W - 2 * WIN_BORDER;
    Win->ContentH = Win->H - TITLEBAR_HEIGHT - WIN_BORDER;

    FB_FILL_RECT(Win->ContentX, Win->ContentY,
                 Win->ContentW, Win->ContentH, COLOR_WIN_BG);

    /* draw widgets */
    if (Win->OnDraw)
    {
        Win->OnDraw(Win);
    }

    WIDGET_DRAW_ALL(Win);
}

VOID
COMPOSITOR_DRAW_CURSOR(
    INT64 X,
    INT64 Y
    )
{
    /* simple arrow cursor: 12x16 */
    static const UINT8 CursorBitmap[16] = {
        0x80, 0xC0, 0xE0, 0xF0,
        0xF8, 0xFC, 0xFE, 0xFF,
        0xFC, 0xFC, 0xF0, 0xD8,
        0x98, 0x0C, 0x0C, 0x06
    };
    INT64 Row, Col;

    for (Row = 0; Row < 16; Row++)
    {
        for (Col = 0; Col < 8; Col++)
        {
            INT64 Px = X + Col;
            INT64 Py = Y + Row;
            if (Px < 0 || Py < 0) continue;
            if ((UINT32)Px >= FbWidth || (UINT32)Py >= FbHeight) continue;

            if (CursorBitmap[Row] & (0x80 >> Col))
            {
                UINT32 *RowPtr = (UINT32 *)((UINT8 *)Framebuffer + Py * FbPitch);
                if (Col == 0 || Row == 0 ||
                    !(CursorBitmap[Row] & (0x80 >> (Col - 1))) ||
                    (Row > 0 && !(CursorBitmap[Row - 1] & (0x80 >> Col))))
                {
                    RowPtr[Px] = COLOR_CURSOR_BORDER;
                }
                else
                {
                    RowPtr[Px] = COLOR_CURSOR;
                }
            }
        }
    }
}

static VOID
DRAW_TASKBAR(VOID)
{
    UINT32 TbY = FbHeight - TASKBAR_HEIGHT;
    FB_FILL_RECT(0, TbY, FbWidth, TASKBAR_HEIGHT, COLOR_TASKBAR_BG);
    FB_DRAW_HLINE(0, TbY, FbWidth, COLOR_WIN_BORDER);

    UINT32 BtnX = 8;
    UINT32 BtnW = 120;
    UINT32 BtnH = TASKBAR_HEIGHT - 8;
    UINT32 BtnY = TbY + 4;
    UINT32 WinCount = GET_WINDOW_COUNT();
    UINT32 I;

    for (I = 0; I < WinCount; I++)
    {
        PWINDOW Win = GET_WINDOW_BY_INDEX(I);
        if (!Win) continue;

        UINT32 BtnColor = (Win->Flags & WIN_FLAG_FOCUSED) ? COLOR_TASKBAR_ACT : COLOR_TASKBAR_BTN;
        FB_FILL_RECT(BtnX, BtnY, BtnW, BtnH, BtnColor);
        FB_DRAW_RECT(BtnX, BtnY, BtnW, BtnH, COLOR_WIN_BORDER);

        UINT32 TxtX = BtnX + 6;
        UINT32 TxtY = BtnY + (BtnH - FONT_HEIGHT) / 2;
        DRAW_TEXT(TxtX, TxtY, Win->Title, COLOR_TEXT, BtnColor);

        BtnX += BtnW + 4;
    }

    /* uptime on far right */
    UINT64 Ticks = GET_TICK_COUNT();
    UINT64 Secs = Ticks / 100;
    char TimeBuf[16];
    UINT32 Pos = 0;
    UINT64 Mins = Secs / 60;
    UINT64 S = Secs % 60;
    UINT64 H = Mins / 60;
    UINT64 M = Mins % 60;

    TimeBuf[Pos++] = '0' + (char)(H / 10);
    TimeBuf[Pos++] = '0' + (char)(H % 10);
    TimeBuf[Pos++] = ':';
    TimeBuf[Pos++] = '0' + (char)(M / 10);
    TimeBuf[Pos++] = '0' + (char)(M % 10);
    TimeBuf[Pos++] = ':';
    TimeBuf[Pos++] = '0' + (char)(S / 10);
    TimeBuf[Pos++] = '0' + (char)(S % 10);
    TimeBuf[Pos] = '\0';

    UINT32 TimeW = Pos * FONT_WIDTH;
    DRAW_TEXT(FbWidth - TimeW - 12, TbY + (TASKBAR_HEIGHT - FONT_HEIGHT) / 2,
              TimeBuf, COLOR_TEXT, COLOR_TASKBAR_BG);
}

static VOID
SORT_WINDOWS_BY_Z(
    PWINDOW *Sorted,
    UINT32 Count
    )
{
    UINT32 I, J;
    for (I = 0; I < Count; I++)
    {
        for (J = I + 1; J < Count; J++)
        {
            if (Sorted[J]->ZOrder < Sorted[I]->ZOrder)
            {
                PWINDOW Tmp = Sorted[I];
                Sorted[I] = Sorted[J];
                Sorted[J] = Tmp;
            }
        }
    }
}

VOID
COMPOSITOR_DRAW_FRAME(VOID)
{
    /* desktop background */
    FB_FILL_RECT(0, 0, FbWidth, FbHeight - TASKBAR_HEIGHT, COLOR_DESKTOP_BG);

    /* sort windows by z-order and draw back-to-front */
    UINT32 Count = GET_WINDOW_COUNT();
    PWINDOW Sorted[MAX_WINDOWS];
    UINT32 I;

    for (I = 0; I < Count; I++)
    {
        Sorted[I] = GET_WINDOW_BY_INDEX(I);
    }
    SORT_WINDOWS_BY_Z(Sorted, Count);

    for (I = 0; I < Count; I++)
    {
        COMPOSITOR_DRAW_WINDOW(Sorted[I]);
    }

    DRAW_TASKBAR();

    MOUSE_STATE Ms = MOUSE_GET_STATE();
    COMPOSITOR_DRAW_CURSOR(Ms.X, Ms.Y);

    FB_FLIP();
}

static PWINDOW DragWindow = 0;

VOID
COMPOSITOR_RUN(VOID)
{
    COMPOSITOR_DRAW_FRAME();

    for (;;)
    {
        __asm__ volatile("hlt");

        UINT8 NeedRedraw = 0;

        while (EVENT_HAS_EVENT())
        {
            EVENT Ev = EVENT_POLL();

            if (Ev.Type == EVENT_MOUSE)
            {
                NeedRedraw = 1;
                MOUSE_STATE Ms = Ev.Mouse;

                if (Ms.Buttons & MOUSE_BTN_LEFT)
                {
                    if (DragWindow && (DragWindow->Flags & WIN_FLAG_DRAGGING))
                    {
                        INT64 NewX = Ms.X - DragWindow->DragOffX;
                        INT64 NewY = Ms.Y - DragWindow->DragOffY;
                        if (NewX < 0) NewX = 0;
                        if (NewY < 0) NewY = 0;
                        if (NewX + (INT64)DragWindow->W > (INT64)FbWidth)
                            NewX = (INT64)FbWidth - (INT64)DragWindow->W;
                        if (NewY + (INT64)DragWindow->H > (INT64)(FbHeight - TASKBAR_HEIGHT))
                            NewY = (INT64)(FbHeight - TASKBAR_HEIGHT) - (INT64)DragWindow->H;
                        DragWindow->X = (UINT32)NewX;
                        DragWindow->Y = (UINT32)NewY;
                    }
                    else
                    {
                        /* check taskbar button clicks */
                        UINT32 TbY = FbHeight - TASKBAR_HEIGHT;
                        if (Ms.Y >= (INT64)TbY)
                        {
                            UINT32 BtnX = 8;
                            UINT32 BtnW = 120;
                            UINT32 WinCount = GET_WINDOW_COUNT();
                            UINT32 J;
                            for (J = 0; J < WinCount; J++)
                            {
                                if (Ms.X >= (INT64)BtnX && Ms.X < (INT64)(BtnX + BtnW))
                                {
                                    PWINDOW W = GET_WINDOW_BY_INDEX(J);
                                    if (W) FOCUS_WINDOW(W);
                                    break;
                                }
                                BtnX += BtnW + 4;
                            }
                        }
                        else
                        {
                            PWINDOW Hit = WINDOW_AT_POINT(Ms.X, Ms.Y);
                            if (Hit)
                            {
                                FOCUS_WINDOW(Hit);

                                if (WINDOW_CLOSE_BTN_HIT(Hit, Ms.X, Ms.Y))
                                {
                                    DESTROY_WINDOW(Hit);
                                    DragWindow = 0;
                                }
                                else if (WINDOW_TITLEBAR_HIT(Hit, Ms.X, Ms.Y))
                                {
                                    Hit->Flags |= WIN_FLAG_DRAGGING;
                                    Hit->DragOffX = Ms.X - (INT64)Hit->X;
                                    Hit->DragOffY = Ms.Y - (INT64)Hit->Y;
                                    DragWindow = Hit;
                                }
                                else if (Hit->OnEvent)
                                {
                                    Hit->OnEvent(Hit, &Ev);
                                }

                                WIDGET_DISPATCH_MOUSE(Hit, &Ms);
                            }
                        }
                    }
                }
                else
                {
                    if (DragWindow)
                    {
                        DragWindow->Flags &= ~WIN_FLAG_DRAGGING;
                        DragWindow = 0;
                    }
                }
            }
            else if (Ev.Type == EVENT_KEY)
            {
                NeedRedraw = 1;
                PWINDOW Focused = GET_FOCUSED_WINDOW();
                if (Focused)
                {
                    if (Focused->OnEvent)
                    {
                        Focused->OnEvent(Focused, &Ev);
                    }
                    WIDGET_DISPATCH_KEY(Focused, &Ev.Key);
                }
            }
        }

        /* also redraw if mouse moved (polling for smooth cursor) */
        if (MOUSE_HAS_EVENT())
        {
            MOUSE_CONSUME();
            NeedRedraw = 1;
        }

        if (NeedRedraw)
        {
            COMPOSITOR_DRAW_FRAME();
        }
    }
}

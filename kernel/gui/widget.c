#include "widget.h"
#include "window.h"
#include "../gfx/draw.h"
#include "../gfx/font.h"
#include "../mm/heap.h"

extern UINT32 *Framebuffer;
extern UINT32  FbWidth;
extern UINT32  FbHeight;
extern UINT32  FbPitch;

static VOID
DRAW_TEXT_CLIPPED(
    UINT32 X,
    UINT32 Y,
    const char *Text,
    UINT32 Fg,
    UINT32 Bg,
    UINT32 MaxW
    )
{
    UINT32 I = 0;
    while (Text[I])
    {
        UINT32 CharX = X + I * FONT_WIDTH;
        if (CharX + FONT_WIDTH > X + MaxW) break;

        UINT32 Glyph = (UINT32)(unsigned char)Text[I];
        if (Glyph >= 128) Glyph = '?';
        const uint8_t *Bitmap = FONT_8X16[Glyph];
        UINT32 Row, Col;
        for (Row = 0; Row < FONT_HEIGHT; Row++)
        {
            for (Col = 0; Col < FONT_WIDTH; Col++)
            {
                UINT32 Px = CharX + Col;
                UINT32 Py = Y + Row;
                if (Px >= FbWidth || Py >= FbHeight) continue;
                UINT32 *RowPtr = (UINT32 *)((UINT8 *)Framebuffer + Py * FbPitch);
                if (Bitmap[Row] & (0x80 >> Col))
                    RowPtr[Px] = Fg;
                else if (Bg != 0xFF000000)
                    RowPtr[Px] = Bg;
            }
        }
        I++;
    }
}

static VOID
ADD_WIDGET(
    PWINDOW Win,
    PWIDGET W
    )
{
    W->Next = Win->Widgets;
    Win->Widgets = W;
}

PWIDGET
CREATE_LABEL(
    PWINDOW Win,
    UINT32 RelX,
    UINT32 RelY,
    const char *Text,
    UINT32 Color
    )
{
    PWIDGET W = (PWIDGET)KMALLOC(sizeof(WIDGET));
    if (!W) return 0;

    W->Type    = WIDGET_LABEL;
    W->RelX    = RelX;
    W->RelY    = RelY;
    W->W       = 0;
    W->H       = FONT_HEIGHT;
    W->Focused = 0;
    W->Label.Text  = Text;
    W->Label.Color = Color;
    W->Next = 0;

    ADD_WIDGET(Win, W);
    return W;
}

PWIDGET
CREATE_BUTTON(
    PWINDOW Win,
    UINT32 RelX,
    UINT32 RelY,
    UINT32 BW,
    UINT32 BH,
    const char *Text,
    UINT32 BgColor,
    UINT32 FgColor,
    WIDGET_CLICK_FN OnClick
    )
{
    PWIDGET W = (PWIDGET)KMALLOC(sizeof(WIDGET));
    if (!W) return 0;

    W->Type    = WIDGET_BUTTON;
    W->RelX    = RelX;
    W->RelY    = RelY;
    W->W       = BW;
    W->H       = BH;
    W->Focused = 0;
    W->Button.Text    = Text;
    W->Button.BgColor = BgColor;
    W->Button.FgColor = FgColor;
    W->Button.Hovered = 0;
    W->Button.Pressed = 0;
    W->Button.OnClick = OnClick;
    W->Next = 0;

    ADD_WIDGET(Win, W);
    return W;
}

PWIDGET
CREATE_TEXTBOX(
    PWINDOW Win,
    UINT32 RelX,
    UINT32 RelY,
    UINT32 BW,
    UINT32 BH,
    WIDGET_TEXT_FN OnSubmit
    )
{
    PWIDGET W = (PWIDGET)KMALLOC(sizeof(WIDGET));
    if (!W) return 0;

    W->Type    = WIDGET_TEXTBOX;
    W->RelX    = RelX;
    W->RelY    = RelY;
    W->W       = BW;
    W->H       = BH;
    W->Focused = 0;
    W->Textbox.Len       = 0;
    W->Textbox.CursorPos = 0;
    W->Textbox.Buf[0]    = '\0';
    W->Textbox.BgColor   = 0x002E3440;
    W->Textbox.FgColor   = 0x00ECEFF4;
    W->Textbox.OnSubmit  = OnSubmit;
    W->Next = 0;

    ADD_WIDGET(Win, W);
    return W;
}

VOID
WIDGET_DRAW_ALL(
    PWINDOW Win
    )
{
    PWIDGET W = Win->Widgets;
    while (W)
    {
        UINT32 AbsX = Win->ContentX + W->RelX;
        UINT32 AbsY = Win->ContentY + W->RelY;

        if (W->Type == WIDGET_LABEL)
        {
            DRAW_TEXT_CLIPPED(AbsX, AbsY, W->Label.Text,
                              W->Label.Color, 0xFF000000,
                              Win->ContentW - W->RelX);
        }
        else if (W->Type == WIDGET_BUTTON)
        {
            UINT32 Bg = W->Button.Pressed ? W->Button.FgColor : W->Button.BgColor;
            UINT32 Fg = W->Button.Pressed ? W->Button.BgColor : W->Button.FgColor;
            FB_FILL_RECT(AbsX, AbsY, W->W, W->H, Bg);
            FB_DRAW_RECT(AbsX, AbsY, W->W, W->H, 0x004C566A);

            UINT32 TxtY = AbsY + (W->H - FONT_HEIGHT) / 2;
            UINT32 TxtX = AbsX + 6;
            DRAW_TEXT_CLIPPED(TxtX, TxtY, W->Button.Text, Fg, Bg, W->W - 12);
        }
        else if (W->Type == WIDGET_TEXTBOX)
        {
            FB_FILL_RECT(AbsX, AbsY, W->W, W->H, W->Textbox.BgColor);
            FB_DRAW_RECT(AbsX, AbsY, W->W, W->H,
                         W->Focused ? 0x005E81AC : 0x004C566A);

            UINT32 TxtX = AbsX + 4;
            UINT32 TxtY = AbsY + (W->H - FONT_HEIGHT) / 2;
            DRAW_TEXT_CLIPPED(TxtX, TxtY, W->Textbox.Buf,
                              W->Textbox.FgColor, W->Textbox.BgColor,
                              W->W - 8);

            /* blinking cursor */
            if (W->Focused)
            {
                UINT32 CurX = TxtX + W->Textbox.CursorPos * FONT_WIDTH;
                FB_FILL_RECT(CurX, TxtY, 2, FONT_HEIGHT, W->Textbox.FgColor);
            }
        }

        W = W->Next;
    }
}

VOID
WIDGET_DISPATCH_KEY(
    PWINDOW Win,
    KEY_EVENT *Ev
    )
{
    if (!Ev->Pressed) return;

    PWIDGET W = Win->Widgets;
    while (W)
    {
        if (W->Focused && W->Type == WIDGET_TEXTBOX)
        {
            if (Ev->Scancode == KEY_ENTER)
            {
                if (W->Textbox.OnSubmit)
                {
                    W->Textbox.OnSubmit(Win, W, W->Textbox.Buf);
                }
                W->Textbox.Len = 0;
                W->Textbox.CursorPos = 0;
                W->Textbox.Buf[0] = '\0';
                return;
            }
            else if (Ev->Scancode == KEY_BACKSPACE)
            {
                if (W->Textbox.Len > 0)
                {
                    W->Textbox.Len--;
                    W->Textbox.CursorPos--;
                    W->Textbox.Buf[W->Textbox.Len] = '\0';
                }
                return;
            }
            else if (Ev->Ascii && W->Textbox.Len < TEXTBOX_MAX_LEN - 1)
            {
                W->Textbox.Buf[W->Textbox.Len++] = Ev->Ascii;
                W->Textbox.Buf[W->Textbox.Len] = '\0';
                W->Textbox.CursorPos = W->Textbox.Len;
                return;
            }
        }
        W = W->Next;
    }
}

VOID
WIDGET_DISPATCH_MOUSE(
    PWINDOW Win,
    MOUSE_STATE *Ms
    )
{
    if (!(Ms->Buttons & 0x01)) return;

    INT64 LocalX = Ms->X - (INT64)Win->ContentX;
    INT64 LocalY = Ms->Y - (INT64)Win->ContentY;

    /* unfocus all widgets first */
    PWIDGET W = Win->Widgets;
    while (W)
    {
        W->Focused = 0;
        if (W->Type == WIDGET_BUTTON)
            W->Button.Pressed = 0;
        W = W->Next;
    }

    W = Win->Widgets;
    while (W)
    {
        if (LocalX >= (INT64)W->RelX && LocalX < (INT64)(W->RelX + W->W) &&
            LocalY >= (INT64)W->RelY && LocalY < (INT64)(W->RelY + W->H))
        {
            if (W->Type == WIDGET_BUTTON)
            {
                W->Button.Pressed = 1;
                if (W->Button.OnClick)
                {
                    W->Button.OnClick(Win, W);
                }
            }
            else if (W->Type == WIDGET_TEXTBOX)
            {
                W->Focused = 1;
            }
            return;
        }
        W = W->Next;
    }
}

#include "desktop.h"
#include "window.h"
#include "widget.h"
#include "compositor.h"
#include "../gfx/draw.h"
#include "../gfx/font.h"
#include "../mm/heap.h"
#include "../drivers/keyboard.h"

extern UINT32 FbWidth;
extern UINT32 FbHeight;
extern UINT32 FbPitch;
extern UINT32 *Framebuffer;
extern VOID FB_PRINT(const char *S);
extern VOID FB_PRINT_HEX(UINT64 Value);
extern UINT64 GET_TICK_COUNT(VOID);

#define TERM_MAX_LINES   64
#define TERM_LINE_LEN    80
#define TERM_COLOR_BG    0x002E3440
#define TERM_COLOR_FG    0x00D8DEE9
#define TERM_COLOR_INPUT 0x0088C0D0

typedef struct _TERM_STATE
{
    char    Lines[TERM_MAX_LINES][TERM_LINE_LEN];
    UINT32  LineCount;
    UINT32  ScrollTop;
    PWIDGET InputBox;
    PWINDOW Win;
} TERM_STATE, *PTERM_STATE;

static TERM_STATE TermState;

static UINT8
STR_EQ(
    const char *A,
    const char *B
    )
{
    while (*A && *B)
    {
        if (*A != *B) return 0;
        A++;
        B++;
    }
    return *A == *B;
}

static VOID
KSTRCPY_N(
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
}

static VOID
TERM_ADD_LINE(
    const char *Line
    )
{
    if (TermState.LineCount < TERM_MAX_LINES)
    {
        KSTRCPY_N(TermState.Lines[TermState.LineCount], Line, TERM_LINE_LEN);
        TermState.LineCount++;
    }
    else
    {
        UINT32 I;
        for (I = 0; I < TERM_MAX_LINES - 1; I++)
        {
            KSTRCPY_N(TermState.Lines[I], TermState.Lines[I + 1], TERM_LINE_LEN);
        }
        KSTRCPY_N(TermState.Lines[TERM_MAX_LINES - 1], Line, TERM_LINE_LEN);
    }
}

static VOID
UINT_TO_DEC(
    UINT64 Value,
    char *Buf,
    UINT32 BufSize
    )
{
    char Tmp[21];
    UINT32 I = 0;
    UINT32 J;

    if (Value == 0)
    {
        Buf[0] = '0';
        Buf[1] = '\0';
        return;
    }

    while (Value > 0 && I < 20)
    {
        Tmp[I++] = '0' + (Value % 10);
        Value /= 10;
    }

    for (J = 0; J < I && J < BufSize - 1; J++)
    {
        Buf[J] = Tmp[I - 1 - J];
    }
    Buf[J] = '\0';
}

static VOID
TERM_CMD_HELP(VOID)
{
    TERM_ADD_LINE("  help       show this message");
    TERM_ADD_LINE("  clear      clear terminal");
    TERM_ADD_LINE("  info       system information");
    TERM_ADD_LINE("  uptime     time since boot");
    TERM_ADD_LINE("  echo <s>   print text");
    TERM_ADD_LINE("  sysinfo    open system info window");
    TERM_ADD_LINE("  terminal   open another terminal");
}

static VOID
TERM_CMD_INFO(VOID)
{
    char Buf[TERM_LINE_LEN];
    char Num[21];
    UINT32 I;

    TERM_ADD_LINE("  x86_64-os  |  UEFI boot  |  ring 0");

    /* build resolution string manually */
    for (I = 0; I < TERM_LINE_LEN; I++) Buf[I] = '\0';
    Buf[0] = ' '; Buf[1] = ' ';
    Buf[2] = 'f'; Buf[3] = 'b'; Buf[4] = ':'; Buf[5] = ' ';
    UINT_TO_DEC(FbWidth, Num, 21);
    UINT32 Pos = 6;
    for (I = 0; Num[I] && Pos < TERM_LINE_LEN - 1; I++) Buf[Pos++] = Num[I];
    Buf[Pos++] = 'x';
    UINT_TO_DEC(FbHeight, Num, 21);
    for (I = 0; Num[I] && Pos < TERM_LINE_LEN - 1; I++) Buf[Pos++] = Num[I];
    Buf[Pos] = '\0';
    TERM_ADD_LINE(Buf);

    TERM_ADD_LINE("  double buffered compositor");
    TERM_ADD_LINE("  PIT timer @ 100hz");
}

static VOID
TERM_CMD_UPTIME(VOID)
{
    UINT64 Ticks = GET_TICK_COUNT();
    UINT64 Secs = Ticks / 100;
    char Buf[TERM_LINE_LEN];
    char Num[21];
    UINT32 Pos = 0;
    UINT32 I;

    Buf[Pos++] = ' '; Buf[Pos++] = ' ';
    UINT_TO_DEC(Secs, Num, 21);
    for (I = 0; Num[I] && Pos < TERM_LINE_LEN - 1; I++) Buf[Pos++] = Num[I];
    Buf[Pos++] = ' '; Buf[Pos++] = 's'; Buf[Pos++] = 'e'; Buf[Pos++] = 'c';
    Buf[Pos] = '\0';
    TERM_ADD_LINE(Buf);
}

static VOID
TERM_ON_SUBMIT(
    PWINDOW Win,
    PWIDGET W,
    const char *Text
    )
{
    char CmdLine[TERM_LINE_LEN];
    CmdLine[0] = '>'; CmdLine[1] = ' ';
    KSTRCPY_N(CmdLine + 2, Text, TERM_LINE_LEN - 2);
    TERM_ADD_LINE(CmdLine);

    if (STR_EQ(Text, "help"))
        TERM_CMD_HELP();
    else if (STR_EQ(Text, "clear"))
        TermState.LineCount = 0;
    else if (STR_EQ(Text, "info"))
        TERM_CMD_INFO();
    else if (STR_EQ(Text, "uptime"))
        TERM_CMD_UPTIME();
    else if (STR_EQ(Text, "sysinfo"))
        DESKTOP_OPEN_SYSINFO();
    else if (STR_EQ(Text, "terminal"))
        DESKTOP_OPEN_TERMINAL();
    else if (Text[0] == 'e' && Text[1] == 'c' && Text[2] == 'h' &&
             Text[3] == 'o' && Text[4] == ' ')
    {
        char EchoBuf[TERM_LINE_LEN];
        EchoBuf[0] = ' '; EchoBuf[1] = ' ';
        KSTRCPY_N(EchoBuf + 2, Text + 5, TERM_LINE_LEN - 2);
        TERM_ADD_LINE(EchoBuf);
    }
    else
    {
        char ErrBuf[TERM_LINE_LEN];
        UINT32 Pos = 0;
        UINT32 I;
        const char *Prefix = "  unknown: ";
        for (I = 0; Prefix[I] && Pos < TERM_LINE_LEN - 1; I++) ErrBuf[Pos++] = Prefix[I];
        for (I = 0; Text[I] && Pos < TERM_LINE_LEN - 1; I++) ErrBuf[Pos++] = Text[I];
        ErrBuf[Pos] = '\0';
        TERM_ADD_LINE(ErrBuf);
    }

    (void)Win;
    (void)W;
}

static VOID
TERM_ON_DRAW(
    PWINDOW Win
    )
{
    UINT32 VisibleLines = (Win->ContentH - 28) / FONT_HEIGHT;
    UINT32 StartLine = 0;
    if (TermState.LineCount > VisibleLines)
    {
        StartLine = TermState.LineCount - VisibleLines;
    }

    UINT32 I;
    for (I = StartLine; I < TermState.LineCount; I++)
    {
        UINT32 Row = I - StartLine;
        UINT32 TxtX = Win->ContentX + 4;
        UINT32 TxtY = Win->ContentY + 4 + Row * FONT_HEIGHT;

        UINT32 J = 0;
        while (TermState.Lines[I][J])
        {
            UINT32 Glyph = (UINT32)(unsigned char)TermState.Lines[I][J];
            if (Glyph >= 128) Glyph = '?';
            const uint8_t *Bitmap = FONT_8X16[Glyph];
            UINT32 R, C;
            for (R = 0; R < FONT_HEIGHT; R++)
            {
                for (C = 0; C < FONT_WIDTH; C++)
                {
                    if (Bitmap[R] & (0x80 >> C))
                    {
                        UINT32 Px = TxtX + J * FONT_WIDTH + C;
                        UINT32 Py = TxtY + R;
                        if (Px < Win->ContentX + Win->ContentW &&
                            Py < Win->ContentY + Win->ContentH - 28)
                        {
                            if (Px < FbWidth && Py < FbHeight)
                            {
                                UINT32 *RowPtr = (UINT32 *)((UINT8 *)Framebuffer + Py * FbPitch);
                                RowPtr[Px] = TERM_COLOR_FG;
                            }
                        }
                    }
                }
            }
            J++;
        }
    }
}

PWINDOW
DESKTOP_OPEN_TERMINAL(VOID)
{
    static UINT32 TermCount = 0;
    UINT32 OffX = 60 + (TermCount % 5) * 30;
    UINT32 OffY = 40 + (TermCount % 5) * 30;
    TermCount++;

    PWINDOW Win = CREATE_WINDOW(OffX, OffY, 520, 380, "Terminal",
                                WIN_FLAG_CLOSABLE);
    if (!Win) return 0;

    Win->OnDraw = TERM_ON_DRAW;
    TermState.Win = Win;

    /* input box at bottom of content area */
    PWIDGET Tb = CREATE_TEXTBOX(Win, 4, Win->ContentH - 24, Win->ContentW - 8, 22,
                                TERM_ON_SUBMIT);
    if (Tb) Tb->Focused = 1;
    TermState.InputBox = Tb;

    if (TermState.LineCount == 0)
    {
        TERM_ADD_LINE("x86_64-os terminal");
        TERM_ADD_LINE("type 'help' for commands");
        TERM_ADD_LINE("");
    }

    return Win;
}

static VOID
SYSINFO_ON_DRAW(
    PWINDOW Win
    )
{
    (void)Win;
}

static VOID
ON_BTN_TERMINAL(
    PWINDOW Win,
    PWIDGET W
    )
{
    DESKTOP_OPEN_TERMINAL();
    (void)Win;
    (void)W;
}

static VOID
ON_BTN_SYSINFO(
    PWINDOW Win,
    PWIDGET W
    )
{
    DESKTOP_OPEN_SYSINFO();
    (void)Win;
    (void)W;
}

PWINDOW
DESKTOP_OPEN_SYSINFO(VOID)
{
    PWINDOW Win = CREATE_WINDOW(200, 100, 340, 260, "System Info",
                                WIN_FLAG_CLOSABLE);
    if (!Win) return 0;

    Win->OnDraw = SYSINFO_ON_DRAW;

    CREATE_LABEL(Win, 10, 10, "x86_64-os", 0x0088C0D0);
    CREATE_LABEL(Win, 10, 30, "UEFI boot | ring 0 kernel", 0x00D8DEE9);
    CREATE_LABEL(Win, 10, 56, "Subsystems:", 0x00A3BE8C);
    CREATE_LABEL(Win, 10, 76, "  PMM  VMM  Heap  GDT  IDT", 0x00D8DEE9);
    CREATE_LABEL(Win, 10, 96, "  PIC  PIT  Keyboard  Mouse", 0x00D8DEE9);
    CREATE_LABEL(Win, 10, 116, "  Scheduler  VT-x Hypervisor", 0x00D8DEE9);
    CREATE_LABEL(Win, 10, 142, "Display:", 0x00A3BE8C);
    CREATE_LABEL(Win, 10, 162, "  Double-buffered compositor", 0x00D8DEE9);

    return Win;
}

VOID
INIT_DESKTOP(VOID)
{
    TermState.LineCount = 0;
    TermState.ScrollTop = 0;
    TermState.InputBox  = 0;
    TermState.Win       = 0;

    /* open a launcher window */
    PWINDOW Launcher = CREATE_WINDOW(
        FbWidth / 2 - 140, FbHeight / 2 - 80,
        280, 160, "Launcher", 0);
    if (Launcher)
    {
        CREATE_LABEL(Launcher, 10, 10, "Welcome to x86_64-os", 0x0088C0D0);
        CREATE_BUTTON(Launcher, 10, 40, 120, 28, "Terminal",
                      0x005E81AC, 0x00ECEFF4, ON_BTN_TERMINAL);
        CREATE_BUTTON(Launcher, 150, 40, 120, 28, "System Info",
                      0x00A3BE8C, 0x002E3440, ON_BTN_SYSINFO);
        CREATE_LABEL(Launcher, 10, 80, "click buttons or drag", 0x004C566A);
        CREATE_LABEL(Launcher, 10, 96, "windows by their title bar", 0x004C566A);
    }
}

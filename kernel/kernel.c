#include "include/kernel.h"
#include "gfx/font.h"
#include "gfx/draw.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "sched/task.h"
#include "hv/vmx.h"
#include "drivers/keyboard.h"

extern VOID TIMER_TICK(VOID);
extern UINT64 GET_TICK_COUNT(VOID);

static volatile UINT8 SchedulerActive = 0;

UINT32 *Framebuffer;
UINT32  FbWidth;
UINT32  FbHeight;
UINT32  FbPitch;
UINT32  FbPixelFormat;

static UINT32  CursorCol = 0;
static UINT32  CursorRow = 0;
static UINT32  MaxCols;
static UINT32  MaxRows;

#define FG_COLOR 0x00FFFFFF
#define BG_COLOR 0x00000000

static VOID
FB_PUT_PIXEL(
    UINT32 X,
    UINT32 Y,
    UINT32 Color
    )
{
    if (X < FbWidth && Y < FbHeight)
    {
        UINT32 *Row = (UINT32 *)((UINT8 *)Framebuffer + Y * FbPitch);
        Row[X] = Color;
    }
}

static VOID
FB_DRAW_CHAR(
    UINT32 X,
    UINT32 Y,
    char C,
    UINT32 Fg,
    UINT32 Bg
    )
{
    UINT32 Glyph = (UINT32)(unsigned char)C;
    if (Glyph >= 128) Glyph = '?';

    const uint8_t *Bitmap = FONT_8X16[Glyph];
    UINT32 Row, Col;

    for (Row = 0; Row < FONT_HEIGHT; Row++)
    {
        for (Col = 0; Col < FONT_WIDTH; Col++)
        {
            UINT32 Color = (Bitmap[Row] & (0x80 >> Col)) ? Fg : Bg;
            FB_PUT_PIXEL(X + Col, Y + Row, Color);
        }
    }
}

static VOID
FB_SCROLL(VOID)
{
    UINT8 *Dst = (UINT8 *)Framebuffer;
    UINT8 *Src = (UINT8 *)Framebuffer + FONT_HEIGHT * FbPitch;
    UINT32 CopySize = (FbHeight - FONT_HEIGHT) * FbPitch;
    UINT32 I;

    for (I = 0; I < CopySize; I++)
    {
        Dst[I] = Src[I];
    }

    UINT8 *ClearStart = (UINT8 *)Framebuffer + (FbHeight - FONT_HEIGHT) * FbPitch;
    UINT32 ClearSize = FONT_HEIGHT * FbPitch;
    for (I = 0; I < ClearSize; I++)
    {
        ClearStart[I] = 0;
    }
}

static VOID
FB_PUTCHAR(
    char C
    )
{
    if (C == '\n')
    {
        CursorCol = 0;
        CursorRow++;
        if (CursorRow >= MaxRows)
        {
            FB_SCROLL();
            CursorRow = MaxRows - 1;
        }
        return;
    }

    FB_DRAW_CHAR(
        CursorCol * FONT_WIDTH,
        CursorRow * FONT_HEIGHT,
        C,
        FG_COLOR,
        BG_COLOR
        );

    CursorCol++;
    if (CursorCol >= MaxCols)
    {
        CursorCol = 0;
        CursorRow++;
        if (CursorRow >= MaxRows)
        {
            FB_SCROLL();
            CursorRow = MaxRows - 1;
        }
    }
}

VOID
FB_PRINT(
    const char *S
    )
{
    while (*S)
    {
        FB_PUTCHAR(*S);
        S++;
    }
}

VOID
FB_PRINT_HEX(
    UINT64 Value
    )
{
    char Hex[] = "0123456789ABCDEF";
    char Buf[19];
    INT64 I;

    Buf[0] = '0';
    Buf[1] = 'x';
    for (I = 15; I >= 0; I--)
    {
        Buf[2 + (15 - I)] = Hex[(Value >> (I * 4)) & 0xF];
    }
    Buf[18] = '\0';
    FB_PRINT(Buf);
}

static VOID
FB_CLEAR(VOID)
{
    UINT32 Total = FbHeight * (FbPitch / 4);
    UINT32 I;
    for (I = 0; I < Total; I++)
    {
        Framebuffer[I] = BG_COLOR;
    }
    CursorCol = 0;
    CursorRow = 0;
}

VOID
ISR_HANDLER(
    UINT64 *Regs
    )
{
    UINT64 Vector    = Regs[15];
    UINT64 ErrorCode = Regs[16];

    if (Vector == 1)
    {
        UINT64 Dr6 = READ_DR6();
        FB_PRINT("  HW WATCHPOINT HIT! DR6=");
        FB_PRINT_HEX(Dr6);
        FB_PRINT("\n");
        WRITE_DR7(0);
        WRITE_DR6(0);
        return;
    }

    if (Vector == 14)
    {
        UINT64 FaultAddr = READ_CR2();
        FB_PRINT("\n*** PAGE FAULT ***\n");
        FB_PRINT("  addr:  ");
        FB_PRINT_HEX(FaultAddr);
        FB_PRINT("\n  error: ");
        FB_PRINT_HEX(ErrorCode);
        FB_PRINT("\n  rip:   ");
        FB_PRINT_HEX(Regs[17]);
        FB_PRINT("\n");

        if (!(ErrorCode & 1)) FB_PRINT("  [page not present]\n");
        if (ErrorCode & 2)    FB_PRINT("  [write]\n");
        else                  FB_PRINT("  [read]\n");
        if (ErrorCode & 4)    FB_PRINT("  [user mode]\n");
        else                  FB_PRINT("  [kernel mode]\n");
    }
    else
    {
        FB_PRINT("EXCEPTION #");
        FB_PRINT_HEX(Vector);
        FB_PRINT(" err=");
        FB_PRINT_HEX(ErrorCode);
        FB_PRINT("\n");
    }

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}

VOID
IRQ_HANDLER(
    UINT64 *Regs
    )
{
    UINT64 IrqNum = Regs[15];

    if (IrqNum == 0)
    {
        TIMER_TICK();
        PIC_SEND_EOI(0);
        if (SchedulerActive)
        {
            SCHEDULE();
        }
        return;
    }

    if (IrqNum == 1)
    {
        KB_IRQ_HANDLER();
    }

    PIC_SEND_EOI((UINT8)IrqNum);
}

static UINT8
STREQUAL(
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

static UINT32
STRLEN(
    const char *S
    )
{
    UINT32 Len = 0;
    while (S[Len]) Len++;
    return Len;
}

static VOID
FB_PRINT_DEC(
    UINT64 Value
    )
{
    char Buf[21];
    INT64 I = 0;

    if (Value == 0)
    {
        FB_PUTCHAR('0');
        return;
    }

    while (Value > 0)
    {
        Buf[I++] = '0' + (Value % 10);
        Value /= 10;
    }

    while (--I >= 0)
    {
        FB_PUTCHAR(Buf[I]);
    }
}

#define SHELL_BUF_SIZE 256
#define SHELL_PROMPT   "kernel> "

#define COLOR_TITLE_BG  0x002A2D3E
#define COLOR_TITLE_FG  0x0088C0D0
#define COLOR_ACCENT    0x005E81AC
#define COLOR_SUCCESS   0x00A3BE8C
#define COLOR_ERROR     0x00BF616A
#define COLOR_INFO      0x00EBCB8B
#define COLOR_BORDER    0x004C566A

static VOID
SHELL_DRAW_BANNER(VOID)
{
    UINT32 BannerH = FONT_HEIGHT * 3;
    FB_FILL_RECT(0, 0, FbWidth, BannerH, COLOR_TITLE_BG);
    FB_DRAW_HLINE(0, BannerH, FbWidth, COLOR_ACCENT);
    FB_DRAW_HLINE(0, BannerH + 1, FbWidth, COLOR_ACCENT);

    const char *Title = "x86_64-os kernel shell";
    UINT32 TitleLen = STRLEN(Title);
    UINT32 TitleX = (FbWidth - TitleLen * FONT_WIDTH) / 2;
    UINT32 TitleY = FONT_HEIGHT;
    UINT32 I;

    for (I = 0; I < TitleLen; I++)
    {
        FB_DRAW_CHAR(TitleX + I * FONT_WIDTH, TitleY, Title[I], COLOR_TITLE_FG, COLOR_TITLE_BG);
    }
}

static VOID
SHELL_CMD_HELP(VOID)
{
    FB_PRINT("  help       show this message\n");
    FB_PRINT("  clear      clear the screen\n");
    FB_PRINT("  info       system information\n");
    FB_PRINT("  mem        memory stats\n");
    FB_PRINT("  draw       draw primitive demo\n");
    FB_PRINT("  uptime     seconds since boot\n");
    FB_PRINT("  echo <s>   print text back\n");
    FB_PRINT("  reboot     triple fault reset\n");
}

static VOID
SHELL_CMD_INFO(VOID)
{
    FB_PRINT("  x86_64-os  |  UEFI boot  |  ring 0\n");
    FB_PRINT("  fb: ");
    FB_PRINT_DEC(FbWidth);
    FB_PRINT("x");
    FB_PRINT_DEC(FbHeight);
    FB_PRINT(" (pitch ");
    FB_PRINT_DEC(FbPitch);
    FB_PRINT(")\n");
    FB_PRINT("  timer: PIT @ ");
    FB_PRINT_DEC(TIMER_HZ);
    FB_PRINT("hz\n");
    FB_PRINT("  pixel format: ");
    if (FbPixelFormat == PIXEL_FORMAT_RGBX)
        FB_PRINT("RGBX\n");
    else
        FB_PRINT("BGRX\n");
}

static VOID
SHELL_CMD_DRAW(VOID)
{
    UINT32 BaseX = 40;
    UINT32 BaseY = CursorRow * FONT_HEIGHT + FONT_HEIGHT;

    FB_FILL_RECT(BaseX, BaseY, 80, 60, COLOR_ACCENT);
    FB_DRAW_RECT(BaseX + 100, BaseY, 80, 60, COLOR_SUCCESS);
    FB_DRAW_LINE(BaseX + 200, BaseY, BaseX + 280, BaseY + 60, COLOR_ERROR);
    FB_DRAW_LINE(BaseX + 280, BaseY, BaseX + 200, BaseY + 60, COLOR_ERROR);
    FB_FILL_CIRCLE(BaseX + 340, BaseY + 30, 28, COLOR_INFO);
    FB_DRAW_CIRCLE(BaseX + 420, BaseY + 30, 28, COLOR_TITLE_FG);

    CursorRow += (60 / FONT_HEIGHT) + 2;
    if (CursorRow >= MaxRows)
    {
        CursorRow = MaxRows - 1;
    }
    FB_PRINT("  fill_rect | draw_rect | lines | fill_circle | circle\n");
}

static VOID
SHELL_CMD_UPTIME(VOID)
{
    UINT64 Ticks = GET_TICK_COUNT();
    UINT64 Secs  = Ticks / TIMER_HZ;

    FB_PRINT("  ");
    FB_PRINT_DEC(Secs);
    FB_PRINT(" seconds (");
    FB_PRINT_DEC(Ticks);
    FB_PRINT(" ticks)\n");
}

static VOID
SHELL_CMD_REBOOT(VOID)
{
    FB_PRINT("  rebooting...\n");

    /* triple fault: load a null IDT and trigger an interrupt */
    IDT_PTR NullIdt;
    NullIdt.Limit = 0;
    NullIdt.Base  = 0;
    __asm__ volatile("lidt %0" : : "m"(NullIdt));
    __asm__ volatile("int3");

    for (;;) { __asm__ volatile("hlt"); }
}

static VOID
SHELL_PROCESS_CMD(
    char *Buf,
    UINT32 Len
    )
{
    if (Len == 0) return;

    if (STREQUAL(Buf, "help"))
    {
        SHELL_CMD_HELP();
    }
    else if (STREQUAL(Buf, "clear"))
    {
        FB_CLEAR();
        SHELL_DRAW_BANNER();
        CursorRow = 4;
        CursorCol = 0;
    }
    else if (STREQUAL(Buf, "info"))
    {
        SHELL_CMD_INFO();
    }
    else if (STREQUAL(Buf, "mem"))
    {
        FB_PRINT("  heap: KMALLOC/KFREE active\n");
        FB_PRINT("  pmm:  bitmap allocator\n");
        FB_PRINT("  vmm:  4-level paging, higher-half @ 0xFFFF800000000000\n");
    }
    else if (STREQUAL(Buf, "draw"))
    {
        SHELL_CMD_DRAW();
    }
    else if (STREQUAL(Buf, "uptime"))
    {
        SHELL_CMD_UPTIME();
    }
    else if (Len > 5 && Buf[0] == 'e' && Buf[1] == 'c' && Buf[2] == 'h' && Buf[3] == 'o' && Buf[4] == ' ')
    {
        FB_PRINT("  ");
        FB_PRINT(Buf + 5);
        FB_PRINT("\n");
    }
    else if (STREQUAL(Buf, "reboot"))
    {
        SHELL_CMD_REBOOT();
    }
    else
    {
        FB_PRINT("  unknown command: ");
        FB_PRINT(Buf);
        FB_PRINT("\n  type 'help' for commands\n");
    }
}

static VOID
SHELL_RUN(VOID)
{
    char Buf[SHELL_BUF_SIZE];
    UINT32 Pos = 0;

    FB_CLEAR();
    SHELL_DRAW_BANNER();

    CursorRow = 4;
    CursorCol = 0;
    FB_PRINT("type 'help' for available commands\n\n");
    FB_PRINT(SHELL_PROMPT);

    for (;;)
    {
        KEY_EVENT Ev = KB_POLL();
        if (!Ev.Pressed) continue;

        if (Ev.Scancode == KEY_ENTER)
        {
            FB_PUTCHAR('\n');
            Buf[Pos] = '\0';
            SHELL_PROCESS_CMD(Buf, Pos);
            Pos = 0;
            FB_PRINT(SHELL_PROMPT);
        }
        else if (Ev.Scancode == KEY_BACKSPACE)
        {
            if (Pos > 0)
            {
                Pos--;
                if (CursorCol > 0)
                {
                    CursorCol--;
                }
                else if (CursorRow > 0)
                {
                    CursorRow--;
                    CursorCol = MaxCols - 1;
                }
                FB_DRAW_CHAR(
                    CursorCol * FONT_WIDTH,
                    CursorRow * FONT_HEIGHT,
                    ' ', FG_COLOR, BG_COLOR
                    );
            }
        }
        else if (Ev.Ascii && Pos < SHELL_BUF_SIZE - 1)
        {
            Buf[Pos++] = Ev.Ascii;
            FB_PUTCHAR(Ev.Ascii);
        }
    }
}

static VOID
DEMO_TASK_A(VOID)
{
    UINT64 I;
    for (I = 0; I < 3; I++)
    {
        FB_PRINT("[task A] tick ");
        FB_PRINT_HEX(I);
        FB_PRINT("\n");
        YIELD();
    }
}

static VOID
DEMO_TASK_B(VOID)
{
    UINT64 I;
    for (I = 0; I < 3; I++)
    {
        FB_PRINT("[task B] tick ");
        FB_PRINT_HEX(I);
        FB_PRINT("\n");
        YIELD();
    }
}

VOID
KERNEL_MAIN(
    PBOOT_INFO BootInfo
    )
{
    if (!BootInfo || !BootInfo->FramebufferBase)
    {
        for (;;) { __asm__ volatile("hlt"); }
    }

    Framebuffer   = (UINT32 *)BootInfo->FramebufferBase;
    FbWidth       = BootInfo->FramebufferWidth;
    FbHeight      = BootInfo->FramebufferHeight;
    FbPitch       = BootInfo->FramebufferPitch;
    FbPixelFormat = BootInfo->FramebufferPixelFormat;

    MaxCols = FbWidth / FONT_WIDTH;
    MaxRows = FbHeight / FONT_HEIGHT;

    FB_CLEAR();
    FB_PRINT("kernel alive\n");
    FB_PRINT("fb: ");
    FB_PRINT_HEX(FbWidth);
    FB_PRINT("x");
    FB_PRINT_HEX(FbHeight);
    FB_PRINT("\n");

    FB_PRINT("init gdt...\n");
    INIT_GDT();
    FB_PRINT("gdt ok\n");

    INIT_PIC();
    FB_PRINT("pic remapped\n");

    INIT_IDT();
    FB_PRINT("idt ok\n");

    INIT_TIMER();
    FB_PRINT("timer armed @ 100hz\n");

    INIT_KEYBOARD();
    FB_PRINT("keyboard ok\n");

    INIT_PMM(BootInfo);
    FB_PRINT("pmm ok\n");

    INIT_VMM(BootInfo);
    FB_PRINT("vmm ok\n");

    FB_PRINT("init heap...\n");
    INIT_HEAP();
    FB_PRINT("heap ok\n");

    INIT_CPU_SECURITY();

    INIT_SCHEDULER();
    FB_PRINT("scheduler ok\n");

    PTASK TaskA = CREATE_TASK(DEMO_TASK_A);
    PTASK TaskB = CREATE_TASK(DEMO_TASK_B);
    FB_PRINT("created tasks A (tid=");
    FB_PRINT_HEX(TaskA->Tid);
    FB_PRINT(") B (tid=");
    FB_PRINT_HEX(TaskB->Tid);
    FB_PRINT(")\n");

    SchedulerActive = 1;
    __asm__ volatile("sti");

    while (TaskA->State != TASK_DEAD || TaskB->State != TASK_DEAD)
    {
        __asm__ volatile("hlt");
    }

    SchedulerActive = 0;
    __asm__ volatile("cli");
    FB_PRINT("tasks complete, back in idle\n");

    /* ---- hardware debug register watchpoint ---- */
    FB_PRINT("\n--- hw watchpoint ---\n");
    volatile UINT64 WatchedVar = 0;
    FB_PRINT("setting DR0 on ");
    FB_PRINT_HEX((UINT64)&WatchedVar);
    FB_PRINT("\n");

    WRITE_DR0((UINT64)&WatchedVar);
    WRITE_DR6(0);
    WRITE_DR7(0x00090001ULL);

    WatchedVar = 42;

    FB_PRINT("after write, WatchedVar = ");
    FB_PRINT_HEX(WatchedVar);
    FB_PRINT("\n");

    /* ---- launch hypervisor ---- */
    FB_CLEAR();
    FB_PRINT("--- hypervisor ---\n");
    UINT8 HvResult = INIT_VMX();
    if (HvResult)
    {
        FB_PRINT("hv not available, skipping hv demos\n");
    }
    else
    {
        FB_PRINT("os now running under hypervisor control\n");
    }

    /* ---- CR0.WP bypass attempt (hypervisor should block it) ---- */
    FB_PRINT("\n--- cr0.wp bypass ---\n");
    UINT64 WpPhys = ALLOC_PAGE();
    UINT64 WpVirt = 0x0000000500000000ULL;

    MAP_PAGE(WpVirt, WpPhys, PTE_WRITABLE);
    *(volatile UINT64 *)WpVirt = 0xDEADULL;
    FB_PRINT("wrote 0xDEAD to writable page\n");

    PAGE_TABLE_ENTRY *WpPte = GET_PTE_PTR(WpVirt);
    *WpPte = (WpPhys & PTE_ADDR_MASK) | PTE_PRESENT;
    INVLPG(WpVirt);
    FB_PRINT("page now read-only\n");

    FB_PRINT("attempting CR0.WP clear...\n");
    UINT64 Cr0 = READ_CR0();
    WRITE_CR0(Cr0 & ~(1ULL << 16));

    UINT64 Cr0After = READ_CR0();
    if (Cr0After & (1ULL << 16))
    {
        FB_PRINT("CR0.WP still set! hypervisor blocked it\n");
    }
    else
    {
        FB_PRINT("CR0.WP cleared (no hv protection)\n");
        *(volatile UINT64 *)WpVirt = 0xBEEFULL;
        WRITE_CR0(Cr0);
        FB_PRINT("CR0.WP restored\n");

        UINT64 WpRead = *(volatile UINT64 *)WpVirt;
        FB_PRINT("read back: ");
        FB_PRINT_HEX(WpRead);
        FB_PRINT("\n");
    }

    UNMAP_PAGE(WpVirt);
    FREE_PAGE(WpPhys);

    if (!HvResult)
    {
        /* ---- hypercall interface ---- */
        FB_PRINT("\n--- hypercall interface ---\n");
        UINT64 PingResult = VMCALL_HV(HVCALL_PING, 0, 0);
        FB_PRINT("VMCALL ping reply: ");
        FB_PRINT_HEX(PingResult);
        FB_PRINT("\n");

        /* ---- monitor trap flag single-stepping ---- */
        FB_PRINT("\n--- monitor trap flag ---\n");
        FB_PRINT("single-stepping 5 guest instructions...\n");
        UINT64 MtfResult = VMCALL_HV(HVCALL_MTF_START, 5, 0);
        if (MtfResult == 0)
        {
            __asm__ volatile("nop");
            __asm__ volatile("nop");
            __asm__ volatile("nop");
            __asm__ volatile("nop");
            __asm__ volatile("nop");
            FB_PRINT("guest resumes (was invisible to us)\n");
        }
        else
        {
            FB_PRINT("mtf not available in this environment\n");
        }

        /* ---- LSTAR intercept & redirect ---- */
        FB_PRINT("\n--- lstar intercept ---\n");
        UINT64 CurLstar = VMCALL_HV(HVCALL_LSTAR_GET, 0, 0);
        FB_PRINT("current LSTAR: ");
        FB_PRINT_HEX(CurLstar);
        FB_PRINT("\n");

        FB_PRINT("direct WRMSR to LSTAR (should be denied)...\n");
        WRMSR(0xC0000082, 0xDEADC0DE00000000ULL);

        CurLstar = VMCALL_HV(HVCALL_LSTAR_GET, 0, 0);
        FB_PRINT("LSTAR unchanged: ");
        FB_PRINT_HEX(CurLstar);
        FB_PRINT("\n");

        FB_PRINT("hypercall redirect (authorized)...\n");
        VMCALL_HV(HVCALL_LSTAR_REDIRECT, 0xFFFF800000001000ULL, 0);

        CurLstar = VMCALL_HV(HVCALL_LSTAR_GET, 0, 0);
        FB_PRINT("LSTAR after redirect: ");
        FB_PRINT_HEX(CurLstar);
        FB_PRINT("\n");

        VMCALL_HV(HVCALL_LSTAR_RESTORE, 0, 0);
        CurLstar = VMCALL_HV(HVCALL_LSTAR_GET, 0, 0);
        FB_PRINT("LSTAR after restore: ");
        FB_PRINT_HEX(CurLstar);
        FB_PRINT("\n");

        /* ---- hypervisor self-protection ---- */
        FB_PRINT("\n--- hv self-protection ---\n");
        UINT64 IntResult = VMCALL_HV(HVCALL_CHECK_INTEGRITY, 0, 0);
        FB_PRINT("vmcs integrity check: ");
        if (IntResult == 0)
            FB_PRINT("CLEAN\n");
        else
            FB_PRINT("CORRUPTED!\n");
        FB_PRINT("vmxon/vmcs/msr-bitmap unmapped from guest\n");

        /* ---- anti-debug enforcement ---- */
        FB_PRINT("\n--- anti-debug ---\n");
        VMCALL_HV(HVCALL_ANTIDEBUG_ON, 0, 0);

        volatile UINT64 DebugTarget = 0;
        WRITE_DR0((UINT64)&DebugTarget);
        WRITE_DR6(0);
        WRITE_DR7(0x00090001ULL);

        FB_PRINT("hw breakpoint armed, triggering write...\n");
        DebugTarget = 0x1337;

        UINT64 Dr7After = READ_DR7();
        FB_PRINT("DR7 after: ");
        FB_PRINT_HEX(Dr7After);
        FB_PRINT("\n");
        if (!(Dr7After & 1))
            FB_PRINT("hypervisor wiped debug registers\n");

        FB_PRINT("executing INT3...\n");
        __asm__ volatile("int3");
        FB_PRINT("INT3 swallowed by hypervisor\n");

        VMCALL_HV(HVCALL_ANTIDEBUG_OFF, 0, 0);
    }

    FB_PRINT("\nall demos complete, entering shell...\n");
    __asm__ volatile("sti");

    SHELL_RUN();
}

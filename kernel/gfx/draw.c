#include "draw.h"
#include "../mm/heap.h"

extern UINT32 *Framebuffer;
extern UINT32  FbWidth;
extern UINT32  FbHeight;
extern UINT32  FbPitch;

UINT32 *BackBuffer  = 0;
static UINT32 *FrontBuffer = 0;

VOID
INIT_DOUBLE_BUFFER(VOID)
{
    UINT64 Size = (UINT64)FbHeight * FbPitch;
    BackBuffer = (UINT32 *)KMALLOC(Size);

    UINT8 *Dst = (UINT8 *)BackBuffer;
    UINT8 *Src = (UINT8 *)Framebuffer;
    UINT64 I;
    for (I = 0; I < Size; I++)
    {
        Dst[I] = Src[I];
    }

    FrontBuffer = Framebuffer;
    Framebuffer = BackBuffer;
}

VOID
FB_FLIP(VOID)
{
    if (!FrontBuffer) return;

    UINT64 Size = (UINT64)FbHeight * FbPitch;
    UINT8 *Dst = (UINT8 *)FrontBuffer;
    UINT8 *Src = (UINT8 *)BackBuffer;
    UINT64 I;
    for (I = 0; I < Size; I++)
    {
        Dst[I] = Src[I];
    }
}

static inline VOID
FAST_PUT_PIXEL(
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

VOID
FB_DRAW_HLINE(
    UINT32 X,
    UINT32 Y,
    UINT32 Len,
    UINT32 Color
    )
{
    if (Y >= FbHeight)
    {
        return;
    }

    UINT32 *Row = (UINT32 *)((UINT8 *)Framebuffer + Y * FbPitch);
    UINT32 End = X + Len;
    UINT32 I;

    if (End > FbWidth)
    {
        End = FbWidth;
    }
    for (I = X; I < End; I++)
    {
        Row[I] = Color;
    }
}

VOID
FB_DRAW_VLINE(
    UINT32 X,
    UINT32 Y,
    UINT32 Len,
    UINT32 Color
    )
{
    if (X >= FbWidth)
    {
        return;
    }

    UINT32 End = Y + Len;
    UINT32 I;

    if (End > FbHeight)
    {
        End = FbHeight;
    }
    for (I = Y; I < End; I++)
    {
        UINT32 *Row = (UINT32 *)((UINT8 *)Framebuffer + I * FbPitch);
        Row[X] = Color;
    }
}

VOID
FB_FILL_RECT(
    UINT32 X,
    UINT32 Y,
    UINT32 W,
    UINT32 H,
    UINT32 Color
    )
{
    UINT32 I;

    if (X >= FbWidth || Y >= FbHeight)
    {
        return;
    }
    if (X + W > FbWidth)
    {
        W = FbWidth - X;
    }
    if (Y + H > FbHeight)
    {
        H = FbHeight - Y;
    }

    for (I = 0; I < H; I++)
    {
        FB_DRAW_HLINE(X, Y + I, W, Color);
    }
}

VOID
FB_DRAW_RECT(
    UINT32 X,
    UINT32 Y,
    UINT32 W,
    UINT32 H,
    UINT32 Color
    )
{
    FB_DRAW_HLINE(X, Y, W, Color);
    FB_DRAW_HLINE(X, Y + H - 1, W, Color);
    FB_DRAW_VLINE(X, Y, H, Color);
    FB_DRAW_VLINE(X + W - 1, Y, H, Color);
}

VOID
FB_DRAW_LINE(
    INT64 X0,
    INT64 Y0,
    INT64 X1,
    INT64 Y1,
    UINT32 Color
    )
{
    INT64 Dx = X1 - X0;
    INT64 Dy = Y1 - Y0;
    INT64 Sx = (Dx > 0) ? 1 : -1;
    INT64 Sy = (Dy > 0) ? 1 : -1;
    INT64 Err, E2;

    if (Dx < 0) Dx = -Dx;
    if (Dy < 0) Dy = -Dy;

    Err = Dx - Dy;

    for (;;)
    {
        FAST_PUT_PIXEL((UINT32)X0, (UINT32)Y0, Color);

        if (X0 == X1 && Y0 == Y1)
        {
            break;
        }

        E2 = Err * 2;
        if (E2 > -Dy)
        {
            Err -= Dy;
            X0  += Sx;
        }
        if (E2 < Dx)
        {
            Err += Dx;
            Y0  += Sy;
        }
    }
}

VOID
FB_BLIT_BITMAP(
    UINT32 DstX,
    UINT32 DstY,
    UINT32 W,
    UINT32 H,
    const UINT32 *Pixels
    )
{
    UINT32 Row, Col;
    UINT32 ClipW = W;
    UINT32 ClipH = H;

    if (DstX >= FbWidth || DstY >= FbHeight)
    {
        return;
    }
    if (DstX + ClipW > FbWidth)
    {
        ClipW = FbWidth - DstX;
    }
    if (DstY + ClipH > FbHeight)
    {
        ClipH = FbHeight - DstY;
    }

    for (Row = 0; Row < ClipH; Row++)
    {
        UINT32 *DstRow = (UINT32 *)((UINT8 *)Framebuffer + (DstY + Row) * FbPitch);
        const UINT32 *SrcRow = Pixels + Row * W;

        for (Col = 0; Col < ClipW; Col++)
        {
            DstRow[DstX + Col] = SrcRow[Col];
        }
    }
}

VOID
FB_DRAW_CIRCLE(
    INT64 Cx,
    INT64 Cy,
    INT64 R,
    UINT32 Color
    )
{
    INT64 X = 0;
    INT64 Y = R;
    INT64 D = 3 - 2 * R;

    while (X <= Y)
    {
        FAST_PUT_PIXEL((UINT32)(Cx + X), (UINT32)(Cy + Y), Color);
        FAST_PUT_PIXEL((UINT32)(Cx - X), (UINT32)(Cy + Y), Color);
        FAST_PUT_PIXEL((UINT32)(Cx + X), (UINT32)(Cy - Y), Color);
        FAST_PUT_PIXEL((UINT32)(Cx - X), (UINT32)(Cy - Y), Color);
        FAST_PUT_PIXEL((UINT32)(Cx + Y), (UINT32)(Cy + X), Color);
        FAST_PUT_PIXEL((UINT32)(Cx - Y), (UINT32)(Cy + X), Color);
        FAST_PUT_PIXEL((UINT32)(Cx + Y), (UINT32)(Cy - X), Color);
        FAST_PUT_PIXEL((UINT32)(Cx - Y), (UINT32)(Cy - X), Color);

        if (D < 0)
        {
            D += 4 * X + 6;
        }
        else
        {
            D += 4 * (X - Y) + 10;
            Y--;
        }
        X++;
    }
}

VOID
FB_FILL_CIRCLE(
    INT64 Cx,
    INT64 Cy,
    INT64 R,
    UINT32 Color
    )
{
    INT64 X = 0;
    INT64 Y = R;
    INT64 D = 3 - 2 * R;

    while (X <= Y)
    {
        FB_DRAW_HLINE((UINT32)(Cx - X), (UINT32)(Cy + Y), (UINT32)(2 * X + 1), Color);
        FB_DRAW_HLINE((UINT32)(Cx - X), (UINT32)(Cy - Y), (UINT32)(2 * X + 1), Color);
        FB_DRAW_HLINE((UINT32)(Cx - Y), (UINT32)(Cy + X), (UINT32)(2 * Y + 1), Color);
        FB_DRAW_HLINE((UINT32)(Cx - Y), (UINT32)(Cy - X), (UINT32)(2 * Y + 1), Color);

        if (D < 0)
        {
            D += 4 * X + 6;
        }
        else
        {
            D += 4 * (X - Y) + 10;
            Y--;
        }
        X++;
    }
}

#ifndef DRAW_H
#define DRAW_H

#include "../include/kernel.h"

VOID
INIT_DOUBLE_BUFFER(VOID);

VOID
FB_FLIP(VOID);

VOID
FB_FILL_RECT(
    UINT32 X,
    UINT32 Y,
    UINT32 W,
    UINT32 H,
    UINT32 Color
    );

VOID
FB_DRAW_RECT(
    UINT32 X,
    UINT32 Y,
    UINT32 W,
    UINT32 H,
    UINT32 Color
    );

VOID
FB_DRAW_HLINE(
    UINT32 X,
    UINT32 Y,
    UINT32 Len,
    UINT32 Color
    );

VOID
FB_DRAW_VLINE(
    UINT32 X,
    UINT32 Y,
    UINT32 Len,
    UINT32 Color
    );

VOID
FB_DRAW_LINE(
    INT64 X0,
    INT64 Y0,
    INT64 X1,
    INT64 Y1,
    UINT32 Color
    );

VOID
FB_BLIT_BITMAP(
    UINT32 DstX,
    UINT32 DstY,
    UINT32 W,
    UINT32 H,
    const UINT32 *Pixels
    );

VOID
FB_DRAW_CIRCLE(
    INT64 Cx,
    INT64 Cy,
    INT64 R,
    UINT32 Color
    );

VOID
FB_FILL_CIRCLE(
    INT64 Cx,
    INT64 Cy,
    INT64 R,
    UINT32 Color
    );

#endif

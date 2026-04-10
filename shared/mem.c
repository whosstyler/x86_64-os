#include "mem.h"

VOID
MEMCPY(
    PVOID Dst,
    PVOID Src,
    UINTN Size
    )
{
    UINT8 *D = (UINT8 *)Dst;
    UINT8 *S = (UINT8 *)Src;
    UINTN I;

    for (I = 0; I < Size; I++)
    {
        D[I] = S[I];
    }
}

VOID
MEMSET(
    PVOID Dst,
    UINT8 Value,
    UINTN Size
    )
{
    UINT8 *D = (UINT8 *)Dst;
    UINTN I;

    for (I = 0; I < Size; I++)
    {
        D[I] = Value;
    }
}

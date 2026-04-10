#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>

typedef uint8_t UINT8;
typedef uint64_t UINTN;
typedef void VOID;
typedef VOID *PVOID;

VOID
MEMCPY(
    PVOID Dst,
    PVOID Src,
    UINTN Size
    );

VOID
MEMSET(
    PVOID Dst,
    UINT8 Value,
    UINTN Size
    );

#endif

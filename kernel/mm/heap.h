#ifndef HEAP_H
#define HEAP_H

#include "../include/kernel.h"

typedef struct _BLOCK_HEADER
{
    UINT64                Size;
    UINT8                 IsFree;
    struct _BLOCK_HEADER *Next;
    struct _BLOCK_HEADER *Prev;
} BLOCK_HEADER, *PBLOCK_HEADER;

#define BLOCK_HEADER_SIZE  sizeof(BLOCK_HEADER)
#define HEAP_MIN_SPLIT     32

VOID
INIT_HEAP(VOID);

PVOID
KMALLOC(
    UINT64 Size
    );

VOID
KFREE(
    PVOID Ptr
    );

PVOID
KREALLOC(
    PVOID Ptr,
    UINT64 NewSize
    );

#endif

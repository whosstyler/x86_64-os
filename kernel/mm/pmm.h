#ifndef PMM_H
#define PMM_H

#include "../include/kernel.h"

typedef struct _PMM_STATS
{
    UINT64 TotalPages;
    UINT64 UsablePages;
    UINT64 ReservedPages;
    UINT64 FreePages;
    UINT64 UsedPages;
} PMM_STATS;

VOID
INIT_PMM(
    PBOOT_INFO BootInfo
    );

UINT64
ALLOC_PAGE(VOID);

VOID
FREE_PAGE(
    UINT64 PhysAddr
    );

VOID
GET_PMM_STATS(
    PMM_STATS *Stats
    );

#endif

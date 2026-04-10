#ifndef EPT_H
#define EPT_H

#include "../include/kernel.h"

#define EPT_READ        (1ULL << 0)
#define EPT_WRITE       (1ULL << 1)
#define EPT_EXECUTE     (1ULL << 2)
#define EPT_RWX         (EPT_READ | EPT_WRITE | EPT_EXECUTE)
#define EPT_MEMTYPE_WB  (6ULL << 3)
#define EPT_HUGE        (1ULL << 7)

#define EPT_ADDR_MASK   0x000FFFFFFFFFF000ULL

#define EPT_ENTRIES     512

typedef UINT64 EPT_ENTRY;

UINT64 INIT_EPT(VOID);

VOID
EPT_SET_PAGE_PERMS(
    UINT64 EptPml4Phys,
    UINT64 PhysAddr,
    UINT64 Perms
    );

extern UINT64 EptPml4Phys;

#endif

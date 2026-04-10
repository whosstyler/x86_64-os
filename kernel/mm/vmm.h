#ifndef VMM_H
#define VMM_H

#include "../include/kernel.h"

typedef UINT64 PAGE_TABLE_ENTRY;

#define PTE_PRESENT     (1ULL << 0)
#define PTE_WRITABLE    (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_WRITE_THROUGH (1ULL << 3)
#define PTE_NO_CACHE    (1ULL << 4)
#define PTE_ACCESSED    (1ULL << 5)
#define PTE_DIRTY       (1ULL << 6)
#define PTE_HUGE        (1ULL << 7)
#define PTE_GLOBAL      (1ULL << 8)
#define PTE_NO_EXECUTE  (1ULL << 63)

#define PTE_ADDR_MASK   0x000FFFFFFFFFF000ULL

#define ENTRIES_PER_TABLE 512

#define HIGHER_HALF_BASE 0xFFFF800000000000ULL

#define PML4_INDEX(v) (((v) >> 39) & 0x1FF)
#define PDPT_INDEX(v) (((v) >> 30) & 0x1FF)
#define PD_INDEX(v)   (((v) >> 21) & 0x1FF)
#define PT_INDEX(v)   (((v) >> 12) & 0x1FF)

VOID
INIT_VMM(
    PBOOT_INFO BootInfo
    );

VOID
MAP_PAGE(
    UINT64 VirtAddr,
    UINT64 PhysAddr,
    UINT64 Flags
    );

VOID
UNMAP_PAGE(
    UINT64 VirtAddr
    );

UINT64
VIRT_TO_PHYS(
    UINT64 VirtAddr
    );

PAGE_TABLE_ENTRY *
GET_PTE_PTR(
    UINT64 VirtAddr
    );

#endif

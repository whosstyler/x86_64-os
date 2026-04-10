#include "pmm.h"

/*
 * Bitmap physical memory manager.
 *
 * One bit per 4KB page frame. A set bit means "in use".
 * The bitmap itself must live somewhere in memory — we place it
 * right after the kernel's BSS, at a location we find from the
 * memory map. On init we mark everything as reserved (all 1s),
 * then walk the UEFI memory map and clear bits for usable regions.
 *
 * This approach is simple and correct. A free-list allocator would
 * be faster for large allocations, but bitmap is easier to debug
 * and perfectly fine for page-at-a-time allocation.
 */

extern char _kernel_start[];
extern char _kernel_end[];

static UINT8  *Bitmap;
static UINT64  BitmapSize;
static UINT64  TotalPages;
static UINT64  UsablePages;
static UINT64  FreePages;

static VOID
SET_BIT(
    UINT64 PageIndex
    )
{
    Bitmap[PageIndex / 8] |= (1 << (PageIndex % 8));
}

static VOID
CLEAR_BIT(
    UINT64 PageIndex
    )
{
    Bitmap[PageIndex / 8] &= ~(1 << (PageIndex % 8));
}

static UINT8
TEST_BIT(
    UINT64 PageIndex
    )
{
    return (Bitmap[PageIndex / 8] >> (PageIndex % 8)) & 1;
}

static UINT8
IS_USABLE_TYPE(
    UINT32 Type
    )
{
    return (Type == EFI_CONVENTIONAL_MEMORY
         || Type == EFI_LOADER_CODE
         || Type == EFI_LOADER_DATA
         || Type == EFI_BOOT_SERVICES_CODE
         || Type == EFI_BOOT_SERVICES_DATA);
}

VOID
INIT_PMM(
    PBOOT_INFO BootInfo
    )
{
    EFI_MEMORY_DESCRIPTOR *Map = (EFI_MEMORY_DESCRIPTOR *)BootInfo->MemoryMap;
    UINT64 MapSize  = BootInfo->MemoryMapSize;
    UINT64 DescSize = BootInfo->DescriptorSize;
    UINT64 Entries  = MapSize / DescSize;

    /* pass 1: find the highest physical address to size the bitmap */
    UINT64 HighestAddr = 0;
    UINT64 I;

    for (I = 0; I < Entries; I++)
    {
        EFI_MEMORY_DESCRIPTOR *Desc =
            (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + I * DescSize);

        UINT64 RegionEnd = Desc->PhysicalStart + Desc->NumberOfPages * PAGE_SIZE;
        if (RegionEnd > HighestAddr)
        {
            HighestAddr = RegionEnd;
        }
    }

    TotalPages = HighestAddr / PAGE_SIZE;
    BitmapSize = (TotalPages + 7) / 8;

    /* pass 2: find a usable region large enough to hold the bitmap.
     * We look for the first conventional memory region that can fit it
     * and that starts above 1MB (avoid low memory quirks).
     */
    Bitmap = (UINT8 *)0;

    for (I = 0; I < Entries; I++)
    {
        EFI_MEMORY_DESCRIPTOR *Desc =
            (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + I * DescSize);

        if (!IS_USABLE_TYPE(Desc->Type))
        {
            continue;
        }

        UINT64 RegionSize = Desc->NumberOfPages * PAGE_SIZE;
        UINT64 RegionBase = Desc->PhysicalStart;

        if (RegionBase < 0x200000)
        {
            continue;
        }

        if (RegionSize >= BitmapSize + PAGE_SIZE)
        {
            /* align bitmap to page boundary */
            Bitmap = (UINT8 *)((RegionBase + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
            break;
        }
    }

    if (!Bitmap)
    {
        /* no suitable region — halt */
        for (;;) { __asm__ volatile("hlt"); }
    }

    /* mark everything as reserved (all bits set) */
    for (I = 0; I < BitmapSize; I++)
    {
        Bitmap[I] = 0xFF;
    }

    /* pass 3: clear bits for usable memory regions */
    UsablePages = 0;

    for (I = 0; I < Entries; I++)
    {
        EFI_MEMORY_DESCRIPTOR *Desc =
            (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + I * DescSize);

        if (!IS_USABLE_TYPE(Desc->Type))
        {
            continue;
        }

        UINT64 Base = Desc->PhysicalStart / PAGE_SIZE;
        UINT64 Count = Desc->NumberOfPages;
        UINT64 J;

        for (J = 0; J < Count; J++)
        {
            CLEAR_BIT(Base + J);
        }

        UsablePages += Count;
    }

    /* reserve the pages occupied by the bitmap itself */
    UINT64 BitmapStartPage = (UINT64)Bitmap / PAGE_SIZE;
    UINT64 BitmapPageCount = (BitmapSize + PAGE_SIZE - 1) / PAGE_SIZE;
    UINT64 J;

    for (J = 0; J < BitmapPageCount; J++)
    {
        SET_BIT(BitmapStartPage + J);
    }

    /* reserve page 0 (null page — never allocate address 0) */
    SET_BIT(0);

    /* reserve low 1MB (legacy BIOS area, UEFI runtime, etc.) */
    UINT64 LowPages = 0x100000 / PAGE_SIZE;
    for (J = 0; J < LowPages; J++)
    {
        SET_BIT(J);
    }

    /* reserve the kernel's own pages (.text through .bss + stack) */
    UINT64 KernelStartPage = (UINT64)_kernel_start / PAGE_SIZE;
    UINT64 KernelEndPage   = ((UINT64)_kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (J = KernelStartPage; J < KernelEndPage; J++)
    {
        SET_BIT(J);
    }

    /* count actual free pages */
    FreePages = 0;
    for (I = 0; I < TotalPages; I++)
    {
        if (!TEST_BIT(I))
        {
            FreePages++;
        }
    }
}

UINT64
ALLOC_PAGE(VOID)
{
    UINT64 I;

    for (I = 0; I < TotalPages; I++)
    {
        if (!TEST_BIT(I))
        {
            SET_BIT(I);
            FreePages--;
            return I * PAGE_SIZE;
        }
    }

    return 0;
}

VOID
FREE_PAGE(
    UINT64 PhysAddr
    )
{
    UINT64 PageIndex = PhysAddr / PAGE_SIZE;

    if (PageIndex >= TotalPages)
    {
        return;
    }

    if (TEST_BIT(PageIndex))
    {
        CLEAR_BIT(PageIndex);
        FreePages++;
    }
}

VOID
GET_PMM_STATS(
    PMM_STATS *Stats
    )
{
    Stats->TotalPages    = TotalPages;
    Stats->UsablePages   = UsablePages;
    Stats->ReservedPages = TotalPages - UsablePages;
    Stats->FreePages     = FreePages;
    Stats->UsedPages     = UsablePages - FreePages;
}

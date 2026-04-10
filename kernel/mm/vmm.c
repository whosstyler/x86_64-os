#include "vmm.h"
#include "pmm.h"

static PAGE_TABLE_ENTRY *Pml4;

static VOID
ZERO_PAGE(
    UINT64 PhysAddr
    )
{
    UINT8 *P = (UINT8 *)PhysAddr;
    UINT64 I;
    for (I = 0; I < PAGE_SIZE; I++)
    {
        P[I] = 0;
    }
}

static PAGE_TABLE_ENTRY *
GET_OR_CREATE_TABLE(
    PAGE_TABLE_ENTRY *Table,
    UINT64 Index,
    UINT64 Flags
    )
{
    if (Table[Index] & PTE_PRESENT)
    {
        return (PAGE_TABLE_ENTRY *)(Table[Index] & PTE_ADDR_MASK);
    }

    UINT64 NewPage = ALLOC_PAGE();
    if (!NewPage)
    {
        return (PAGE_TABLE_ENTRY *)0;
    }

    ZERO_PAGE(NewPage);
    Table[Index] = NewPage | Flags | PTE_PRESENT;
    return (PAGE_TABLE_ENTRY *)NewPage;
}

VOID
INIT_VMM(
    PBOOT_INFO BootInfo
    )
{
    (VOID)BootInfo;

    UINT64 Pml4Phys = ALLOC_PAGE();
    ZERO_PAGE(Pml4Phys);
    Pml4 = (PAGE_TABLE_ENTRY *)Pml4Phys;

    UINT64 PdptPhys = ALLOC_PAGE();
    ZERO_PAGE(PdptPhys);
    PAGE_TABLE_ENTRY *Pdpt = (PAGE_TABLE_ENTRY *)PdptPhys;

    Pml4[0] = PdptPhys | PTE_PRESENT | PTE_WRITABLE;
    Pml4[PML4_INDEX(HIGHER_HALF_BASE)] = PdptPhys | PTE_PRESENT | PTE_WRITABLE;

    UINT64 PhysAddr = 0;
    UINT64 I;

    for (I = 0; I < 4; I++)
    {
        UINT64 PdPhys = ALLOC_PAGE();
        ZERO_PAGE(PdPhys);
        PAGE_TABLE_ENTRY *Pd = (PAGE_TABLE_ENTRY *)PdPhys;

        Pdpt[I] = PdPhys | PTE_PRESENT | PTE_WRITABLE;

        UINT64 J;
        for (J = 0; J < ENTRIES_PER_TABLE; J++)
        {
            Pd[J] = PhysAddr | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE;
            PhysAddr += 0x200000;
        }
    }

    WRITE_CR3(Pml4Phys);
}

VOID
MAP_PAGE(
    UINT64 VirtAddr,
    UINT64 PhysAddr,
    UINT64 Flags
    )
{
    UINT64 Pml4Idx = PML4_INDEX(VirtAddr);
    UINT64 PdptIdx = PDPT_INDEX(VirtAddr);
    UINT64 PdIdx   = PD_INDEX(VirtAddr);
    UINT64 PtIdx   = PT_INDEX(VirtAddr);

    PAGE_TABLE_ENTRY *Pdpt = GET_OR_CREATE_TABLE(Pml4, Pml4Idx, PTE_WRITABLE);
    if (!Pdpt) return;

    PAGE_TABLE_ENTRY *Pd = GET_OR_CREATE_TABLE(Pdpt, PdptIdx, PTE_WRITABLE);
    if (!Pd) return;

    if (Pd[PdIdx] & PTE_HUGE)
    {
        return;
    }

    PAGE_TABLE_ENTRY *Pt = GET_OR_CREATE_TABLE(Pd, PdIdx, PTE_WRITABLE);
    if (!Pt) return;

    Pt[PtIdx] = (PhysAddr & PTE_ADDR_MASK) | Flags | PTE_PRESENT;
    INVLPG(VirtAddr);
}

VOID
UNMAP_PAGE(
    UINT64 VirtAddr
    )
{
    UINT64 Pml4Idx = PML4_INDEX(VirtAddr);
    UINT64 PdptIdx = PDPT_INDEX(VirtAddr);
    UINT64 PdIdx   = PD_INDEX(VirtAddr);
    UINT64 PtIdx   = PT_INDEX(VirtAddr);

    if (!(Pml4[Pml4Idx] & PTE_PRESENT)) return;
    PAGE_TABLE_ENTRY *Pdpt = (PAGE_TABLE_ENTRY *)(Pml4[Pml4Idx] & PTE_ADDR_MASK);

    if (!(Pdpt[PdptIdx] & PTE_PRESENT)) return;
    PAGE_TABLE_ENTRY *Pd = (PAGE_TABLE_ENTRY *)(Pdpt[PdptIdx] & PTE_ADDR_MASK);

    if (!(Pd[PdIdx] & PTE_PRESENT)) return;
    if (Pd[PdIdx] & PTE_HUGE) return;

    PAGE_TABLE_ENTRY *Pt = (PAGE_TABLE_ENTRY *)(Pd[PdIdx] & PTE_ADDR_MASK);

    Pt[PtIdx] = 0;
    INVLPG(VirtAddr);
}

UINT64
VIRT_TO_PHYS(
    UINT64 VirtAddr
    )
{
    UINT64 Pml4Idx = PML4_INDEX(VirtAddr);
    UINT64 PdptIdx = PDPT_INDEX(VirtAddr);
    UINT64 PdIdx   = PD_INDEX(VirtAddr);
    UINT64 PtIdx   = PT_INDEX(VirtAddr);

    if (!(Pml4[Pml4Idx] & PTE_PRESENT)) return 0;
    PAGE_TABLE_ENTRY *Pdpt = (PAGE_TABLE_ENTRY *)(Pml4[Pml4Idx] & PTE_ADDR_MASK);

    if (!(Pdpt[PdptIdx] & PTE_PRESENT)) return 0;
    PAGE_TABLE_ENTRY *Pd = (PAGE_TABLE_ENTRY *)(Pdpt[PdptIdx] & PTE_ADDR_MASK);

    if (!(Pd[PdIdx] & PTE_PRESENT)) return 0;

    if (Pd[PdIdx] & PTE_HUGE)
    {
        return (Pd[PdIdx] & 0x000FFFFFFFE00000ULL) | (VirtAddr & 0x1FFFFF);
    }

    PAGE_TABLE_ENTRY *Pt = (PAGE_TABLE_ENTRY *)(Pd[PdIdx] & PTE_ADDR_MASK);

    if (!(Pt[PtIdx] & PTE_PRESENT)) return 0;
    return (Pt[PtIdx] & PTE_ADDR_MASK) | (VirtAddr & 0xFFF);
}

PAGE_TABLE_ENTRY *
GET_PTE_PTR(
    UINT64 VirtAddr
    )
{
    UINT64 Pml4Idx = PML4_INDEX(VirtAddr);
    UINT64 PdptIdx = PDPT_INDEX(VirtAddr);
    UINT64 PdIdx   = PD_INDEX(VirtAddr);
    UINT64 PtIdx   = PT_INDEX(VirtAddr);

    if (!(Pml4[Pml4Idx] & PTE_PRESENT)) return (PAGE_TABLE_ENTRY *)0;
    PAGE_TABLE_ENTRY *Pdpt = (PAGE_TABLE_ENTRY *)(Pml4[Pml4Idx] & PTE_ADDR_MASK);

    if (!(Pdpt[PdptIdx] & PTE_PRESENT)) return (PAGE_TABLE_ENTRY *)0;
    PAGE_TABLE_ENTRY *Pd = (PAGE_TABLE_ENTRY *)(Pdpt[PdptIdx] & PTE_ADDR_MASK);

    if (!(Pd[PdIdx] & PTE_PRESENT)) return (PAGE_TABLE_ENTRY *)0;
    if (Pd[PdIdx] & PTE_HUGE) return &Pd[PdIdx];

    PAGE_TABLE_ENTRY *Pt = (PAGE_TABLE_ENTRY *)(Pd[PdIdx] & PTE_ADDR_MASK);
    return &Pt[PtIdx];
}

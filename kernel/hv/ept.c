#include "ept.h"
#include "../mm/pmm.h"

UINT64 EptPml4Phys;

static VOID
EPT_ZERO_PAGE(
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

UINT64
INIT_EPT(VOID)
{
    UINT64 Pml4Phys = ALLOC_PAGE();
    EPT_ZERO_PAGE(Pml4Phys);

    UINT64 PdptPhys = ALLOC_PAGE();
    EPT_ZERO_PAGE(PdptPhys);

    EPT_ENTRY *Pml4 = (EPT_ENTRY *)Pml4Phys;
    EPT_ENTRY *Pdpt = (EPT_ENTRY *)PdptPhys;

    Pml4[0] = PdptPhys | EPT_RWX;

    UINT64 I;
    UINT64 PhysAddr = 0;

    for (I = 0; I < 4; I++)
    {
        UINT64 PdPhys = ALLOC_PAGE();
        EPT_ZERO_PAGE(PdPhys);
        EPT_ENTRY *Pd = (EPT_ENTRY *)PdPhys;

        Pdpt[I] = PdPhys | EPT_RWX;

        UINT64 J;
        for (J = 0; J < EPT_ENTRIES; J++)
        {
            Pd[J] = PhysAddr | EPT_RWX | EPT_MEMTYPE_WB | EPT_HUGE;
            PhysAddr += 0x200000;
        }
    }

    EptPml4Phys = Pml4Phys;

    /* EPTP: WB memory type (6), page walk length 3 (4-level - 1), PML4 address */
    UINT64 Eptp = Pml4Phys | (3ULL << 3) | 6ULL;
    return Eptp;
}

VOID
EPT_SET_PAGE_PERMS(
    UINT64 Pml4P,
    UINT64 PhysAddr,
    UINT64 Perms
    )
{
    EPT_ENTRY *Pml4 = (EPT_ENTRY *)Pml4P;

    UINT64 Pml4Idx = (PhysAddr >> 39) & 0x1FF;
    UINT64 PdptIdx = (PhysAddr >> 30) & 0x1FF;
    UINT64 PdIdx   = (PhysAddr >> 21) & 0x1FF;

    if (!(Pml4[Pml4Idx] & EPT_READ)) return;
    EPT_ENTRY *Pdpt = (EPT_ENTRY *)(Pml4[Pml4Idx] & EPT_ADDR_MASK);

    if (!(Pdpt[PdptIdx] & EPT_READ)) return;
    EPT_ENTRY *Pd = (EPT_ENTRY *)(Pdpt[PdptIdx] & EPT_ADDR_MASK);

    if (Pd[PdIdx] & EPT_HUGE)
    {
        UINT64 BaseAddr = Pd[PdIdx] & 0x000FFFFFFFE00000ULL;
        Pd[PdIdx] = BaseAddr | Perms | EPT_MEMTYPE_WB | EPT_HUGE;
    }
}

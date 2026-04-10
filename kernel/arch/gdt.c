#include "../include/kernel.h"

static GDT_ENTRY Gdt[3];
static GDT_PTR   GdtPtr;

extern VOID GDT_FLUSH(GDT_PTR *Ptr);

static VOID
SET_GDT_ENTRY(
    GDT_ENTRY *Entry,
    UINT32 Base,
    UINT32 Limit,
    UINT8 Access,
    UINT8 Flags
    )
{
    Entry->BaseLow       = (UINT16)(Base & 0xFFFF);
    Entry->BaseMid       = (UINT8)((Base >> 16) & 0xFF);
    Entry->BaseHigh      = (UINT8)((Base >> 24) & 0xFF);
    Entry->LimitLow      = (UINT16)(Limit & 0xFFFF);
    Entry->FlagsLimitHigh = (UINT8)(((Limit >> 16) & 0x0F) | ((Flags & 0x0F) << 4));
    Entry->Access        = Access;
}

VOID
INIT_GDT(VOID)
{
    SET_GDT_ENTRY(&Gdt[0], 0, 0, 0, 0);
    SET_GDT_ENTRY(&Gdt[1], 0, 0xFFFFF, 0x9A, 0x0A);
    SET_GDT_ENTRY(&Gdt[2], 0, 0xFFFFF, 0x92, 0x0C);

    GdtPtr.Limit = sizeof(Gdt) - 1;
    GdtPtr.Base  = (UINT64)&Gdt[0];

    GDT_FLUSH(&GdtPtr);
}

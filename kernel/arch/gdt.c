#include "../include/kernel.h"

static GDT_ENTRY Gdt[5];
static GDT_PTR   GdtPtr;
static TSS64     Tss;

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
    UINT64 I;

    SET_GDT_ENTRY(&Gdt[0], 0, 0, 0, 0);
    SET_GDT_ENTRY(&Gdt[1], 0, 0xFFFFF, 0x9A, 0x0A);
    SET_GDT_ENTRY(&Gdt[2], 0, 0xFFFFF, 0x92, 0x0C);

    /* zero the TSS */
    UINT8 *TssBytes = (UINT8 *)&Tss;
    for (I = 0; I < sizeof(TSS64); I++)
    {
        TssBytes[I] = 0;
    }
    Tss.IopbOffset = sizeof(TSS64);

    /* TSS descriptor (16 bytes = 2 GDT entries) */
    UINT64 TssBase = (UINT64)&Tss;
    UINT32 TssLimit = sizeof(TSS64) - 1;
    UINT8 *Desc = (UINT8 *)&Gdt[3];

    Desc[0]  = (UINT8)(TssLimit & 0xFF);
    Desc[1]  = (UINT8)((TssLimit >> 8) & 0xFF);
    Desc[2]  = (UINT8)(TssBase & 0xFF);
    Desc[3]  = (UINT8)((TssBase >> 8) & 0xFF);
    Desc[4]  = (UINT8)((TssBase >> 16) & 0xFF);
    Desc[5]  = 0x89;
    Desc[6]  = (UINT8)((TssLimit >> 16) & 0x0F);
    Desc[7]  = (UINT8)((TssBase >> 24) & 0xFF);
    Desc[8]  = (UINT8)((TssBase >> 32) & 0xFF);
    Desc[9]  = (UINT8)((TssBase >> 40) & 0xFF);
    Desc[10] = (UINT8)((TssBase >> 48) & 0xFF);
    Desc[11] = (UINT8)((TssBase >> 56) & 0xFF);
    Desc[12] = 0;
    Desc[13] = 0;
    Desc[14] = 0;
    Desc[15] = 0;

    GdtPtr.Limit = sizeof(Gdt) - 1;
    GdtPtr.Base  = (UINT64)&Gdt[0];

    GDT_FLUSH(&GdtPtr);

    __asm__ volatile("ltr %w0" : : "r"((UINT16)0x18));
}

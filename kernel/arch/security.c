#include "../include/kernel.h"

#define CR4_SMEP (1ULL << 20)
#define CR4_SMAP (1ULL << 21)

VOID
INIT_CPU_SECURITY(VOID)
{
    UINT32 Eax, Ebx, Ecx, Edx;
    UINT64 Cr4 = READ_CR4();

    DO_CPUID(7, 0, &Eax, &Ebx, &Ecx, &Edx);

    if (Ebx & (1 << 7))
    {
        Cr4 |= CR4_SMEP;
        FB_PRINT("smep enabled\n");
    }
    else
    {
        FB_PRINT("smep not supported\n");
    }

    if (Ebx & (1 << 20))
    {
        Cr4 |= CR4_SMAP;
        FB_PRINT("smap enabled\n");
    }
    else
    {
        FB_PRINT("smap not supported\n");
    }

    WRITE_CR4(Cr4);
}

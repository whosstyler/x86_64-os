#include "vmx.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"

static UINT64 MsrBitmapPhys;

static UINT64 IdtHash;
static UINT64 GdtHash;
static UINT64 LstarHash;

static UINT64 SavedLstar = 0;
static UINT8  LstarRedirected = 0;

static UINT64 ExpHostRip;
static UINT64 ExpHostRsp;
static UINT64 ExpHostCr3;
static UINT64 ExpMsrBitmap;
static UINT64 ExpCr0Mask;
static UINT64 ExpCr4Mask;
static UINT32 ExpPinCtl;
static UINT32 ExpExitCtl;

VOID
SNAPSHOT_VMCS_INTEGRITY(VOID)
{
    ExpHostRip   = VMREAD(VMCS_HOST_RIP);
    ExpHostRsp   = VMREAD(VMCS_HOST_RSP);
    ExpHostCr3   = VMREAD(VMCS_HOST_CR3);
    ExpMsrBitmap = VMREAD(VMCS_MSR_BITMAP);
    ExpCr0Mask   = VMREAD(VMCS_CR0_GUEST_HOST_MASK);
    ExpCr4Mask   = VMREAD(VMCS_CR4_GUEST_HOST_MASK);
    ExpPinCtl    = (UINT32)VMREAD(VMCS_PIN_BASED_VM_EXEC_CONTROL);
    ExpExitCtl   = (UINT32)VMREAD(VMCS_VM_EXIT_CONTROLS);
}

UINT8
VERIFY_VMCS_INTEGRITY(VOID)
{
    if (VMREAD(VMCS_HOST_RIP) != ExpHostRip)
    {
        FB_PRINT("[HV] *** VMCS HIJACK: HOST_RIP ***\n");
        return 1;
    }
    if (VMREAD(VMCS_HOST_RSP) != ExpHostRsp)
    {
        FB_PRINT("[HV] *** VMCS HIJACK: HOST_RSP ***\n");
        return 1;
    }
    if (VMREAD(VMCS_HOST_CR3) != ExpHostCr3)
    {
        FB_PRINT("[HV] *** VMCS HIJACK: HOST_CR3 ***\n");
        return 1;
    }
    if (VMREAD(VMCS_MSR_BITMAP) != ExpMsrBitmap)
    {
        FB_PRINT("[HV] *** VMCS HIJACK: MSR_BITMAP ***\n");
        return 1;
    }
    if (VMREAD(VMCS_CR0_GUEST_HOST_MASK) != ExpCr0Mask)
    {
        FB_PRINT("[HV] *** VMCS HIJACK: CR0_MASK ***\n");
        return 1;
    }
    if (VMREAD(VMCS_CR4_GUEST_HOST_MASK) != ExpCr4Mask)
    {
        FB_PRINT("[HV] *** VMCS HIJACK: CR4_MASK ***\n");
        return 1;
    }
    if ((UINT32)VMREAD(VMCS_PIN_BASED_VM_EXEC_CONTROL) != ExpPinCtl)
    {
        FB_PRINT("[HV] *** VMCS HIJACK: PIN_CONTROLS ***\n");
        return 1;
    }
    if ((UINT32)VMREAD(VMCS_VM_EXIT_CONTROLS) != ExpExitCtl)
    {
        FB_PRINT("[HV] *** VMCS HIJACK: EXIT_CONTROLS ***\n");
        return 1;
    }
    return 0;
}

VOID
PROTECT_HV_PAGES(
    UINT64 VmxonPhys,
    UINT64 VmcsPhys,
    UINT64 MsrBitmapPhys
    )
{
    UNMAP_PAGE(VmxonPhys);
    UNMAP_PAGE(VmcsPhys);
    UNMAP_PAGE(MsrBitmapPhys);
    FB_PRINT("[HV] vmxon/vmcs/msr-bitmap pages unmapped from guest\n");
}

#define CR0_WP  (1ULL << 16)
#define CR4_SMEP_BIT (1ULL << 20)
#define CR4_SMAP_BIT (1ULL << 21)

static UINT64
FNV1A(
    UINT8 *Data,
    UINT64 Len
    )
{
    UINT64 Hash = 14695981039346656037ULL;
    UINT64 I;
    for (I = 0; I < Len; I++)
    {
        Hash ^= Data[I];
        Hash *= 1099511628211ULL;
    }
    return Hash;
}

static VOID
MSR_BITMAP_SET_WRITE(
    UINT8 *Bitmap,
    UINT32 Msr
    )
{
    if (Msr <= 0x1FFF)
    {
        UINT32 Byte = 0x800 + (Msr / 8);
        UINT8  Bit  = Msr % 8;
        Bitmap[Byte] |= (1 << Bit);
    }
    else if (Msr >= 0xC0000000 && Msr <= 0xC0001FFF)
    {
        UINT32 Offset = Msr - 0xC0000000;
        UINT32 Byte = 0xC00 + (Offset / 8);
        UINT8  Bit  = Offset % 8;
        Bitmap[Byte] |= (1 << Bit);
    }
}

UINT64
INIT_HV_SECURITY(VOID)
{
    MsrBitmapPhys = ALLOC_PAGE();
    UINT8 *Bitmap = (UINT8 *)MsrBitmapPhys;
    UINT64 I;
    for (I = 0; I < PAGE_SIZE; I++)
    {
        Bitmap[I] = 0;
    }

    MSR_BITMAP_SET_WRITE(Bitmap, 0x176);       /* IA32_SYSENTER_EIP */
    MSR_BITMAP_SET_WRITE(Bitmap, 0x1D9);       /* IA32_DEBUGCTL */
    MSR_BITMAP_SET_WRITE(Bitmap, 0xC0000082);  /* LSTAR */

    return MsrBitmapPhys;
}

VOID
SETUP_CR_MASKS(VOID)
{
    VMWRITE(VMCS_CR0_GUEST_HOST_MASK, CR0_WP);
    VMWRITE(VMCS_CR0_READ_SHADOW, READ_CR0());

    VMWRITE(VMCS_CR4_GUEST_HOST_MASK, CR4_SMEP_BIT | CR4_SMAP_BIT);
    VMWRITE(VMCS_CR4_READ_SHADOW, READ_CR4());
}

VOID
SNAPSHOT_INTEGRITY(VOID)
{
    GDT_PTR GdtReg;
    IDT_PTR IdtReg;

    __asm__ volatile("sgdt %0" : "=m"(GdtReg));
    __asm__ volatile("sidt %0" : "=m"(IdtReg));

    IdtHash = FNV1A((UINT8 *)IdtReg.Base, IdtReg.Limit + 1);
    GdtHash = FNV1A((UINT8 *)GdtReg.Base, GdtReg.Limit + 1);

    UINT64 Lstar = RDMSR(0xC0000082);
    LstarHash = FNV1A((UINT8 *)&Lstar, sizeof(Lstar));
}

UINT8
CHECK_INTEGRITY(VOID)
{
    GDT_PTR GdtReg;
    IDT_PTR IdtReg;
    UINT8 Tampered = 0;

    __asm__ volatile("sgdt %0" : "=m"(GdtReg));
    __asm__ volatile("sidt %0" : "=m"(IdtReg));

    UINT64 CurIdtHash = FNV1A((UINT8 *)IdtReg.Base, IdtReg.Limit + 1);
    if (CurIdtHash != IdtHash)
    {
        FB_PRINT("[HV] IDT TAMPERED!\n");
        Tampered = 1;
    }

    UINT64 CurGdtHash = FNV1A((UINT8 *)GdtReg.Base, GdtReg.Limit + 1);
    if (CurGdtHash != GdtHash)
    {
        FB_PRINT("[HV] GDT TAMPERED!\n");
        Tampered = 1;
    }

    UINT64 Lstar = RDMSR(0xC0000082);
    UINT64 CurLstarHash = FNV1A((UINT8 *)&Lstar, sizeof(Lstar));
    if (CurLstarHash != LstarHash)
    {
        FB_PRINT("[HV] LSTAR TAMPERED!\n");
        Tampered = 1;
    }

    return Tampered;
}

UINT64
GET_LSTAR(VOID)
{
    return RDMSR(0xC0000082);
}

UINT64
REDIRECT_LSTAR(
    UINT64 NewAddr
    )
{
    if (!LstarRedirected)
    {
        SavedLstar = RDMSR(0xC0000082);
    }

    WRMSR(0xC0000082, NewAddr);
    LstarRedirected = 1;

    LstarHash = FNV1A((UINT8 *)&NewAddr, sizeof(NewAddr));

    FB_PRINT("[HV] LSTAR redirected ");
    FB_PRINT_HEX(SavedLstar);
    FB_PRINT(" -> ");
    FB_PRINT_HEX(NewAddr);
    FB_PRINT("\n");
    return 0;
}

VOID
RESTORE_LSTAR(VOID)
{
    if (LstarRedirected)
    {
        WRMSR(0xC0000082, SavedLstar);
        LstarHash = FNV1A((UINT8 *)&SavedLstar, sizeof(SavedLstar));
        FB_PRINT("[HV] LSTAR restored to ");
        FB_PRINT_HEX(SavedLstar);
        FB_PRINT("\n");
        LstarRedirected = 0;
        SavedLstar = 0;
    }
}

UINT8
HANDLE_CR_ACCESS(
    PGUEST_REGS Regs,
    UINT64 Qualification
    )
{
    UINT64 CrNum    = Qualification & 0xF;
    UINT64 AccType  = (Qualification >> 4) & 0x3;
    UINT64 RegIdx   = (Qualification >> 8) & 0xF;

    UINT64 *RegTable[] = {
        &Regs->Rax, &Regs->Rcx, &Regs->Rdx, &Regs->Rbx,
        (UINT64 *)0, &Regs->Rbp, &Regs->Rsi, &Regs->Rdi,
        &Regs->R8,  &Regs->R9,  &Regs->R10, &Regs->R11,
        &Regs->R12, &Regs->R13, &Regs->R14, &Regs->R15
    };

    if (AccType == 0 && CrNum == 0)
    {
        UINT64 NewCr0 = *RegTable[RegIdx];
        if (!(NewCr0 & CR0_WP))
        {
            FB_PRINT("[HV] BLOCKED CR0.WP clear!\n");
            NewCr0 |= CR0_WP;
        }
        VMWRITE(VMCS_GUEST_CR0, NewCr0);
        VMWRITE(VMCS_CR0_READ_SHADOW, NewCr0);
    }
    else if (AccType == 0 && CrNum == 4)
    {
        UINT64 NewCr4 = *RegTable[RegIdx];
        UINT64 OldCr4 = VMREAD(VMCS_GUEST_CR4);
        if ((OldCr4 & CR4_SMEP_BIT) && !(NewCr4 & CR4_SMEP_BIT))
        {
            FB_PRINT("[HV] BLOCKED CR4.SMEP clear!\n");
            NewCr4 |= CR4_SMEP_BIT;
        }
        if ((OldCr4 & CR4_SMAP_BIT) && !(NewCr4 & CR4_SMAP_BIT))
        {
            FB_PRINT("[HV] BLOCKED CR4.SMAP clear!\n");
            NewCr4 |= CR4_SMAP_BIT;
        }
        VMWRITE(VMCS_GUEST_CR4, NewCr4);
        VMWRITE(VMCS_CR4_READ_SHADOW, NewCr4);
    }

    UINT64 Rip = VMREAD(VMCS_GUEST_RIP);
    UINT64 Len = VMREAD(VMCS_VM_EXIT_INSTRUCTION_LEN);
    VMWRITE(VMCS_GUEST_RIP, Rip + Len);
    return 0;
}

UINT8
HANDLE_MSR_WRITE(
    PGUEST_REGS Regs
    )
{
    UINT32 Msr = (UINT32)Regs->Rcx;
    UINT64 Value = ((UINT64)(UINT32)Regs->Rdx << 32) | (UINT32)Regs->Rax;

    if (Msr == 0xC0000082)
    {
        UINT64 CurLstar = RDMSR(0xC0000082);
        FB_PRINT("[HV] intercepted LSTAR write: ");
        FB_PRINT_HEX(CurLstar);
        FB_PRINT(" -> ");
        FB_PRINT_HEX(Value);
        FB_PRINT(" DENIED (use hypercall)\n");
    }
    else if (Msr == 0x1D9)
    {
        FB_PRINT("[HV] BLOCKED IA32_DEBUGCTL write\n");
    }
    else if (Msr == 0x176)
    {
        FB_PRINT("[HV] BLOCKED SYSENTER_EIP write\n");
    }
    else
    {
        WRMSR(Msr, Value);
    }

    UINT64 Rip = VMREAD(VMCS_GUEST_RIP);
    UINT64 Len = VMREAD(VMCS_VM_EXIT_INSTRUCTION_LEN);
    VMWRITE(VMCS_GUEST_RIP, Rip + Len);
    return 0;
}

#include "vmx.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"

extern VOID VMEXIT_STUB(VOID);
extern UINT64 INIT_HV_SECURITY(VOID);
extern VOID SETUP_CR_MASKS(VOID);
extern VOID SNAPSHOT_INTEGRITY(VOID);
extern VOID SNAPSHOT_VMCS_INTEGRITY(VOID);
extern VOID PROTECT_HV_PAGES(UINT64, UINT64, UINT64);

#define HOST_STACK_SIZE (PAGE_SIZE * 4)

UINT8 VmxPreemptionTimerActive = 0;
UINT8 VmxMtfSupported = 0;

static UINT64 VmxonRegionPhys;
static UINT64 VmcsRegionPhys;

static UINT32
ADJUST_CONTROLS(
    UINT32 Desired,
    UINT32 Msr
    )
{
    UINT64 MsrValue = RDMSR(Msr);
    UINT32 Low  = (UINT32)MsrValue;
    UINT32 High = (UINT32)(MsrValue >> 32);
    Desired |= Low;
    Desired &= High;
    return Desired;
}

static VOID
SETUP_VMCS(
    UINT64 HostStackTop,
    UINT64 MsrBitmapPhys
    )
{
    UINT64 Cr0 = READ_CR0();
    UINT64 Cr3 = READ_CR3();
    UINT64 Cr4 = READ_CR4();

    UINT64 BasicMsr = RDMSR(MSR_IA32_VMX_BASIC);
    UINT8 UseTrueCtls = (BasicMsr >> 55) & 1;

    UINT32 PinMsr  = UseTrueCtls ? MSR_IA32_VMX_TRUE_PINBASED  : MSR_IA32_VMX_PINBASED_CTLS;
    UINT32 ProcMsr = UseTrueCtls ? MSR_IA32_VMX_TRUE_PROCBASED : MSR_IA32_VMX_PROCBASED_CTLS;
    UINT32 ExitMsr = UseTrueCtls ? MSR_IA32_VMX_TRUE_EXIT      : MSR_IA32_VMX_EXIT_CTLS;
    UINT32 EntryMsr = UseTrueCtls ? MSR_IA32_VMX_TRUE_ENTRY    : MSR_IA32_VMX_ENTRY_CTLS;

    UINT32 PinBased = ADJUST_CONTROLS(
        PIN_BASED_PREEMPTION_TIMER,
        PinMsr);

    VmxPreemptionTimerActive = (PinBased & PIN_BASED_PREEMPTION_TIMER) ? 1 : 0;

    UINT32 ProcBased = ADJUST_CONTROLS(
        CPU_BASED_ACTIVATE_SECONDARY | CPU_BASED_USE_MSR_BITMAPS,
        ProcMsr);

    UINT64 ProcMsrVal = RDMSR(ProcMsr);
    UINT32 ProcAllowed1 = (UINT32)(ProcMsrVal >> 32);
    VmxMtfSupported = (ProcAllowed1 & CPU_BASED_MONITOR_TRAP_FLAG) ? 1 : 0;

    UINT32 ProcBased2 = ADJUST_CONTROLS(
        SECONDARY_ENABLE_RDTSCP,
        MSR_IA32_VMX_PROCBASED_CTLS2);

    UINT32 ExitCtls = ADJUST_CONTROLS(
        VM_EXIT_HOST_ADDR_SPACE_SIZE,
        ExitMsr);

    UINT32 EntryCtls = ADJUST_CONTROLS(
        VM_ENTRY_IA32E_MODE_GUEST,
        EntryMsr);

    /* controls */
    VMWRITE(VMCS_PIN_BASED_VM_EXEC_CONTROL, PinBased);
    VMWRITE(VMCS_CPU_BASED_VM_EXEC_CONTROL, ProcBased);
    VMWRITE(VMCS_SECONDARY_VM_EXEC_CONTROL, ProcBased2);
    VMWRITE(VMCS_VM_EXIT_CONTROLS, ExitCtls);
    VMWRITE(VMCS_VM_ENTRY_CONTROLS, EntryCtls);

    VMWRITE(VMCS_MSR_BITMAP, MsrBitmapPhys);
    VMWRITE(VMCS_VMCS_LINK_POINTER, 0xFFFFFFFFFFFFFFFFULL);
    VMWRITE(VMCS_EXCEPTION_BITMAP, 0);
    VMWRITE(VMCS_CR3_TARGET_COUNT, 0);
    VMWRITE(VMCS_VM_EXIT_MSR_STORE_COUNT, 0);
    VMWRITE(VMCS_VM_EXIT_MSR_LOAD_COUNT, 0);
    VMWRITE(VMCS_VM_ENTRY_MSR_LOAD_COUNT, 0);
    if (VmxPreemptionTimerActive)
    {
        VMWRITE(VMCS_VMX_PREEMPTION_TIMER, 0x10000000);
    }

    /* host state */
    VMWRITE(VMCS_HOST_CR0, Cr0);
    VMWRITE(VMCS_HOST_CR3, Cr3);
    VMWRITE(VMCS_HOST_CR4, Cr4);

    VMWRITE(VMCS_HOST_CS_SELECTOR, 0x08);
    VMWRITE(VMCS_HOST_SS_SELECTOR, 0x10);
    VMWRITE(VMCS_HOST_DS_SELECTOR, 0x10);
    VMWRITE(VMCS_HOST_ES_SELECTOR, 0x10);
    VMWRITE(VMCS_HOST_FS_SELECTOR, 0x10);
    VMWRITE(VMCS_HOST_GS_SELECTOR, 0x10);
    VMWRITE(VMCS_HOST_TR_SELECTOR, 0x18);

    GDT_PTR GdtReg;
    IDT_PTR IdtReg;
    __asm__ volatile("sgdt %0" : "=m"(GdtReg));
    __asm__ volatile("sidt %0" : "=m"(IdtReg));

    VMWRITE(VMCS_HOST_GDTR_BASE, GdtReg.Base);
    VMWRITE(VMCS_HOST_IDTR_BASE, IdtReg.Base);
    VMWRITE(VMCS_HOST_TR_BASE, 0);

    VMWRITE(VMCS_HOST_FS_BASE, 0);
    VMWRITE(VMCS_HOST_GS_BASE, 0);

    VMWRITE(VMCS_HOST_SYSENTER_CS, RDMSR(MSR_IA32_SYSENTER_CS));
    VMWRITE(VMCS_HOST_SYSENTER_ESP, RDMSR(MSR_IA32_SYSENTER_ESP));
    VMWRITE(VMCS_HOST_SYSENTER_EIP, RDMSR(MSR_IA32_SYSENTER_EIP));

    VMWRITE(VMCS_HOST_IA32_EFER, RDMSR(MSR_IA32_EFER));

    VMWRITE(VMCS_HOST_RSP, HostStackTop);
    VMWRITE(VMCS_HOST_RIP, (UINT64)VMEXIT_STUB);

    /* guest state */
    VMWRITE(VMCS_GUEST_CR0, Cr0);
    VMWRITE(VMCS_GUEST_CR3, Cr3);
    VMWRITE(VMCS_GUEST_CR4, Cr4);

    VMWRITE(VMCS_GUEST_DR7, 0x400);
    VMWRITE(VMCS_GUEST_RFLAGS, 0x02);

    VMWRITE(VMCS_GUEST_CS_SELECTOR, 0x08);
    VMWRITE(VMCS_GUEST_CS_BASE, 0);
    VMWRITE(VMCS_GUEST_CS_LIMIT, 0xFFFFFFFF);
    VMWRITE(VMCS_GUEST_CS_AR, 0xA09B);

    VMWRITE(VMCS_GUEST_SS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_SS_BASE, 0);
    VMWRITE(VMCS_GUEST_SS_LIMIT, 0xFFFFFFFF);
    VMWRITE(VMCS_GUEST_SS_AR, 0xC093);

    VMWRITE(VMCS_GUEST_DS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_DS_BASE, 0);
    VMWRITE(VMCS_GUEST_DS_LIMIT, 0xFFFFFFFF);
    VMWRITE(VMCS_GUEST_DS_AR, 0xC093);

    VMWRITE(VMCS_GUEST_ES_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_ES_BASE, 0);
    VMWRITE(VMCS_GUEST_ES_LIMIT, 0xFFFFFFFF);
    VMWRITE(VMCS_GUEST_ES_AR, 0xC093);

    VMWRITE(VMCS_GUEST_FS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_FS_BASE, 0);
    VMWRITE(VMCS_GUEST_FS_LIMIT, 0xFFFFFFFF);
    VMWRITE(VMCS_GUEST_FS_AR, 0xC093);

    VMWRITE(VMCS_GUEST_GS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_GS_BASE, 0);
    VMWRITE(VMCS_GUEST_GS_LIMIT, 0xFFFFFFFF);
    VMWRITE(VMCS_GUEST_GS_AR, 0xC093);

    VMWRITE(VMCS_GUEST_TR_SELECTOR, 0x18);
    VMWRITE(VMCS_GUEST_TR_BASE, 0);
    VMWRITE(VMCS_GUEST_TR_LIMIT, 0x67);
    VMWRITE(VMCS_GUEST_TR_AR, 0x008B);

    VMWRITE(VMCS_GUEST_LDTR_SELECTOR, 0);
    VMWRITE(VMCS_GUEST_LDTR_BASE, 0);
    VMWRITE(VMCS_GUEST_LDTR_LIMIT, 0xFFFF);
    VMWRITE(VMCS_GUEST_LDTR_AR, 0x10000);

    VMWRITE(VMCS_GUEST_GDTR_BASE, GdtReg.Base);
    VMWRITE(VMCS_GUEST_GDTR_LIMIT, GdtReg.Limit);
    VMWRITE(VMCS_GUEST_IDTR_BASE, IdtReg.Base);
    VMWRITE(VMCS_GUEST_IDTR_LIMIT, IdtReg.Limit);

    VMWRITE(VMCS_GUEST_IA32_DEBUGCTL, 0);
    VMWRITE(VMCS_GUEST_IA32_EFER, RDMSR(MSR_IA32_EFER));
    VMWRITE(VMCS_GUEST_SYSENTER_CS, RDMSR(MSR_IA32_SYSENTER_CS));
    VMWRITE(VMCS_GUEST_SYSENTER_ESP, RDMSR(MSR_IA32_SYSENTER_ESP));
    VMWRITE(VMCS_GUEST_SYSENTER_EIP, RDMSR(MSR_IA32_SYSENTER_EIP));

    VMWRITE(VMCS_GUEST_ACTIVITY_STATE, 0);
    VMWRITE(VMCS_GUEST_INTERRUPTIBILITY, 0);
    VMWRITE(VMCS_GUEST_PENDING_DBG, 0);

    /* CR0/CR4 security masks */
    SETUP_CR_MASKS();
}

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

UINT8
INIT_VMX(VOID)
{
    UINT32 Eax, Ebx, Ecx, Edx;

    /* check VMX support */
    DO_CPUID(1, 0, &Eax, &Ebx, &Ecx, &Edx);
    if (!(Ecx & (1 << 5)))
    {
        FB_PRINT("[HV] vmx not supported\n");
        return 1;
    }

    /* check/enable IA32_FEATURE_CONTROL */
    UINT64 FeatureCtl = RDMSR(MSR_IA32_FEATURE_CONTROL);
    if (!(FeatureCtl & FEATURE_CONTROL_LOCKED))
    {
        FeatureCtl |= FEATURE_CONTROL_LOCKED | FEATURE_CONTROL_VMXON;
        WRMSR(MSR_IA32_FEATURE_CONTROL, FeatureCtl);
    }
    else if (!(FeatureCtl & FEATURE_CONTROL_VMXON))
    {
        FB_PRINT("[HV] vmxon disabled by bios\n");
        return 1;
    }

    /* adjust CR0/CR4 for VMX fixed bits */
    UINT64 Cr0 = READ_CR0();
    Cr0 |= RDMSR(MSR_IA32_VMX_CR0_FIXED0);
    Cr0 &= RDMSR(MSR_IA32_VMX_CR0_FIXED1);
    WRITE_CR0(Cr0);

    UINT64 Cr4 = READ_CR4();
    Cr4 |= CR4_VMXE;
    Cr4 |= RDMSR(MSR_IA32_VMX_CR4_FIXED0);
    Cr4 &= RDMSR(MSR_IA32_VMX_CR4_FIXED1);
    WRITE_CR4(Cr4);

    /* get revision ID */
    UINT64 BasicMsr = RDMSR(MSR_IA32_VMX_BASIC);
    UINT32 RevisionId = (UINT32)(BasicMsr & 0x7FFFFFFF);

    /* allocate VMXON region */
    VmxonRegionPhys = ALLOC_PAGE();
    ZERO_PAGE(VmxonRegionPhys);
    *(UINT32 *)VmxonRegionPhys = RevisionId;

    if (ASM_VMXON(&VmxonRegionPhys))
    {
        FB_PRINT("[HV] vmxon failed\n");
        FREE_PAGE(VmxonRegionPhys);
        return 1;
    }

    FB_PRINT("[HV] vmxon ok\n");

    /* allocate VMCS */
    VmcsRegionPhys = ALLOC_PAGE();
    ZERO_PAGE(VmcsRegionPhys);
    *(UINT32 *)VmcsRegionPhys = RevisionId;

    if (ASM_VMCLEAR(&VmcsRegionPhys))
    {
        FB_PRINT("[HV] vmclear failed\n");
        ASM_VMXOFF();
        return 1;
    }

    if (ASM_VMPTRLD(&VmcsRegionPhys))
    {
        FB_PRINT("[HV] vmptrld failed\n");
        ASM_VMXOFF();
        return 1;
    }

    FB_PRINT("[HV] vmcs loaded\n");

    /* set up MSR bitmap + security enforcement */
    UINT64 MsrBitmapPhys = INIT_HV_SECURITY();
    FB_PRINT("[HV] msr protection ok\n");

    /* allocate host stack */
    UINT8 *HostStack = (UINT8 *)KMALLOC(HOST_STACK_SIZE);
    UINT64 HostStackTop = (UINT64)(HostStack + HOST_STACK_SIZE);

    /* snapshot integrity baselines */
    SNAPSHOT_INTEGRITY();

    /* configure VMCS */
    SETUP_VMCS(HostStackTop, MsrBitmapPhys);
    FB_PRINT("[HV] vmcs configured\n");

    /* snapshot VMCS fields for tamper detection */
    SNAPSHOT_VMCS_INTEGRITY();

    /* VMLAUNCH - after this we're a VMX guest */
    FB_PRINT("[HV] launching...\n");
    UINT8 Result = VMX_LAUNCH();

    if (Result)
    {
        UINT64 Err = VMREAD(VMCS_VM_INSTRUCTION_ERROR);
        FB_PRINT("[HV] vmlaunch FAILED err=");
        FB_PRINT_HEX(Err);
        FB_PRINT("\n");
        ASM_VMXOFF();
        return 1;
    }

    FB_PRINT("[HV] hypervisor ACTIVE\n");

    /* unmap HV-critical pages from guest virtual address space */
    PROTECT_HV_PAGES(VmxonRegionPhys, VmcsRegionPhys, MsrBitmapPhys);

    return 0;
}

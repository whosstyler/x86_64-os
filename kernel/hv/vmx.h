#ifndef VMX_H
#define VMX_H

#include "../include/kernel.h"

#define MSR_IA32_FEATURE_CONTROL    0x03A
#define MSR_IA32_VMX_BASIC          0x480
#define MSR_IA32_VMX_PINBASED_CTLS  0x481
#define MSR_IA32_VMX_PROCBASED_CTLS 0x482
#define MSR_IA32_VMX_EXIT_CTLS      0x483
#define MSR_IA32_VMX_ENTRY_CTLS     0x484
#define MSR_IA32_VMX_MISC           0x485
#define MSR_IA32_VMX_CR0_FIXED0     0x486
#define MSR_IA32_VMX_CR0_FIXED1     0x487
#define MSR_IA32_VMX_CR4_FIXED0     0x488
#define MSR_IA32_VMX_CR4_FIXED1     0x489
#define MSR_IA32_VMX_PROCBASED_CTLS2 0x48B
#define MSR_IA32_VMX_EPT_VPID_CAP  0x48C
#define MSR_IA32_VMX_TRUE_PINBASED  0x48D
#define MSR_IA32_VMX_TRUE_PROCBASED 0x48E
#define MSR_IA32_VMX_TRUE_EXIT      0x48F
#define MSR_IA32_VMX_TRUE_ENTRY     0x490

#define MSR_IA32_DEBUGCTL           0x1D9
#define MSR_IA32_SYSENTER_CS        0x174
#define MSR_IA32_SYSENTER_ESP       0x175
#define MSR_IA32_SYSENTER_EIP       0x176
#define MSR_IA32_EFER               0xC0000080
#define MSR_IA32_LSTAR              0xC0000082
#define MSR_IA32_FS_BASE            0xC0000100
#define MSR_IA32_GS_BASE            0xC0000101

#define FEATURE_CONTROL_LOCKED      (1ULL << 0)
#define FEATURE_CONTROL_VMXON       (1ULL << 2)

#define CR4_VMXE                    (1ULL << 13)

/* pin-based controls */
#define PIN_BASED_PREEMPTION_TIMER  (1UL << 6)

/* primary proc-based controls */
#define CPU_BASED_MONITOR_TRAP_FLAG  (1UL << 27)
#define CPU_BASED_USE_MSR_BITMAPS    (1UL << 28)
#define CPU_BASED_ACTIVATE_SECONDARY (1UL << 31)

/* secondary proc-based controls */
#define SECONDARY_ENABLE_EPT        (1UL << 1)
#define SECONDARY_ENABLE_RDTSCP     (1UL << 3)
#define SECONDARY_ENABLE_XSAVES     (1UL << 20)

/* vm-exit controls */
#define VM_EXIT_HOST_ADDR_SPACE_SIZE (1UL << 9)
#define VM_EXIT_ACK_INTR_ON_EXIT    (1UL << 15)

/* vm-entry controls */
#define VM_ENTRY_IA32E_MODE_GUEST   (1UL << 9)

/* VMCS field encodings */
#define VMCS_VIRTUAL_PROCESSOR_ID       0x00000000
#define VMCS_GUEST_ES_SELECTOR          0x00000800
#define VMCS_GUEST_CS_SELECTOR          0x00000802
#define VMCS_GUEST_SS_SELECTOR          0x00000804
#define VMCS_GUEST_DS_SELECTOR          0x00000806
#define VMCS_GUEST_FS_SELECTOR          0x00000808
#define VMCS_GUEST_GS_SELECTOR          0x0000080A
#define VMCS_GUEST_LDTR_SELECTOR        0x0000080C
#define VMCS_GUEST_TR_SELECTOR          0x0000080E
#define VMCS_HOST_ES_SELECTOR           0x00000C00
#define VMCS_HOST_CS_SELECTOR           0x00000C02
#define VMCS_HOST_SS_SELECTOR           0x00000C04
#define VMCS_HOST_DS_SELECTOR           0x00000C06
#define VMCS_HOST_FS_SELECTOR           0x00000C08
#define VMCS_HOST_GS_SELECTOR           0x00000C0A
#define VMCS_HOST_TR_SELECTOR           0x00000C0C

#define VMCS_MSR_BITMAP                 0x00002004
#define VMCS_EPT_POINTER                0x0000201A
#define VMCS_VMCS_LINK_POINTER          0x00002800
#define VMCS_GUEST_IA32_DEBUGCTL        0x00002802
#define VMCS_GUEST_IA32_EFER            0x00002806
#define VMCS_HOST_IA32_EFER             0x00002C02

#define VMCS_PIN_BASED_VM_EXEC_CONTROL  0x00004000
#define VMCS_CPU_BASED_VM_EXEC_CONTROL  0x00004002
#define VMCS_EXCEPTION_BITMAP           0x00004004
#define VMCS_CR3_TARGET_COUNT           0x0000400A
#define VMCS_VM_EXIT_CONTROLS           0x0000400C
#define VMCS_VM_EXIT_MSR_STORE_COUNT    0x0000400E
#define VMCS_VM_EXIT_MSR_LOAD_COUNT     0x00004010
#define VMCS_VM_ENTRY_CONTROLS          0x00004012
#define VMCS_VM_ENTRY_MSR_LOAD_COUNT    0x00004014
#define VMCS_SECONDARY_VM_EXEC_CONTROL  0x0000401E
#define VMCS_VM_INSTRUCTION_ERROR       0x00004400
#define VMCS_VM_EXIT_REASON             0x00004402
#define VMCS_VM_EXIT_INTR_INFO          0x00004404
#define VMCS_VM_EXIT_INSTRUCTION_LEN    0x0000440C

#define VMCS_GUEST_ES_LIMIT             0x00004800
#define VMCS_GUEST_CS_LIMIT             0x00004802
#define VMCS_GUEST_SS_LIMIT             0x00004804
#define VMCS_GUEST_DS_LIMIT             0x00004806
#define VMCS_GUEST_FS_LIMIT             0x00004808
#define VMCS_GUEST_GS_LIMIT             0x0000480A
#define VMCS_GUEST_LDTR_LIMIT           0x0000480C
#define VMCS_GUEST_TR_LIMIT             0x0000480E
#define VMCS_GUEST_GDTR_LIMIT           0x00004810
#define VMCS_GUEST_IDTR_LIMIT           0x00004812
#define VMCS_GUEST_ES_AR                0x00004814
#define VMCS_GUEST_CS_AR                0x00004816
#define VMCS_GUEST_SS_AR                0x00004818
#define VMCS_GUEST_DS_AR                0x0000481A
#define VMCS_GUEST_FS_AR                0x0000481C
#define VMCS_GUEST_GS_AR               0x0000481E
#define VMCS_GUEST_LDTR_AR              0x00004820
#define VMCS_GUEST_TR_AR                0x00004822
#define VMCS_GUEST_INTERRUPTIBILITY     0x00004824
#define VMCS_GUEST_ACTIVITY_STATE       0x00004826
#define VMCS_GUEST_SYSENTER_CS          0x0000482A
#define VMCS_VMX_PREEMPTION_TIMER       0x0000482E
#define VMCS_HOST_SYSENTER_CS           0x00004C00

#define VMCS_CR0_GUEST_HOST_MASK        0x00006000
#define VMCS_CR4_GUEST_HOST_MASK        0x00006002
#define VMCS_CR0_READ_SHADOW            0x00006004
#define VMCS_CR4_READ_SHADOW            0x00006006
#define VMCS_EXIT_QUALIFICATION         0x00006400
#define VMCS_GUEST_CR0                  0x00006800
#define VMCS_GUEST_CR3                  0x00006802
#define VMCS_GUEST_CR4                  0x00006804
#define VMCS_GUEST_ES_BASE              0x00006806
#define VMCS_GUEST_CS_BASE              0x00006808
#define VMCS_GUEST_SS_BASE              0x0000680A
#define VMCS_GUEST_DS_BASE              0x0000680C
#define VMCS_GUEST_FS_BASE              0x0000680E
#define VMCS_GUEST_GS_BASE              0x00006810
#define VMCS_GUEST_LDTR_BASE            0x00006812
#define VMCS_GUEST_TR_BASE              0x00006814
#define VMCS_GUEST_GDTR_BASE            0x00006816
#define VMCS_GUEST_IDTR_BASE            0x00006818
#define VMCS_GUEST_DR7                  0x0000681A
#define VMCS_GUEST_RSP                  0x0000681C
#define VMCS_GUEST_RIP                  0x0000681E
#define VMCS_GUEST_RFLAGS               0x00006820
#define VMCS_GUEST_PENDING_DBG          0x00006822
#define VMCS_GUEST_SYSENTER_ESP         0x00006824
#define VMCS_GUEST_SYSENTER_EIP         0x00006826
#define VMCS_HOST_CR0                   0x00006C00
#define VMCS_HOST_CR3                   0x00006C02
#define VMCS_HOST_CR4                   0x00006C04
#define VMCS_HOST_FS_BASE               0x00006C06
#define VMCS_HOST_GS_BASE               0x00006C08
#define VMCS_HOST_TR_BASE               0x00006C0A
#define VMCS_HOST_GDTR_BASE             0x00006C0C
#define VMCS_HOST_IDTR_BASE             0x00006C0E
#define VMCS_HOST_SYSENTER_ESP          0x00006C10
#define VMCS_HOST_SYSENTER_EIP          0x00006C12
#define VMCS_HOST_RSP                   0x00006C14
#define VMCS_HOST_RIP                   0x00006C16

/* VM exit reasons */
#define EXIT_REASON_EXCEPTION_NMI       0
#define EXIT_REASON_EXTERNAL_INTERRUPT  1
#define EXIT_REASON_TRIPLE_FAULT        2
#define EXIT_REASON_CPUID               10
#define EXIT_REASON_HLT                 12
#define EXIT_REASON_INVD                13
#define EXIT_REASON_VMCALL              18
#define EXIT_REASON_CR_ACCESS           28
#define EXIT_REASON_MSR_READ            31
#define EXIT_REASON_MSR_WRITE           32
#define EXIT_REASON_EPT_VIOLATION       48
#define EXIT_REASON_EPT_MISCONFIG       49
#define EXIT_REASON_PREEMPT_TIMER       52
#define EXIT_REASON_MONITOR_TRAP_FLAG    37
#define EXIT_REASON_XSETBV              55

/* hypercall codes (guest RAX on VMCALL) */
#define HVCALL_PING             0x1
#define HVCALL_MTF_START        0x10
#define HVCALL_MTF_STOP         0x11
#define HVCALL_LSTAR_GET        0x20
#define HVCALL_LSTAR_REDIRECT   0x21
#define HVCALL_LSTAR_RESTORE    0x22
#define HVCALL_CHECK_INTEGRITY  0x30
#define HVCALL_ANTIDEBUG_ON     0x40
#define HVCALL_ANTIDEBUG_OFF    0x41

static inline UINT64
VMCALL_HV(UINT64 Code, UINT64 Arg1, UINT64 Arg2)
{
    UINT64 Ret;
    __asm__ volatile("vmcall"
        : "=a"(Ret)
        : "a"(Code), "c"(Arg1), "d"(Arg2)
        : "memory");
    return Ret;
}

typedef struct _GUEST_REGS
{
    UINT64 R15;
    UINT64 R14;
    UINT64 R13;
    UINT64 R12;
    UINT64 R11;
    UINT64 R10;
    UINT64 R9;
    UINT64 R8;
    UINT64 Rdi;
    UINT64 Rsi;
    UINT64 Rbp;
    UINT64 Rbx;
    UINT64 Rdx;
    UINT64 Rcx;
    UINT64 Rax;
} GUEST_REGS, *PGUEST_REGS;

extern UINT8 ASM_VMXON(UINT64 *PhysAddr);
extern UINT8 ASM_VMCLEAR(UINT64 *PhysAddr);
extern UINT8 ASM_VMPTRLD(UINT64 *PhysAddr);
extern VOID  ASM_VMXOFF(VOID);
extern UINT64 ASM_VMREAD(UINT64 Field);
extern VOID   ASM_VMWRITE(UINT64 Field, UINT64 Value);
extern UINT8  VMX_LAUNCH(VOID);

#define VMREAD(f)       ASM_VMREAD((UINT64)(f))
#define VMWRITE(f, v)   ASM_VMWRITE((UINT64)(f), (UINT64)(v))

extern UINT8 VmxPreemptionTimerActive;
extern UINT8 VmxMtfSupported;

UINT8 INIT_VMX(VOID);

UINT64 VMEXIT_DISPATCH(PGUEST_REGS Regs);

#endif

#include "vmx.h"

extern UINT8  HANDLE_CR_ACCESS(PGUEST_REGS Regs, UINT64 Qualification);
extern UINT8  HANDLE_MSR_WRITE(PGUEST_REGS Regs);
extern UINT8  CHECK_INTEGRITY(VOID);
extern UINT8  VERIFY_VMCS_INTEGRITY(VOID);
extern UINT64 GET_LSTAR(VOID);
extern UINT64 REDIRECT_LSTAR(UINT64 NewAddr);
extern VOID   RESTORE_LSTAR(VOID);

static UINT64 MtfStepsRemaining = 0;

UINT64
VMEXIT_DISPATCH(
    PGUEST_REGS Regs
    )
{
    if (VERIFY_VMCS_INTEGRITY())
    {
        return 1;
    }

    UINT64 ExitReason = VMREAD(VMCS_VM_EXIT_REASON) & 0xFFFF;
    UINT64 Qualification = VMREAD(VMCS_EXIT_QUALIFICATION);
    UINT64 Rip, Len;

    switch (ExitReason)
    {
    case EXIT_REASON_EXCEPTION_NMI:
    {
        UINT64 IntrInfo = VMREAD(VMCS_VM_EXIT_INTR_INFO);
        UINT8 Vector = IntrInfo & 0xFF;

        if (Vector == 1)
        {
            UINT64 GuestRip = VMREAD(VMCS_GUEST_RIP);
            UINT64 GuestDr6 = READ_DR6();

            FB_PRINT("[HV] ANTI-DEBUG: #DB at rip=");
            FB_PRINT_HEX(GuestRip);
            FB_PRINT(" dr6=");
            FB_PRINT_HEX(GuestDr6);
            FB_PRINT("\n");

            WRITE_DR0(0);
            WRITE_DR6(0);
            VMWRITE(VMCS_GUEST_DR7, 0x400);
            return 0;
        }
        else if (Vector == 3)
        {
            UINT64 GuestRip = VMREAD(VMCS_GUEST_RIP);

            FB_PRINT("[HV] ANTI-DEBUG: #BP at rip=");
            FB_PRINT_HEX(GuestRip);
            FB_PRINT("\n");

            VMWRITE(VMCS_GUEST_RIP, GuestRip + 1);
            return 0;
        }

        return 0;
    }

    case EXIT_REASON_CPUID:
    {
        UINT32 Eax, Ebx, Ecx, Edx;
        DO_CPUID((UINT32)Regs->Rax, (UINT32)Regs->Rcx, &Eax, &Ebx, &Ecx, &Edx);

        if ((UINT32)Regs->Rax == 1)
        {
            Ecx &= ~(1U << 5);
        }

        Regs->Rax = Eax;
        Regs->Rbx = Ebx;
        Regs->Rcx = Ecx;
        Regs->Rdx = Edx;

        Rip = VMREAD(VMCS_GUEST_RIP);
        Len = VMREAD(VMCS_VM_EXIT_INSTRUCTION_LEN);
        VMWRITE(VMCS_GUEST_RIP, Rip + Len);
        return 0;
    }

    case EXIT_REASON_CR_ACCESS:
        return HANDLE_CR_ACCESS(Regs, Qualification);

    case EXIT_REASON_MSR_READ:
    {
        UINT32 Msr = (UINT32)Regs->Rcx;
        UINT64 Value = RDMSR(Msr);
        Regs->Rax = (UINT32)Value;
        Regs->Rdx = (UINT32)(Value >> 32);

        Rip = VMREAD(VMCS_GUEST_RIP);
        Len = VMREAD(VMCS_VM_EXIT_INSTRUCTION_LEN);
        VMWRITE(VMCS_GUEST_RIP, Rip + Len);
        return 0;
    }

    case EXIT_REASON_MSR_WRITE:
        return HANDLE_MSR_WRITE(Regs);

    case EXIT_REASON_VMCALL:
    {
        UINT64 Code = Regs->Rax;
        UINT64 Arg1 = Regs->Rcx;

        Rip = VMREAD(VMCS_GUEST_RIP);
        Len = VMREAD(VMCS_VM_EXIT_INSTRUCTION_LEN);
        VMWRITE(VMCS_GUEST_RIP, Rip + Len);

        switch (Code)
        {
        case HVCALL_PING:
            Regs->Rax = 0x4856;
            break;

        case HVCALL_MTF_START:
        {
            if (!VmxMtfSupported)
            {
                FB_PRINT("[HV] MTF not supported\n");
                Regs->Rax = (UINT64)-1;
                break;
            }
            MtfStepsRemaining = Arg1;
            UINT64 ProcCtl = VMREAD(VMCS_CPU_BASED_VM_EXEC_CONTROL);
            ProcCtl |= CPU_BASED_MONITOR_TRAP_FLAG;
            VMWRITE(VMCS_CPU_BASED_VM_EXEC_CONTROL, ProcCtl);
            FB_PRINT("[HV] MTF on, steps=");
            FB_PRINT_HEX(Arg1);
            FB_PRINT("\n");
            Regs->Rax = 0;
            break;
        }

        case HVCALL_MTF_STOP:
        {
            MtfStepsRemaining = 0;
            UINT64 ProcCtl = VMREAD(VMCS_CPU_BASED_VM_EXEC_CONTROL);
            ProcCtl &= ~(UINT64)CPU_BASED_MONITOR_TRAP_FLAG;
            VMWRITE(VMCS_CPU_BASED_VM_EXEC_CONTROL, ProcCtl);
            Regs->Rax = 0;
            break;
        }

        case HVCALL_LSTAR_GET:
            Regs->Rax = GET_LSTAR();
            break;

        case HVCALL_LSTAR_REDIRECT:
            Regs->Rax = REDIRECT_LSTAR(Arg1);
            break;

        case HVCALL_LSTAR_RESTORE:
            RESTORE_LSTAR();
            Regs->Rax = 0;
            break;

        case HVCALL_CHECK_INTEGRITY:
            Regs->Rax = 0;
            break;

        case HVCALL_ANTIDEBUG_ON:
        {
            UINT64 ExBmp = VMREAD(VMCS_EXCEPTION_BITMAP);
            ExBmp |= (1UL << 1) | (1UL << 3);
            VMWRITE(VMCS_EXCEPTION_BITMAP, ExBmp);
            FB_PRINT("[HV] anti-debug ON (trapping #DB/#BP)\n");
            Regs->Rax = 0;
            break;
        }

        case HVCALL_ANTIDEBUG_OFF:
        {
            UINT64 ExBmp = VMREAD(VMCS_EXCEPTION_BITMAP);
            ExBmp &= ~((1ULL << 1) | (1ULL << 3));
            VMWRITE(VMCS_EXCEPTION_BITMAP, ExBmp);
            FB_PRINT("[HV] anti-debug OFF\n");
            Regs->Rax = 0;
            break;
        }

        default:
            FB_PRINT("[HV] unknown hypercall ");
            FB_PRINT_HEX(Code);
            FB_PRINT("\n");
            Regs->Rax = (UINT64)-1;
            break;
        }
        return 0;
    }

    case EXIT_REASON_EPT_VIOLATION:
    {
        UINT64 GuestPhys = VMREAD(0x2400);
        FB_PRINT("[HV] EPT violation at ");
        FB_PRINT_HEX(GuestPhys);
        FB_PRINT(" qual=");
        FB_PRINT_HEX(Qualification);
        FB_PRINT("\n");
        return 0;
    }

    case EXIT_REASON_EPT_MISCONFIG:
    {
        FB_PRINT("[HV] EPT misconfig!\n");
        return 1;
    }

    case EXIT_REASON_MONITOR_TRAP_FLAG:
    {
        UINT64 GuestRip = VMREAD(VMCS_GUEST_RIP);
        FB_PRINT("[HV] MTF step rip=");
        FB_PRINT_HEX(GuestRip);
        FB_PRINT("\n");

        if (MtfStepsRemaining > 0)
        {
            MtfStepsRemaining--;
            if (MtfStepsRemaining == 0)
            {
                UINT64 ProcCtl = VMREAD(VMCS_CPU_BASED_VM_EXEC_CONTROL);
                ProcCtl &= ~(UINT64)CPU_BASED_MONITOR_TRAP_FLAG;
                VMWRITE(VMCS_CPU_BASED_VM_EXEC_CONTROL, ProcCtl);
                FB_PRINT("[HV] MTF complete\n");
            }
        }
        return 0;
    }

    case EXIT_REASON_PREEMPT_TIMER:
    {
        CHECK_INTEGRITY();
        if (VmxPreemptionTimerActive)
        {
            VMWRITE(VMCS_VMX_PREEMPTION_TIMER, 0x10000000);
        }
        return 0;
    }

    case EXIT_REASON_HLT:
    {
        Rip = VMREAD(VMCS_GUEST_RIP);
        Len = VMREAD(VMCS_VM_EXIT_INSTRUCTION_LEN);
        VMWRITE(VMCS_GUEST_RIP, Rip + Len);
        return 0;
    }

    case EXIT_REASON_XSETBV:
    {
        __asm__ volatile("xsetbv"
            : : "a"((UINT32)Regs->Rax), "d"((UINT32)Regs->Rdx), "c"((UINT32)Regs->Rcx));
        Rip = VMREAD(VMCS_GUEST_RIP);
        Len = VMREAD(VMCS_VM_EXIT_INSTRUCTION_LEN);
        VMWRITE(VMCS_GUEST_RIP, Rip + Len);
        return 0;
    }

    case EXIT_REASON_INVD:
    {
        __asm__ volatile("wbinvd");
        Rip = VMREAD(VMCS_GUEST_RIP);
        Len = VMREAD(VMCS_VM_EXIT_INSTRUCTION_LEN);
        VMWRITE(VMCS_GUEST_RIP, Rip + Len);
        return 0;
    }

    default:
        FB_PRINT("[HV] unhandled exit reason=");
        FB_PRINT_HEX(ExitReason);
        FB_PRINT(" rip=");
        FB_PRINT_HEX(VMREAD(VMCS_GUEST_RIP));
        FB_PRINT("\n");
        return 1;
    }
}

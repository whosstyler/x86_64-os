#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

typedef void VOID;
typedef VOID *PVOID;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int64_t INT64;
typedef uint64_t UINTN;

typedef struct _BOOT_INFO
{
    UINT64 MemoryMap;
    UINT64 MemoryMapSize;
    UINT64 MemoryMapKey;
    UINT64 DescriptorSize;
    UINT32 DescriptorVersion;
    UINT64 FramebufferBase;
    UINT32 FramebufferWidth;
    UINT32 FramebufferHeight;
    UINT32 FramebufferPitch;
    UINT32 FramebufferPixelFormat;
} BOOT_INFO, *PBOOT_INFO;

#define PIXEL_FORMAT_RGBX 0
#define PIXEL_FORMAT_BGRX 1

#define EFI_CONVENTIONAL_MEMORY  7
#define EFI_LOADER_CODE          1
#define EFI_LOADER_DATA          2
#define EFI_BOOT_SERVICES_CODE   3
#define EFI_BOOT_SERVICES_DATA   4

typedef struct _EFI_MEMORY_DESCRIPTOR
{
    UINT32 Type;
    UINT64 PhysicalStart;
    UINT64 VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

#define PAGE_SIZE 4096

typedef struct __attribute__((packed)) _GDT_ENTRY
{
    UINT16 LimitLow;
    UINT16 BaseLow;
    UINT8  BaseMid;
    UINT8  Access;
    UINT8  FlagsLimitHigh;
    UINT8  BaseHigh;
} GDT_ENTRY;

typedef struct __attribute__((packed)) _GDT_PTR
{
    UINT16 Limit;
    UINT64 Base;
} GDT_PTR;

VOID INIT_GDT(VOID);

typedef struct __attribute__((packed)) _IDT_ENTRY
{
    UINT16 OffsetLow;
    UINT16 Selector;
    UINT8  Ist;
    UINT8  TypeAttr;
    UINT16 OffsetMid;
    UINT32 OffsetHigh;
    UINT32 Zero;
} IDT_ENTRY;

typedef struct __attribute__((packed)) _IDT_PTR
{
    UINT16 Limit;
    UINT64 Base;
} IDT_PTR;

#define IDT_ENTRIES 256

VOID INIT_IDT(VOID);

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define ICW1_INIT  0x11
#define ICW4_8086  0x01
#define IRQ_BASE   0x20

VOID INIT_PIC(VOID);
VOID PIC_SEND_EOI(UINT8 Irq);

#define PIT_CH0_DATA  0x40
#define PIT_CMD       0x43
#define PIT_FREQ      1193182
#define TIMER_HZ      100

VOID INIT_TIMER(VOID);

static inline VOID
OUTB(
    UINT16 Port,
    UINT8 Value
    )
{
    __asm__ volatile("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static inline UINT8
INB(
    UINT16 Port
    )
{
    UINT8 Value;
    __asm__ volatile("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

static inline VOID
IO_WAIT(VOID)
{
    OUTB(0x80, 0);
}

static inline UINT64
READ_CR3(VOID)
{
    UINT64 Value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(Value));
    return Value;
}

static inline VOID
WRITE_CR3(
    UINT64 Value
    )
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(Value) : "memory");
}

static inline UINT64
READ_CR2(VOID)
{
    UINT64 Value;
    __asm__ volatile("mov %%cr2, %0" : "=r"(Value));
    return Value;
}

static inline VOID
INVLPG(
    UINT64 Addr
    )
{
    __asm__ volatile("invlpg (%0)" : : "r"(Addr) : "memory");
}

static inline UINT64
READ_CR0(VOID)
{
    UINT64 Value;
    __asm__ volatile("mov %%cr0, %0" : "=r"(Value));
    return Value;
}

static inline VOID
WRITE_CR0(
    UINT64 Value
    )
{
    __asm__ volatile("mov %0, %%cr0" : : "r"(Value) : "memory");
}

static inline VOID
WRITE_DR0(
    UINT64 Addr
    )
{
    __asm__ volatile("mov %0, %%dr0" : : "r"(Addr));
}

static inline UINT64
READ_DR6(VOID)
{
    UINT64 Value;
    __asm__ volatile("mov %%dr6, %0" : "=r"(Value));
    return Value;
}

static inline VOID
WRITE_DR6(
    UINT64 Value
    )
{
    __asm__ volatile("mov %0, %%dr6" : : "r"(Value));
}

static inline VOID
WRITE_DR7(
    UINT64 Value
    )
{
    __asm__ volatile("mov %0, %%dr7" : : "r"(Value));
}

static inline UINT64
READ_DR7(VOID)
{
    UINT64 Value;
    __asm__ volatile("mov %%dr7, %0" : "=r"(Value));
    return Value;
}

static inline UINT64
READ_CR4(VOID)
{
    UINT64 Value;
    __asm__ volatile("mov %%cr4, %0" : "=r"(Value));
    return Value;
}

static inline VOID
WRITE_CR4(
    UINT64 Value
    )
{
    __asm__ volatile("mov %0, %%cr4" : : "r"(Value) : "memory");
}

static inline VOID
DO_CPUID(
    UINT32 Leaf,
    UINT32 Subleaf,
    UINT32 *Eax,
    UINT32 *Ebx,
    UINT32 *Ecx,
    UINT32 *Edx
    )
{
    __asm__ volatile("cpuid"
        : "=a"(*Eax), "=b"(*Ebx), "=c"(*Ecx), "=d"(*Edx)
        : "a"(Leaf), "c"(Subleaf));
}

static inline UINT64
RDMSR(
    UINT32 Msr
    )
{
    UINT32 Lo, Hi;
    __asm__ volatile("rdmsr" : "=a"(Lo), "=d"(Hi) : "c"(Msr));
    return ((UINT64)Hi << 32) | Lo;
}

static inline VOID
WRMSR(
    UINT32 Msr,
    UINT64 Value
    )
{
    __asm__ volatile("wrmsr"
        : : "c"(Msr), "a"((UINT32)Value), "d"((UINT32)(Value >> 32)));
}

typedef struct __attribute__((packed)) _TSS64
{
    UINT32 Reserved0;
    UINT64 Rsp0;
    UINT64 Rsp1;
    UINT64 Rsp2;
    UINT64 Reserved1;
    UINT64 Ist1;
    UINT64 Ist2;
    UINT64 Ist3;
    UINT64 Ist4;
    UINT64 Ist5;
    UINT64 Ist6;
    UINT64 Ist7;
    UINT64 Reserved2;
    UINT16 Reserved3;
    UINT16 IopbOffset;
} TSS64, *PTSS64;

typedef struct __attribute__((packed)) _INTERRUPT_FRAME
{
    UINT64 Rip;
    UINT64 Cs;
    UINT64 Rflags;
    UINT64 Rsp;
    UINT64 Ss;
} INTERRUPT_FRAME;

VOID INIT_CPU_SECURITY(VOID);
VOID FB_PRINT(const char *S);
VOID FB_PRINT_HEX(UINT64 Value);

VOID
KERNEL_MAIN(
    PBOOT_INFO BootInfo
    );

#endif

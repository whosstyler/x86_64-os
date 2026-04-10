#include "../include/kernel.h"

static IDT_ENTRY Idt[IDT_ENTRIES];
static IDT_PTR   IdtPtr;

extern VOID ISR_STUB_0(VOID);
extern VOID ISR_STUB_1(VOID);
extern VOID ISR_STUB_2(VOID);
extern VOID ISR_STUB_3(VOID);
extern VOID ISR_STUB_4(VOID);
extern VOID ISR_STUB_5(VOID);
extern VOID ISR_STUB_6(VOID);
extern VOID ISR_STUB_7(VOID);
extern VOID ISR_STUB_8(VOID);
extern VOID ISR_STUB_9(VOID);
extern VOID ISR_STUB_10(VOID);
extern VOID ISR_STUB_11(VOID);
extern VOID ISR_STUB_12(VOID);
extern VOID ISR_STUB_13(VOID);
extern VOID ISR_STUB_14(VOID);
extern VOID ISR_STUB_15(VOID);
extern VOID ISR_STUB_16(VOID);

extern VOID IRQ_STUB_0(VOID);
extern VOID IRQ_STUB_1(VOID);

static VOID
SET_IDT_ENTRY(
    IDT_ENTRY *Entry,
    UINT64 Handler,
    UINT16 Selector,
    UINT8 TypeAttr
    )
{
    Entry->OffsetLow  = (UINT16)(Handler & 0xFFFF);
    Entry->OffsetMid  = (UINT16)((Handler >> 16) & 0xFFFF);
    Entry->OffsetHigh = (UINT32)((Handler >> 32) & 0xFFFFFFFF);
    Entry->Selector   = Selector;
    Entry->Ist        = 0;
    Entry->TypeAttr   = TypeAttr;
    Entry->Zero       = 0;
}

#define GATE_INT  0x8E
#define GATE_TRAP 0x8F

VOID
INIT_IDT(VOID)
{
    UINTN I;

    for (I = 0; I < IDT_ENTRIES; I++)
    {
        SET_IDT_ENTRY(&Idt[I], 0, 0, 0);
    }

    SET_IDT_ENTRY(&Idt[0],  (UINT64)ISR_STUB_0,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[1],  (UINT64)ISR_STUB_1,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[2],  (UINT64)ISR_STUB_2,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[3],  (UINT64)ISR_STUB_3,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[4],  (UINT64)ISR_STUB_4,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[5],  (UINT64)ISR_STUB_5,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[6],  (UINT64)ISR_STUB_6,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[7],  (UINT64)ISR_STUB_7,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[8],  (UINT64)ISR_STUB_8,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[9],  (UINT64)ISR_STUB_9,  0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[10], (UINT64)ISR_STUB_10, 0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[11], (UINT64)ISR_STUB_11, 0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[12], (UINT64)ISR_STUB_12, 0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[13], (UINT64)ISR_STUB_13, 0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[14], (UINT64)ISR_STUB_14, 0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[15], (UINT64)ISR_STUB_15, 0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[16], (UINT64)ISR_STUB_16, 0x08, GATE_INT);

    SET_IDT_ENTRY(&Idt[IRQ_BASE + 0], (UINT64)IRQ_STUB_0, 0x08, GATE_INT);
    SET_IDT_ENTRY(&Idt[IRQ_BASE + 1], (UINT64)IRQ_STUB_1, 0x08, GATE_INT);

    IdtPtr.Limit = sizeof(Idt) - 1;
    IdtPtr.Base  = (UINT64)&Idt[0];

    __asm__ volatile("lidt %0" : : "m"(IdtPtr));
}

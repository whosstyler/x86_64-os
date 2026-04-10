#include "task.h"
#include "../mm/heap.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"

PTASK CurrentTask;

static UINT64 NextTid = 0;
static UINT64 NextStackVirt = 0x0000000600000000ULL;

static VOID
TASK_TRAMPOLINE(VOID)
{
    __asm__ volatile("sti");
    CurrentTask->Entry();
    CurrentTask->State = TASK_DEAD;
    YIELD();
    for (;;) { __asm__ volatile("hlt"); }
}

VOID
INIT_SCHEDULER(VOID)
{
    PTASK Idle = (PTASK)KMALLOC(sizeof(TASK));

    Idle->Rsp       = 0;
    Idle->Entry     = (TASK_ENTRY)0;
    Idle->State     = TASK_RUNNING;
    Idle->Tid       = NextTid++;
    Idle->StackBase = (UINT8 *)0;
    Idle->Next      = Idle;

    CurrentTask = Idle;
}

PTASK
CREATE_TASK(
    TASK_ENTRY Entry
    )
{
    PTASK Task = (PTASK)KMALLOC(sizeof(TASK));

    UINT64 GuardVirt = NextStackVirt;
    NextStackVirt += PAGE_SIZE;

    UINT64 StackVirt = NextStackVirt;
    UINT64 I;
    for (I = 0; I < TASK_STACK_PAGES; I++)
    {
        UINT64 Phys = ALLOC_PAGE();
        MAP_PAGE(StackVirt + I * PAGE_SIZE, Phys, PTE_WRITABLE);
    }
    NextStackVirt += TASK_STACK_SIZE;

    (VOID)GuardVirt;

    Task->StackBase = (UINT8 *)StackVirt;
    Task->Entry     = Entry;
    Task->State     = TASK_READY;
    Task->Tid       = NextTid++;

    UINT64 *Sp = (UINT64 *)(Task->StackBase + TASK_STACK_SIZE);

    *(--Sp) = (UINT64)TASK_TRAMPOLINE;
    *(--Sp) = 0;
    *(--Sp) = 0;
    *(--Sp) = 0;
    *(--Sp) = 0;
    *(--Sp) = 0;
    *(--Sp) = 0;

    Task->Rsp = (UINT64)Sp;

    __asm__ volatile("cli");

    PTASK Last = CurrentTask;
    while (Last->Next != CurrentTask)
    {
        Last = Last->Next;
    }
    Last->Next = Task;
    Task->Next = CurrentTask;

    __asm__ volatile("sti");

    return Task;
}

VOID
SCHEDULE(VOID)
{
    PTASK Old = CurrentTask;
    PTASK Next = Old->Next;

    while (Next != Old && Next->State == TASK_DEAD)
    {
        Next = Next->Next;
    }

    if (Next == Old || Next->State == TASK_DEAD)
    {
        return;
    }

    if (Old->State == TASK_RUNNING)
    {
        Old->State = TASK_READY;
    }

    Next->State = TASK_RUNNING;
    CurrentTask = Next;
    CONTEXT_SWITCH(Old, Next);
}

VOID
YIELD(VOID)
{
    SCHEDULE();
}

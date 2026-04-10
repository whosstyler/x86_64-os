#include "task.h"
#include "../mm/heap.h"

PTASK CurrentTask;

static UINT64 NextTid = 0;

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
    Task->StackBase = (UINT8 *)KMALLOC(TASK_STACK_SIZE);
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

#ifndef TASK_H
#define TASK_H

#include "../include/kernel.h"

typedef VOID (*TASK_ENTRY)(VOID);

#define TASK_READY   0
#define TASK_RUNNING 1
#define TASK_DEAD    2

#define TASK_STACK_SIZE (PAGE_SIZE * 4)

typedef struct _TASK
{
    UINT64      Rsp;
    TASK_ENTRY  Entry;
    UINT64      State;
    UINT64      Tid;
    UINT8      *StackBase;
    struct _TASK *Next;
} TASK, *PTASK;

extern PTASK CurrentTask;

VOID
INIT_SCHEDULER(VOID);

PTASK
CREATE_TASK(
    TASK_ENTRY Entry
    );

VOID
SCHEDULE(VOID);

VOID
YIELD(VOID);

extern VOID
CONTEXT_SWITCH(
    PTASK Old,
    PTASK New
    );

#endif

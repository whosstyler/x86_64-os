#include "heap.h"
#include "pmm.h"

static PBLOCK_HEADER HeapHead;

static UINT64
ALIGN8(
    UINT64 Value
    )
{
    return (Value + 7) & ~7ULL;
}

static PBLOCK_HEADER
EXPAND_HEAP(
    UINT64 MinSize
    )
{
    UINT64 Total = BLOCK_HEADER_SIZE + MinSize;
    UINT64 Pages = (Total + PAGE_SIZE - 1) / PAGE_SIZE;
    UINT64 I;

    /*
     * Allocate contiguous pages. The PMM hands out individual pages,
     * so we grab them one at a time and check they're contiguous.
     * If not, free them and try again with a larger single request.
     * For simplicity we just allocate and hope they're adjacent —
     * early in boot this is almost always true. If a page isn't
     * contiguous we free everything and fall back to single-page.
     */
    UINT64 Base = ALLOC_PAGE();
    if (!Base)
    {
        return (PBLOCK_HEADER)0;
    }

    for (I = 1; I < Pages; I++)
    {
        UINT64 Next = ALLOC_PAGE();
        if (Next != Base + I * PAGE_SIZE)
        {
            /* non-contiguous — free what we got and fall back */
            UINT64 J;
            for (J = 0; J < I; J++)
            {
                FREE_PAGE(Base + J * PAGE_SIZE);
            }
            if (Next)
            {
                FREE_PAGE(Next);
            }

            /* fall back: allocate a single page, cap the block size */
            Base = ALLOC_PAGE();
            if (!Base)
            {
                return (PBLOCK_HEADER)0;
            }
            Pages = 1;
            break;
        }
    }

    PBLOCK_HEADER Block = (PBLOCK_HEADER)Base;
    Block->Size   = Pages * PAGE_SIZE - BLOCK_HEADER_SIZE;
    Block->IsFree = 1;
    Block->Next   = (PBLOCK_HEADER)0;
    Block->Prev   = (PBLOCK_HEADER)0;

    return Block;
}

VOID
INIT_HEAP(VOID)
{
    HeapHead = EXPAND_HEAP(PAGE_SIZE - BLOCK_HEADER_SIZE);
}

static VOID
SPLIT_BLOCK(
    PBLOCK_HEADER Block,
    UINT64 Size
    )
{
    UINT64 Remaining = Block->Size - Size - BLOCK_HEADER_SIZE;

    if (Remaining < HEAP_MIN_SPLIT)
    {
        return;
    }

    PBLOCK_HEADER NewBlock =
        (PBLOCK_HEADER)((UINT8 *)Block + BLOCK_HEADER_SIZE + Size);

    NewBlock->Size   = Remaining;
    NewBlock->IsFree = 1;
    NewBlock->Next   = Block->Next;
    NewBlock->Prev   = Block;

    if (Block->Next)
    {
        Block->Next->Prev = NewBlock;
    }

    Block->Next = NewBlock;
    Block->Size = Size;
}

PVOID
KMALLOC(
    UINT64 Size
    )
{
    if (Size == 0)
    {
        return (PVOID)0;
    }

    Size = ALIGN8(Size);

    PBLOCK_HEADER Cur = HeapHead;

    while (Cur)
    {
        if (Cur->IsFree && Cur->Size >= Size)
        {
            SPLIT_BLOCK(Cur, Size);
            Cur->IsFree = 0;
            return (PVOID)((UINT8 *)Cur + BLOCK_HEADER_SIZE);
        }
        if (!Cur->Next)
        {
            break;
        }
        Cur = Cur->Next;
    }

    PBLOCK_HEADER NewBlock = EXPAND_HEAP(Size);
    if (!NewBlock)
    {
        return (PVOID)0;
    }

    if (Cur)
    {
        Cur->Next      = NewBlock;
        NewBlock->Prev = Cur;
    }
    else
    {
        HeapHead = NewBlock;
    }

    SPLIT_BLOCK(NewBlock, Size);
    NewBlock->IsFree = 0;
    return (PVOID)((UINT8 *)NewBlock + BLOCK_HEADER_SIZE);
}

static VOID
COALESCE(
    PBLOCK_HEADER Block
    )
{
    if (Block->Next && Block->Next->IsFree)
    {
        Block->Size += BLOCK_HEADER_SIZE + Block->Next->Size;
        Block->Next  = Block->Next->Next;
        if (Block->Next)
        {
            Block->Next->Prev = Block;
        }
    }

    if (Block->Prev && Block->Prev->IsFree)
    {
        Block->Prev->Size += BLOCK_HEADER_SIZE + Block->Size;
        Block->Prev->Next  = Block->Next;
        if (Block->Next)
        {
            Block->Next->Prev = Block->Prev;
        }
    }
}

VOID
KFREE(
    PVOID Ptr
    )
{
    if (!Ptr)
    {
        return;
    }

    PBLOCK_HEADER Block =
        (PBLOCK_HEADER)((UINT8 *)Ptr - BLOCK_HEADER_SIZE);

    Block->IsFree = 1;
    COALESCE(Block);
}

PVOID
KREALLOC(
    PVOID Ptr,
    UINT64 NewSize
    )
{
    if (!Ptr)
    {
        return KMALLOC(NewSize);
    }

    if (NewSize == 0)
    {
        KFREE(Ptr);
        return (PVOID)0;
    }

    NewSize = ALIGN8(NewSize);

    PBLOCK_HEADER Block =
        (PBLOCK_HEADER)((UINT8 *)Ptr - BLOCK_HEADER_SIZE);

    if (Block->Size >= NewSize)
    {
        return Ptr;
    }

    PVOID NewPtr = KMALLOC(NewSize);
    if (!NewPtr)
    {
        return (PVOID)0;
    }

    UINT64 CopySize = Block->Size;
    UINT8 *Dst = (UINT8 *)NewPtr;
    UINT8 *Src = (UINT8 *)Ptr;
    UINT64 I;
    for (I = 0; I < CopySize; I++)
    {
        Dst[I] = Src[I];
    }

    KFREE(Ptr);
    return NewPtr;
}

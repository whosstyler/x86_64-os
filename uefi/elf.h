#ifndef ELF_H
#define ELF_H

#include <stdint.h>

typedef uint16_t ELF64_HALF;
typedef uint32_t ELF64_WORD;
typedef uint64_t ELF64_XWORD;
typedef uint64_t ELF64_ADDR;
typedef uint64_t ELF64_OFF;
typedef uint8_t  UINT8;
typedef uint32_t UINT32;

#define ELF_MAGIC       0x464C457F
#define ELF_CLASS_64    2
#define ELF_DATA_LE     1
#define ELF_TYPE_EXEC   2
#define ELF_MACHINE_X64 0x3E

#define PT_LOAD         1

typedef struct _ELF64_EHDR
{
    UINT8       Ident[16];
    ELF64_HALF  Type;
    ELF64_HALF  Machine;
    ELF64_WORD  Version;
    ELF64_ADDR  Entry;
    ELF64_OFF   PhOff;
    ELF64_OFF   ShOff;
    ELF64_WORD  Flags;
    ELF64_HALF  EhSize;
    ELF64_HALF  PhEntSize;
    ELF64_HALF  PhNum;
    ELF64_HALF  ShEntSize;
    ELF64_HALF  ShNum;
    ELF64_HALF  ShStrNdx;
} ELF64_EHDR, *PELF64_EHDR;

typedef struct _ELF64_PHDR
{
    ELF64_WORD  Type;
    ELF64_WORD  Flags;
    ELF64_OFF   Offset;
    ELF64_ADDR  VAddr;
    ELF64_ADDR  PAddr;
    ELF64_XWORD FileSize;
    ELF64_XWORD MemSize;
    ELF64_XWORD Align;
} ELF64_PHDR, *PELF64_PHDR;

#endif

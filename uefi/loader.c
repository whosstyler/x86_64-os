#include "uefi.h"
#include "elf.h"
#include "../shared/mem.h"

static EFI_SYSTEM_TABLE *gST;

static VOID
PRINT(
    CHAR16 *Msg
    )
{
    if (gST && gST->ConOut)
    {
        gST->ConOut->OutputString(gST->ConOut, Msg);
    }
}

static VOID
PRINT_HEX(
    UINT64 Value
    )
{
    CHAR16 Buf[19];
    CHAR16 Hex[] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
    INTN I;

    Buf[0] = '0';
    Buf[1] = 'x';
    for (I = 15; I >= 0; I--)
    {
        Buf[2 + (15 - I)] = Hex[(Value >> (I * 4)) & 0xF];
    }
    Buf[18] = 0;
    PRINT(Buf);
}

static VOID
HALT(
    CHAR16 *Msg
    )
{
    PRINT(Msg);
    for (;;)
    {
        __asm__ volatile("hlt");
    }
}

static EFI_GUID gEfiLoadedImageGuid  = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID gEfiSimpleFsGuid     = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

static CHAR16 KernelPath[] = {
    '\\', 'k', 'e', 'r', 'n', 'e', 'l', '.', 'e', 'l', 'f', 0
};

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
} BOOT_INFO;

typedef VOID(*KERNEL_ENTRY_FN)(BOOT_INFO *Info);

static EFI_STATUS
LOAD_KERNEL_ELF(
    EFI_HANDLE ImageHandle,
    UINT64 *EntryPoint
    )
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
    EFI_FILE_PROTOCOL *RootDir;
    EFI_FILE_PROTOCOL *KernelFile;

    Status = gST->BootServices->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageGuid,
        (PVOID *)&LoadedImage
        );
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'L','I',' ','f','a','i','l','\r','\n',0});
    }

    Status = gST->BootServices->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFsGuid,
        (PVOID *)&Fs
        );
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'F','S',' ','f','a','i','l','\r','\n',0});
    }

    Status = Fs->OpenVolume(Fs, &RootDir);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'R','D',' ','f','a','i','l','\r','\n',0});
    }

    Status = RootDir->Open(RootDir, &KernelFile, KernelPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'K','F',' ','n','o','t',' ','f','o','u','n','d','\r','\n',0});
    }

    ELF64_EHDR Ehdr;
    UINTN ReadSize = sizeof(ELF64_EHDR);
    Status = KernelFile->Read(KernelFile, &ReadSize, &Ehdr);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'E','H',' ','r','e','a','d','\r','\n',0});
    }

    UINT32 Magic = *(UINT32 *)Ehdr.Ident;
    if (Magic != ELF_MAGIC)
    {
        HALT((CHAR16[]){'b','a','d',' ','E','L','F','\r','\n',0});
    }
    if (Ehdr.Ident[4] != ELF_CLASS_64 || Ehdr.Machine != ELF_MACHINE_X64)
    {
        HALT((CHAR16[]){'n','o','t',' ','x','6','4','\r','\n',0});
    }

    PRINT((CHAR16[]){'E','L','F',' ','o','k','\r','\n',0});

    UINTN KernelBufSize = 2 * 1024 * 1024;
    UINT8 *KernelBuf;
    Status = gST->BootServices->AllocatePool(EFI_LOADER_DATA, KernelBufSize, (PVOID *)&KernelBuf);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'K','B',' ','a','l','l','o','c','\r','\n',0});
    }

    KernelFile->Close(KernelFile);
    Status = RootDir->Open(RootDir, &KernelFile, KernelPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'K','F',' ','r','e','o','p','e','n','\r','\n',0});
    }

    UINTN ActualRead = KernelBufSize;
    Status = KernelFile->Read(KernelFile, &ActualRead, KernelBuf);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'K','F',' ','r','e','a','d','\r','\n',0});
    }
    KernelFile->Close(KernelFile);
    RootDir->Close(RootDir);

    PRINT((CHAR16[]){'r','e','a','d',' ',0});
    PRINT_HEX(ActualRead);
    PRINT((CHAR16[]){' ','b','y','t','e','s','\r','\n',0});

    PELF64_EHDR Hdr = (PELF64_EHDR)KernelBuf;
    PELF64_PHDR Ph  = (PELF64_PHDR)(KernelBuf + Hdr->PhOff);

    UINTN I;
    for (I = 0; I < Hdr->PhNum; I++)
    {
        if (Ph[I].Type != PT_LOAD)
        {
            continue;
        }

        UINTN Pages = (Ph[I].MemSize + 4095) / 4096;
        EFI_PHYSICAL_ADDRESS SegAddr = Ph[I].PAddr;

        Status = gST->BootServices->AllocatePages(
            AllocateAddress,
            EFI_LOADER_DATA,
            Pages,
            &SegAddr
            );
        if (EFI_ERROR(Status))
        {
            PRINT((CHAR16[]){'s','e','g',' ','@',0});
            PRINT_HEX(Ph[I].PAddr);
            PRINT((CHAR16[]){'\r','\n',0});
            HALT((CHAR16[]){'A','P',' ','f','a','i','l','\r','\n',0});
        }

        MEMCPY((PVOID)SegAddr, KernelBuf + Ph[I].Offset, Ph[I].FileSize);

        if (Ph[I].MemSize > Ph[I].FileSize)
        {
            MEMSET(
                (PVOID)(SegAddr + Ph[I].FileSize),
                0,
                Ph[I].MemSize - Ph[I].FileSize
                );
        }
    }

    gST->BootServices->FreePool(KernelBuf);

    *EntryPoint = Hdr->Entry;

    PRINT((CHAR16[]){'k','e','r','n','e','l',' ','@',' ',0});
    PRINT_HEX(*EntryPoint);
    PRINT((CHAR16[]){'\r','\n',0});

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EfiMain(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )
{
    EFI_STATUS Status;
    UINT64 KernelEntry;

    gST = SystemTable;

    PRINT((CHAR16[]){'u','e','f','i',' ','l','o','a','d','e','r',' ','a','l','i','v','e','\r','\n',0});

    Status = LOAD_KERNEL_ELF(ImageHandle, &KernelEntry);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'l','o','a','d',' ','f','a','i','l','\r','\n',0});
    }

    UINTN MapSize = 0;
    UINTN MapKey = 0;
    UINTN DescSize = 0;
    UINT32 DescVersion = 0;
    EFI_MEMORY_DESCRIPTOR *Map = (EFI_MEMORY_DESCRIPTOR *)0;

    Status = gST->BootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescSize, &DescVersion);
    MapSize += DescSize * 4;

    Status = gST->BootServices->AllocatePool(EFI_LOADER_DATA, MapSize, (PVOID *)&Map);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'M','M',' ','a','l','l','o','c','\r','\n',0});
    }

    Status = gST->BootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescSize, &DescVersion);
    if (EFI_ERROR(Status))
    {
        HALT((CHAR16[]){'M','M',' ','f','a','i','l','\r','\n',0});
    }

    PRINT((CHAR16[]){'m','e','m','m','a','p',' ','o','k','\r','\n',0});

    EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)0;

    Status = gST->BootServices->LocateProtocol(&GopGuid, (PVOID)0, (PVOID *)&Gop);

    BOOT_INFO BootInfo;

    if (Gop && Gop->Mode && Gop->Mode->Info)
    {
        BootInfo.FramebufferBase   = Gop->Mode->FrameBufferBase;
        BootInfo.FramebufferWidth  = Gop->Mode->Info->HorizontalResolution;
        BootInfo.FramebufferHeight = Gop->Mode->Info->VerticalResolution;
        BootInfo.FramebufferPitch  = Gop->Mode->Info->PixelsPerScanLine * 4;

        if (Gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
        {
            BootInfo.FramebufferPixelFormat = 0;
        }
        else
        {
            BootInfo.FramebufferPixelFormat = 1;
        }

        PRINT((CHAR16[]){'g','o','p',' ',0});
        PRINT_HEX(BootInfo.FramebufferWidth);
        PRINT((CHAR16[]){'x',0});
        PRINT_HEX(BootInfo.FramebufferHeight);
        PRINT((CHAR16[]){'\r','\n',0});
    }
    else
    {
        BootInfo.FramebufferBase = 0;
        PRINT((CHAR16[]){'n','o',' ','g','o','p','\r','\n',0});
    }

    BootInfo.MemoryMap         = (UINT64)Map;
    BootInfo.MemoryMapSize     = MapSize;
    BootInfo.MemoryMapKey      = MapKey;
    BootInfo.DescriptorSize    = DescSize;
    BootInfo.DescriptorVersion = DescVersion;

    PRINT((CHAR16[]){'e','x','i','t','i','n','g',' ','b','o','o','t',' ','s','v','c','s','\r','\n',0});

    Status = gST->BootServices->ExitBootServices(ImageHandle, MapKey);
    if (EFI_ERROR(Status))
    {
        MapSize = 0;
        gST->BootServices->GetMemoryMap(&MapSize, (EFI_MEMORY_DESCRIPTOR *)0, &MapKey, &DescSize, &DescVersion);
        MapSize += DescSize * 4;
        gST->BootServices->FreePool(Map);
        gST->BootServices->AllocatePool(EFI_LOADER_DATA, MapSize, (PVOID *)&Map);
        gST->BootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescSize, &DescVersion);

        BootInfo.MemoryMap         = (UINT64)Map;
        BootInfo.MemoryMapSize     = MapSize;
        BootInfo.MemoryMapKey      = MapKey;
        BootInfo.DescriptorSize    = DescSize;
        BootInfo.DescriptorVersion = DescVersion;

        Status = gST->BootServices->ExitBootServices(ImageHandle, MapKey);
        if (EFI_ERROR(Status))
        {
            for (;;) { __asm__ volatile("hlt"); }
        }
    }

    KERNEL_ENTRY_FN Entry = (KERNEL_ENTRY_FN)KernelEntry;
    Entry(&BootInfo);

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}

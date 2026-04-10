#ifndef UEFI_H
#define UEFI_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint64_t UINTN;
typedef int64_t INTN;
typedef uint16_t CHAR16;
typedef uint8_t BOOLEAN;
typedef void VOID;
typedef VOID *PVOID;

typedef UINTN EFI_STATUS;
typedef PVOID EFI_HANDLE;
typedef PVOID EFI_EVENT;
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;

#define EFI_SUCCESS              ((EFI_STATUS)0)
#define EFI_NOT_READY            ((EFI_STATUS)6)
#define EFI_BUFFER_TOO_SMALL     ((EFI_STATUS)(1ULL << 63 | 5))

#define EFI_ERROR_BIT            (1ULL << 63)
#define EFI_ERROR(s)             ((s) & EFI_ERROR_BIT)

#if defined(__x86_64__)
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif

#define IN
#define OUT
#define OPTIONAL

typedef struct _EFI_TABLE_HEADER EFI_TABLE_HEADER;
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct _EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct _EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE_PROTOCOL;

typedef struct _EFI_GUID
{
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    { 0x5B1B31A1, 0x9562, 0x11d2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    { 0x0964E5B22, 0x6459, 0x11d2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

struct _EFI_TABLE_HEADER
{
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 Crc32;
    UINT32 Reserved;
};

typedef struct _EFI_INPUT_KEY
{
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef EFI_STATUS(EFIAPI *EFI_TEXT_STRING)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    IN CHAR16 *String
    );

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
{
    PVOID Reset;
    EFI_TEXT_STRING OutputString;
};

typedef EFI_STATUS(EFIAPI *EFI_INPUT_READ_KEY)(
    IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    OUT EFI_INPUT_KEY *Key
    );

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL
{
    PVOID Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    EFI_EVENT WaitForKey;
};

typedef UINT32 EFI_MEMORY_TYPE;

#define EFI_LOADER_CODE          1
#define EFI_LOADER_DATA          2
#define EFI_BOOT_SERVICES_CODE   3
#define EFI_BOOT_SERVICES_DATA   4
#define EFI_CONVENTIONAL_MEMORY  7

typedef struct _EFI_MEMORY_DESCRIPTOR
{
    UINT32               Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS  VirtualStart;
    UINT64               NumberOfPages;
    UINT64               Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef enum _EFI_ALLOCATE_TYPE
{
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef enum _EFI_LOCATE_SEARCH_TYPE
{
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_PAGES)(
    IN EFI_ALLOCATE_TYPE Type,
    IN EFI_MEMORY_TYPE MemoryType,
    IN UINTN Pages,
    IN OUT EFI_PHYSICAL_ADDRESS *Memory
    );

typedef EFI_STATUS(EFIAPI *EFI_FREE_PAGES)(
    IN EFI_PHYSICAL_ADDRESS Memory,
    IN UINTN Pages
    );

typedef EFI_STATUS(EFIAPI *EFI_GET_MEMORY_MAP)(
    IN OUT UINTN *MemoryMapSize,
    OUT EFI_MEMORY_DESCRIPTOR *MemoryMap,
    OUT UINTN *MapKey,
    OUT UINTN *DescriptorSize,
    OUT UINT32 *DescriptorVersion
    );

typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_POOL)(
    IN EFI_MEMORY_TYPE PoolType,
    IN UINTN Size,
    OUT PVOID *Buffer
    );

typedef EFI_STATUS(EFIAPI *EFI_FREE_POOL)(
    IN PVOID Buffer
    );

typedef EFI_STATUS(EFIAPI *EFI_HANDLE_PROTOCOL)(
    IN EFI_HANDLE Handle,
    IN EFI_GUID *Protocol,
    OUT PVOID *Interface
    );

typedef EFI_STATUS(EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    IN EFI_HANDLE ImageHandle,
    IN UINTN MapKey
    );

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_HANDLE)(
    IN EFI_LOCATE_SEARCH_TYPE SearchType,
    IN EFI_GUID *Protocol OPTIONAL,
    IN PVOID SearchKey OPTIONAL,
    IN OUT UINTN *BufferSize,
    OUT EFI_HANDLE *Buffer
    );

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_PROTOCOL)(
    IN EFI_GUID *Protocol,
    IN PVOID Registration OPTIONAL,
    OUT PVOID *Interface
    );

struct _EFI_BOOT_SERVICES
{
    EFI_TABLE_HEADER         Hdr;
    PVOID                    RaiseTPL;
    PVOID                    RestoreTPL;
    EFI_ALLOCATE_PAGES       AllocatePages;
    EFI_FREE_PAGES           FreePages;
    EFI_GET_MEMORY_MAP       GetMemoryMap;
    EFI_ALLOCATE_POOL        AllocatePool;
    EFI_FREE_POOL            FreePool;
    PVOID                    CreateEvent;
    PVOID                    SetTimer;
    PVOID                    WaitForEvent;
    PVOID                    SignalEvent;
    PVOID                    CloseEvent;
    PVOID                    CheckEvent;
    PVOID                    InstallProtocolInterface;
    PVOID                    ReinstallProtocolInterface;
    PVOID                    UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL      HandleProtocol;
    PVOID                    _Reserved;
    PVOID                    RegisterProtocolNotify;
    EFI_LOCATE_HANDLE        LocateHandle;
    PVOID                    LocateDevicePath;
    PVOID                    InstallConfigurationTable;
    PVOID                    LoadImage;
    PVOID                    StartImage;
    PVOID                    Exit;
    PVOID                    UnloadImage;
    EFI_EXIT_BOOT_SERVICES  ExitBootServices;
    PVOID                    GetNextMonotonicCount;
    PVOID                    Stall;
    PVOID                    SetWatchdogTimer;
    PVOID                    ConnectController;
    PVOID                    DisconnectController;
    PVOID                    OpenProtocol;
    PVOID                    CloseProtocol;
    PVOID                    OpenProtocolInformation;
    PVOID                    ProtocolsPerHandle;
    PVOID                    LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL      LocateProtocol;
    PVOID                    InstallMultipleProtocolInterfaces;
    PVOID                    UninstallMultipleProtocolInterfaces;
    PVOID                    CalculateCrc32;
    PVOID                    CopyMem;
    PVOID                    SetMem;
    PVOID                    CreateEventEx;
};

#define EFI_FILE_MODE_READ   0x0000000000000001ULL

typedef EFI_STATUS(EFIAPI *EFI_FILE_OPEN)(
    IN EFI_FILE_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **NewHandle,
    IN CHAR16 *FileName,
    IN UINT64 OpenMode,
    IN UINT64 Attributes
    );

typedef EFI_STATUS(EFIAPI *EFI_FILE_CLOSE)(
    IN EFI_FILE_PROTOCOL *This
    );

typedef EFI_STATUS(EFIAPI *EFI_FILE_READ)(
    IN EFI_FILE_PROTOCOL *This,
    IN OUT UINTN *BufferSize,
    OUT PVOID Buffer
    );

struct _EFI_FILE_PROTOCOL
{
    UINT64          Revision;
    EFI_FILE_OPEN   Open;
    EFI_FILE_CLOSE  Close;
    PVOID           Delete;
    EFI_FILE_READ   Read;
    PVOID           Write;
    PVOID           GetPosition;
    PVOID           SetPosition;
    PVOID           GetInfo;
    PVOID           SetInfo;
    PVOID           Flush;
};

typedef EFI_STATUS(EFIAPI *EFI_SIMPLE_FS_OPEN_VOLUME)(
    IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **Root
    );

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
{
    UINT64                   Revision;
    EFI_SIMPLE_FS_OPEN_VOLUME OpenVolume;
};

struct _EFI_LOADED_IMAGE_PROTOCOL
{
    UINT32          Revision;
    EFI_HANDLE      ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE      DeviceHandle;
    PVOID           FilePath;
    PVOID           Reserved;
    UINT32          LoadOptionsSize;
    PVOID           LoadOptions;
    PVOID           ImageBase;
    UINT64          ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    PVOID           Unload;
};

struct _EFI_SYSTEM_TABLE
{
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    PVOID RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    PVOID ConfigurationTable;
};

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef enum _EFI_GRAPHICS_PIXEL_FORMAT
{
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct _EFI_PIXEL_BITMASK
{
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct _EFI_GRAPHICS_OUTPUT_MODE_INFORMATION
{
    UINT32                    Version;
    UINT32                    HorizontalResolution;
    UINT32                    VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK         PixelInformation;
    UINT32                    PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE
{
    UINT32                                 MaxMode;
    UINT32                                 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
    UINTN                                  SizeOfInfo;
    EFI_PHYSICAL_ADDRESS                   FrameBufferBase;
    UINTN                                  FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef EFI_STATUS(EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

typedef EFI_STATUS(EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber
    );

struct _EFI_GRAPHICS_OUTPUT_PROTOCOL
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE   SetMode;
    PVOID                                   Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE      *Mode;
};

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } }

#endif

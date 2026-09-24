#ifndef EFI_H
#define EFI_H
#include "../include/types.h"
#define EFIAPI __attribute__((ms_abi))
typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t UINTN;
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uint16_t CHAR16;
#define EFI_SUCCESS 0
#define EFI_LOAD_OPTION_NONE 0
typedef struct { uint32_t Type; EFI_PHYSICAL_ADDRESS PhysicalStart; uint64_t NumberOfPages; uint64_t Attribute; } EFI_MEMORY_DESCRIPTOR;
typedef struct { EFI_STATUS (EFIAPI *OutputString)(void*,CHAR16*); } EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct EFI_GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} EFI_GUID;

typedef struct EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE_PROTOCOL;
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

struct EFI_BOOT_SERVICES {
    EFI_STATUS (EFIAPI *RaiseTPL)(UINTN);
    EFI_STATUS (EFIAPI *RestoreTPL)(UINTN);
    EFI_STATUS (EFIAPI *AllocatePages)(UINT32, UINT32, UINTN, EFI_PHYSICAL_ADDRESS *);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *, EFI_MEMORY_DESCRIPTOR *, UINTN *, UINTN *, UINT32 *);
    EFI_STATUS (EFIAPI *AllocatePool)(UINT32, UINTN, void **);
    EFI_STATUS (EFIAPI *FreePool)(void *);
    EFI_STATUS (EFIAPI *CreateEvent)(void);
    EFI_STATUS (EFIAPI *SetTimer)(void);
    EFI_STATUS (EFIAPI *WaitForEvent)(void);
    EFI_STATUS (EFIAPI *SignalEvent)(void);
    EFI_STATUS (EFIAPI *CloseEvent)(void);
    EFI_STATUS (EFIAPI *CheckEvent)(void);
    EFI_STATUS (EFIAPI *InstallProtocolInterface)(void);
    EFI_STATUS (EFIAPI *ReinstallProtocolInterface)(void);
    EFI_STATUS (EFIAPI *UninstallProtocolInterface)(void);
    EFI_STATUS (EFIAPI *GetProtocol)(EFI_HANDLE, const EFI_GUID *, void **);
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, const EFI_GUID *, void **);
    EFI_STATUS (EFIAPI *RegisteredProtocolNotify)(void);
    EFI_STATUS (EFIAPI *LocateHandle)(void);
    EFI_STATUS (EFIAPI *LocateDevicePath)(void);
    EFI_STATUS (EFIAPI *InstallConfigurationTable)(void);
    EFI_STATUS (EFIAPI *LoadImage)(void);
    EFI_STATUS (EFIAPI *StartImage)(void);
    EFI_STATUS (EFIAPI *Exit)(void);
    EFI_STATUS (EFIAPI *UnloadImage)(void);
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);
    EFI_STATUS (EFIAPI *GetNextHighMonotonicCount)(void);
    EFI_STATUS (EFIAPI *Stall)(void);
    EFI_STATUS (EFIAPI *SetWatchdogTimer)(void);
    EFI_STATUS (EFIAPI *ConnectController)(void);
    EFI_STATUS (EFIAPI *DisconnectController)(void);
    EFI_STATUS (EFIAPI *OpenProtocol)(EFI_HANDLE, const EFI_GUID *, void **, EFI_HANDLE, EFI_HANDLE, UINT32);
};
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t Crc32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    void *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINT32 NrTables;
    UINT32 Reserved2;
    void *Tables;
} EFI_SYSTEM_TABLE;
typedef struct { uint32_t magic; uint32_t fb_addr; uint32_t fb_w; uint32_t fb_h; } efi_boot_info_t;
#define EFI_BOOT_MAGIC 0x4D594F53

struct EFI_LOADED_IMAGE_PROTOCOL {
    UINT64 Revision;
    EFI_HANDLE ParentHandle;
    EFI_HANDLE DeviceHandle;
    void *FilePath;
    void *Reserved;
    void *LoadOptions;
    UINTN LoadOptionsSize;
    EFI_PHYSICAL_ADDRESS ImageBase;
    UINTN ImageSize;
    void *ImageCodeType;
    void *ImageDataType;
    EFI_STATUS (EFIAPI *Unload)(EFI_HANDLE ImageHandle);
};

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    EFI_STATUS (EFIAPI *OpenVolume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This, EFI_FILE_PROTOCOL **Root);
};

struct EFI_FILE_PROTOCOL {
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **NewHandle, CHAR16 *FileName, UINT64 OpenMode, UINT64 Attributes);
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize, void *Buffer);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL *This);
};

#define EFI_FILE_MODE_READ 0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE 0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL
#endif

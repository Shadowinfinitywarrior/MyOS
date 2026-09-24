#include "efi.h"

#define EFI_OPEN_PROTOCOL_GET_PROTOCOL 0x00000002

static const EFI_GUID gEfiLoadedImageProtocolGuid = {0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};

static void efi_print(EFI_SYSTEM_TABLE *st, const CHAR16 *s) {
    if (st && st->ConOut && st->ConOut->OutputString) {
        st->ConOut->OutputString(st->ConOut, (CHAR16*)s);
    }
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *st) {
    EFI_BOOT_SERVICES *bs = st->BootServices;
    efi_print(st, u"MyOS UEFI Loader\r\n");

    EFI_LOADED_IMAGE_PROTOCOL *loaded = NULL;
    EFI_STATUS s = bs->OpenProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&loaded, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (s != EFI_SUCCESS) {
        efi_print(st, u"Failed to get LoadedImage\r\n");
        return s;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    s = bs->OpenProtocol(loaded->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&fs, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (s != EFI_SUCCESS) {
        efi_print(st, u"Failed to get FS\r\n");
        return s;
    }

    EFI_FILE_PROTOCOL *root = NULL;
    s = fs->OpenVolume(fs, &root);
    if (s != EFI_SUCCESS) {
        efi_print(st, u"Failed to open volume\r\n");
        return s;
    }

    EFI_FILE_PROTOCOL *file = NULL;
    s = root->Open(root, &file, (CHAR16*)u"\\kernel.bin", EFI_FILE_MODE_READ, 0);
    if (s != EFI_SUCCESS) {
        efi_print(st, u"Failed to open kernel.bin\r\n");
        return s;
    }

    void *kernel_buf = NULL;
    const UINTN MAX_KERNEL = 16 * 1024 * 1024;
    s = bs->AllocatePool(0, MAX_KERNEL, &kernel_buf);
    if (s != EFI_SUCCESS || !kernel_buf) {
        efi_print(st, u"Failed to allocate\r\n");
        return s;
    }

    UINTN read_size = MAX_KERNEL;
    s = file->Read(file, &read_size, kernel_buf);
    if (s != EFI_SUCCESS) {
        efi_print(st, u"Failed to read\r\n");
        bs->FreePool(kernel_buf);
        return s;
    }

    efi_print(st, u"Loaded kernel.bin\r\n");
    void *kernel_entry = (void*)0x100000;
    __builtin_memcpy(kernel_entry, kernel_buf, read_size);
    file->Close(file);
    root->Close(root);

    UINTN memMapSize = 0;
    UINTN mapKey = 0;
    UINTN descSize = 0;
    UINT32 descVersion = 0;
    bs->GetMemoryMap(&memMapSize, NULL, &mapKey, &descSize, &descVersion);
    static EFI_MEMORY_DESCRIPTOR memMap[1024];
    memMapSize = sizeof(memMap);
    s = bs->GetMemoryMap(&memMapSize, memMap, &mapKey, &descSize, &descVersion);
    if (s == EFI_SUCCESS) {
        efi_print(st, u"ExitBootServices done, jumping to kernel...\r\n");
        bs->FreePool(kernel_buf);
        bs->ExitBootServices(ImageHandle, mapKey);
    }

    void (*kernel_start)(void) = (void (*)(void))0x100000;
    kernel_start();

    for (;;) {
        __asm__ volatile ("hlt");
    }
    return EFI_SUCCESS;
}

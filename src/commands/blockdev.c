#include "blockdev.h"

EFI_STATUS ListDisks(SHELL_CONTEXT *Ctx)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    UINTN DiskNum = 0;
    CHAR16 line[128];

    Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateHandleBuffer, 5, ByProtocol, &BlockIoProtocol, NULL,
                               &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status))
    {
        EFISPrint(Ctx->ST->ConOut, L"No Block Devices Found\n\r");
        return Status;
    }

    for (Index = 0; Index < HandleCount; Index++)
    {
        EFI_BLOCK_IO *BlockIo;

        Status = uefi_call_wrapper(
            Ctx->ST->BootServices->HandleProtocol, 3,
            HandleBuffer[Index], &BlockIoProtocol, (VOID **)&BlockIo);
        if (EFI_ERROR(Status))
            continue;

        if (BlockIo->Media->LogicalPartition)
            continue;

        UINT64 SizeMB = ((BlockIo->Media->LastBlock + 1) * (UINT64)BlockIo->Media->BlockSize) / (1024 * 1024);
        UINT64 SizeGB = ((BlockIo->Media->LastBlock + 1) * (UINT64)BlockIo->Media->BlockSize) / (1024 * 1024 * 1024);
        SPrint(line, sizeof(line),
               L"disk%d  BlockSize=%d  LastBlock=%ld  SizeMB=%ld SizeGB=%ld  Removable=%s\r\n",
               DiskNum, BlockIo->Media->BlockSize, BlockIo->Media->LastBlock, SizeMB, SizeGB,
               BlockIo->Media->RemovableMedia ? L"Yes" : L"No");
        EFISPrint(Ctx->ST->ConOut, line);

        DiskNum++;
    }

    if (DiskNum == 0)
        EFISPrint(Ctx->ST->ConOut, L"No physical disks found\r\n");

    uefi_call_wrapper(Ctx->ST->BootServices->FreePool, 1, HandleBuffer);
    return EFI_SUCCESS;
}
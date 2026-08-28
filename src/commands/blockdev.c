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

    FreePool(Ctx->ST->BootServices, HandleBuffer);
    return EFI_SUCCESS;
}

EFI_STATUS FindDiskByIndex(SHELL_CONTEXT *Ctx, UINTN TargetIndex, EFI_BLOCK_IO **OutBlockIo, EFI_HANDLE *OutHandle)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    UINTN DiskNum = 0;

    Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateHandleBuffer, 5, ByProtocol, &BlockIoProtocol, NULL,
                               &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status))
    {
        EFISPrint(Ctx->ST->ConOut, L"No Block Devices Found\n\r");
        return Status;
    }

    for (Index = 0; Index < HandleCount; Index++)
    {

        Status = uefi_call_wrapper(
            Ctx->ST->BootServices->HandleProtocol, 3,
            HandleBuffer[Index], &BlockIoProtocol, (VOID *)OutBlockIo);

        if (EFI_ERROR(Status))
            continue;

        if ((*OutBlockIo)->Media->LogicalPartition)
            continue;

        if (DiskNum == TargetIndex)
        {
            *OutHandle = HandleBuffer[Index];
            FreePool(Ctx->ST->BootServices, HandleBuffer);
            return EFI_SUCCESS;
        }
        DiskNum++;
    }
    CHAR16 line[128];

    SPrint(line, sizeof(line), L"No physical disks found with index: %ld\r\n", TargetIndex);
    EFISPrint(Ctx->ST->ConOut, line);

    FreePool(Ctx->ST->BootServices, HandleBuffer);
    return EFI_NOT_FOUND;
}

BOOLEAN IsPartitionOfDisk(EFI_DEVICE_PATH_PROTOCOL *DiskPath, EFI_DEVICE_PATH_PROTOCOL *CandidatePath)
{
    EFI_DEVICE_PATH_PROTOCOL *Disk = DiskPath;
    EFI_DEVICE_PATH_PROTOCOL *Candidate = CandidatePath;

    while (!IsDevicePathEnd(Disk))
    {
        if (IsDevicePathEnd(Candidate) && !IsDevicePathEnd(Disk))
            return FALSE;

        if (DevicePathNodeLength(Disk) != DevicePathNodeLength(Candidate))
            return FALSE;

        if (CompareMem(Disk, Candidate, DevicePathNodeLength(Disk)) != 0)
            return FALSE;

        Disk = NextDevicePathNode(Disk);
        Candidate = NextDevicePathNode(Candidate);
    }

    if (IsDevicePathEnd(Candidate) && IsDevicePathEnd(Disk))
        return FALSE;
    else
        return TRUE;
}

EFI_STATUS ListPartitions(SHELL_CONTEXT *Ctx, UINTN DiskNumber)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO *TargetDisk;
    EFI_HANDLE *HandleBuffer;
    EFI_HANDLE DiskHandle;
    EFI_DEVICE_PATH_PROTOCOL *DiskPath;
    UINTN HandleCount;
    UINTN Index;
    UINTN PartNum = 0;
    CHAR16 line[128];

    Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateHandleBuffer, 5, ByProtocol, &BlockIoProtocol, NULL,
                               &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status))
    {
        EFISPrint(Ctx->ST->ConOut, L"No Block Devices Found\n\r");
        return Status;
    }

    Status = FindDiskByIndex(Ctx, DiskNumber, &TargetDisk, &DiskHandle);

    if (EFI_ERROR(Status))
    {
        FreePool(Ctx->ST->BootServices, HandleBuffer);
        return Status;
    }

    Status = uefi_call_wrapper(
        Ctx->ST->BootServices->HandleProtocol, 3,
        DiskHandle, &DevicePathProtocol, (VOID **)&DiskPath);

    if (EFI_ERROR(Status))
    {
        FreePool(Ctx->ST->BootServices, HandleBuffer);
        return Status;
    }

    for (Index = 0; Index < HandleCount; Index++)
    {
        EFI_BLOCK_IO *BlockIo;
        EFI_DEVICE_PATH_PROTOCOL *PartPath;

        Status = uefi_call_wrapper(
            Ctx->ST->BootServices->HandleProtocol, 3,
            HandleBuffer[Index], &BlockIoProtocol, (VOID **)&BlockIo);
        if (EFI_ERROR(Status))
            continue;

        if (!BlockIo->Media->LogicalPartition)
            continue;

        Status = uefi_call_wrapper(
            Ctx->ST->BootServices->HandleProtocol, 3,
            HandleBuffer[Index], &DevicePathProtocol, (VOID **)&PartPath);

        if (EFI_ERROR(Status))
            continue;

        if (!IsPartitionOfDisk(DiskPath, PartPath))
            continue;

        UINT64 SizeMB = ((BlockIo->Media->LastBlock + 1) * (UINT64)BlockIo->Media->BlockSize) / (1024 * 1024);
        UINT64 SizeGB = ((BlockIo->Media->LastBlock + 1) * (UINT64)BlockIo->Media->BlockSize) / (1024 * 1024 * 1024);
        SPrint(line, sizeof(line),
               L"disk%ldpart%ld  BlockSize=%d  LastBlock=%ld  SizeMB=%ld SizeGB=%ld  Removable=%s\r\n",
               DiskNumber, PartNum, BlockIo->Media->BlockSize, BlockIo->Media->LastBlock, SizeMB, SizeGB,
               BlockIo->Media->RemovableMedia ? L"Yes" : L"No");
        EFISPrint(Ctx->ST->ConOut, line);

        PartNum++;
    }

    FreePool(Ctx->ST->BootServices, HandleBuffer);
    return EFI_SUCCESS;
}
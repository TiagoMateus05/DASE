#include "blockdev.h"
#include "../dshell/shellcontext.h"


// PCI config space offsets (PCI Local Bus Spec, Type 0 header)
#define PCI_CFG_ID_REG 0x00     // [15:0] VendorID, [31:16] DeviceID
#define PCI_CFG_CLASS_REG 0x08  // [7:0] Rev, [15:8] ProgIF, [23:16] Sub, [31:24] Class
#define PCI_CFG_SUBSYS_REG 0x2C // [15:0] SubsysVendorID, [31:16] SubsysID

// PCI class codes relevant to storage
#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_CLASS_SERIAL_BUS 0x0C

#define PCI_SUB_SCSI 0x00
#define PCI_SUB_IDE 0x01
#define PCI_SUB_RAID 0x04
#define PCI_SUB_SATA 0x06
#define PCI_SUB_SAS 0x07
#define PCI_SUB_NVM 0x08

#define PCI_SUB_USB 0x03 // within class 0x0C

EFI_STATUS ListDisks(SHELL_CONTEXT *Ctx)
{
    CHAR16 line[160];
    EFI_STATUS Status;

    Status = BuildDiskTable(Ctx);
    if (EFI_ERROR(Status))
    {
        EFISPrint(Ctx->ST->ConOut, L"No Block Devices Found\r\n");
        return Status;
    }

    if (Ctx->DiskCount == 0)
    {
        EFISPrint(Ctx->ST->ConOut, L"No physical disks found\r\n");
        return EFI_SUCCESS;
    }

    for (UINTN i = 0; i < Ctx->DiskCount; i++)
    {
        DISK *D = &Ctx->Disks[i];
        UINT64 Bytes = (D->LastBlock + 1) * (UINT64)D->BlockSize;

        SPrint(line, sizeof(line),
               L"disk%d  BlockSize=%d  LastBlock=%ld  SizeMB=%ld  SizeGB=%ld  Removable=%s\r\n",
               (UINTN)D->Index, (UINTN)D->BlockSize, D->LastBlock,
               Bytes / (1024 * 1024), Bytes / (1024 * 1024 * 1024),
               D->RemovableMedia ? L"Yes" : L"No");
        EFISPrint(Ctx->ST->ConOut, line);
    }
    return EFI_SUCCESS;
}

EFI_STATUS FindDiskByIndex(SHELL_CONTEXT *Ctx, UINTN TargetIndex, DISK **OutDisk)
{
    CHAR16 line[80];

    if (Ctx->DiskCount == 0)
    {
        EFISPrint(Ctx->ST->ConOut, L"Disk table not built\r\n");
        return EFI_NOT_READY;
    }

    if (TargetIndex >= Ctx->DiskCount)
    {
        SPrint(line, sizeof(line),
               L"No physical disk with index: %ld\r\n", TargetIndex);
        EFISPrint(Ctx->ST->ConOut, line);
        return EFI_NOT_FOUND;
    }

    *OutDisk = &Ctx->Disks[TargetIndex];
    return EFI_SUCCESS;
}

BOOLEAN IsPartitionOfDisk(EFI_DEVICE_PATH_PROTOCOL *DiskPath,
                          EFI_DEVICE_PATH_PROTOCOL *CandidatePath)
{
    EFI_DEVICE_PATH_PROTOCOL *Disk = DiskPath;
    EFI_DEVICE_PATH_PROTOCOL *Cand = CandidatePath;

    if (Disk == NULL || Cand == NULL)
        return FALSE;

    while (!IsDevicePathEnd(Disk))
    {
        if (IsDevicePathEnd(Cand))
            return FALSE;
        if (DevicePathNodeLength(Disk) != DevicePathNodeLength(Cand))
            return FALSE;
        if (CompareMem(Disk, Cand, DevicePathNodeLength(Disk)) != 0)
            return FALSE;

        Disk = NextDevicePathNode(Disk);
        Cand = NextDevicePathNode(Cand);
    }

    if (IsDevicePathEnd(Cand))
        return FALSE;

    if (DevicePathType(Cand) != MEDIA_DEVICE_PATH ||
        DevicePathSubType(Cand) != MEDIA_HARDDRIVE_DP)
        return FALSE;

    return TRUE;

}

EFI_STATUS ListPartitions(SHELL_CONTEXT *Ctx, UINTN DiskNumber)
{
    EFI_STATUS Status;
    DISK *Target;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    UINTN PartNum = 0;
    CHAR16 line[128];

    Status = BuildDiskTable(Ctx);
    if (EFI_ERROR(Status))
        return Status;

    Status = FindDiskByIndex(Ctx, DiskNumber, &Target);
    if (EFI_ERROR(Status))
        return Status;

    if (Target->DevicePath == NULL)
    {
        EFISPrint(Ctx->ST->ConOut, L"Disk has no device path\r\n");
        return EFI_NOT_FOUND;
    }

    Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateHandleBuffer, 5,
                               ByProtocol, &BlockIoProtocol, NULL,
                               &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status))
    {
        EFISPrint(Ctx->ST->ConOut, L"No Block Devices Found\r\n");
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

        if (!IsPartitionOfDisk(Target->DevicePath, PartPath))
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

    FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
    return EFI_SUCCESS;
}

EFI_STATUS ListDiskInfo(SHELL_CONTEXT *Ctx)
{
    CHAR16 line[200];
    EFI_STATUS Status;

    Status = BuildDiskTable(Ctx);
    if (EFI_ERROR(Status))
    {
        EFISPrint(Ctx->ST->ConOut, L"No Block Devices Found\r\n");
        return Status;
    }

    for (UINTN i = 0; i < Ctx->DiskCount; i++)
    {
        DISK *CurrentDisk = &Ctx->Disks[i];

        SPrint(line, sizeof(line),
               L"disk%d  %-14s %04x:%04x  sub %04x:%04x  Class=%02x Sub=%02x PI=%02x  [%s]\r\n",
               (UINTN)CurrentDisk->Index, PciVendorName(CurrentDisk->PciVendorId),
               (UINTN)CurrentDisk->PciVendorId, (UINTN)CurrentDisk->PciDeviceId,
               (UINTN)CurrentDisk->PciSubsysVendorId, (UINTN)CurrentDisk->PciSubsysId,
               (UINTN)CurrentDisk->PciClass, (UINTN)CurrentDisk->PciSubclass, (UINTN)CurrentDisk->PciProgIF,
               TransportName(CurrentDisk->Transport));
        EFISPrint(Ctx->ST->ConOut, line);
    }

    if (Ctx->DiskCount == 0)
        EFISPrint(Ctx->ST->ConOut, L"No physical disks found\r\n");

    return EFI_SUCCESS;
}

EFI_STATUS BuildDiskTable(SHELL_CONTEXT *Ctx)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    UINTN DiskNum = 0;

    // Full rebuild every time: a stale entry means wipe could target the
    // wrong device after a hot-plug.
    ZeroMem(Ctx->Disks, sizeof(Ctx->Disks));
    Ctx->DiskCount = 0;

    Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateHandleBuffer, 5,
                               ByProtocol, &BlockIoProtocol, NULL,
                               &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status))
        return Status;

    for (Index = 0; Index < HandleCount; Index++)
    {
        EFI_BLOCK_IO *BlockIo;
        EFI_DEVICE_PATH_PROTOCOL *DiskPath, *WalkPath;
        EFI_HANDLE PciHandle;
        EFI_PCI_IO_PROTOCOL *PciIo;
        UINT32 IdReg = 0, ClassReg = 0, SubsysReg = 0;
        DISK *CurrentDisk;

        if (DiskNum >= MAX_DISKS)
            break;

        Status = uefi_call_wrapper(Ctx->ST->BootServices->HandleProtocol, 3,
                                   HandleBuffer[Index], &BlockIoProtocol,
                                   (VOID **)&BlockIo);
        if (EFI_ERROR(Status))
            continue;

        if (BlockIo->Media->LogicalPartition)
            continue;

        CurrentDisk = &Ctx->Disks[DiskNum];

        CurrentDisk->Index = DiskNum;
        CurrentDisk->Handle = HandleBuffer[Index];
        CurrentDisk->BlockIo = BlockIo;

        CurrentDisk->MediaId = BlockIo->Media->MediaId;
        CurrentDisk->BlockSize = BlockIo->Media->BlockSize;
        CurrentDisk->IoAlign = BlockIo->Media->IoAlign;
        CurrentDisk->LastBlock = BlockIo->Media->LastBlock;
        CurrentDisk->MediaPresent = BlockIo->Media->MediaPresent;
        CurrentDisk->ReadOnly = BlockIo->Media->ReadOnly;
        CurrentDisk->RemovableMedia = BlockIo->Media->RemovableMedia;

        CurrentDisk->Transport = DISK_TRANSPORT_UNKNOWN;

        // --- resolve the PCI controller behind this disk ---
        Status = uefi_call_wrapper(Ctx->ST->BootServices->HandleProtocol, 3,
                                   HandleBuffer[Index], &DevicePathProtocol,
                                   (VOID **)&DiskPath);
        if (EFI_ERROR(Status))
        {
            DiskNum++;
            continue;
        }

        CurrentDisk->DevicePath = DiskPath;

        WalkPath = DiskPath;
        Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateDevicePath, 3,
                                   &PciIoProtocol, &WalkPath, &PciHandle);
        if (EFI_ERROR(Status))
        {
            DiskNum++;
            continue;
        }

        Status = uefi_call_wrapper(Ctx->ST->BootServices->HandleProtocol, 3,
                                   PciHandle, &PciIoProtocol, (VOID **)&PciIo);
        if (EFI_ERROR(Status))
        {
            DiskNum++;
            continue;
        }

        // Count is in units of Width, so one 32-bit read == Count 1.
        uefi_call_wrapper(PciIo->Pci.Read, 5, PciIo,
                          EfiPciIoWidthUint32, PCI_CFG_ID_REG, 1, &IdReg);
        uefi_call_wrapper(PciIo->Pci.Read, 5, PciIo,
                          EfiPciIoWidthUint32, PCI_CFG_CLASS_REG, 1, &ClassReg);
        uefi_call_wrapper(PciIo->Pci.Read, 5, PciIo,
                          EfiPciIoWidthUint32, PCI_CFG_SUBSYS_REG, 1, &SubsysReg);

        CurrentDisk->PciVendorId = (UINT16)(IdReg & 0xFFFF);
        CurrentDisk->PciDeviceId = (UINT16)((IdReg >> 16) & 0xFFFF);
        CurrentDisk->PciSubsysVendorId = (UINT16)(SubsysReg & 0xFFFF);
        CurrentDisk->PciSubsysId = (UINT16)((SubsysReg >> 16) & 0xFFFF);
        CurrentDisk->PciProgIF = (UINT8)((ClassReg >> 8) & 0xFF);
        CurrentDisk->PciSubclass = (UINT8)((ClassReg >> 16) & 0xFF);
        CurrentDisk->PciClass = (UINT8)((ClassReg >> 24) & 0xFF);

        CurrentDisk->Transport = ClassifyTransport(CurrentDisk->PciClass, CurrentDisk->PciSubclass, CurrentDisk->PciProgIF);

        DiskNum++;
    }

    Ctx->DiskCount = DiskNum;
    FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
    return EFI_SUCCESS;
}
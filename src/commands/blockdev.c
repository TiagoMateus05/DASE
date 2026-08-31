#include "blockdev.h"

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

    FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
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
            FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
            return EFI_SUCCESS;
        }
        DiskNum++;
    }
    CHAR16 line[128];

    SPrint(line, sizeof(line), L"No physical disks found with index: %ld\r\n", TargetIndex);
    EFISPrint(Ctx->ST->ConOut, line);

    FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
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
        FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
        return Status;
    }

    Status = uefi_call_wrapper(
        Ctx->ST->BootServices->HandleProtocol, 3,
        DiskHandle, &DevicePathProtocol, (VOID **)&DiskPath);

    if (EFI_ERROR(Status))
    {
        FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
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

    FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
    return EFI_SUCCESS;
}

STATIC CONST CHAR16 *ClassifyController(UINT8 Class, UINT8 Sub, UINT8 ProgIF)
{
    if (Class == PCI_CLASS_MASS_STORAGE)
    {
        switch (Sub)
        {
            case PCI_SUB_SATA:
                return (ProgIF == 0x01) ? L"SATA/AHCI" : L"SATA";
            case PCI_SUB_NVM:
                return (ProgIF == 0x02) ? L"NVMe" : L"NVM";
            case PCI_SUB_IDE:
                return L"IDE";
            case PCI_SUB_RAID:
                return L"RAID  <-- ENABLE IT/HBA MODE";
            case PCI_SUB_SAS:
                return L"SAS/HBA";
            case PCI_SUB_SCSI:
                return L"SCSI/HBA";
            default:
                return L"Storage (unknown)";
        }
    }
    if (Class == PCI_CLASS_SERIAL_BUS && Sub == PCI_SUB_USB)
        return L"USB bridge";

    return L"Unknown";
}

EFI_STATUS ListDiskInfo(SHELL_CONTEXT *Ctx)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    UINTN DiskNum = 0;
    CHAR16 line[160];

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
        EFI_DEVICE_PATH_PROTOCOL *DiskPath, *WalkPath;
        EFI_HANDLE PciHandle;
        EFI_PCI_IO_PROTOCOL *PciIo;
        UINT32 IdReg = 0, ClassReg = 0, SubsysReg = 0;
        UINT8 Class, Sub, ProgIF;

        Status = uefi_call_wrapper(Ctx->ST->BootServices->HandleProtocol, 3,
                                   HandleBuffer[Index], &BlockIoProtocol,
                                   (VOID **)&BlockIo);
        if (EFI_ERROR(Status))
            continue;

        if (BlockIo->Media->LogicalPartition)
            continue;

        // --- resolve the PCI controller behind this disk ---
        Status = uefi_call_wrapper(Ctx->ST->BootServices->HandleProtocol, 3,
                                   HandleBuffer[Index], &DevicePathProtocol,
                                   (VOID **)&DiskPath);
        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line),
                   L"disk%d  <no device path>  UNCLASSIFIED\r\n", DiskNum);
            EFISPrint(Ctx->ST->ConOut, line);
            DiskNum++;
            continue;
        }

        WalkPath = DiskPath;
        Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateDevicePath, 3,
                                   &PciIoProtocol, &WalkPath, &PciHandle);
        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line),
                   L"disk%d  <no PCI ancestor>  UNCLASSIFIED\r\n", DiskNum);
            EFISPrint(Ctx->ST->ConOut, line);
            DiskNum++;
            continue;
        }

        Status = uefi_call_wrapper(Ctx->ST->BootServices->HandleProtocol, 3,
                                   PciHandle, &PciIoProtocol, (VOID **)&PciIo);
        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line),
                   L"disk%d  <no PciIo on ancestor>  UNCLASSIFIED\r\n", DiskNum);
            EFISPrint(Ctx->ST->ConOut, line);
            DiskNum++;
            continue;
        }

        uefi_call_wrapper(PciIo->Pci.Read, 5, PciIo,
                          EfiPciIoWidthUint32, PCI_CFG_ID_REG, 1, &IdReg);
        uefi_call_wrapper(PciIo->Pci.Read, 5, PciIo,
                          EfiPciIoWidthUint32, PCI_CFG_CLASS_REG, 1, &ClassReg);
        uefi_call_wrapper(PciIo->Pci.Read, 5, PciIo,
                          EfiPciIoWidthUint32, PCI_CFG_SUBSYS_REG, 1, &SubsysReg);

        ProgIF = (UINT8)((ClassReg >> 8) & 0xFF);
        Sub = (UINT8)((ClassReg >> 16) & 0xFF);
        Class = (UINT8)((ClassReg >> 24) & 0xFF);

        UINT16 Vid = (UINT16)(IdReg & 0xFFFF);
        UINT16 Did = (UINT16)((IdReg >> 16) & 0xFFFF);

        SPrint(line, sizeof(line),
               L"disk%d  %-14s %04x:%04x  Class=%02x Sub=%02x PI=%02x  [%s]\r\n",
               DiskNum, PciVendorName(Vid), (UINTN)Vid, (UINTN)Did,
               (UINTN)Class, (UINTN)Sub, (UINTN)ProgIF,
               ClassifyController(Class, Sub, ProgIF));
        EFISPrint(Ctx->ST->ConOut, line);

        DiskNum++;
    }

    if (DiskNum == 0)
        EFISPrint(Ctx->ST->ConOut, L"No physical disks found\r\n");

    FreePoolBS(Ctx->ST->BootServices, HandleBuffer);
    return EFI_SUCCESS;
}
#ifndef _BLOCKDEV_H_
#define _BLOCKDEV_H_

#define MAX_DISKS 64

#include <efi.h>
#include <efilib.h>

#include "../includes/wrapper.h"
#include "../io/vendors.h"

struct _EFI_ATA_PASS_THRU_PROTOCOL;

typedef enum
{
    MEDIA_UNKNOWN = 0,
    MEDIA_ROTATIONAL,
    MEDIA_SOLID_STATE,
} MEDIA_TYPE;

typedef struct
{
    // --- identity within this session ---
    UINTN Index;

    EFI_HANDLE Handle;
    EFI_BLOCK_IO *BlockIo;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINT32 MediaId;
    UINT32 BlockSize;
    UINT32 IoAlign;
    EFI_LBA LastBlock;
    BOOLEAN MediaPresent;
    BOOLEAN ReadOnly;
    BOOLEAN RemovableMedia;

    // --- controller (from PCI config space) ---
    UINT16 PciVendorId;
    UINT16 PciDeviceId;
    UINT16 PciSubsysVendorId;
    UINT16 PciSubsysId;
    UINT8 PciClass;
    UINT8 PciSubclass;
    UINT8 PciProgIF;
    DISK_TRANSPORT Transport;

    // --- drive identity (from IDENTIFY DEVICE / Identify Controller) ---
    CHAR16 Model[41];      // ATA words 27-46, byte-pair swapped
    CHAR16 Serial[21];     // ATA words 10-19
    CHAR16 FirmwareRev[9]; // ATA words 23-26
    MEDIA_TYPE Media;
    UINT16 RotationRate;

    struct _EFI_ATA_PASS_THRU_PROTOCOL *AtaPassThru; // NULL if not ATA-addressable
    UINT16 AtaPort;
    UINT16 AtaPmPort;
    EFI_STATUS AtaStatus;

    // --- capability flags (drive the algorithm choice) ---
    BOOLEAN SecuritySupported;
    BOOLEAN SecurityFrozen;
    BOOLEAN EnhancedEraseSupported;
    BOOLEAN SanitizeSupported;

    // --- safety and selection state ---
    BOOLEAN IsBootDevice;
} DISK;

struct _SHELL_CONTEXT;
typedef struct _SHELL_CONTEXT SHELL_CONTEXT;

EFI_STATUS ListDisks(SHELL_CONTEXT *Ctx);
EFI_STATUS FindDiskByIndex(SHELL_CONTEXT *Ctx, UINTN TargetIndex, DISK **OutDisk);
BOOLEAN IsPartitionOfDisk(EFI_DEVICE_PATH_PROTOCOL *DiskPath, EFI_DEVICE_PATH_PROTOCOL *CandidatePath);
EFI_STATUS ListPartitions(SHELL_CONTEXT *Ctx, UINTN DiskNumber);
EFI_STATUS ListDiskInfo(SHELL_CONTEXT *Ctx);
EFI_STATUS BuildDiskTable(SHELL_CONTEXT *Ctx);
EFI_STATUS ListAtaInfo(SHELL_CONTEXT *Ctx);

#endif
#ifndef _BLOCKDEV_H_
#define _BLOCKDEV_H_

#include <efi.h>
#include <efilib.h>

#include "../includes/wrapper.h"
#include "../io/vendors.h"

typedef struct
{
    EFI_SYSTEM_TABLE *ST;
    UINTN Placeholder;
} SHELL_CONTEXT;

EFI_STATUS ListDisks(SHELL_CONTEXT *Ctx);
EFI_STATUS FindDiskByIndex(SHELL_CONTEXT *Ctx, UINTN TargetIndex, EFI_BLOCK_IO **OutBlockIom, EFI_HANDLE *OutHandle);
BOOLEAN IsPartitionOfDisk(EFI_DEVICE_PATH_PROTOCOL *DiskPath, EFI_DEVICE_PATH_PROTOCOL *CandidatePath);
EFI_STATUS ListPartitions(SHELL_CONTEXT *Ctx, UINTN DiskNumber);
EFI_STATUS ListDiskInfo(SHELL_CONTEXT *Ctx);

#endif
#ifndef _DISKIO_H_
#define _DISKIO_H_

#include <efi.h>
#include <efilib.h>
#include "../includes/wrapper.h"

typedef struct
{
    EFI_BLOCK_IO *BlockIo;
    UINT64 StartLba;
    UINT32 BlockSize;
} BLOCK_DEVICE;

EFI_STATUS BlockDeviceInit(BLOCK_DEVICE *Dev, EFI_BLOCK_IO *BlockIo, UINT64 StartLba);
EFI_STATUS BlockDeviceRead(BLOCK_DEVICE *Dev, UINT64 Lba, UINTN NumBlocks, UINT8 *Buffer);


#endif
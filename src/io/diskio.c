#include "diskio.h"

CHAR16 NibbleToHex(UINT8 nibble)
{
    if (nibble < 10)
        return (CHAR16)(nibble + L'0');
    else
        return (CHAR16)(nibble - 10 + L'A');
}

VOID ByteToHexChars(UINT8 byte, CHAR16 *out)
{
    out[0] = NibbleToHex((byte >> 4) & 0x0F);
    out[1] = NibbleToHex(byte & 0x0F);
}

EFI_STATUS BlockDeviceInit(BLOCK_DEVICE *Dev, EFI_BLOCK_IO *BlockIo, UINT64 StartLba)
{
    if (!Dev || !BlockIo || !BlockIo->Media)
        return EFI_INVALID_PARAMETER;

    Dev->BlockIo = BlockIo;
    Dev->StartLba = StartLba;
    Dev->BlockSize = BlockIo->Media->BlockSize;

    return EFI_SUCCESS;
}

EFI_STATUS BlockDeviceRead(BLOCK_DEVICE *Dev, UINT64 Lba, UINTN NumBlocks, UINT8 *Buffer)
{
    if (!Dev || !Dev->BlockIo || !Buffer || NumBlocks == 0)
        return EFI_INVALID_PARAMETER;

    UINT64 AbsoluteLba = Dev->StartLba + Lba;
    UINTN BufferSize = NumBlocks * Dev->BlockSize;

    return uefi_call_wrapper(
        Dev->BlockIo->ReadBlocks, 5,
        Dev->BlockIo,
        Dev->BlockIo->Media->MediaId,
        AbsoluteLba,
        BufferSize,
        Buffer);
}

INTN DiskIoReadCallback(VOID *Context, UINT64 Lba, UINTN NumBlocks, UINT8 *Buffer)
{
    BLOCK_DEVICE *Dev = (BLOCK_DEVICE *)Context;
    EFI_STATUS Status = BlockDeviceRead(Dev, Lba, NumBlocks, Buffer);
    return EFI_ERROR(Status) ? -1 : 0;
}
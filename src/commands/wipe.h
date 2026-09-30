#ifndef _WIPE_H_
#define _WIPE_H_

#include <efi.h>
#include <efilib.h>

#include "../dshell/shell.h"
#include "../dshell/shellcontext.h"
#include "../includes/wrapper.h"
#include "../io/ata.h"
#include "../io/nvm.h"
#include "blockdev.h"

typedef enum
{
    WIPE_METHOD_NONE = 0,
    WIPE_METHOD_OVERWRITE,
    WIPE_METHOD_ATA_SECURITY_ERASE,
    WIPE_METHOD_ATA_SANITIZE,
    WIPE_METHOD_NVME_SANITIZE,
    WIPE_METHOD_NVME_FORMAT,
} WIPE_METHOD;

typedef struct
{
    DISK *Disk;
    EFI_BLOCK_IO2_TOKEN Token;
    VOID *Buffer;
    UINT64 TotalBlocks;
    UINT64 BlocksIssued;
    UINT64 BlocksDone;
    UINTN ChunkBlocks;
    UINTN Row;
    BOOLEAN InFlight;
    BOOLEAN Done;
    EFI_STATUS Result;
    UINTN LastPct;
} WIPE_JOB;

EFI_STATUS   WipeCheckTarget(SHELL_CONTEXT *Ctx, DISK *Disk);
EFI_STATUS   WipeCheckTargetVerbose(SHELL_CONTEXT *Ctx, DISK *Disk);
WIPE_METHOD  WipeSelectMethod(DISK *Disk);
CONST CHAR16 *WipeMethodName(WIPE_METHOD M);
EFI_STATUS   WipeOverwrite(SHELL_CONTEXT *Ctx, DISK *Disk, UINT8 Pattern);
EFI_STATUS   WipeVerification(SHELL_CONTEXT *Ctx, DISK *Disk, UINT8 Pattern);
EFI_STATUS   WipeExecuteSelection(SHELL_CONTEXT *Ctx);

#endif
#ifndef _WIPE_H_
#define _WIPE_H_

#include <efi.h>
#include <efilib.h>

#include "../includes/wrapper.h"
#include "../dshell/shellcontext.h"
#include "../dshell/shell.h"
#include "../io/ata.h"
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

EFI_STATUS WipeCheckTarget(SHELL_CONTEXT *Ctx, DISK *Disk);
EFI_STATUS WipeCheckTargetVerbose(SHELL_CONTEXT *Ctx, DISK *Disk);
WIPE_METHOD WipeSelectMethod(DISK *D);
CONST CHAR16 *WipeMethodName(WIPE_METHOD M);
BOOLEAN WipeConfirm(SHELL_CONTEXT *Ctx, DISK *Disk);
EFI_STATUS WipeOverwrite(SHELL_CONTEXT *Ctx, DISK *Disk, UINT8 Pattern);
EFI_STATUS WipeCheckTarget(SHELL_CONTEXT *Ctx, DISK *Disk);
EFI_STATUS WipeCheckTargetVerbose(SHELL_CONTEXT *Ctx, DISK *Disk);
BOOLEAN WipeConfirm(SHELL_CONTEXT *Ctx, DISK *Disk);

#endif
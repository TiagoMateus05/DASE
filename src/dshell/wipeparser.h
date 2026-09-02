#ifndef _WIPEPARSER_H_
#define _WIPEPARSER_H_

#include "../commands/blockdev.h"
#include <efi.h>

typedef struct
{
    UINTN Index;
    CHAR16 Serial[21];
} WIPE_TARGET;

typedef struct
{

    WIPE_TARGET Targets[MAX_DISKS];
    UINTN TargetCount;

    BOOLEAN List;
    BOOLEAN All;
    BOOLEAN Execute;
    BOOLEAN Interactive;
} WIPE_ARGS;

struct _SHELL_CONTEXT;

EFI_STATUS ListSelectedDisks(struct _SHELL_CONTEXT *Ctx);
EFI_STATUS ParseWipeArgs(struct _SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
VOID WipeRevalidateSelection(struct _SHELL_CONTEXT *Ctx);
EFI_STATUS WipeSelectAdd(struct _SHELL_CONTEXT *Ctx, UINTN DiskIndex);
EFI_STATUS WipeSelectRemove(struct _SHELL_CONTEXT *Ctx, UINTN DiskIndex);
VOID WipeSelectClear(struct _SHELL_CONTEXT *Ctx);
BOOLEAN WipeIsSelected(struct _SHELL_CONTEXT *Ctx, UINTN DiskIndex);

#endif
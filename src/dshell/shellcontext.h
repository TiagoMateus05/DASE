#ifndef _SHELL_TYPES_H_
#define _SHELL_TYPES_H_

#include <efi.h>

#include "../commands/blockdev.h"
#include "wipeparser.h"

typedef struct _SHELL_CONTEXT
{
    EFI_SYSTEM_TABLE *ST;
    EFI_HANDLE ImageHandle;
    DISK Disks[MAX_DISKS];
    UINTN DiskCount;
    WIPE_ARGS WipeArgs;
} SHELL_CONTEXT;

#endif

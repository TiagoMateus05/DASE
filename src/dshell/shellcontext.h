#ifndef _SHELL_TYPES_H_
#define _SHELL_TYPES_H_

#include <efi.h>

#include "../commands/blockdev.h"

typedef struct _SHELL_CONTEXT
{
    EFI_SYSTEM_TABLE *ST;
    DISK              Disks[MAX_DISKS];
    UINTN             DiskCount;
} SHELL_CONTEXT;

#endif

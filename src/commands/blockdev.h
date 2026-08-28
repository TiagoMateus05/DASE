#ifndef _BLOCKDEV_H_
#define _BLOCKDEV_H_

#include <efi.h>
#include <efilib.h>

#include "../includes/wrapper.h"

typedef struct
{
    EFI_SYSTEM_TABLE *ST;
    UINTN Placeholder;
} SHELL_CONTEXT;

EFI_STATUS ListDisks(SHELL_CONTEXT *Ctx);

#endif
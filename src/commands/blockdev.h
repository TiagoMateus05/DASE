#ifndef _BLOCKDEV_H_
#define _BLOCKDEV_H_

#include <efi.h>

typedef struct
{
    EFI_SYSTEM_TABLE *ST;
    UINTN Placeholder;
} SHELL_CONTEXT;

#endif
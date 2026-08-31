#ifndef _SHELL_H_
#define _SHELL_H_

#include <efi.h>
#include <efilib.h>

#include "shellcontext.h"

#include "../commands/blockdev.h"
#include "../commands/commands.h"
#include "../includes/wrapper.h"

EFI_STATUS StartShell(EFI_SYSTEM_TABLE *ST);
EFI_STATUS ReadLine(EFI_SYSTEM_TABLE *ST, CHAR16 *buf, UINTN bufSize);
UINTN Tokenize(CHAR16 *Line, CHAR16 **Argv, UINTN MaxArgs);
EFI_STATUS Dispatch(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);

#endif
#ifndef _COMMANDS_H_
#define _COMMANDS_H_

#include <efi.h>
#include <efilib.h>

#include "../dshell/fsparse.h"
#include "../dshell/pager.h"
#include "../dshell/shell.h"
#include "../dshell/shellcontext.h"
#include "../includes/wrapper.h"
#include "../io/ata.h"
#include "blockdev.h"
#include "textlines.h"
#include "wipe.h"

typedef EFI_STATUS (*COMMAND_FN)(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);

typedef struct
{
    CONST CHAR16 *Name;
    COMMAND_FN Run;
} COMMAND;

extern CONST COMMAND gCommands[];
extern CONST UINTN gCommandCount;

EFI_STATUS CmdListDisks(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS CmdListParts(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS CmdListDiskInfo(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS CmdListAtaInfo(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS CmdClear(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS CmdHelp(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS CmdFat32Test(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS ShutDown(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS CmdCheckWipe(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);
EFI_STATUS CmdWipe(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);

BOOLEAN IsValidNumber(CHAR16 *Str);

#endif
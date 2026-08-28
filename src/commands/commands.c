#include "commands.h"

CONST COMMAND gCommands[] = {
    {L"listdisks", CmdListDisks},
    {L"listparts", CmdListParts},
    {L"help", CmdHelp},
    {L"exit", ShutDown},
};
CONST UINTN gCommandCount = sizeof(gCommands) / sizeof(gCommands[0]);

EFI_STATUS CmdListDisks(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    return ListDisks(Ctx);
}

EFI_STATUS CmdListParts(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{   
    if ( Argc != 2 )
    {
        EFISPrint(Ctx->ST->ConOut, L"Wrong use of listparts: missing disk number\r\n");
        return EFI_INVALID_PARAMETER;
    }
    INTN DiskIndex = Atoi(Argv[1]);
    return ListPartitions(Ctx, DiskIndex);
}

EFI_STATUS CmdHelp(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    PagerTool(Ctx->ST, helpLines, helpLineCount);
    return EFI_SUCCESS;
}

EFI_STATUS ShutDown(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    EFISPrint(Ctx->ST->ConOut, L"Shuting down...\r\n");
    uefi_call_wrapper(Ctx->ST->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);

    return EFI_SUCCESS;
}
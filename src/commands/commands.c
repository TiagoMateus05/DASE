#include "commands.h"

CONST COMMAND gCommands[] = {
    {L"listdisks", CmdListDisks},
    {L"listparts", CmdListParts},
    {L"listdisksinfo", CmdListDiskInfo},
    {L"listatainfo", CmdListAtaInfo},
    {L"checkwipe", CmdCheckWipe},
    {L"wipe", CmdWipe},
    {L"clear", CmdClear},
    {L"help", CmdHelp},
    {L"exit", ShutDown},
    {L"fat32test", CmdFat32Test},
};
CONST UINTN gCommandCount = sizeof(gCommands) / sizeof(gCommands[0]);

EFI_STATUS CmdListDisks(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    return ListDisks(Ctx);
}

EFI_STATUS CmdListParts(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    if (Argc != 2)
    {
        EFISPrint(Ctx->ST->ConOut, L"Wrong use of listparts: missing disk number\r\n");
        return EFI_INVALID_PARAMETER;
    }
    if (!IsValidNumber(Argv[1]))
    {
        EFISPrint(Ctx->ST->ConOut, L"Wrong use of listparts: disk number must be a digit\r\n");
        return EFI_INVALID_PARAMETER;
    }
    INTN DiskIndex = Atoi(Argv[1]);
    return ListPartitions(Ctx, DiskIndex);
}

EFI_STATUS CmdListDiskInfo(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    return ListDiskInfo(Ctx);
}

EFI_STATUS CmdListAtaInfo(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    return ListAtaInfo(Ctx);
}

EFI_STATUS CmdCheckWipe(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    if (Argc < 1 || Argc > 2)
    {
        EFISPrint(Ctx->ST->ConOut, L"Wrong use of checkwipe\r\n");
        return EFI_INVALID_PARAMETER;
    }

    EFI_STATUS Status = BuildDiskTable(Ctx);
    if (EFI_ERROR(Status))
        return Status;
    AtaPopulateTable(Ctx);

    if (Argc == 1)
    {
        for (UINTN i = 0; i < Ctx->DiskCount; i++)
        {
            DISK *D = &Ctx->Disks[i];
            WipeCheckTargetVerbose(Ctx, D);
        }
        return EFI_SUCCESS;
    }
    else
    {
        if (!IsValidNumber(Argv[1]))
        {
            EFISPrint(Ctx->ST->ConOut, L"Wrong use of checkwipe: disk number must be a digit\r\n");
            return EFI_INVALID_PARAMETER;
        }
        INTN DiskIndex = Atoi(Argv[1]);
        DISK *D;
        Status = FindDiskByIndex(Ctx, DiskIndex, &D);
        if (EFI_ERROR(Status))
            return Status;

        return WipeCheckTargetVerbose(Ctx, D);
    }
}

EFI_STATUS CmdClear(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    ClearScreen(Ctx->ST->ConOut);
    return EFI_SUCCESS;
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

BOOLEAN IsValidNumber(CHAR16 *Str)
{
    if (*Str == L'\0')
        return FALSE;

    while (*Str)
    {
        if (!(*Str >= L'0' && *Str <= L'9'))
            return FALSE;
        Str++;
    }
    return TRUE;
}

EFI_STATUS CmdWipe(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    EFI_STATUS Status;
    DISK *D;

    if (Argc != 2 || !IsValidNumber(Argv[1]))
    {
        EFISPrint(Ctx->ST->ConOut, L"Usage: wipe <disk number>\r\n");
        return EFI_INVALID_PARAMETER;
    }

    Status = BuildDiskTable(Ctx);
    if (EFI_ERROR(Status))
        return Status;
    AtaPopulateTable(Ctx);

    Status = FindDiskByIndex(Ctx, (UINTN)Atoi(Argv[1]), &D);
    if (EFI_ERROR(Status))
        return Status;

    Status = WipeCheckTargetVerbose(Ctx, D);
    if (EFI_ERROR(Status))
        return Status;

    if (WipeSelectMethod(D) != WIPE_METHOD_OVERWRITE)
    {
        EFISPrint(Ctx->ST->ConOut,
                  L"No implemented wipe method for this drive.\r\n");
        return EFI_UNSUPPORTED;
    }

    if (!WipeConfirm(Ctx, D))
        return EFI_ABORTED;
    
    return WipeOverwrite(Ctx, D, 0x00);
}
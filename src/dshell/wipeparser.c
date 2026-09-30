#include "wipeparser.h"
#include "../commands/commands.h"
#include "../commands/wipe.h"
#include "../includes/wrapper.h"
#include "shellcontext.h"

EFI_STATUS ListSelectedDisks(SHELL_CONTEXT *Ctx)
{
    WIPE_ARGS *A = &Ctx->WipeArgs;
    CHAR16 line[140];

    if (A->TargetCount == 0)
    {
        EFISPrint(Ctx->ST->ConOut, L"No disks selected\r\n");
        return EFI_SUCCESS;
    }

    EFISPrint(Ctx->ST->ConOut, L"Selected disks:\r\n");
    for (UINTN i = 0; i < A->TargetCount; i++)
    {
        UINTN DiskIndex = A->Targets[i].Index;
        if (DiskIndex >= Ctx->DiskCount)
            continue;

        DISK *D = &Ctx->Disks[DiskIndex];
        SPrint(line, sizeof(line), L"  disk%d: %s\r\n", DiskIndex, D->Serial);
        EFISPrint(Ctx->ST->ConOut, line);
    }
    return EFI_SUCCESS;
}

BOOLEAN WipeIsSelected(SHELL_CONTEXT *Ctx, UINTN DiskIndex)
{
    for (UINTN i = 0; i < Ctx->WipeArgs.TargetCount; i++)
        if (Ctx->WipeArgs.Targets[i].Index == DiskIndex)
            return TRUE;
    return FALSE;
}

EFI_STATUS WipeSelectAdd(SHELL_CONTEXT *Ctx, UINTN DiskIndex)
{
    WIPE_ARGS *A = &Ctx->WipeArgs;
    DISK *D;

    if (DiskIndex >= Ctx->DiskCount)
        return EFI_NOT_FOUND;

    if (WipeIsSelected(Ctx, DiskIndex))
        return EFI_SUCCESS;

    if (A->TargetCount >= MAX_DISKS)
        return EFI_BUFFER_TOO_SMALL;

    D = &Ctx->Disks[DiskIndex];

    if (D->Serial[0] == L'\0')
        return EFI_NO_MEDIA;

    A->Targets[A->TargetCount].Index = DiskIndex;
    StrCpy(A->Targets[A->TargetCount].Serial, D->Serial);
    A->TargetCount++;
    return EFI_SUCCESS;
}

EFI_STATUS WipeSelectRemove(SHELL_CONTEXT *Ctx, UINTN DiskIndex)
{
    WIPE_ARGS *A = &Ctx->WipeArgs;

    for (UINTN i = 0; i < A->TargetCount; i++)
    {
        if (A->Targets[i].Index == DiskIndex)
        {
            for (UINTN j = i + 1; j < A->TargetCount; j++)
                A->Targets[j - 1] = A->Targets[j];
            A->TargetCount--;
            return EFI_SUCCESS;
        }
    }
    return EFI_NOT_FOUND;
}

VOID WipeSelectClear(SHELL_CONTEXT *Ctx)
{
    Ctx->WipeArgs.TargetCount = 0;
}

VOID WipeRevalidateSelection(SHELL_CONTEXT *Ctx)
{
    WIPE_ARGS *A = &Ctx->WipeArgs;
    CHAR16 line[140];
    UINTN i = 0;

    while (i < A->TargetCount)
    {
        BOOLEAN Valid = FALSE;

        if (A->Targets[i].Index < Ctx->DiskCount)
        {
            DISK *D = &Ctx->Disks[A->Targets[i].Index];
            Valid = (StrCmp(D->Serial, A->Targets[i].Serial) == 0);
        }

        if (Valid)
        {
            i++;
            continue;
        }

        // Maybe the drive is still present but has moved to a new index.
        BOOLEAN Moved = FALSE;

        if (A->Targets[i].Serial[0] != L'\0')
        {
            for (UINTN k = 0; k < Ctx->DiskCount; k++)
            {
                if (Ctx->Disks[k].Serial[0] == L'\0')
                    continue;

                if (StrCmp(Ctx->Disks[k].Serial, A->Targets[i].Serial) == 0)
                {
                    SPrint(line, sizeof(line),
                           L"note: %s moved to disk%d; selection updated\r\n",
                           A->Targets[i].Serial, k);
                    EFISPrint(Ctx->ST->ConOut, line);
                    A->Targets[i].Index = k;
                    Moved = TRUE;
                    break;
                }
            }
        }

        if (Moved)
        {
            i++;
            continue;
        }

        SPrint(line, sizeof(line),
               L"note: %s is no longer present; deselected\r\n",
               A->Targets[i].Serial);
        EFISPrint(Ctx->ST->ConOut, line);

        for (UINTN j = i + 1; j < A->TargetCount; j++)
            A->Targets[j - 1] = A->Targets[j];
        A->TargetCount--;
    }
}

EFI_STATUS ParseWipeArgs(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    WIPE_ARGS *A = &Ctx->WipeArgs;
    CHAR16 line[140];
    BOOLEAN HasModifier = FALSE;
    BOOLEAN HasAction = FALSE;

    A->List = FALSE;
    A->All = FALSE;
    A->Execute = FALSE;
    A->Interactive = FALSE;

    if (Argc == 1)
    {
        A->Interactive = TRUE;
        return EFI_SUCCESS;
    }

    for (UINTN i = 1; i < Argc; i++)
    {
        CHAR16 *Arg = Argv[i];

        if (Arg[0] != L'-')
        {
            SPrint(line, sizeof(line), L"wipe: unexpected argument '%s'\r\n", Arg);
            EFISPrint(Ctx->ST->ConOut, line);
            return EFI_INVALID_PARAMETER;
        }

        if (StrCmp(Arg, L"-s") == 0 || StrCmp(Arg, L"-a") == 0 ||
            StrCmp(Arg, L"-x") == 0 || StrCmp(Arg, L"-l") == 0 ||
            StrCmp(Arg, L"-c") == 0)
        {
            HasAction = TRUE;
        }
        else if ((Arg[1] == L'r' && Arg[2] == L'd') || Arg[1] == L'd')
        {
            HasModifier = TRUE;

            CHAR16 *Digits = (Arg[1] == L'r') ? &Arg[3] : &Arg[2];
            if (*Digits == L'\0')
                i++;
        }
        else
        {
            SPrint(line, sizeof(line), L"wipe: unknown flag '%s'\r\n", Arg);
            EFISPrint(Ctx->ST->ConOut, line);
            return EFI_INVALID_PARAMETER;
        }
    }

    if (HasAction && HasModifier)
    {
        EFISPrint(Ctx->ST->ConOut,
                  L"wipe: -a, -x, -l and -c cannot be combined with -d or -rd\r\n");
        return EFI_INVALID_PARAMETER;
    }

    for (UINTN i = 1; i < Argc; i++)
    {
        CHAR16 *Arg = Argv[i];
        BOOLEAN Removing = FALSE;
        CHAR16 *Digits;
        UINTN DiskNum;

        if (StrCmp(Arg, L"-s") == 0)
        {
            A->NoAsync = TRUE;
            continue;
        }

        if (StrCmp(Arg, L"-a") == 0)
        {
            A->All = TRUE;
            continue;
        }
        if (StrCmp(Arg, L"-x") == 0)
        {
            A->Execute = TRUE;
            continue;
        }
        if (StrCmp(Arg, L"-l") == 0)
        {
            A->List = TRUE;
            continue;
        }
        if (StrCmp(Arg, L"-c") == 0)
        {
            WipeSelectClear(Ctx);
            EFISPrint(Ctx->ST->ConOut, L"selection cleared\r\n");
            continue;
        }

        // -rdX must be tested before -dX.
        if (Arg[1] == L'r' && Arg[2] == L'd')
        {
            Removing = TRUE;
            Digits = &Arg[3];
        }
        else
        {
            Digits = &Arg[2];
        }

        if (*Digits == L'\0')
        {
            if (i + 1 >= Argc)
            {
                SPrint(line, sizeof(line),
                       L"wipe: '%s' needs a disk number\r\n", Arg);
                EFISPrint(Ctx->ST->ConOut, line);
                return EFI_INVALID_PARAMETER;
            }
            Digits = Argv[++i];
        }

        if (!IsValidNumber(Digits))
        {
            SPrint(line, sizeof(line),
                   L"wipe: '%s' is not a disk number\r\n", Digits);
            EFISPrint(Ctx->ST->ConOut, line);
            return EFI_INVALID_PARAMETER;
        }

        DiskNum = (UINTN)Atoi(Digits);

        if (DiskNum >= Ctx->DiskCount)
        {
            SPrint(line, sizeof(line),
                   L"wipe: no disk%d (system has %d disks)\r\n",
                   DiskNum, Ctx->DiskCount);
            EFISPrint(Ctx->ST->ConOut, line);
            return EFI_NOT_FOUND;
        }

        if (Removing)
        {
            if (EFI_ERROR(WipeSelectRemove(Ctx, DiskNum)))
            {
                SPrint(line, sizeof(line),
                       L"note: disk%d was not selected\r\n", DiskNum);
                EFISPrint(Ctx->ST->ConOut, line);
            }
            else
            {
                SPrint(line, sizeof(line), L"disk%d deselected\r\n", DiskNum);
                EFISPrint(Ctx->ST->ConOut, line);
            }
        }
        else
        {
            EFI_STATUS Chk = WipeCheckTarget(Ctx, &Ctx->Disks[DiskNum]);
            if (EFI_ERROR(Chk))
            {
                WipeCheckTargetVerbose(Ctx, &Ctx->Disks[DiskNum]);
                return Chk;
            }
            if (WipeSelectMethod(&Ctx->Disks[DiskNum]) == WIPE_METHOD_NONE)
            {
                SPrint(line, sizeof(line),
                       L"wipe: disk%d is not DASE Ready and cannot be selected\r\n",
                       DiskNum);
                EFISPrint(Ctx->ST->ConOut, line);
                return EFI_UNSUPPORTED;
            }
            EFI_STATUS S = WipeSelectAdd(Ctx, DiskNum);

            if (S == EFI_NO_MEDIA)
            {
                SPrint(line, sizeof(line),
                       L"wipe: disk%d reports no serial number and cannot be selected\r\n",
                       DiskNum);
                EFISPrint(Ctx->ST->ConOut, line);
                return S;
            }
            if (EFI_ERROR(S))
                return S;

            SPrint(line, sizeof(line), L"disk%d selected\r\n", DiskNum);
            EFISPrint(Ctx->ST->ConOut, line);
        }
    }

    if (A->NoAsync && !A->Execute)
    {
        EFISPrint(Ctx->ST->ConOut, L"wipe: -s only applies with -x\r\n");
        A->NoAsync = FALSE;
        return EFI_INVALID_PARAMETER;
    }

    if (A->All)
    {
        for (UINTN k = 0; k < Ctx->DiskCount; k++)
        {
            DISK *D = &Ctx->Disks[k];

            if (EFI_ERROR(WipeCheckTarget(Ctx, D)))
                continue;

            if (WipeSelectMethod(D) == WIPE_METHOD_NONE)
            {
                SPrint(line, sizeof(line),
                       L"note: disk%d not added -- no implemented method\r\n", k);
                EFISPrint(Ctx->ST->ConOut, line);
                continue;
            }

            WipeSelectAdd(Ctx, k);
        }
    }

    if (!A->Execute && !A->List && !A->All && !HasModifier)
        A->Interactive = TRUE;

    return EFI_SUCCESS;
}
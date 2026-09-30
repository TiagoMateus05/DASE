#include "wipe.h"

#define WIPE_CHUNK_BYTES (1024 * 1024) // 8MB chunk size for optimized Memory Align
#define WIPE_BAR_WIDTH 28

WIPE_METHOD WipeSelectMethod(DISK *Disk)
{
    if (Disk->Media == MEDIA_ROTATIONAL)
        return WIPE_METHOD_OVERWRITE;

    if (Disk->Transport == DISK_TRANSPORT_NVME)
    {
        if (Disk->NvmePassThru == NULL || EFI_ERROR(Disk->NvmeStatus) ||
            Disk->NvmeNamespaceCount != 1)
            return WIPE_METHOD_NONE;

        if (Disk->NvmeSanitizeSupported)
            return WIPE_METHOD_NVME_SANITIZE;

        if (Disk->NvmeFormatSupported)
            return WIPE_METHOD_NVME_FORMAT;

        return WIPE_METHOD_NONE;
    }

    if (Disk->Transport == DISK_TRANSPORT_SATA ||
        Disk->Transport == DISK_TRANSPORT_IDE)
    {
        if (Disk->SanitizeSupported)
            return WIPE_METHOD_ATA_SANITIZE;

        if (Disk->EnhancedEraseSupported && !Disk->SecurityFrozen)
            return WIPE_METHOD_ATA_SECURITY_ERASE;
    }

    return WIPE_METHOD_NONE;
}

CONST CHAR16 *WipeMethodName(WIPE_METHOD M)
{
    switch (M)
    {
        case WIPE_METHOD_OVERWRITE:
            return L"LBA overwrite";
        case WIPE_METHOD_ATA_SECURITY_ERASE:
            return L"ATA Security Erase";
        case WIPE_METHOD_ATA_SANITIZE:
            return L"ATA Sanitize";
        case WIPE_METHOD_NVME_SANITIZE:
            return L"NVMe Sanitize";
        case WIPE_METHOD_NVME_FORMAT:
            return L"NVMe Format";
        default:
            return L"none available";
    }
}

EFI_STATUS WipeCheckTarget(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    if (Disk->IsBootDevice)
        return EFI_ACCESS_DENIED;
    if (Disk->Transport == DISK_TRANSPORT_RAID)
        return EFI_DEVICE_ERROR;
    if (Disk->Transport == DISK_TRANSPORT_UNKNOWN)
        return EFI_UNSUPPORTED;
    if (!Disk->MediaPresent)
        return EFI_NO_MEDIA;
    if (Disk->ReadOnly)
        return EFI_WRITE_PROTECTED;
    if (Disk->Media == MEDIA_UNKNOWN)
        return EFI_NOT_READY;
    if (Disk->MediaId != Disk->BlockIo->Media->MediaId)
        return EFI_MEDIA_CHANGED;
    return EFI_SUCCESS;
}

EFI_STATUS WipeCheckTargetVerbose(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    CHAR16 line[180];
    EFI_STATUS Status = WipeCheckTarget(Ctx, Disk);

    switch (Status)
    {
        case EFI_ACCESS_DENIED:
            SPrint(line, sizeof(line), L"disk%d  REFUSED: boot device\r\n",
                   (UINTN)Disk->Index);
            break;
        case EFI_DEVICE_ERROR:
            SPrint(line, sizeof(line),
                   L"disk%d  REFUSED: RAID volume -- set the controller to IT/HBA mode\r\n",
                   (UINTN)Disk->Index);
            break;
        case EFI_UNSUPPORTED:
            SPrint(line, sizeof(line),
                   L"disk%d  REFUSED: unrecognised transport\r\n", (UINTN)Disk->Index);
            break;
        case EFI_NO_MEDIA:
            SPrint(line, sizeof(line),
                   L"disk%d  REFUSED: no media present\r\n", (UINTN)Disk->Index);
            break;
        case EFI_WRITE_PROTECTED:
            SPrint(line, sizeof(line),
                   L"disk%d  REFUSED: write protected\r\n", (UINTN)Disk->Index);
            break;
        case EFI_NOT_READY:
            SPrint(line, sizeof(line),
                   L"disk%d  REFUSED: media type unknown -- cannot certify sanitization\r\n",
                   (UINTN)Disk->Index);
            break;
        case EFI_MEDIA_CHANGED:
            SPrint(line, sizeof(line),
                   L"disk%d  REFUSED: media changed since scan -- rescan required\r\n",
                   (UINTN)Disk->Index);
            break;
        default: {
            WIPE_METHOD M = WipeSelectMethod(Disk);
            SPrint(line, sizeof(line),
                   L"disk%d  DASE Ready: %s  method: %s%s\r\n",
                   (UINTN)Disk->Index,
                   Disk->Media == MEDIA_SOLID_STATE ? L"SSD" : L"HDD",
                   WipeMethodName(M),
                   (M == WIPE_METHOD_NONE)
                       ? L"  (no compliant method -- not implemented yet)"
                       : L"");
            break;
        }
    }
    EFISPrint(Ctx->ST->ConOut, line);

    if (Disk->SecurityFrozen)
        EFISPrint(Ctx->ST->ConOut,
                  L"         note: drive is security-frozen; power-cycle it to unfreeze\r\n");

    if (Disk->Transport == DISK_TRANSPORT_NVME)
    {
        if (EFI_ERROR(Disk->NvmeStatus))
        {
            SPrint(line, sizeof(line), L"         note: NVMe identify failed: %r\r\n",
                   Disk->NvmeStatus);
            EFISPrint(Ctx->ST->ConOut, line);
        }
        else if (Disk->NvmeNamespaceCount == 0)
        {
            EFISPrint(Ctx->ST->ConOut,
                      L"         note: could not count the controller's namespaces; NVMe wipe needs exactly one\r\n");
        }
        else if (Disk->NvmeNamespaceCount > 1)
        {
            SPrint(line, sizeof(line),
                   L"         note: controller has %d active namespaces; NVMe wipe needs exactly one\r\n",
                   (UINTN)Disk->NvmeNamespaceCount);
            EFISPrint(Ctx->ST->ConOut, line);
        }
    }

    return Status;
}

// ===========================================================================
// Progress bar
// ===========================================================================

static VOID WipeDrawBar(SHELL_CONTEXT *Ctx, UINTN Row, UINTN DiskIndex,
                        UINT64 Done, UINT64 Total, UINT32 BlockSize)
{
    CHAR16 bar[WIPE_BAR_WIDTH + 1];
    CHAR16 line[160];
    UINTN Pct = (Total == 0) ? 0 : (UINTN)((Done * 100) / Total);
    UINTN Filled = (Pct * WIPE_BAR_WIDTH) / 100;

    for (UINTN i = 0; i < WIPE_BAR_WIDTH; i++)
        bar[i] = (i < Filled) ? L'#' : L'-';
    bar[WIPE_BAR_WIDTH] = L'\0';

    uefi_call_wrapper(Ctx->ST->ConOut->SetCursorPosition, 3,
                      Ctx->ST->ConOut, 0, Row);
    SPrint(line, sizeof(line),
           L"  disk%d  [%s] %3d%%  %ld / %ld MB    ",
           DiskIndex, bar, Pct,
           (Done * BlockSize) / (1024 * 1024),
           (Total * BlockSize) / (1024 * 1024));
    EFISPrint(Ctx->ST->ConOut, line);
}

EFI_STATUS WipeOverwrite(SHELL_CONTEXT *Ctx, DISK *Disk, UINT8 Pattern)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO *Bio = Disk->BlockIo;
    UINT32 BlockSize = Disk->BlockSize;
    UINT64 TotalBlocks = Disk->LastBlock + 1;
    UINT64 BlocksDone = 0;
    UINTN ChunkBlocks, ChunkBytes, Row;
    VOID *Buffer = NULL;
    CHAR16 line[120];

    if (BlockSize == 0 || TotalBlocks == 0)
        return EFI_INVALID_PARAMETER;

    if (Disk->IoAlign > 8)
    {
        EFISPrint(Ctx->ST->ConOut,
                  L"Device requires stricter buffer alignment than AllocatePool provides\r\n");
        return EFI_UNSUPPORTED;
    }

    ChunkBlocks = WIPE_CHUNK_BYTES / BlockSize;
    if (ChunkBlocks == 0)
        ChunkBlocks = 1;
    ChunkBytes = ChunkBlocks * BlockSize;

    Status = uefi_call_wrapper(Ctx->ST->BootServices->AllocatePool, 3,
                               EfiLoaderData, ChunkBytes, &Buffer);
    if (EFI_ERROR(Status))
        return Status;

    SetMem(Buffer, ChunkBytes, Pattern);

    EFISPrint(Ctx->ST->ConOut, L"\r\n");
    Row = Ctx->ST->ConOut->Mode->CursorRow;
    WipeDrawBar(Ctx, Row, Disk->Index, 0, TotalBlocks, BlockSize);

    while (BlocksDone < TotalBlocks)
    {
        UINT64 Remaining = TotalBlocks - BlocksDone;
        UINTN ThisBlocks = (Remaining < ChunkBlocks) ? (UINTN)Remaining : ChunkBlocks;

        if (Bio->Media->MediaId != Disk->MediaId)
        {
            EFISPrint(Ctx->ST->ConOut, L"\r\nABORT: media changed during wipe\r\n");
            Status = EFI_MEDIA_CHANGED;
            goto done;
        }

        Status = uefi_call_wrapper(Bio->WriteBlocks, 5, Bio, Disk->MediaId,
                                   (EFI_LBA)BlocksDone, ThisBlocks * BlockSize, Buffer);
        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line), L"\r\nABORT at LBA %ld: %r\r\n",
                   BlocksDone, Status);
            EFISPrint(Ctx->ST->ConOut, line);
            goto done;
        }

        BlocksDone += ThisBlocks;
        WipeDrawBar(Ctx, Row, Disk->Index, BlocksDone, TotalBlocks, BlockSize);
    }

    Status = uefi_call_wrapper(Bio->FlushBlocks, 1, Bio);
    if (EFI_ERROR(Status))
    {
        SPrint(line, sizeof(line), L"\r\nFlush failed: %r\r\n", Status);
        EFISPrint(Ctx->ST->ConOut, line);
        goto done;
    }

    EFISPrint(Ctx->ST->ConOut, L"\r\n  overwrite complete\r\n");
    Status = EFI_SUCCESS;

done:
    FreePoolBS(Ctx->ST->BootServices, Buffer);
    return Status;
}

EFI_STATUS WipeVerification(SHELL_CONTEXT *Ctx, DISK *Disk, UINT8 Pattern)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO *Bio = Disk->BlockIo;
    UINT32 BlockSize = Disk->BlockSize;
    UINT64 TotalBlocks = Disk->LastBlock + 1;
    UINT64 BlocksDone = 0;
    UINTN ChunkBlocks, ChunkBytes;
    VOID *Buffer = NULL;
    CHAR16 line[140];

    ChunkBlocks = WIPE_CHUNK_BYTES / BlockSize;
    if (ChunkBlocks == 0)
        ChunkBlocks = 1;
    ChunkBytes = ChunkBlocks * BlockSize;

    Status = uefi_call_wrapper(Ctx->ST->BootServices->AllocatePool, 3,
                               EfiLoaderData, ChunkBytes, &Buffer);
    if (EFI_ERROR(Status))
        return Status;

    while (BlocksDone < TotalBlocks)
    {
        UINT64 Remaining = TotalBlocks - BlocksDone;
        UINTN ThisBlocks = (Remaining < ChunkBlocks) ? (UINTN)Remaining : ChunkBlocks;
        UINTN ThisBytes = ThisBlocks * BlockSize;

        Status = uefi_call_wrapper(Bio->ReadBlocks, 5, Bio, Disk->MediaId,
                                   (EFI_LBA)BlocksDone, ThisBytes, Buffer);
        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line),
                   L"\r\nABORT at LBA %ld: read failed: %r\r\n", BlocksDone, Status);
            EFISPrint(Ctx->ST->ConOut, line);
            goto done;
        }

        for (UINTN i = 0; i < ThisBytes; i++)
        {
            if (((UINT8 *)Buffer)[i] != Pattern)
            {
                SPrint(line, sizeof(line),
                       L"\r\nABORT at LBA %ld: verification failed at byte %ld\r\n",
                       BlocksDone, i);
                EFISPrint(Ctx->ST->ConOut, line);
                Status = EFI_COMPROMISED_DATA;
                goto done;
            }
        }
        BlocksDone += ThisBlocks;
    }

    SPrint(line, sizeof(line), L"  verified %ld of %ld blocks\r\n",
           BlocksDone, TotalBlocks);
    EFISPrint(Ctx->ST->ConOut, line);
    Status = EFI_SUCCESS;

done:
    FreePoolBS(Ctx->ST->BootServices, Buffer);
    return Status;
}

// ===========================================================================
// NVMe
// ===========================================================================

static EFI_STATUS WipeNvmeSanitize(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    EFI_STATUS Status;
    UINT64 TotalBlocks = Disk->LastBlock + 1;
    UINT32 Action;
    CONST CHAR16 *ActionName;
    UINT16 NvmeSt = 0, Progress = 0, SStat = 0;
    UINTN Row, LogFailures = 0;
    CHAR16 line[140];

    if (Disk->NvmeSanitizeBlockEraseSupported)
    {
        Action = NVME_SANACT_BLOCK_ERASE;
        ActionName = L"block erase";
    }
    else if (Disk->NvmeSanitizeCryptoSupported)
    {
        Action = NVME_SANACT_CRYPTO_ERASE;
        ActionName = L"crypto erase";
    }
    else if (Disk->NvmeSanitizeOverwriteSupported)
    {
        Action = NVME_SANACT_OVERWRITE;
        ActionName = L"overwrite";
    }
    else
        return EFI_UNSUPPORTED;

    SPrint(line, sizeof(line), L"  starting sanitize (%s)\r\n", ActionName);
    EFISPrint(Ctx->ST->ConOut, line);

    Status = NvmeSanitizeStart(Ctx, Disk, Action, &NvmeSt);
    if (EFI_ERROR(Status))
    {
        SPrint(line, sizeof(line), L"  sanitize refused: %r (sct %x sc %02x)\r\n",
               Status, (UINTN)(NvmeSt >> 8), (UINTN)(NvmeSt & 0xFF));
        EFISPrint(Ctx->ST->ConOut, line);
        return Status;
    }

    Row = Ctx->ST->ConOut->Mode->CursorRow;
    WipeDrawBar(Ctx, Row, Disk->Index, 0, TotalBlocks, Disk->BlockSize);

    for (;;)
    {
        uefi_call_wrapper(Ctx->ST->BootServices->Stall, 1, 1000 * 1000);

        Status = NvmeSanitizeStatus(Ctx, Disk, &Progress, &SStat);
        if (EFI_ERROR(Status))
        {
            if (++LogFailures < 5)
                continue;
            SPrint(line, sizeof(line),
                   L"\r\n  lost track of sanitize: %r -- do not trust this drive until it is re-run\r\n",
                   Status);
            EFISPrint(Ctx->ST->ConOut, line);
            return Status;
        }
        LogFailures = 0;

        if ((SStat & NVME_SSTAT_MASK) != NVME_SSTAT_IN_PROGRESS)
            break;

        WipeDrawBar(Ctx, Row, Disk->Index, (TotalBlocks * Progress) >> 16,
                    TotalBlocks, Disk->BlockSize);
    }

    switch (SStat & NVME_SSTAT_MASK)
    {
        case NVME_SSTAT_COMPLETED:
        case NVME_SSTAT_COMPLETED_NO_DEALLOC:
            WipeDrawBar(Ctx, Row, Disk->Index, TotalBlocks, TotalBlocks, Disk->BlockSize);
            EFISPrint(Ctx->ST->ConOut, L"\r\n  sanitize complete\r\n");
            return EFI_SUCCESS;

        case NVME_SSTAT_FAILED:
            EFISPrint(Ctx->ST->ConOut,
                      L"\r\n  sanitize FAILED -- the drive refuses I/O until a sanitize succeeds; wipe it again\r\n");
            return EFI_DEVICE_ERROR;

        default:
            SPrint(line, sizeof(line),
                   L"\r\n  sanitize ended in unexpected state %d\r\n",
                   (UINTN)(SStat & NVME_SSTAT_MASK));
            EFISPrint(Ctx->ST->ConOut, line);
            return EFI_DEVICE_ERROR;
    }
}

static EFI_STATUS WipeNvmeFormat(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    EFI_STATUS Status;
    UINT16 NvmeSt = 0;
    CHAR16 line[140];

    EFISPrint(Ctx->ST->ConOut,
              L"  formatting with User Data Erase -- no progress is reported, this can take minutes\r\n");

    Status = NvmeFormat(Ctx, Disk, NVME_SES_USER_DATA_ERASE, &NvmeSt);
    if (EFI_ERROR(Status))
    {
        SPrint(line, sizeof(line), L"  format failed: %r (sct %x sc %02x)\r\n",
               Status, (UINTN)(NvmeSt >> 8), (UINTN)(NvmeSt & 0xFF));
        EFISPrint(Ctx->ST->ConOut, line);
        return Status;
    }

    EFISPrint(Ctx->ST->ConOut, L"  format complete\r\n");
    return EFI_SUCCESS;
}

// What an erased namespace should read back as. DLFEAT says so on most
// drives; otherwise take LBA 0's first byte and let verification hold the
// entire drive to it -- anything short of a uniformly 00h or FFh drive fails.
static UINT8 WipeNvmeErasedPattern(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    VOID *Block = NULL;
    UINT8 Pattern = 0x00;

    switch (Disk->NvmeDlfeat & NVME_DLFEAT_READ_MASK)
    {
        case NVME_DLFEAT_READS_ZERO:
            return 0x00;
        case NVME_DLFEAT_READS_ONES:
            return 0xFF;
    }

    if (EFI_ERROR(uefi_call_wrapper(Ctx->ST->BootServices->AllocatePool, 3,
                                    EfiLoaderData, Disk->BlockSize, &Block)))
        return Pattern;

    if (!EFI_ERROR(uefi_call_wrapper(Disk->BlockIo->ReadBlocks, 5, Disk->BlockIo,
                                     Disk->MediaId, (EFI_LBA)0, Disk->BlockSize, Block)) &&
        ((UINT8 *)Block)[0] == 0xFF)
        Pattern = 0xFF;

    FreePoolBS(Ctx->ST->BootServices, Block);
    return Pattern;
}

static EFI_STATUS WipeRunJobs(SHELL_CONTEXT *Ctx, WIPE_JOB *Jobs, UINTN JobCount)
{
    UINTN Active = JobCount;

    for (UINTN j = 0; j < JobCount; j++)
    {
        WIPE_JOB *J = &Jobs[j];
        EFI_STATUS S;

        J->ChunkBlocks = WIPE_CHUNK_BYTES / J->Disk->BlockSize;
        if (J->ChunkBlocks == 0)
            J->ChunkBlocks = 1;

        S = uefi_call_wrapper(Ctx->ST->BootServices->AllocatePool, 3,
                              EfiLoaderData,
                              J->ChunkBlocks * J->Disk->BlockSize, &J->Buffer);
        if (EFI_ERROR(S))
        {
            J->Done = TRUE;
            J->Result = S;
            Active--;
            continue;
        }

        SetMem(J->Buffer, J->ChunkBlocks * J->Disk->BlockSize, 0x00);

        ZeroMem(&J->Token, sizeof(J->Token));
        S = uefi_call_wrapper(Ctx->ST->BootServices->CreateEvent, 5,
                              0, TPL_CALLBACK, NULL, NULL, &J->Token.Event);
        if (EFI_ERROR(S))
        {
            J->Done = TRUE;
            J->Result = S;
            Active--;
            continue;
        }

        WipeDrawBar(Ctx, J->Row, J->Disk->Index, 0, J->TotalBlocks,
                    J->Disk->BlockSize);
    }

    while (Active > 0)
    {
        for (UINTN j = 0; j < JobCount; j++)
        {
            WIPE_JOB *J = &Jobs[j];
            EFI_STATUS S;

            if (J->Done)
                continue;

            if (J->InFlight)
            {
                S = uefi_call_wrapper(Ctx->ST->BootServices->CheckEvent, 1,
                                      J->Token.Event);
                if (S == EFI_NOT_READY)
                    continue;

                J->InFlight = FALSE;

                if (EFI_ERROR(J->Token.TransactionStatus))
                {
                    J->Result = J->Token.TransactionStatus;
                    J->Done = TRUE;
                    Active--;
                    continue;
                }

                J->BlocksDone = J->BlocksIssued;

                UINTN Pct = (UINTN)((J->BlocksDone * 100) / J->TotalBlocks);
                if (Pct != J->LastPct)
                {
                    WipeDrawBar(Ctx, J->Row, J->Disk->Index, J->BlocksDone,
                                J->TotalBlocks, J->Disk->BlockSize);
                    J->LastPct = Pct;
                }

                if (J->BlocksDone >= J->TotalBlocks)
                {
                    uefi_call_wrapper(J->Disk->BlockIo->FlushBlocks, 1,
                                      J->Disk->BlockIo);
                    J->Result = EFI_SUCCESS;
                    J->Done = TRUE;
                    Active--;
                    continue;
                }
            }

            if (J->Disk->BlockIo->Media->MediaId != J->Disk->MediaId)
            {
                J->Result = EFI_MEDIA_CHANGED;
                J->Done = TRUE;
                Active--;
                continue;
            }

            {
                UINT64 Remaining = J->TotalBlocks - J->BlocksIssued;
                UINTN ThisBlocks = (Remaining < J->ChunkBlocks)
                                       ? (UINTN)Remaining
                                       : J->ChunkBlocks;

                S = uefi_call_wrapper(J->Disk->BlockIo2->WriteBlocksEx, 6,
                                      J->Disk->BlockIo2, J->Disk->MediaId,
                                      (EFI_LBA)J->BlocksIssued, &J->Token,
                                      ThisBlocks * J->Disk->BlockSize, J->Buffer);
                if (EFI_ERROR(S))
                {
                    J->Result = S;
                    J->Done = TRUE;
                    Active--;
                    continue;
                }

                J->BlocksIssued += ThisBlocks;
                J->InFlight = TRUE;
            }
        }
    }

    for (UINTN j = 0; j < JobCount; j++)
    {
        WIPE_JOB *J = &Jobs[j];
        if (J->Token.Event)
            uefi_call_wrapper(Ctx->ST->BootServices->CloseEvent, 1, J->Token.Event);
        if (J->Buffer)
            FreePoolBS(Ctx->ST->BootServices, J->Buffer);
    }
    return EFI_SUCCESS;
}

static DISK *WipeResolveTarget(SHELL_CONTEXT *Ctx, UINTN SelIndex)
{
    WIPE_ARGS *A = &Ctx->WipeArgs;
    CHAR16 line[160];
    DISK *D;

    if (A->Targets[SelIndex].Index >= Ctx->DiskCount)
        return NULL;

    D = &Ctx->Disks[A->Targets[SelIndex].Index];

    if (StrCmp(D->Serial, A->Targets[SelIndex].Serial) != 0)
    {
        SPrint(line, sizeof(line),
               L"disk%d SKIPPED: serial changed since selection\r\n",
               (UINTN)D->Index);
        EFISPrint(Ctx->ST->ConOut, line);
        return NULL;
    }

    if (EFI_ERROR(WipeCheckTarget(Ctx, D)))
    {
        SPrint(line, sizeof(line), L"disk%d SKIPPED: no longer wipeable\r\n",
               (UINTN)D->Index);
        EFISPrint(Ctx->ST->ConOut, line);
        return NULL;
    }

    return D;
}

EFI_STATUS WipeExecuteSelection(SHELL_CONTEXT *Ctx)
{
    WIPE_ARGS *A = &Ctx->WipeArgs;
    CHAR16 line[180];
    CHAR16 Typed[64];
    UINTN Succeeded = 0, Failed = 0, Skipped = 0;

    WIPE_JOB Jobs[MAX_DISKS];
    UINTN JobCount = 0;

    if (A->TargetCount == 0)
    {
        EFISPrint(Ctx->ST->ConOut, L"wipe: nothing selected\r\n");
        return EFI_NOT_READY;
    }

    // --- review screen ---
    ClearScreen(Ctx->ST->ConOut);
    EFISPrint(Ctx->ST->ConOut,
              L"=========================================================\r\n");
    EFISPrint(Ctx->ST->ConOut,
              L" THE FOLLOWING DRIVES WILL BE PERMANENTLY DESTROYED\r\n");
    EFISPrint(Ctx->ST->ConOut,
              L"=========================================================\r\n\r\n");

    for (UINTN i = 0; i < A->TargetCount; i++)
    {
        DISK *D;
        if (A->Targets[i].Index >= Ctx->DiskCount)
            continue;
        D = &Ctx->Disks[A->Targets[i].Index];

        SPrint(line, sizeof(line),
               L" disk%d  %-20s  %-16s  %ld MB  [%s]\r\n",
               (UINTN)D->Index, D->Model, D->Serial,
               ((D->LastBlock + 1) * (UINT64)D->BlockSize) / (1024 * 1024),
               WipeMethodName(WipeSelectMethod(D)));
        EFISPrint(Ctx->ST->ConOut, line);
    }

    SPrint(line, sizeof(line),
           L"\r\n %d drive(s) selected. This cannot be undone.\r\n", A->TargetCount);
    EFISPrint(Ctx->ST->ConOut, line);
    EFISPrint(Ctx->ST->ConOut,
              L"\r\nType  I confirm  to proceed, or anything else to cancel:\r\n> ");

    ReadLine(Ctx->ST, Typed, sizeof(Typed) / sizeof(Typed[0]));
    EFISPrint(Ctx->ST->ConOut, L"\r\n");

    if (StrCmp(Typed, L"I confirm") != 0)
    {
        EFISPrint(Ctx->ST->ConOut, L"Cancelled. Nothing was written.\r\n");
        return EFI_ABORTED;
    }

    for (UINTN i = 0; i < A->TargetCount; i++)
    {
        DISK *D = WipeResolveTarget(Ctx, i);
        WIPE_METHOD M;

        if (D == NULL)
        {
            Skipped++;
            continue;
        }

        M = WipeSelectMethod(D);

        if (M == WIPE_METHOD_OVERWRITE && !A->NoAsync && D->BlockIo2 != NULL)
        {
            ZeroMem(&Jobs[JobCount], sizeof(WIPE_JOB));
            Jobs[JobCount].Disk = D;
            Jobs[JobCount].TotalBlocks = D->LastBlock + 1;
            JobCount++;
            continue;
        }

        SPrint(line, sizeof(line), L"\r\ndisk%d (%s): %s\r\n",
               (UINTN)D->Index, D->Serial, WipeMethodName(M));
        EFISPrint(Ctx->ST->ConOut, line);

        EFI_STATUS S;
        UINT8 Pattern = 0x00;
        if (M == WIPE_METHOD_OVERWRITE)
            S = WipeOverwrite(Ctx, D, Pattern);
        else if (M == WIPE_METHOD_NVME_SANITIZE)
            S = WipeNvmeSanitize(Ctx, D);
        else if (M == WIPE_METHOD_NVME_FORMAT)
            S = WipeNvmeFormat(Ctx, D);
        else
        {
            SPrint(line, sizeof(line), L"  %s not implemented\r\n",
                   WipeMethodName(M));
            EFISPrint(Ctx->ST->ConOut, line);
            Skipped++;
            continue;
        }

        if (EFI_ERROR(S))
        {
            Failed++;
            continue;
        }

        if (M == WIPE_METHOD_NVME_SANITIZE || M == WIPE_METHOD_NVME_FORMAT)
        {
            Pattern = WipeNvmeErasedPattern(Ctx, D);
            SPrint(line, sizeof(line), L"  verifying (expecting %02x)...\r\n",
                   (UINTN)Pattern);
            EFISPrint(Ctx->ST->ConOut, line);
        }

        if (EFI_ERROR(WipeVerification(Ctx, D, Pattern)))
        {
            Failed++;
            continue;
        }
        Succeeded++;
    }

    // --- run the batch, then verify each member ---
    if (JobCount > 0)
    {
        UINTN BaseRow;

        EFISPrint(Ctx->ST->ConOut, L"\r\n");
        for (UINTN j = 0; j < JobCount; j++)
            EFISPrint(Ctx->ST->ConOut, L"\r\n");
        BaseRow = Ctx->ST->ConOut->Mode->CursorRow - JobCount;
        for (UINTN j = 0; j < JobCount; j++)
            Jobs[j].Row = BaseRow + j;

        WipeRunJobs(Ctx, Jobs, JobCount);

        uefi_call_wrapper(Ctx->ST->ConOut->SetCursorPosition, 3,
                          Ctx->ST->ConOut, 0, BaseRow + JobCount);
        for (UINTN j = 0; j < JobCount; j++)
        {
            DISK *D = Jobs[j].Disk;

            if (EFI_ERROR(Jobs[j].Result))
            {
                SPrint(line, sizeof(line), L"disk%d FAILED: %r\r\n",
                       (UINTN)D->Index, Jobs[j].Result);
                EFISPrint(Ctx->ST->ConOut, line);
                Failed++;
                continue;
            }

            SPrint(line, sizeof(line), L"disk%d verifying...\r\n", (UINTN)D->Index);
            EFISPrint(Ctx->ST->ConOut, line);

            if (EFI_ERROR(WipeVerification(Ctx, D, 0x00)))
                Failed++;
            else
                Succeeded++;
        }
    }

    // --- summary ---
    EFISPrint(Ctx->ST->ConOut,
              L"\r\n---------------------------------------------------------\r\n");
    SPrint(line, sizeof(line), L" %d wiped and verified, %d failed, %d skipped\r\n",
           Succeeded, Failed, Skipped);
    EFISPrint(Ctx->ST->ConOut, line);

    if (Failed == 0 && Skipped == 0)
        WipeSelectClear(Ctx);

    return (Failed > 0) ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}
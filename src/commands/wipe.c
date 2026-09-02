#include "wipe.h"

#define WIPE_CHUNK_BYTES (1024 * 1024)

EFI_STATUS WipeOverwrite(SHELL_CONTEXT *Ctx, DISK *Disk, UINT8 Pattern)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO *Bio = Disk->BlockIo;
    UINT32 BlockSize = Disk->BlockSize;
    UINT64 TotalBlocks = Disk->LastBlock + 1;
    UINT64 BlocksDone = 0;
    UINTN ChunkBlocks;
    UINTN ChunkBytes;
    VOID *Buffer = NULL;
    UINTN Col, Row;
    CHAR16 line[120];

    if (BlockSize == 0 || TotalBlocks == 0)
        return EFI_INVALID_PARAMETER;

    // Allocate a buffer for one chunk of blocks
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

    uefi_call_wrapper(Ctx->ST->ConOut->QueryMode, 4, Ctx->ST->ConOut,
                      Ctx->ST->ConOut->Mode->Mode, &Col, &Row);
    Row = Ctx->ST->ConOut->Mode->CursorRow;

    while (BlocksDone < TotalBlocks)
    {
        UINT64 Remaining = TotalBlocks - BlocksDone;
        UINT64 ThisBlocks = (Remaining > ChunkBlocks) ? ChunkBlocks : Remaining;

        // Media CHeck:
        if (Bio->Media->MediaId != Disk->MediaId)
        {
            EFISPrint(Ctx->ST->ConOut,
                      L"\r\nABORT: media changed during wipe\r\n");
            Status = EFI_MEDIA_CHANGED;
            goto done;
        }

        Status = uefi_call_wrapper(Bio->WriteBlocks, 5,
                                   Bio, Disk->MediaId, BlocksDone,
                                   ThisBlocks * BlockSize, Buffer);

        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line),
                   L"\r\nABORT at LBA %ld: %r\r\n", BlocksDone, Status);
            EFISPrint(Ctx->ST->ConOut, line);
            goto done;
        }

        BlocksDone += ThisBlocks;

        uefi_call_wrapper(Ctx->ST->ConOut->SetCursorPosition, 3,
                          Ctx->ST->ConOut, 0, Row);
        SPrint(line, sizeof(line),
               L"  disk%d: %ld / %ld blocks  (%ld%%)      ",
               (UINTN)Disk->Index, BlocksDone, TotalBlocks,
               (BlocksDone * 100) / TotalBlocks);
        EFISPrint(Ctx->ST->ConOut, line);

    }
    
    Status = uefi_call_wrapper(Bio->FlushBlocks, 1, Bio);

    if (EFI_ERROR(Status))
    {
        SPrint(line, sizeof(line), L"\r\nFlush failed: %r\r\n", Status);
        EFISPrint(Ctx->ST->ConOut, line);
        goto done;
    }
    EFISPrint(Ctx->ST->ConOut, L"\r\n  overwrite complete\r\n");

done:
    FreePoolBS(Ctx->ST->BootServices, Buffer);
    return Status;
}

WIPE_METHOD WipeSelectMethod(DISK *Disk)
{
    if (Disk->Media == MEDIA_ROTATIONAL)
        return WIPE_METHOD_OVERWRITE;

    if (Disk->Transport == DISK_TRANSPORT_NVME)
    {
        // TODO: these flags come from ATA IDENTIFY and are never set for NVMe.
        // Real support needs Identify Controller (CNS 0x01) OACS bits.
        return WIPE_METHOD_NONE;
    }

    if (Disk->Transport == DISK_TRANSPORT_SATA || Disk->Transport == DISK_TRANSPORT_IDE)
    {
        // TODO: these flags come from ATA IDENTIFY and are never set for SATA/IDE.
        // Real support needs Identify Controller (CNS 0x01) OACS bits.
        if (Disk->SanitizeSupported) // TODO: never set; needs ACS word 59
            return WIPE_METHOD_ATA_SANITIZE;

        if (Disk->EnhancedEraseSupported && !Disk->SecurityFrozen)
            return WIPE_METHOD_ATA_SECURITY_ERASE;
    }

    return WIPE_METHOD_NONE;
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

EFI_STATUS WipeCheckTargetVerbose(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    CHAR16 line[180];
    EFI_STATUS Status = WipeCheckTarget(Ctx, Disk);

    switch (Status)
    {
        case EFI_ACCESS_DENIED:
            SPrint(line, sizeof(line),
                   L"disk%d  REFUSED: boot device\r\n", (UINTN)Disk->Index);
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
                   (M == WIPE_METHOD_NONE) ? L"  (no compliant method -- not implemented yet)" : L"");
            break;
        }
    }

    EFISPrint(Ctx->ST->ConOut, line);

    if (Disk->SecurityFrozen)
    {
        SPrint(line, sizeof(line),
               L"         note: drive is security-frozen; suspend/resume to unfreeze\r\n");
        EFISPrint(Ctx->ST->ConOut, line);
    }

    return Status;
}

BOOLEAN WipeConfirm(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    CHAR16 line[200];
    CHAR16 Typed[64];
    UINT64 Bytes = (Disk->LastBlock + 1) * (UINT64)Disk->BlockSize;

    EFISPrint(Ctx->ST->ConOut,
              L"\r\n=====================================================\r\n");
    EFISPrint(Ctx->ST->ConOut,
              L" THIS WILL PERMANENTLY DESTROY ALL DATA ON THIS DRIVE\r\n");
    EFISPrint(Ctx->ST->ConOut,
              L"=====================================================\r\n");

    SPrint(line, sizeof(line), L"  disk%d   %s\r\n",
           (UINTN)Disk->Index, Disk->Model);
    EFISPrint(Ctx->ST->ConOut, line);

    SPrint(line, sizeof(line), L"  serial  %s\r\n", Disk->Serial);
    EFISPrint(Ctx->ST->ConOut, line);

    SPrint(line, sizeof(line), L"  size    %ld MB  (%ld blocks x %d bytes)\r\n",
           Bytes / (1024 * 1024), Disk->LastBlock + 1, (UINTN)Disk->BlockSize);
    EFISPrint(Ctx->ST->ConOut, line);

    SPrint(line, sizeof(line), L"  method  %s\r\n",
           WipeMethodName(WipeSelectMethod(Disk)));
    EFISPrint(Ctx->ST->ConOut, line);

    if (Disk->Serial[0] == L'\0')
    {
        EFISPrint(Ctx->ST->ConOut,
                  L"\r\nThis drive reports no serial number. Refusing to wipe.\r\n");
        return FALSE;
    }

    EFISPrint(Ctx->ST->ConOut,
              L"\r\nType the serial number exactly to proceed, or anything else to cancel:\r\n> ");

    ReadLine(Ctx->ST, Typed, sizeof(Typed) / sizeof(Typed[0]));
    EFISPrint(Ctx->ST->ConOut, L"\r\n");

    if (StrCmp(Typed, Disk->Serial) != 0)
    {
        EFISPrint(Ctx->ST->ConOut, L"Cancelled.\r\n");
        return FALSE;
    }
    return TRUE;
}

EFI_STATUS WipeVerification(SHELL_CONTEXT *Ctx, DISK *Disk, UINT8 Pattern)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO *Bo = Disk->BlockIo;
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
        UINT64 ThisBlocks = (Remaining > ChunkBlocks) ? (UINTN)ChunkBlocks : Remaining;
        UINTN ThisBytes = ThisBlocks * BlockSize;

        Status = uefi_call_wrapper(Bo->ReadBlocks, 5,
                                   Bo, Disk->MediaId, BlocksDone,
                                   ThisBytes, Buffer);
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
    SPrint(line, sizeof(line),
           L"  verified %ld of %ld blocks\r\n", BlocksDone, TotalBlocks);
    EFISPrint(Ctx->ST->ConOut, line);
    Status = EFI_SUCCESS;

done:
    FreePoolBS(Ctx->ST->BootServices, Buffer);
    return Status;
}

EFI_STATUS WipeExecuteSelection(SHELL_CONTEXT *Ctx)
{
    WIPE_ARGS *A = &Ctx->WipeArgs;
    CHAR16     line[180];
    CHAR16     Typed[64];
    UINTN      Succeeded = 0, Failed = 0, Skipped = 0;

    if (A->TargetCount == 0)
    {
        EFISPrint(Ctx->ST->ConOut, L"wipe: nothing selected\r\n");
        return EFI_NOT_READY;
    }

    ClearScreen(Ctx->ST->ConOut);
    EFISPrint(Ctx->ST->ConOut,
              L"=========================================================\r\n");
    EFISPrint(Ctx->ST->ConOut,
              L" THE FOLLOWING DRIVES WILL BE PERMANENTLY DESTROYED\r\n");
    EFISPrint(Ctx->ST->ConOut,
              L"=========================================================\r\n\r\n");

    for (UINTN i = 0; i < A->TargetCount; i++)
    {
        DISK  *D;
        UINT64 Bytes;

        if (A->Targets[i].Index >= Ctx->DiskCount)
            continue;

        D     = &Ctx->Disks[A->Targets[i].Index];
        Bytes = (D->LastBlock + 1) * (UINT64)D->BlockSize;

        SPrint(line, sizeof(line),
               L" disk%d  %-20s  %-16s  %ld MB  [%s]\r\n",
               (UINTN)D->Index, D->Model, D->Serial,
               Bytes / (1024 * 1024),
               WipeMethodName(WipeSelectMethod(D)));
        EFISPrint(Ctx->ST->ConOut, line);
    }

    SPrint(line, sizeof(line),
           L"\r\n %d drive(s) selected. This cannot be undone.\r\n",
           A->TargetCount);
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

    // --- execute ---
    for (UINTN i = 0; i < A->TargetCount; i++)
    {
        DISK       *D;
        EFI_STATUS  Status;
        WIPE_METHOD Method;

        if (A->Targets[i].Index >= Ctx->DiskCount)
        {
            Skipped++;
            continue;
        }

        D = &Ctx->Disks[A->Targets[i].Index];

        if (StrCmp(D->Serial, A->Targets[i].Serial) != 0)
        {
            SPrint(line, sizeof(line),
                   L"\r\ndisk%d SKIPPED: serial changed since selection\r\n",
                   (UINTN)D->Index);
            EFISPrint(Ctx->ST->ConOut, line);
            Skipped++;
            continue;
        }

        // Re-check: the selection may be minutes old.
        Status = WipeCheckTarget(Ctx, D);
        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line),
                   L"\r\ndisk%d SKIPPED: %r\r\n", (UINTN)D->Index, Status);
            EFISPrint(Ctx->ST->ConOut, line);
            Skipped++;
            continue;
        }

        Method = WipeSelectMethod(D);
        if (Method != WIPE_METHOD_OVERWRITE)
        {
            SPrint(line, sizeof(line),
                   L"\r\ndisk%d SKIPPED: %s not implemented\r\n",
                   (UINTN)D->Index, WipeMethodName(Method));
            EFISPrint(Ctx->ST->ConOut, line);
            Skipped++;
            continue;
        }

        SPrint(line, sizeof(line), L"\r\ndisk%d (%s): wiping...\r\n",
               (UINTN)D->Index, D->Serial);
        EFISPrint(Ctx->ST->ConOut, line);

        Status = WipeOverwrite(Ctx, D, 0x00);
        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line), L"disk%d FAILED: %r\r\n",
                   (UINTN)D->Index, Status);
            EFISPrint(Ctx->ST->ConOut, line);
            Failed++;
            continue;
        }

        EFISPrint(Ctx->ST->ConOut, L"  verifying...\r\n");
        Status = WipeVerification(Ctx, D, 0x00);
        if (EFI_ERROR(Status))
        {
            SPrint(line, sizeof(line),
                   L"disk%d FAILED VERIFICATION: %r\r\n",
                   (UINTN)D->Index, Status);
            EFISPrint(Ctx->ST->ConOut, line);
            Failed++;
            continue;
        }

        Succeeded++;
    }

    // --- summary ---
    EFISPrint(Ctx->ST->ConOut,
              L"\r\n---------------------------------------------------------\r\n");
    SPrint(line, sizeof(line),
           L" %d wiped and verified, %d failed, %d skipped\r\n",
           Succeeded, Failed, Skipped);
    EFISPrint(Ctx->ST->ConOut, line);

    // Wiped drives stay selected only if something went wrong, so a repeat
    // -x cannot silently re-run a batch that already succeeded.
    if (Failed == 0 && Skipped == 0)
        WipeSelectClear(Ctx);

    return (Failed > 0) ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}
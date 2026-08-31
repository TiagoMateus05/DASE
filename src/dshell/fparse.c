#include "fsparse.h"

VOID Fat32NameToChar16(FAT32_DIR_ENTRY *Entry, CHAR16 *Out)
{
    for (UINTN i = 0; i < Entry->NameLen; i++)
        Out[i] = (CHAR16)Entry->Name[i]; // widen: zero high byte, same trick as NibbleToHex

    Out[Entry->NameLen] = L'\0';
}

EFI_STATUS CmdFat32Test(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    EFI_STATUS Status;
    BLOCK_DEVICE Dev;
    UINT8 Sector0[512];
    FAT32_DIR_ENTRY Entries[16];
    UINTN EntryCount;
    CHAR16 line[300];
    CHAR16 nameBuf[MAX_NAME_LEN + 1];
    UINT8 FsHandle[256];
    DISK *Disk;

    Status = BuildDiskTable(Ctx);
    if (EFI_ERROR(Status))
        return Status;

    Status = FindDiskByIndex(Ctx, 0, &Disk);
    if (EFI_ERROR(Status))
        return Status;

    Status = BlockDeviceInit(&Dev, Disk->BlockIo, 0);
    if (EFI_ERROR(Status))
        return Status;

    Status = BlockDeviceRead(&Dev, 0, 1, Sector0);
    if (EFI_ERROR(Status))
    {
        EFISPrint(Ctx->ST->ConOut, L"Failed to read boot sector\r\n");
        return Status;
    }

    CHAR16 hsdbg[64];
    SPrint(hsdbg, sizeof(hsdbg), L"handle_size = %ld\r\n", fat32_handle_size());
    EFISPrint(Ctx->ST->ConOut, hsdbg);

    INTN MountResult = fat32_mount(FsHandle, Sector0, 512, DiskIoReadCallback, &Dev);
    CHAR16 mdbg[64];
    SPrint(mdbg, sizeof(mdbg), L"mount() raw return = %ld\r\n", MountResult);
    EFISPrint(Ctx->ST->ConOut, mdbg);

    if (MountResult != 0)
    {
        EFISPrint(Ctx->ST->ConOut, L"Not a valid FAT32 filesystem\r\n");
        return EFI_UNSUPPORTED;
    }

    FAT32_DEBUG_INFO Debug;
    INTN DebugResult = fat32_debug_info(FsHandle, &Debug);
    CHAR16 ddbg[64];
    SPrint(ddbg, sizeof(ddbg), L"debug_info() raw return = %ld\r\n", DebugResult);
    EFISPrint(Ctx->ST->ConOut, ddbg);

    if (DebugResult == 0)
    {
        CHAR16 dbgline[200];
        SPrint(dbgline, sizeof(dbgline),
               L"BPS=%d SPC=%d Reserved=%d NumFATs=%d SecPerFAT=%ld RootClu=%ld DataStart=%ld\r\n",
               Debug.BytesPerSector, Debug.SectorsPerCluster, Debug.ReservedSectorCount,
               Debug.NumFats, Debug.SectorsPerFat32, Debug.RootCluster, Debug.DataStartLba);
        EFISPrint(Ctx->ST->ConOut, dbgline);
    }

    EFISPrint(Ctx->ST->ConOut, L"Mounted. Root directory contents:\r\n");

    EFISPrint(Ctx->ST->ConOut, L"Mounted. Tree:\r\n");
    Fat32WalkTree(Ctx, FsHandle, 0); // 0 = root
    return EFI_SUCCESS;

    CHAR16 dbg[64];
    SPrint(dbg, sizeof(dbg), L"EntryCount = %ld\r\n", EntryCount);
    EFISPrint(Ctx->ST->ConOut, dbg);

    for (UINTN i = 0; i < EntryCount; i++)
    {
        Fat32NameToChar16(&Entries[i], nameBuf);
        SPrint(line, sizeof(line), L"%s  %s  size=%ld  cluster=%ld\r\n",
               Entries[i].IsDir ? L"[DIR]" : L"[FIL]",
               nameBuf, Entries[i].Size, Entries[i].Location);
        EFISPrint(Ctx->ST->ConOut, line);
    }

    return EFI_SUCCESS;
}

static BOOLEAN IsDotEntry(FAT32_DIR_ENTRY *Entry)
{
    if (Entry->NameLen == 1 && Entry->Name[0] == '.')
        return TRUE;
    if (Entry->NameLen == 2 && Entry->Name[0] == '.' && Entry->Name[1] == '.')
        return TRUE;
    return FALSE;
}

EFI_STATUS Fat32WalkTree(SHELL_CONTEXT *Ctx, VOID *Handle, UINT64 StartCluster)
{
    DIR_QUEUE_ITEM Queue[MAX_DIR_QUEUE];
    UINTN QueueHead = 0;
    UINTN QueueTail = 0;
    FAT32_DIR_ENTRY Entries[16];
    CHAR16 nameBuf[MAX_NAME_LEN + 1];
    CHAR16 line[400];

    Queue[QueueTail].Cluster = StartCluster;
    Queue[QueueTail].Depth = 0;
    QueueTail++;

    while (QueueHead < QueueTail)
    {
        UINT64 Cluster = Queue[QueueHead].Cluster;
        UINTN Depth = Queue[QueueHead].Depth;
        QueueHead++;

        UINTN Count = fat32_read_dir(Handle, Cluster, Entries, 16);

        for (UINTN i = 0; i < Count; i++)
        {
            Fat32NameToChar16(&Entries[i], nameBuf);

            // Indent by depth so the tree structure is visible
            CHAR16 indent[64];
            UINTN indentLen = Depth * 2;
            if (indentLen > 62)
                indentLen = 62;
            for (UINTN k = 0; k < indentLen; k++)
                indent[k] = L' ';
            indent[indentLen] = L'\0';

            SPrint(line, sizeof(line), L"%s%s  %s  size=%ld\r\n",
                   indent,
                   Entries[i].IsDir ? L"[DIR]" : L"[FIL]",
                   nameBuf,
                   Entries[i].Size);
            EFISPrint(Ctx->ST->ConOut, line);

            if (Entries[i].IsDir && !IsDotEntry(&Entries[i]))
            {
                if (QueueTail < MAX_DIR_QUEUE)
                {
                    Queue[QueueTail].Cluster = Entries[i].Location;
                    Queue[QueueTail].Depth = Depth + 1;
                    QueueTail++;
                }
                else
                {
                    EFISPrint(Ctx->ST->ConOut, L"(queue full, tree truncated)\r\n");
                }
            }
        }
    }

    return EFI_SUCCESS;
}
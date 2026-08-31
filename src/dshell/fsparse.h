#ifndef _FSPARSE_H_
#define _FSPARSE_H_

#include "../commands/blockdev.h"
#include "../dshell/shellcontext.h"
#include "../io/diskio.h"
#include <efi.h>

#define MAX_NAME_LEN 255

#define MAX_DIR_QUEUE 256

typedef struct
{
    UINT64 Cluster;
    UINTN Depth;
} DIR_QUEUE_ITEM;

typedef struct
{
    UINT8 Name[MAX_NAME_LEN];
    UINTN NameLen;
    BOOLEAN IsDir;
    UINT64 Size;
    UINT64 Location;
} FAT32_DIR_ENTRY;

typedef struct
{
    UINT16 BytesPerSector;
    UINT8 SectorsPerCluster;
    UINT16 ReservedSectorCount;
    UINT8 NumFats;
    UINT32 SectorsPerFat32;
    UINT32 RootCluster;
    UINT32 DataStartLba;
} FAT32_DEBUG_INFO;

extern UINTN fat32_handle_size(VOID);

extern INTN fat32_mount(VOID *Handle, CONST UINT8 *BootSector, UINTN BootSectorLen,
                        BLOCK_READ_FN ReadFn, VOID *Context);

extern UINTN fat32_read_dir(VOID *Handle, UINT64 Location,
                            FAT32_DIR_ENTRY *Out, UINTN OutCapacity);

extern INTN fat32_debug_info(VOID *Handle, FAT32_DEBUG_INFO *Out);

VOID Fat32NameToChar16(FAT32_DIR_ENTRY *Entry, CHAR16 *Out);
EFI_STATUS Fat32WalkTree(SHELL_CONTEXT *Ctx, VOID *Handle, UINT64 StartCluster);
EFI_STATUS CmdFat32Test(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv);

#endif
#include "nvm.h"
#include "../commands/blockdev.h"
#include "../dshell/shellcontext.h"
#include "../includes/wrapper.h"
#include "devpath.h"

static EFI_GUID gNvmePassThruProtocolGuid = EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL_GUID;

static EFI_STATUS NvmeAdminCmd(EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *Pt, UINT8 OpCode,
                               UINT32 Nsid, UINT32 Cdw10, UINT32 Cdw11,
                               VOID *Buffer, UINT32 Length, UINT64 Timeout,
                               UINT16 *OutNvmeStatus)
{
    EFI_NVM_EXPRESS_COMMAND Cmd;
    EFI_NVM_EXPRESS_COMPLETION Cpl;
    EFI_NVM_EXPRESS_PASS_THRU_COMMAND_PACKET Packet;
    EFI_STATUS Status;

    ZeroMem(&Cmd, sizeof(Cmd));
    ZeroMem(&Cpl, sizeof(Cpl));
    ZeroMem(&Packet, sizeof(Packet));

    Cmd.Cdw0.OpCode = OpCode;
    Cmd.Nsid = Nsid;
    Cmd.Cdw10 = Cdw10;
    Cmd.Cdw11 = Cdw11;
    Cmd.Flags = CDW10_VALID | CDW11_VALID;

    Packet.NvmeCmd = &Cmd;
    Packet.NvmeCompletion = &Cpl;
    Packet.TransferBuffer = Buffer;
    Packet.TransferLength = Length;
    Packet.QueueType = NVME_ADMIN_QUEUE;
    Packet.CommandTimeout = Timeout;

    Status = uefi_call_wrapper(Pt->PassThru, 4, Pt, Nsid, &Packet, NULL);

    // DW3: [24:17] Status Code, [27:25] Status Code Type
    if (OutNvmeStatus != NULL)
        *OutNvmeStatus = (UINT16)((Cpl.DW3 >> 17) & 0x7FF);
    return Status;
}

static EFI_STATUS NvmeAllocBuffer(SHELL_CONTEXT *Ctx, EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *Pt,
                                  UINTN Size, VOID **Out)
{
    EFI_STATUS Status;

    // AllocatePool only guarantees 8-byte alignment.
    if (Pt->Mode->IoAlign > 8)
        return EFI_UNSUPPORTED;

    Status = uefi_call_wrapper(Ctx->ST->BootServices->AllocatePool, 3,
                               EfiLoaderData, Size, Out);
    if (!EFI_ERROR(Status))
        ZeroMem(*Out, Size);
    return Status;
}

EFI_STATUS NvmeCountHandles(SHELL_CONTEXT *Ctx)
{
    EFI_HANDLE *Handles;
    UINTN Count = 0;
    CHAR16 line[80];
    EFI_STATUS Status;

    Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateHandleBuffer, 5,
                               ByProtocol, &gNvmePassThruProtocolGuid, NULL,
                               &Count, &Handles);
    SPrint(line, sizeof(line), L"NvmePassThru handles: %d (%r)\r\n",
           (UINTN)Count, Status);
    EFISPrint(Ctx->ST->ConOut, line);
    if (!EFI_ERROR(Status))
        FreePoolBS(Ctx->ST->BootServices, Handles);
    return EFI_SUCCESS;
}

EFI_STATUS NvmeGetPassThru(SHELL_CONTEXT *Ctx, DISK *Disk,
                           EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL **OutPt,
                           UINT32 *OutNsid)
{
    EFI_STATUS Status;
    EFI_DEVICE_PATH_PROTOCOL *WalkPath;
    EFI_HANDLE NvmeHandle;
    EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *Pt;

    if (Disk->DevicePath == NULL)
        return EFI_INVALID_PARAMETER;

    WalkPath = Disk->DevicePath;
    Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateDevicePath, 3,
                               &gNvmePassThruProtocolGuid, &WalkPath, &NvmeHandle);
    if (EFI_ERROR(Status))
        return EFI_NO_MAPPING;

    Status = uefi_call_wrapper(Ctx->ST->BootServices->HandleProtocol, 3,
                               NvmeHandle, &gNvmePassThruProtocolGuid,
                               (VOID **)&Pt);
    if (EFI_ERROR(Status))
        return EFI_PROTOCOL_ERROR;

    {
        EFI_DEVICE_PATH_PROTOCOL *DiskNode = LastMessagingNode(Disk->DevicePath);
        UINT32 Ns = 0xFFFFFFFF;

        if (DiskNode == NULL)
            return EFI_UNSUPPORTED;

        while (!EFI_ERROR(uefi_call_wrapper(Pt->GetNextNamespace, 2, Pt, &Ns)))
        {
            EFI_DEVICE_PATH_PROTOCOL *Built = NULL;
            BOOLEAN Match;

            Status = uefi_call_wrapper(Pt->BuildDevicePath, 3, Pt, Ns, &Built);
            if (EFI_ERROR(Status) || Built == NULL)
                continue;

            Match = (DevicePathNodeLength(Built) == DevicePathNodeLength(DiskNode)) &&
                    (CompareMem(Built, DiskNode, DevicePathNodeLength(Built)) == 0);

            FreePoolBS(Ctx->ST->BootServices, Built);

            if (Match)
            {
                *OutNsid = Ns;
                *OutPt = Pt;
                return EFI_SUCCESS;
            }
        }
    }

    return EFI_NOT_FOUND;
}

VOID NvmStringToChar16(UINT16 *Id, UINTN WordOffset, UINTN WordCount,
                       CHAR16 *Out)
{
    UINTN o = 0;

    for (UINTN i = 0; i < WordCount; i++)
    {
        UINT16 w = Id[WordOffset + i];
        Out[o++] = (CHAR16)((w >> 8) & 0xFF); // high byte first
        Out[o++] = (CHAR16)(w & 0xFF);
    }
    Out[o] = L'\0';

    while (o > 0 && Out[o - 1] == L' ')
        Out[--o] = L'\0';
}

static UINT32 NvmeCountActiveNamespaces(SHELL_CONTEXT *Ctx,
                                        EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *Pt,
                                        UINT32 Nn)
{
    UINT32 *List = NULL;
    UINT32 Count = 0;

    if (EFI_ERROR(NvmeAllocBuffer(Ctx, Pt, 4096, (VOID **)&List)))
        return 0;

    // CNS 02h (NVMe 1.1+): up to 1024 active NSIDs above Nsid, zero-terminated.
    // GetNextNamespace is no substitute -- EDK2 walks every NSID up to NN,
    // active or not.
    if (!EFI_ERROR(NvmeAdminCmd(Pt, NVME_ADMIN_IDENTIFY, 0, NVME_CNS_ACTIVE_NS_LIST, 0,
                                List, 4096, NVME_ADMIN_TIMEOUT, NULL)))
    {
        while (Count < 1024 && List[Count] != 0)
            Count++;
    }
    else if (Nn == 1)
    {
        Count = 1; // NVMe 1.0 has no list, but one NSID leaves no doubt
    }

    FreePoolBS(Ctx->ST->BootServices, List);
    return Count;
}

EFI_STATUS NvmeIdentifyController(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *Pt = NULL;
    UINT32 Nsid = 0;
    UINT8 *Id = NULL;
    EFI_STATUS Status;

    Status = NvmeGetPassThru(Ctx, Disk, &Pt, &Nsid);
    if (EFI_ERROR(Status))
        return Status;

    Status = NvmeAllocBuffer(Ctx, Pt, 4096, (VOID **)&Id);
    if (EFI_ERROR(Status))
        return Status;

    Status = NvmeAdminCmd(Pt, NVME_ADMIN_IDENTIFY, 0, NVME_CNS_CONTROLLER, 0,
                          Id, 4096, NVME_ADMIN_TIMEOUT, NULL);
    if (EFI_ERROR(Status))
    {
        FreePoolBS(Ctx->ST->BootServices, Id);
        return Status;
    }

    Disk->NvmePassThru = Pt;
    Disk->NvmeNsid = Nsid;

    // Serial   : bytes 4..23   (20 ASCII)
    // Model    : bytes 24..63  (40 ASCII)
    // Firmware : bytes 64..71  (8 ASCII)
    // OACS     : bytes 256..257 (UINT16)
    // SANICAP  : bytes 328..331 (UINT32)
    // NN       : bytes 516..519 (UINT32)
    // FNA      : byte  524
    {
        CHAR16 buf[41];
        UINTN n;

        // ASCII, no swap
        n = 0;
        for (UINTN i = 4; i <= 23; i++)
            buf[n++] = (CHAR16)Id[i];
        buf[n] = 0;
        while (n > 0 && buf[n - 1] == L' ')
            buf[--n] = 0;
        StrCpy(Disk->Serial, buf);

        n = 0;
        for (UINTN i = 24; i <= 63; i++)
            buf[n++] = (CHAR16)Id[i];
        buf[n] = 0;
        while (n > 0 && buf[n - 1] == L' ')
            buf[--n] = 0;
        StrCpy(Disk->Model, buf);

        n = 0;
        for (UINTN i = 64; i <= 71; i++)
            buf[n++] = (CHAR16)Id[i];
        buf[n] = 0;
        while (n > 0 && buf[n - 1] == L' ')
            buf[--n] = 0;
        StrCpy(Disk->FirmwareRev, buf);
    }

    UINT16 Oacs = (UINT16)(Id[256] | (Id[257] << 8));
    UINT32 Sanicap = (UINT32)Id[328] | ((UINT32)Id[329] << 8) |
                     ((UINT32)Id[330] << 16) | ((UINT32)Id[331] << 24);
    UINT32 Nn = (UINT32)Id[516] | ((UINT32)Id[517] << 8) |
                ((UINT32)Id[518] << 16) | ((UINT32)Id[519] << 24);

    Disk->NvmeFormatSupported = (Oacs & NVME_OACS_FORMAT) ? TRUE : FALSE;

    Disk->NvmeSanitizeCryptoSupported = (Sanicap & NVME_SANICAP_CRYPTO_ERASE) ? TRUE : FALSE;
    Disk->NvmeSanitizeBlockEraseSupported = (Sanicap & NVME_SANICAP_BLOCK_ERASE) ? TRUE : FALSE;
    Disk->NvmeSanitizeOverwriteSupported = (Sanicap & NVME_SANICAP_OVERWRITE) ? TRUE : FALSE;
    Disk->NvmeSanitizeSupported = Disk->NvmeSanitizeCryptoSupported ||
                                  Disk->NvmeSanitizeBlockEraseSupported ||
                                  Disk->NvmeSanitizeOverwriteSupported;

    Disk->NvmeFna = Id[524];

    Disk->Media = MEDIA_SOLID_STATE;

    FreePoolBS(Ctx->ST->BootServices, Id);

    Disk->NvmeNamespaceCount = NvmeCountActiveNamespaces(Ctx, Pt, Nn);
    return EFI_SUCCESS;
}

EFI_STATUS NvmeIdentifyNamespace(SHELL_CONTEXT *Ctx, DISK *Disk)
{
    EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *Pt = Disk->NvmePassThru;
    UINT8 *Id = NULL;
    UINT64 Nsze = 0;
    UINTN LbafIndex;
    UINT8 Lbads;
    EFI_STATUS Status;

    if (Pt == NULL)
        return EFI_NOT_READY;

    Status = NvmeAllocBuffer(Ctx, Pt, 4096, (VOID **)&Id);
    if (EFI_ERROR(Status))
        return Status;

    Status = NvmeAdminCmd(Pt, NVME_ADMIN_IDENTIFY, Disk->NvmeNsid, NVME_CNS_NAMESPACE, 0,
                          Id, 4096, NVME_ADMIN_TIMEOUT, NULL);
    if (EFI_ERROR(Status))
    {
        FreePoolBS(Ctx->ST->BootServices, Id);
        return Status;
    }

    // NSZE   : bytes 0..7   (UINT64, in logical blocks)
    // FLBAS  : byte  26     [3:0] format index, [4] extended metadata, [6:5] index hi
    // DPS    : byte  29     [2:0] PI type, [3] PI in first 8 bytes of metadata
    // DLFEAT : byte  33
    // LBAF[] : bytes 128.., 4 each; byte 2 of each is LBADS (log2 block size)
    for (UINTN i = 0; i < 8; i++)
        Nsze |= (UINT64)Id[i] << (8 * i);

    Disk->NvmeFlbas = Id[26];
    Disk->NvmeDps = Id[29];
    Disk->NvmeDlfeat = Id[33];

    LbafIndex = (Disk->NvmeFlbas & 0x0F) | (((Disk->NvmeFlbas >> 5) & 0x03) << 4);
    Lbads = Id[128 + LbafIndex * 4 + 2];

    FreePoolBS(Ctx->ST->BootServices, Id);

    if (Nsze != Disk->LastBlock + 1 || Lbads >= 32 ||
        ((UINT32)1 << Lbads) != Disk->BlockSize)
        return EFI_NOT_FOUND;

    return EFI_SUCCESS;
}

EFI_STATUS NvmePopulateTable(SHELL_CONTEXT *Ctx)
{
    for (UINTN i = 0; i < Ctx->DiskCount; i++)
    {
        DISK *D = &Ctx->Disks[i];

        if (D->Transport != DISK_TRANSPORT_NVME)
            continue;

        D->NvmeStatus = NvmeIdentifyController(Ctx, D);
        if (!EFI_ERROR(D->NvmeStatus))
            D->NvmeStatus = NvmeIdentifyNamespace(Ctx, D);
    }
    return EFI_SUCCESS;
}

EFI_STATUS NvmeFormat(SHELL_CONTEXT *Ctx, DISK *Disk, UINT32 Ses, UINT16 *OutNvmeStatus)
{
    UINT32 Cdw10, Nsid;

    if (Disk->NvmePassThru == NULL)
        return EFI_NOT_READY;

    Cdw10 = (Disk->NvmeFlbas & 0x0F)                 // [3:0]   LBAF
            | ((Disk->NvmeFlbas >> 4) & 0x01) << 4   // [4]     MSET
            | (Disk->NvmeDps & 0x07) << 5            // [7:5]   PI
            | ((Disk->NvmeDps >> 3) & 0x01) << 8     // [8]     PIL
            | (Ses & 0x07) << 9                      // [11:9]  SES
            | ((Disk->NvmeFlbas >> 5) & 0x03) << 12; // [13:12] LBAFU

    Nsid = (Disk->NvmeFna & (NVME_FNA_FORMAT_ALL_NS | NVME_FNA_ERASE_ALL_NS))
               ? NVME_ALL_NAMESPACES
               : Disk->NvmeNsid;

    return NvmeAdminCmd(Disk->NvmePassThru, NVME_ADMIN_FORMAT_NVM, Nsid, Cdw10, 0,
                        NULL, 0, NVME_FORMAT_TIMEOUT, OutNvmeStatus);
}

EFI_STATUS NvmeSanitizeStart(SHELL_CONTEXT *Ctx, DISK *Disk, UINT32 Action, UINT16 *OutNvmeStatus)
{
    UINT32 Cdw10 = Action & 0x07; // [2:0] SANACT; AUSE and NDAS left 0

    if (Disk->NvmePassThru == NULL)
        return EFI_NOT_READY;

    // [7:4] OWPASS: 0 would mean 16 passes. One pass of zeros (CDW11).
    if (Action == NVME_SANACT_OVERWRITE)
        Cdw10 |= 1 << 4;

    return NvmeAdminCmd(Disk->NvmePassThru, NVME_ADMIN_SANITIZE, 0, Cdw10, 0,
                        NULL, 0, NVME_SANITIZE_TIMEOUT, OutNvmeStatus);
}

EFI_STATUS NvmeSanitizeStatus(SHELL_CONTEXT *Ctx, DISK *Disk, UINT16 *OutProgress, UINT16 *OutStatus)
{
    UINT8 *Log = NULL;
    EFI_STATUS Status;

    if (Disk->NvmePassThru == NULL)
        return EFI_NOT_READY;

    Status = NvmeAllocBuffer(Ctx, Disk->NvmePassThru, NVME_LOG_SANITIZE_STATUS_SIZE,
                             (VOID **)&Log);
    if (EFI_ERROR(Status))
        return Status;

    // CDW10: [7:0] LID, [31:16] NUMDL (0-based dword count)
    Status = NvmeAdminCmd(Disk->NvmePassThru, NVME_ADMIN_GET_LOG_PAGE, NVME_ALL_NAMESPACES,
                          NVME_LOG_SANITIZE_STATUS |
                              ((NVME_LOG_SANITIZE_STATUS_SIZE / 4 - 1) << 16),
                          0, Log, NVME_LOG_SANITIZE_STATUS_SIZE, NVME_ADMIN_TIMEOUT, NULL);
    if (!EFI_ERROR(Status))
    {
        *OutProgress = (UINT16)(Log[0] | (Log[1] << 8)); // SPROG: n / 65536
        *OutStatus = (UINT16)(Log[2] | (Log[3] << 8));   // SSTAT
    }

    FreePoolBS(Ctx->ST->BootServices, Log);
    return Status;
}
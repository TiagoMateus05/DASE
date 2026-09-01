#include "ata.h"
#include "../commands/blockdev.h"
#include "../dshell/shellcontext.h"
#include "../includes/wrapper.h"

static EFI_GUID gAtaPassThruProtocolGuid = EFI_ATA_PASS_THRU_PROTOCOL_GUID;

static EFI_DEVICE_PATH_PROTOCOL *LastMessagingNode(EFI_DEVICE_PATH_PROTOCOL *Path)
{
    EFI_DEVICE_PATH_PROTOCOL *Node = Path;
    EFI_DEVICE_PATH_PROTOCOL *Last = NULL;

    while (!IsDevicePathEnd(Node))
    {
        if (DevicePathType(Node) == MESSAGING_DEVICE_PATH)
            Last = Node;
        Node = NextDevicePathNode(Node);
    }
    return Last;
}

EFI_STATUS AtaGetPassThru(SHELL_CONTEXT *Ctx, DISK *Disk,
                          EFI_ATA_PASS_THRU_PROTOCOL **OutPassThru,
                          UINT16 *OutPort, UINT16 *OutPmPort)
{
    EFI_STATUS                  Status;
    EFI_DEVICE_PATH_PROTOCOL   *WalkPath;
    EFI_DEVICE_PATH_PROTOCOL   *DiskNode;
    EFI_HANDLE                  AtaHandle;
    EFI_ATA_PASS_THRU_PROTOCOL *AtaPassThru;
    UINT16                      Port = 0xFFFF;

    if (Disk->DevicePath == NULL)
        return EFI_INVALID_PARAMETER;

    WalkPath = Disk->DevicePath;
    Status = uefi_call_wrapper(Ctx->ST->BootServices->LocateDevicePath, 3,
                               &gAtaPassThruProtocolGuid, &WalkPath, &AtaHandle);
    if (EFI_ERROR(Status))
        return EFI_NO_MAPPING;

    Status = uefi_call_wrapper(Ctx->ST->BootServices->HandleProtocol, 3,
                               AtaHandle, &gAtaPassThruProtocolGuid,
                               (VOID **)&AtaPassThru);
    if (EFI_ERROR(Status))
        return EFI_PROTOCOL_ERROR;

    DiskNode = LastMessagingNode(Disk->DevicePath);
    if (DiskNode == NULL)
        return EFI_UNSUPPORTED;

    while (!EFI_ERROR(uefi_call_wrapper(AtaPassThru->GetNextPort, 2,
                                        AtaPassThru, &Port)))
    {
        UINT16 PmPort = 0xFFFF;

        while (!EFI_ERROR(uefi_call_wrapper(AtaPassThru->GetNextDevice, 3,
                                            AtaPassThru, Port, &PmPort)))
        {
            EFI_DEVICE_PATH_PROTOCOL *Built = NULL;
            BOOLEAN                   Match;

            Status = uefi_call_wrapper(AtaPassThru->BuildDevicePath, 4,
                                       AtaPassThru, Port, PmPort, &Built);
            if (EFI_ERROR(Status) || Built == NULL)
                continue;

            Match = (DevicePathNodeLength(Built) == DevicePathNodeLength(DiskNode)) &&
                    (CompareMem(Built, DiskNode, DevicePathNodeLength(Built)) == 0);

            FreePoolBS(Ctx->ST->BootServices, Built);

            if (Match)
            {
                *OutPort     = Port;
                *OutPmPort   = PmPort;
                *OutPassThru = AtaPassThru;
                return EFI_SUCCESS;
            }
        }
    }

    return EFI_NOT_FOUND;
}

VOID AtaStringToChar16(UINT16 *Id, UINTN WordOffset, UINTN WordCount,
                              CHAR16 *Out)
{
    UINTN o = 0;

    for (UINTN i = 0; i < WordCount; i++)
    {
        UINT16 w = Id[WordOffset + i];
        Out[o++] = (CHAR16)((w >> 8) & 0xFF);   // high byte first
        Out[o++] = (CHAR16)(w & 0xFF);
    }
    Out[o] = L'\0';

    // Trim trailing spaces -- ATA pads with them.
    while (o > 0 && Out[o - 1] == L' ')
        Out[--o] = L'\0';
}

EFI_STATUS AtaIdentify(SHELL_CONTEXT *Ctx, DISK *Disk, UINT16 *IdBuf)
{
    EFI_ATA_PASS_THRU_PROTOCOL       *Pt = NULL;
    UINT16                            Port = 0, PmPort = 0;
    EFI_ATA_STATUS_BLOCK              Asb;
    EFI_ATA_COMMAND_BLOCK             Acb;
    EFI_ATA_PASS_THRU_COMMAND_PACKET  Packet;
    EFI_STATUS                        Status;

    Status = AtaGetPassThru(Ctx, Disk, &Pt, &Port, &PmPort);
    if (EFI_ERROR(Status))
        return Status;

    ZeroMem(&Asb,    sizeof(Asb));
    ZeroMem(&Acb,    sizeof(Acb));
    ZeroMem(&Packet, sizeof(Packet));

    Acb.AtaCommand     = ATA_CMD_IDENTIFY_DEVICE;
    Acb.AtaSectorCount = 1;

    Packet.Asb              = &Asb;
    Packet.Acb              = &Acb;
    Packet.InDataBuffer     = IdBuf;
    Packet.InTransferLength = 512;
    Packet.Protocol         = EFI_ATA_PASS_THRU_CMD_PROTOCOL_PIO_DATA_IN;
    Packet.Length           = EFI_ATA_PASS_THRU_LENGTH_BYTES;
    Packet.Timeout          = 30ULL * 10 * 1000 * 1000;   // 30s, 100ns units

    Status = uefi_call_wrapper(Pt->PassThru, 5, Pt, Port, PmPort, &Packet, NULL);
    if (EFI_ERROR(Status))
        return Status;

    Disk->AtaPassThru = Pt;
    Disk->AtaPort     = Port;
    Disk->AtaPmPort   = PmPort;
    return EFI_SUCCESS;
}

EFI_STATUS AtaPopulateTable(SHELL_CONTEXT *Ctx)
{
    for (UINTN i = 0; i < Ctx->DiskCount; i++)
    {
        DISK   *D = &Ctx->Disks[i];
        UINT16  Id[256] __attribute__((aligned(16)));
        UINT16  Sec;

        if (D->Transport != DISK_TRANSPORT_SATA &&
            D->Transport != DISK_TRANSPORT_IDE)
        {
            if (D->Transport == DISK_TRANSPORT_NVME)
                D->Media = MEDIA_SOLID_STATE;

            D->AtaStatus = EFI_UNSUPPORTED;
            continue;
        }

        D->AtaStatus = AtaIdentify(Ctx, D, Id);
        if (EFI_ERROR(D->AtaStatus))
            continue;

        AtaStringToChar16(Id, ATA_ID_WORD_MODEL,    20, D->Model);
        AtaStringToChar16(Id, ATA_ID_WORD_SERIAL,   10, D->Serial);
        AtaStringToChar16(Id, ATA_ID_WORD_FIRMWARE,  4, D->FirmwareRev);

        D->RotationRate = Id[ATA_ID_WORD_ROTATION_RATE];

        if (D->RotationRate == ATA_ROTATION_SOLID_STATE)
            D->Media = MEDIA_SOLID_STATE;
        else if (D->RotationRate == ATA_ROTATION_NOT_REPORTED)
            D->Media = MEDIA_UNKNOWN;    // NOT the same as rotational
        else
            D->Media = MEDIA_ROTATIONAL;

        Sec = Id[ATA_ID_WORD_SECURITY_STATUS];
        D->SecuritySupported      = (Sec & ATA_SEC_SUPPORTED)      ? TRUE : FALSE;
        D->SecurityFrozen         = (Sec & ATA_SEC_FROZEN)         ? TRUE : FALSE;
        D->EnhancedEraseSupported = (Sec & ATA_SEC_ENHANCED_ERASE) ? TRUE : FALSE;
    }
    return EFI_SUCCESS;
}
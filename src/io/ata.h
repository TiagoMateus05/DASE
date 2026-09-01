#ifndef _ATA_H_
#define _ATA_H_

#include "../commands/blockdev.h"
#include <efi.h>

//
// EFI_ATA_PASS_THRU_PROTOCOL. Not present in gnu-efi's headers, so the
// structures below are transcribed from the UEFI Specification,
// "ATA Pass Thru Protocol". Field order is load-bearing -- do not reorder.
//

#define EFI_ATA_PASS_THRU_PROTOCOL_GUID \
    {0x1d3de7f0, 0x0807, 0x424f, {0xaa, 0x69, 0x11, 0xa5, 0x4e, 0x19, 0xa4, 0x6f}}

typedef struct _EFI_ATA_PASS_THRU_PROTOCOL EFI_ATA_PASS_THRU_PROTOCOL;

// Mode->Attributes bits
#define EFI_ATA_PASS_THRU_ATTRIBUTES_PHYSICAL 0x0001
#define EFI_ATA_PASS_THRU_ATTRIBUTES_LOGICAL 0x0002
#define EFI_ATA_PASS_THRU_ATTRIBUTES_NONBLOCKIO 0x0004

typedef struct
{
    UINT32 Attributes;
    UINT32 IoAlign;
} EFI_ATA_PASS_THRU_MODE;

// Register images returned by the device after the command completes.
typedef struct
{
    UINT8 Reserved1[2];
    UINT8 AtaStatus;
    UINT8 AtaError;
    UINT8 AtaSectorNumber;
    UINT8 AtaCylinderLow;
    UINT8 AtaCylinderHigh;
    UINT8 AtaDeviceHead;
    UINT8 AtaSectorNumberExp;
    UINT8 AtaCylinderLowExp;
    UINT8 AtaCylinderHighExp;
    UINT8 Reserved2;
    UINT8 AtaSectorCount;
    UINT8 AtaSectorCountExp;
    UINT8 Reserved3[6];
} EFI_ATA_STATUS_BLOCK;

// Register images written to the device to issue the command.
typedef struct
{
    UINT8 Reserved1[2];
    UINT8 AtaCommand;
    UINT8 AtaFeatures;
    UINT8 AtaSectorNumber;
    UINT8 AtaCylinderLow;
    UINT8 AtaCylinderHigh;
    UINT8 AtaDeviceHead;
    UINT8 AtaSectorNumberExp;
    UINT8 AtaCylinderLowExp;
    UINT8 AtaCylinderHighExp;
    UINT8 AtaFeaturesExp;
    UINT8 AtaSectorCount;
    UINT8 AtaSectorCountExp;
    UINT8 Reserved2[6];
} EFI_ATA_COMMAND_BLOCK;

typedef struct
{
    EFI_ATA_STATUS_BLOCK *Asb;
    EFI_ATA_COMMAND_BLOCK *Acb;
    UINT64 Timeout; // 100ns units; 0 == wait forever
    VOID *InDataBuffer;
    VOID *OutDataBuffer;
    UINT32 InTransferLength;
    UINT32 OutTransferLength;
    UINT8 Protocol; // EFI_ATA_PASS_THRU_CMD_PROTOCOL_*
    UINT8 Length;   // EFI_ATA_PASS_THRU_LENGTH_*
} EFI_ATA_PASS_THRU_COMMAND_PACKET;

typedef EFI_STATUS(EFIAPI *EFI_ATA_PASS_THRU_PASSTHRU)(
    IN EFI_ATA_PASS_THRU_PROTOCOL *This,
    IN UINT16 Port,
    IN UINT16 PortMultiplierPort,
    IN OUT EFI_ATA_PASS_THRU_COMMAND_PACKET *Packet,
    IN EFI_EVENT Event OPTIONAL);

typedef EFI_STATUS(EFIAPI *EFI_ATA_PASS_THRU_GET_NEXT_PORT)(
    IN EFI_ATA_PASS_THRU_PROTOCOL *This,
    IN OUT UINT16 *Port);

typedef EFI_STATUS(EFIAPI *EFI_ATA_PASS_THRU_GET_NEXT_DEVICE)(
    IN EFI_ATA_PASS_THRU_PROTOCOL *This,
    IN UINT16 Port,
    IN OUT UINT16 *PortMultiplierPort);

typedef EFI_STATUS(EFIAPI *EFI_ATA_PASS_THRU_BUILD_DEVICE_PATH)(
    IN EFI_ATA_PASS_THRU_PROTOCOL *This,
    IN UINT16 Port,
    IN UINT16 PortMultiplierPort,
    IN OUT EFI_DEVICE_PATH_PROTOCOL **DevicePath);

typedef EFI_STATUS(EFIAPI *EFI_ATA_PASS_THRU_GET_DEVICE)(
    IN EFI_ATA_PASS_THRU_PROTOCOL *This,
    IN EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    OUT UINT16 *Port,
    OUT UINT16 *PortMultiplierPort);

typedef EFI_STATUS(EFIAPI *EFI_ATA_PASS_THRU_RESET_PORT)(
    IN EFI_ATA_PASS_THRU_PROTOCOL *This,
    IN UINT16 Port);

typedef EFI_STATUS(EFIAPI *EFI_ATA_PASS_THRU_RESET_DEVICE)(
    IN EFI_ATA_PASS_THRU_PROTOCOL *This,
    IN UINT16 Port,
    IN UINT16 PortMultiplierPort);

struct _EFI_ATA_PASS_THRU_PROTOCOL
{
    EFI_ATA_PASS_THRU_MODE *Mode;
    EFI_ATA_PASS_THRU_PASSTHRU PassThru;
    EFI_ATA_PASS_THRU_GET_NEXT_PORT GetNextPort;
    EFI_ATA_PASS_THRU_GET_NEXT_DEVICE GetNextDevice;
    EFI_ATA_PASS_THRU_BUILD_DEVICE_PATH BuildDevicePath;
    EFI_ATA_PASS_THRU_GET_DEVICE GetDevice;
    EFI_ATA_PASS_THRU_RESET_PORT ResetPort;
    EFI_ATA_PASS_THRU_RESET_DEVICE ResetDevice;
};

//
// Packet->Protocol values (ATA command protocols)
//
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_ATA_HARDWARE_RESET 0x00
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_ATA_SOFTWARE_RESET 0x01
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_ATA_NON_DATA 0x02
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_PIO_DATA_IN 0x04
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_PIO_DATA_OUT 0x05
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_DMA 0x06
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_DMA_QUEUED 0x07
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_DEVICE_DIAGNOSTIC 0x08
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_DEVICE_RESET 0x09
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_UDMA_DATA_IN 0x0A
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_UDMA_DATA_OUT 0x0B
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_FPDMA 0x0C
#define EFI_ATA_PASS_THRU_CMD_PROTOCOL_RETURN_RESPONSE 0xFF

//
// Packet->Length: which command-block field carries the transfer length
//
#define EFI_ATA_PASS_THRU_LENGTH_BYTES 0x80
#define EFI_ATA_PASS_THRU_LENGTH_MASK 0x30
#define EFI_ATA_PASS_THRU_LENGTH_NO_DATA_TRANSFER 0x00
#define EFI_ATA_PASS_THRU_LENGTH_FEATURES 0x10
#define EFI_ATA_PASS_THRU_LENGTH_SECTOR_COUNT 0x20
#define EFI_ATA_PASS_THRU_LENGTH_TPSIU 0x30

//
// ATA command opcodes (ACS-4)
//
#define ATA_CMD_IDENTIFY_DEVICE 0xEC
#define ATA_CMD_SECURITY_SET_PASS 0xF1
#define ATA_CMD_SECURITY_UNLOCK 0xF2
#define ATA_CMD_SECURITY_ERASE_PREP 0xF3
#define ATA_CMD_SECURITY_ERASE_UNIT 0xF4
#define ATA_CMD_SANITIZE 0xB4

//
// IDENTIFY DEVICE result: 512 bytes == 256 little-endian 16-bit words.
// Word offsets (ACS-4, IDENTIFY DEVICE data table).
//
#define ATA_ID_WORD_SERIAL 10   // words 10-19, 20 chars, byte-pair swapped
#define ATA_ID_WORD_FIRMWARE 23 // words 23-26, 8 chars
#define ATA_ID_WORD_MODEL 27    // words 27-46, 40 chars
#define ATA_ID_WORD_CAPABILITIES 49
#define ATA_ID_WORD_SECURITY_STATUS 128
#define ATA_ID_WORD_DATA_SET_MGMT 169 // bit 0: TRIM supported
#define ATA_ID_WORD_ROTATION_RATE 217 // 0x0001 = non-rotating; 0 = not reported

// Word 128 (security status) bits
#define ATA_SEC_SUPPORTED (1 << 0)
#define ATA_SEC_ENABLED (1 << 1)
#define ATA_SEC_LOCKED (1 << 2)
#define ATA_SEC_FROZEN (1 << 3)
#define ATA_SEC_COUNT_EXPIRED (1 << 4)
#define ATA_SEC_ENHANCED_ERASE (1 << 5)

// Word 217 sentinel values
#define ATA_ROTATION_NOT_REPORTED 0x0000
#define ATA_ROTATION_SOLID_STATE 0x0001

EFI_STATUS AtaGetPassThru(SHELL_CONTEXT *Ctx, DISK *Disk, EFI_ATA_PASS_THRU_PROTOCOL **OutPassThru, UINT16 *OutPort, UINT16 *OutPmPort);
VOID AtaStringToChar16(UINT16 *Id, UINTN WordOffset, UINTN WordCount, CHAR16 *Out);
EFI_STATUS AtaIdentify(SHELL_CONTEXT *Ctx, DISK *Disk, UINT16 *IdBuf);
EFI_STATUS AtaPopulateTable(SHELL_CONTEXT *Ctx);

#endif
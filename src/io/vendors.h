#ifndef _VENDORS_H_
#define _VENDORS_H_

// This is the list of assigned vendor IDs. Needs updating if vendors
// are not in this list
#include <efi.h>
#include <efilib.h>

typedef enum
{
    DISK_TRANSPORT_UNKNOWN = 0, // refuse to wipe
    DISK_TRANSPORT_SATA,
    DISK_TRANSPORT_NVME,
    DISK_TRANSPORT_USB,
    DISK_TRANSPORT_IDE,
    DISK_TRANSPORT_SAS,
    DISK_TRANSPORT_RAID, // virtual disk: refuse, tell user to set IT mode
} DISK_TRANSPORT;

typedef struct
{
    UINT16 Id;
    CONST CHAR16 *Name;
} PCI_VENDOR;

STATIC CONST CHAR16 *PciVendorName(UINT16 Id);
CONST CHAR16 *PciVendorName(UINT16 Id);
DISK_TRANSPORT ClassifyTransport(UINT8 Class, UINT8 Sub, UINT8 ProgIF);
CONST CHAR16 *TransportName(DISK_TRANSPORT T);

#endif
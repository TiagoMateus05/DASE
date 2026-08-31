#ifndef _VENDORS_H_
#define _VENDORS_H_

// This is the list of assigned vendor IDs. Needs updating if vendors
// are not in this list
#include <efi.h>
#include <efilib.h>

typedef struct {
    UINT16        Id;
    CONST CHAR16 *Name;
} PCI_VENDOR;

STATIC CONST CHAR16 *PciVendorName(UINT16 Id);
CONST CHAR16 *PciVendorName(UINT16 Id);

#endif
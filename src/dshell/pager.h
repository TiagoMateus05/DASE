#ifndef _PAGER_H_
#define _PAGER_H_

#include <efi.h>

#include "../includes/wrapper.h"

VOID PagerTool(EFI_SYSTEM_TABLE *ST, CHAR16 **Lines, UINTN LineCount);

#endif
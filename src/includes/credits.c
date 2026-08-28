#include <efi.h>

#include "credits.h"
#include "wrapper.h"

VOID _WaitForKey(EFI_SYSTEM_TABLE *ST)
{
    UINTN index;
    EFI_INPUT_KEY key;
    WaitForKey(ST, &index);
    ReadKeyStroke(ST->ConIn, &key);
}

VOID Credits(EFI_SYSTEM_TABLE *ST)
{
    // PAGE 1
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    EFISPrint(ST->ConOut, L"====================================================\r\n");
    EFISPrint(ST->ConOut, L" Disk Auditor and Secure Eraser v0.1\r\n");
    EFISPrint(ST->ConOut, L" Copyright (c) 2026 Tiago Mateus. All rights reserved.\r\n");
    EFISPrint(ST->ConOut, L"====================================================\r\n");
    EFISPrint(ST->ConOut, L"\r\n");
    EFISPrint(ST->ConOut, L" DASE COMMUNITY LICENSE v1.0\r\n");
    EFISPrint(ST->ConOut, L"\r\n");
    EFISPrint(ST->ConOut, L" 1. GRANT - PERSONAL / NON-PROFIT / EDUCATIONAL USE\r\n");
    EFISPrint(ST->ConOut, L"    Permission is granted, free of charge, to any\r\n");
    EFISPrint(ST->ConOut, L"    individual, non-profit organization, or accredited\r\n");
    EFISPrint(ST->ConOut, L"    educational institution to use, copy, modify, and\r\n");
    EFISPrint(ST->ConOut, L"    distribute this software for any purpose, subject\r\n");
    EFISPrint(ST->ConOut, L"    to the conditions below.\r\n");
    EFISPrint(ST->ConOut, L"\r\n");
    EFISPrint(ST->ConOut, L" 2. GRANT - COMMERCIAL USE\r\n");
    EFISPrint(ST->ConOut, L"    Any for-profit company or commercial organization\r\n");
    EFISPrint(ST->ConOut, L"    using this software in the course of its business\r\n");
    EFISPrint(ST->ConOut, L"    operations must obtain a Commercial License from\r\n");
    EFISPrint(ST->ConOut, L"    the copyright holder. Current fee: EUR 20/month\r\n");
    EFISPrint(ST->ConOut, L"    per organization. See contact info below.\r\n");
    EFISPrint(ST->ConOut, L"\r\n");
    EFISPrint(ST->ConOut, L"-- Press any key for more (1/2) --\r\n");
    _WaitForKey(ST);

    // PAGE 2
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    EFISPrint(ST->ConOut, L" 3. CONDITIONS\r\n");
    EFISPrint(ST->ConOut, L"    a. The above copyright notice and this license\r\n");
    EFISPrint(ST->ConOut, L"       must be included in all copies or substantial\r\n");
    EFISPrint(ST->ConOut, L"       portions of the software.\r\n");
    EFISPrint(ST->ConOut, L"    b. Source code modifications must be documented\r\n");
    EFISPrint(ST->ConOut, L"       and, if redistributed, made available under\r\n");
    EFISPrint(ST->ConOut, L"       this same license.\r\n");
    EFISPrint(ST->ConOut, L"    c. THIS SOFTWARE IS PROVIDED \"AS IS\", WITHOUT\r\n");
    EFISPrint(ST->ConOut, L"       WARRANTY OF ANY KIND. The copyright holder\r\n");
    EFISPrint(ST->ConOut, L"       makes no guarantee that any data-erasure\r\n");
    EFISPrint(ST->ConOut, L"       function fully and irrecoverably destroys data\r\n");
    EFISPrint(ST->ConOut, L"       on all storage media types, and disclaims all\r\n");
    EFISPrint(ST->ConOut, L"       liability for data loss or damages arising\r\n");
    EFISPrint(ST->ConOut, L"       from use of this software, to the maximum\r\n");
    EFISPrint(ST->ConOut, L"       extent permitted by law.\r\n");
    EFISPrint(ST->ConOut, L"\r\n");
    EFISPrint(ST->ConOut, L" 4. DEFINITIONS\r\n");
    EFISPrint(ST->ConOut, L"    \"Commercial organization\" excludes registered\r\n");
    EFISPrint(ST->ConOut, L"    non-profits and accredited universities/schools,\r\n");
    EFISPrint(ST->ConOut, L"    which fall under Section 1 regardless of size.\r\n");
    EFISPrint(ST->ConOut, L"\r\n");
    EFISPrint(ST->ConOut, L" For commercial licensing check information\r\n");
    EFISPrint(ST->ConOut, L"====================================================\r\n");
    EFISPrint(ST->ConOut, L"\r\n");
    EFISPrint(ST->ConOut, L"Press any key to continue...\r\n");
    _WaitForKey(ST);

    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
}
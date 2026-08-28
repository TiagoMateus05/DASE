// Backend for Rust's `#![no_std]` panic handler (see src/parser/src/lib.rs).
// Rust has no console or power-management access on its own in a freestanding
// build, so its panic handler calls back into these two C functions to talk
// to UEFI: report the error, then shut the machine down.

#include <efi.h>
#include <efilib.h>

#include "includes/wrapper.h"

static EFI_SYSTEM_TABLE *gDaseST = NULL;

// Call once from efi_main before any Rust code can run, so the callbacks below
// have a SystemTable to work with.
VOID dase_ffi_init(EFI_SYSTEM_TABLE *SystemTable)
{
    gDaseST = SystemTable;
}

VOID dase_report_error(const CHAR8 *msg, UINTN len)
{
    if (!gDaseST)
        return;

    CHAR16 buf[256];
    UINTN n = len < 255 ? len : 255;
    for (UINTN i = 0; i < n; i++)
        buf[i] = (CHAR16)msg[i];
    buf[n] = 0;

    SetAttribute(gDaseST->ConOut, EFI_TEXT_ATTR(EFI_RED, EFI_BLACK));
    uefi_call_wrapper(gDaseST->ConOut->OutputString, 2, gDaseST->ConOut, L"\r\n[PANIC] ");
    uefi_call_wrapper(gDaseST->ConOut->OutputString, 2, gDaseST->ConOut, buf);
    uefi_call_wrapper(gDaseST->ConOut->OutputString, 2, gDaseST->ConOut, L"\r\n");
}

// Waits a few seconds, then powers the machine off via the UEFI runtime
VOID dase_shutdown(VOID)
{
    if (gDaseST)
    {
        uefi_call_wrapper(gDaseST->BootServices->Stall, 1, 5 * 1000 * 1000); // 5s, in microseconds
        uefi_call_wrapper(gDaseST->RuntimeServices->ResetSystem, 4,
                          EfiResetShutdown, EFI_SUCCESS, 0, NULL);
    }

    while (1)
        __asm__("hlt");
}

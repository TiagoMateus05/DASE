#include <efi.h>
#include <efilib.h>

#include "dshell/shell.h"
#include "includes/credits.h"
#include "includes/wrapper.h"

VOID setConsoleMode(EFI_SYSTEM_TABLE *ST)
{
    UINTN columns, rows;
    UINTN bestMode = 0;
    UINTN i;

    for (i = 0; i < ST->ConOut->Mode->MaxMode; i++)
    {
        EFI_STATUS Status = uefi_call_wrapper(ST->ConOut->QueryMode, 4, ST->ConOut,
                                              i, &columns, &rows);
        if (!EFI_ERROR(Status) && columns >= 90 && columns <= 120)
        {
            bestMode = i;
            break;
        }
    }

    uefi_call_wrapper(ST->ConOut->SetMode, 2, ST->ConOut, bestMode);
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST)
{
    InitializeLib(ImageHandle, ST);
    dase_ffi_init(ST); // let Rust's panic handler reach the console/ResetSystem

    uefi_call_wrapper(ST->BootServices->SetWatchdogTimer, 4, 0, 0, 0, NULL);

    setConsoleMode(ST);
    // ClearScreen(ST->ConOut);
    SetAttribute(ST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));

    // Credits
    Credits(ST);
    ClearScreen(ST->ConOut);

    StartShell(ST);
    return EFI_SUCCESS;
}

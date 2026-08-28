#include "pager.h"
#include "../includes/wrapper.h"
#include <efi.h>
#include <efilib.h>

VOID PagerTool(EFI_SYSTEM_TABLE *ST, CHAR16 **Lines, UINTN LineCount)
{
    UINTN col, screenRows;
    UINTN scrollOffset = 0;
    UINTN visibleRows;
    BOOLEAN running = TRUE;
    EFI_INPUT_KEY Key;
    UINTN Index;

    // Query Number of rows
    uefi_call_wrapper(ST->ConOut->QueryMode, 4, ST->ConOut, ST->ConOut->Mode->Mode, &col, &screenRows);

    visibleRows = screenRows - 1;

    while (running)
    {
        ClearScreen(ST->ConOut);

        for (UINTN i = 0; i < visibleRows; i++)
        {
            UINTN lineIdx = scrollOffset + i;
            if (lineIdx >= LineCount)
                break;
            EFISPrint(ST->ConOut, Lines[lineIdx]);
        }

        // Status bar at bottom
        uefi_call_wrapper(ST->ConOut->SetCursorPosition, 3, ST->ConOut, 0, visibleRows);
        if (scrollOffset + visibleRows >= LineCount)
        {
            EFISPrint(ST->ConOut, L"-- END -- (UP/DOWN/PGUP/PGDN, Q to quit)         ");
        }
        else
        {
            EFISPrint(ST->ConOut, L"-- MORE -- (UP/DOWN/PGUP/PGDN, Q to quit)        ");
        }

        WaitForKey(ST, &Index);
        ReadKeyStroke(ST->ConIn, &Key);

        switch (Key.ScanCode)
        {
            case SCAN_UP:
                if (scrollOffset > 0)
                    scrollOffset--;
                break;
            case SCAN_DOWN:
                if (scrollOffset + visibleRows < LineCount)
                    scrollOffset++;
                break;
            case SCAN_PAGE_UP:
                scrollOffset = (scrollOffset >= visibleRows) ? scrollOffset - visibleRows : 0;
                break;
            case SCAN_PAGE_DOWN:
                if (scrollOffset + visibleRows < LineCount)
                {
                    scrollOffset += visibleRows;
                    if (scrollOffset + visibleRows > LineCount)
                    {
                        scrollOffset = (LineCount > visibleRows) ? LineCount - visibleRows : 0;
                    }
                }
                break;
            case SCAN_HOME:
                scrollOffset = 0;
                break;
            case SCAN_END:
                scrollOffset = (LineCount > visibleRows) ? LineCount - visibleRows : 0;
                break;
            case SCAN_ESC:
                running = FALSE;
                break;
            default:
                // Vim-style j/k, and q to quit (unicode char, not scancode)
                if (Key.UnicodeChar == L'j' || Key.UnicodeChar == L'J')
                {
                    if (scrollOffset + visibleRows < LineCount)
                        scrollOffset++;
                }
                else if (Key.UnicodeChar == L'k' || Key.UnicodeChar == L'K')
                {
                    if (scrollOffset > 0)
                        scrollOffset--;
                }
                else if (Key.UnicodeChar == L'q' || Key.UnicodeChar == L'Q')
                {
                    running = FALSE;
                }
                break;
        }
    }

    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
}
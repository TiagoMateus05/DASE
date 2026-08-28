#include "shell.h"

#define MAX_ARGS 16

EFI_STATUS StartShell(EFI_SYSTEM_TABLE *ST)
{
    SHELL_CONTEXT Ctx = {0};
    Ctx.ST = ST;

    for (;;)
    {
        CHAR16 buf[256];
        CHAR16 *Argv[MAX_ARGS];

        EFISPrint(Ctx.ST->ConOut, L"DASE> ");
        ReadLine(Ctx.ST, buf, sizeof(buf) / sizeof(buf[0]));
        EFISPrint(Ctx.ST->ConOut, L"\r\n");
        UINTN Argc = Tokenize(buf, Argv, MAX_ARGS);
        Dispatch(&Ctx, Argc, Argv);
    }
    return EFI_SUCCESS;
}

EFI_STATUS ReadLine(EFI_SYSTEM_TABLE *ST, CHAR16 *buf, UINTN bufSize)
{
    UINTN index;
    EFI_INPUT_KEY key;
    UINTN pos = 0;

    // Loop through line
    for (;;)
    {
        CHAR16 echo[2];

        WaitForKey(ST, &index);
        ReadKeyStroke(ST->ConIn, &key);

        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN)
            break;
        else if (key.UnicodeChar == CHAR_BACKSPACE)
        {
            if (pos > 0)
            {
                EFISPrint(ST->ConOut, L"\b \b");
                pos--;
            }        }
        else if (pos < bufSize - 1)
        {
            buf[pos++] = key.UnicodeChar;

            echo[0] = key.UnicodeChar;
            echo[1] = L'\0';
            EFISPrint(ST->ConOut, echo);
        }
    }

    buf[pos] = L'\0';
    return EFI_SUCCESS;
}

UINTN Tokenize(CHAR16 *Line, CHAR16 **Argv, UINTN MaxArgs)
{
    UINTN Argc = 0;
    CHAR16 *Ptr = Line;

    while (*Ptr && Argc < MaxArgs)
    {
        while (*Ptr == L' ')
            Ptr++;
        if (!*Ptr)
            break;

        // Quoted commands
        if (*Ptr == L'"')
        {
            Ptr++;
            Argv[Argc++] = Ptr;
            while (*Ptr && *Ptr != L'"')
                Ptr++; // Consume white space until closing
            if (*Ptr)
            {
                *Ptr = L'\0';
                Ptr++;
            }
        }
        else
        { // Normal TEXT words
            Argv[Argc++] = Ptr;
            while (*Ptr && *Ptr != L' ')
                Ptr++;
            if (*Ptr)
            {
                *Ptr = L'\0';
                Ptr++;
            }
        }
    }
    return Argc;
}

EFI_STATUS Dispatch(SHELL_CONTEXT *Ctx, UINTN Argc, CHAR16 **Argv)
{
    if (Argc == 0)
        return EFI_SUCCESS;

    for (UINTN i = 0; i < gCommandCount; i++)
    {
        if (StrCmp(Argv[0], gCommands[i].Name) == 0)
        {
            return gCommands[i].Run(Ctx, Argc, Argv);
        }
    }
    EFISPrint(Ctx->ST->ConOut, L"Unknown command: ");
    EFISPrint(Ctx->ST->ConOut, Argv[0]);
    EFISPrint(Ctx->ST->ConOut, L"\r\n");
    return EFI_NOT_FOUND;
}
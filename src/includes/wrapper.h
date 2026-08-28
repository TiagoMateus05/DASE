#ifndef WRAPPER_H
#define WRAPPER_H

#include <efi.h>

// ConOut Wrappers
#define ClearScreen(out) uefi_call_wrapper((out)->ClearScreen, 1, (out))
#define SetAttribute(out, attr) uefi_call_wrapper((out)->SetAttribute, 2, (out), (attr))
#define SetMode(out, attr) uefi_call_wrapper((out)->SetMode, 2, (out), (attr))

#define EFISPrint(out, txt) uefi_call_wrapper((out)->OutputString, 2, (out), (txt))

// ConIn Wrappers
#define WaitForKey(st, index) uefi_call_wrapper((st)->BootServices->WaitForEvent, 3, 1, &(st)->ConIn->WaitForKey, (index))
#define ReadKeyStroke(in, key) uefi_call_wrapper((in)->ReadKeyStroke, 2, (in), (key))

// Memory Pool
#define FreePoolBS(bs, mem) uefi_call_wrapper((bs)->FreePool, 1, (mem))

// src/panic.c — backend for Rust's panic handler (src/parser/src/lib.rs).
VOID dase_ffi_init(EFI_SYSTEM_TABLE *SystemTable);
VOID dase_report_error(const CHAR8 *msg, UINTN len);
VOID dase_shutdown(VOID);

#endif
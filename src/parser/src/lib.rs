#![no_std]

mod vfs;
mod fat32;

use core::fmt::{self, Write};
use vfs::{FileSystem, DirEntry, BlockReadFn};
use fat32::Fat32Fs;


extern "C" {
    fn dase_report_error(msg: *const u8, len: usize);
    fn dase_shutdown() -> !;
}

#[no_mangle]
pub extern "C" fn fat32_handle_size() -> usize {
    core::mem::size_of::<Fat32Fs>()
}

#[no_mangle]
pub extern "C" fn fat32_mount(
    handle: *mut core::ffi::c_void,   // C-provided storage for the Fat32Fs
    boot_sector: *const u8,
    boot_sector_len: usize,
    read_fn: BlockReadFn,
    context: *mut core::ffi::c_void,
) -> isize {
    if handle.is_null() || boot_sector.is_null() {
        return -1;
    }
    let sector = unsafe { core::slice::from_raw_parts(boot_sector, boot_sector_len) };
    match Fat32Fs::mount(sector, read_fn, context) {
        Some(fs) => {
            unsafe { core::ptr::write(handle as *mut Fat32Fs, fs); }
            0
        }
        None => -1,
    }
}

#[no_mangle]
pub extern "C" fn fat32_read_dir(
    handle: *mut core::ffi::c_void,
    location: u64,
    out: *mut DirEntry,
    out_capacity: usize,
) -> usize {
    if handle.is_null() || out.is_null() {
        return 0;
    }
    let fs = unsafe { &*(handle as *const Fat32Fs) };
    let out_slice = unsafe { core::slice::from_raw_parts_mut(out, out_capacity) };
    fs.read_dir(location, out_slice)
}

#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    let mut buf = FixedBuf::<256>::new();
    let _ = write!(buf, "{info}");

    unsafe {
        dase_report_error(buf.as_ptr(), buf.len());
        dase_shutdown();
    }
}

/// Minimal fixed-capacity `core::fmt::Write` sink.
struct FixedBuf<const N: usize> {
    data: [u8; N],
    len: usize,
}

impl<const N: usize> FixedBuf<N> {
    fn new() -> Self {
        Self { data: [0; N], len: 0 }
    }

    fn as_ptr(&self) -> *const u8 {
        self.data.as_ptr()
    }

    fn len(&self) -> usize {
        self.len
    }
}

impl<const N: usize> Write for FixedBuf<N> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let bytes = s.as_bytes();
        let space = N - self.len;
        let n = bytes.len().min(space);
        self.data[self.len..self.len + n].copy_from_slice(&bytes[..n]);
        self.len += n;
        Ok(())
    }
}

#[repr(C)]
pub struct Fat32DebugInfo {
    pub bytes_per_sector: u16,
    pub sectors_per_cluster: u8,
    pub reserved_sector_count: u16,
    pub num_fats: u8,
    pub sectors_per_fat32: u32,
    pub root_cluster: u32,
    pub data_start_lba: u32,
}

#[no_mangle]
pub extern "C" fn fat32_debug_info(
    handle: *const core::ffi::c_void,
    out: *mut Fat32DebugInfo,
) -> isize {
    if handle.is_null() || out.is_null() {
        return -1;
    }
    let fs = unsafe { &*(handle as *const Fat32Fs) };
    unsafe { (*out) = fs.debug_info(); }
    0
}
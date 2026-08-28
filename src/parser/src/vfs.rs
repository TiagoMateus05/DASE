#![allow(dead_code)]

pub const MAX_NAME_LEN: usize = 255;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DirEntry {
    pub name: [u8; MAX_NAME_LEN],
    pub name_len: usize,
    pub is_dir: bool,
    pub size: u64,
    pub location: u64,
}

pub type BlockReadFn = extern "C" fn(
    context: *mut core::ffi::c_void,
    lba: u64,
    num_blocks: usize,
    buffer: *mut u8,
) -> isize;

pub trait FileSystem {
    fn mount(
        boot_sector: &[u8],
        read_fn: BlockReadFn,
        context: *mut core::ffi::c_void,
    ) -> Option<Self>
    where
        Self: Sized;

    fn read_dir(&self, location: u64, out: &mut [DirEntry]) -> usize;
}
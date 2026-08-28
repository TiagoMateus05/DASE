#![no_std]

use core::fmt::{self, Write};

// Implemented in src/panic.c
extern "C" {
    fn dase_report_error(msg: *const u8, len: usize);
    fn dase_shutdown() -> !;
}

#[no_mangle]
pub extern "C" fn fsparse_add(a: i32, b: i32) -> i32 {
    a + b
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

#[no_mangle]
pub extern "C" fn fsparse_panic_test() {
    let v: [i32; 3] = [1, 2, 3];
    let i = core::hint::black_box(5usize); // opaque to the compiler, so it can't prove this at compile time
    let _ = v[i];
}
use crate::vfs::{BlockReadFn, DirEntry, FileSystem, MAX_NAME_LEN};

pub struct Fat32Bpb {
    pub bytes_per_sector: u16,
    pub sectors_per_cluster: u8,
    pub reserved_sector_count: u16,
    pub num_fats: u8,
    pub sectors_per_fat32: u32,
    pub root_cluster: u32,
    pub total_sectors: u32,
}

impl Fat32Bpb {
    pub fn parse(sector: &[u8]) -> Option<Self> {
        if sector.len() < 512 {
            return None;
        }
        if sector[510] != 0x55 || sector[511] != 0xAA {
            return None;
        }

        let bytes_per_sector      = u16::from_le_bytes([sector[0x0B], sector[0x0C]]);
        let sectors_per_cluster   = sector[0x0D];
        let reserved_sector_count = u16::from_le_bytes([sector[0x0E], sector[0x0F]]);
        let num_fats              = sector[0x10];
        let total_sectors16       = u16::from_le_bytes([sector[0x13], sector[0x14]]);
        let fat_sz16              = u16::from_le_bytes([sector[0x16], sector[0x17]]);
        let total_sectors32       = u32::from_le_bytes([sector[0x20], sector[0x21], sector[0x22], sector[0x23]]);
        let sectors_per_fat32     = u32::from_le_bytes([sector[0x24], sector[0x25], sector[0x26], sector[0x27]]);
        let root_cluster          = u32::from_le_bytes([sector[0x2C], sector[0x2D], sector[0x2E], sector[0x2F]]);

        if fat_sz16 != 0 {
            return None; // FAT12/16, not FAT32
        }
        if bytes_per_sector == 0 || sectors_per_cluster == 0 {
            return None;
        }

        Some(Fat32Bpb {
            bytes_per_sector,
            sectors_per_cluster,
            reserved_sector_count,
            num_fats,
            sectors_per_fat32,
            root_cluster,
            total_sectors: if total_sectors32 != 0 { total_sectors32 } else { total_sectors16 as u32 },
        })
    }

    pub fn fat_start_lba(&self) -> u32 {
        self.reserved_sector_count as u32
    }

    pub fn data_start_lba(&self) -> u32 {
        self.fat_start_lba() + (self.num_fats as u32 * self.sectors_per_fat32)
    }

    pub fn cluster_to_lba(&self, cluster: u32) -> u32 {
        self.data_start_lba() + (cluster - 2) * self.sectors_per_cluster as u32
    }
}

pub struct Fat32Fs {
    bpb: Fat32Bpb,
    read_fn: BlockReadFn,
    context: *mut core::ffi::c_void,
}

// Largest sector size to support reading in one shot (4096 covers every
// real-world case; 512 is by far the most common).
const MAX_SECTOR_BYTES: usize = 4096;

// Largest cluster to support (128 sectors * 512 bytes). If a disk uses
// a bigger cluster than this, read_dir will simply fail on it -- an
// acceptable limitation for now.
const MAX_CLUSTER_BYTES: usize = 65536;

impl Fat32Fs {
    fn read_sectors(&self, lba: u32, count: usize, buf: &mut [u8]) -> bool {
        let status = (self.read_fn)(self.context, lba as u64, count, buf.as_mut_ptr());
        status == 0
    }

    /// Follows the FAT to find the cluster after `cluster`.
    /// Returns None once the chain ends, or on a read failure.
    fn next_cluster(&self, cluster: u32) -> Option<u32> {
        let fat_offset  = cluster * 4; // 4 bytes per FAT32 entry
        let sector_size = self.bpb.bytes_per_sector as u32;
        let fat_sector  = self.bpb.fat_start_lba() + (fat_offset / sector_size);
        let entry_off   = (fat_offset % sector_size) as usize;

        let mut sector_buf = [0u8; MAX_SECTOR_BYTES];
        if !self.read_sectors(fat_sector, 1, &mut sector_buf[..sector_size as usize]) {
            return None;
        }

        let raw = u32::from_le_bytes([
            sector_buf[entry_off],
            sector_buf[entry_off + 1],
            sector_buf[entry_off + 2],
            sector_buf[entry_off + 3],
        ]);
        let next = raw & 0x0FFF_FFFF; // top 4 bits are reserved

        if next >= 0x0FFF_FFF8 {
            None
        } else {
            Some(next)
        }
    }

    fn parse_short_entry(raw: &[u8]) -> Option<DirEntry> {
        let first = raw[0];
        if first == 0x00 || first == 0xE5 {
            return None; 
        }

        let attr = raw[11];
        if attr & 0x08 != 0 {
            return None; // volume label
        }
        if attr == 0x0F {
            return None; 
        }

        let mut name = [0u8; MAX_NAME_LEN];
        let mut len = 0;

        for &b in &raw[0..8] {
            if b == b' ' { break; }
            name[len] = b;
            len += 1;
        }

        let mut wrote_dot = false;
        for &b in &raw[8..11] {
            if b == b' ' { break; }
            if !wrote_dot {
                name[len] = b'.';
                len += 1;
                wrote_dot = true;
            }
            name[len] = b;
            len += 1;
        }

        let is_dir       = attr & 0x10 != 0;
        let cluster_hi   = u16::from_le_bytes([raw[20], raw[21]]) as u32;
        let cluster_lo   = u16::from_le_bytes([raw[26], raw[27]]) as u32;
        let start_cluster = (cluster_hi << 16) | cluster_lo;
        let size = u32::from_le_bytes([raw[28], raw[29], raw[30], raw[31]]) as u64;

        Some(DirEntry {
            name,
            name_len: len,
            is_dir,
            size,
            location: start_cluster as u64,
        })
    }

    pub fn debug_info(&self) -> crate::Fat32DebugInfo {
    crate::Fat32DebugInfo {
        bytes_per_sector: self.bpb.bytes_per_sector,
        sectors_per_cluster: self.bpb.sectors_per_cluster,
        reserved_sector_count: self.bpb.reserved_sector_count,
        num_fats: self.bpb.num_fats,
        sectors_per_fat32: self.bpb.sectors_per_fat32,
        root_cluster: self.bpb.root_cluster,
        data_start_lba: self.bpb.data_start_lba(),
    }
}
}

impl FileSystem for Fat32Fs {
    fn mount(
        boot_sector: &[u8],
        read_fn: BlockReadFn,
        context: *mut core::ffi::c_void,
    ) -> Option<Self> {
        Fat32Bpb::parse(boot_sector).map(|bpb| Fat32Fs { bpb, read_fn, context })
    }

    fn read_dir(&self, location: u64, out: &mut [DirEntry]) -> usize {
        let mut cluster = if location == 0 {
            self.bpb.root_cluster
        } else {
            location as u32
        };

        let bytes_per_sector = self.bpb.bytes_per_sector as usize;
        let sectors_per_cluster = self.bpb.sectors_per_cluster as usize;
        let cluster_size = bytes_per_sector * sectors_per_cluster;

        if cluster_size > MAX_CLUSTER_BYTES {
            return 0;
        }

        let mut buf = [0u8; MAX_CLUSTER_BYTES];
        let mut count = 0;
        let mut iterations = 0;

        loop {
            iterations += 1;
            if iterations > 1024 {
                break; // guard against a corrupt or cyclic FAT chain
            }

            let lba = self.bpb.cluster_to_lba(cluster);
            if !self.read_sectors(lba, sectors_per_cluster, &mut buf[..cluster_size]) {
                break;
            }

            let mut offset = 0;
            let mut hit_end = false;

            while offset + 32 <= cluster_size {
                let raw = &buf[offset..offset + 32];

                if raw[0] == 0x00 {
                    hit_end = true;
                    break;
                }

                if let Some(entry) = Self::parse_short_entry(raw) {
                    if count < out.len() {
                        out[count] = entry;
                        count += 1;
                    } else {
                        return count;
                    }
                }

                offset += 32;
            }

            if hit_end {
                break;
            }

            match self.next_cluster(cluster) {
                Some(next) => cluster = next,
                None => break,
            }
        }

        count
    }
}
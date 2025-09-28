use std::fmt::{self, Display};
use std::io;

use crate::bindings::{
    blk_io_trace, BLK_IO_TRACE_MAGIC, BLK_TC_DISCARD, BLK_TC_DRV_DATA, BLK_TC_NOTIFY, BLK_TC_READ,
    BLK_TC_SYNC, BLK_TC_WRITE, __BLK_TA_ABORT, __BLK_TA_BACKMERGE, __BLK_TA_BOUNCE,
    __BLK_TA_COMPLETE, __BLK_TA_DRV_DATA, __BLK_TA_FRONTMERGE, __BLK_TA_GETRQ, __BLK_TA_INSERT,
    __BLK_TA_ISSUE, __BLK_TA_PLUG, __BLK_TA_REMAP, __BLK_TA_REQUEUE, __BLK_TA_SLEEPRQ,
    __BLK_TA_SPLIT, __BLK_TA_UNPLUG_IO, __BLK_TA_UNPLUG_TIMER,
};

enum Endianness {
    Unknown,
    Native,
    NonNative,
}

#[derive(PartialEq, Debug)]
pub enum IoCategory {
    Unknown,
    Read,
    Write,
    Discard,
}

impl IoCategory {
    pub fn from(trace: &BlkIoTrace) -> Self {
        let trace = trace.trace();
        if (trace.action & BLK_TC_ACT(BLK_TC_READ)) != 0 {
            Self::Read
        } else if (trace.action & BLK_TC_ACT(BLK_TC_WRITE)) != 0 {
            Self::Write
        } else if (trace.action & BLK_TC_ACT(BLK_TC_DISCARD)) != 0 {
            Self::Discard
        } else {
            Self::Unknown
        }
    }
}

#[derive(PartialEq, Debug)]
pub enum IoAction {
    Unknown,
    Complete,
}

impl IoAction {
    pub fn from(trace: &BlkIoTrace) -> Self {
        let act = trace.trace().action & 0xffff;
        if act == __BLK_TA_COMPLETE {
            Self::Complete
        } else {
            Self::Unknown
        }
    }
}

#[derive(PartialEq, Debug)]
pub struct BlkIoTrace(blk_io_trace);

impl BlkIoTrace {
    pub const SIZE: usize = std::mem::size_of::<blk_io_trace>();

    pub fn from_bytes(bytes: [u8; Self::SIZE]) -> io::Result<Self> {
        let io_trace = unsafe { std::mem::transmute(bytes) };
        let mut trace = Self(io_trace);

        match trace.get_data_endianness() {
            Endianness::Native => (),
            Endianness::NonNative => {
                trace.correct_endianness();
            }
            Endianness::Unknown => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "Bad trace entry magic",
                ));
            }
        }

        Ok(trace)
    }

    #[cfg(test)]
    pub fn for_test(sequence: u32, time: u64) -> io::Result<BlkIoTrace> {
        use crate::bindings::BLK_IO_TRACE_VERSION;

        let bytes: [u8; BlkIoTrace::SIZE] = unsafe {
            std::mem::transmute(blk_io_trace {
                magic: BLK_IO_TRACE_MAGIC | BLK_IO_TRACE_VERSION,
                sequence,
                time,
                sector: 1024,
                bytes: 4096,
                action: (__BLK_TA_QUEUE | BLK_TC_ACT(BLK_TC_WRITE)),
                pid: 123,
                device: (253 << 20 | 1),
                cpu: 2,
                error: 0,
                pdu_len: 0,
            })
        };

        BlkIoTrace::from_bytes(bytes)
    }

    fn get_data_endianness(&self) -> Endianness {
        let magic = self.0.magic;

        if (magic & 0xffffff00) as u32 == BLK_IO_TRACE_MAGIC {
            Endianness::Native
        } else if u32::swap_bytes(magic) & 0xffffff00 == BLK_IO_TRACE_MAGIC {
            Endianness::NonNative
        } else {
            Endianness::Unknown
        }
    }

    fn correct_endianness(&mut self) {
        self.0.magic = u32::swap_bytes(self.0.magic);
        self.0.sequence = u32::swap_bytes(self.0.sequence);
        self.0.time = u64::swap_bytes(self.0.time);
        self.0.sector = u64::swap_bytes(self.0.sector);
        self.0.bytes = u32::swap_bytes(self.0.bytes);
        self.0.action = u32::swap_bytes(self.0.action);
        self.0.pid = u32::swap_bytes(self.0.pid);
        self.0.device = u32::swap_bytes(self.0.device);
        self.0.cpu = u32::swap_bytes(self.0.cpu);
        self.0.error = u16::swap_bytes(self.0.error);
        self.0.pdu_len = u16::swap_bytes(self.0.pdu_len);
    }

    pub fn real_event(&self) -> bool {
        (self.0.action & BLK_TC_ACT(BLK_TC_NOTIFY)) == 0
            && (self.0.action & BLK_TC_ACT(BLK_TC_DISCARD)) == 0
            && (self.0.action & BLK_TC_ACT(BLK_TC_DRV_DATA)) == 0
    }

    pub fn trace(&self) -> &blk_io_trace {
        &self.0
    }

    pub fn reduce_time(&mut self, delta: u64) {
        self.0.time -= delta;
    }

    pub fn io_category(&self) -> IoCategory {
        IoCategory::from(&self)
    }

    pub fn io_action(&self) -> IoAction {
        IoAction::from(&self)
    }

    pub fn blks(&self) -> u32 {
        self.0.bytes >> 9
    }
}

impl Display for BlkIoTrace {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let trace = self.0;

        let rw = if (trace.action & BLK_TC_ACT(BLK_TC_WRITE)) != 0 {
            "W"
        } else if (trace.action & BLK_TC_ACT(BLK_TC_READ)) != 0 {
            "R"
        } else if (trace.action & BLK_TC_ACT(BLK_TC_DISCARD)) != 0 {
            "D"
        } else if (trace.action & BLK_TC_ACT(BLK_TC_SYNC)) != 0 {
            "S"
        } else {
            "N"
        };

        #[allow(non_snake_case)]
        let action_char = match trace.action & 0x0000ffff {
            __BLK_TA_QUEUE => 'Q',
            __BLK_TA_BACKMERGE => 'M',
            __BLK_TA_FRONTMERGE => 'F',
            __BLK_TA_GETRQ => 'G',
            __BLK_TA_SLEEPRQ => 'S',
            __BLK_TA_REQUEUE => 'R',
            __BLK_TA_ISSUE => 'I',
            __BLK_TA_COMPLETE => 'C',
            __BLK_TA_PLUG => 'P',
            __BLK_TA_UNPLUG_IO => 'U',
            __BLK_TA_UNPLUG_TIMER => 'T',
            __BLK_TA_INSERT => 'A',
            __BLK_TA_SPLIT => 'S',
            __BLK_TA_BOUNCE => 'B',
            __BLK_TA_REMAP => 'm',
            __BLK_TA_ABORT => 'X',
            __BLK_TA_DRV_DATA => 'D',
            _ => '?',
        };

        write!(
            f,
            "{},{} {} {}.{:09} {} {} {} {} + {}",
            trace.cpu,
            trace.pid,
            trace.sequence,
            trace.time / 1_000_000_000,
            trace.time % 1_000_000_000,
            action_char,
            rw,
            trace.error,
            trace.sector,
            trace.bytes
        )
    }
}

// Manually define constants and macros that bindgen doesn't handle well.
const BLK_TC_SHIFT: u32 = 16;
#[allow(non_snake_case)]
const fn BLK_TC_ACT(act: u32) -> u32 {
    act << BLK_TC_SHIFT
}
const __BLK_TA_QUEUE: u32 = 1;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_format() {
        let trace = BlkIoTrace::for_test(1, 1234567890).unwrap();
        let formatted = trace.to_string();
        assert_eq!(formatted, "2,123 1 1.234567890 Q W 0 1024 + 4096");
    }

    #[test]
    fn test_endianness_correction() {
        let native_trace = BlkIoTrace::for_test(1, 200).unwrap();

        // Construct the buffer with the non native trace.
        let mut non_native_trace = BlkIoTrace::for_test(1, 200).unwrap();
        non_native_trace.correct_endianness();
        let non_native_trace_buf: [u8; BlkIoTrace::SIZE] =
            unsafe { std::mem::transmute(*non_native_trace.trace()) };

        // Construct the BlkIoTrace from the non native buffer.
        let non_native_trace = BlkIoTrace::from_bytes(non_native_trace_buf).unwrap();

        assert_eq!(native_trace, non_native_trace);
    }

    #[test]
    fn test_blks() {
        let mut trace = BlkIoTrace::for_test(1, 1).unwrap();
        trace.0.bytes = 512;
        assert_eq!(trace.blks(), 1);
        trace.0.bytes = 1024;
        assert_eq!(trace.blks(), 2);
        trace.0.bytes = 4096;
        assert_eq!(trace.blks(), 8);
        trace.0.bytes = 0;
        assert_eq!(trace.blks(), 0);
    }

    #[test]
    fn test_io_category() {
        let mut trace = BlkIoTrace::for_test(1, 1).unwrap();
        trace.0.action = BLK_TC_ACT(BLK_TC_READ);
        assert_eq!(trace.io_category(), IoCategory::Read);
        trace.0.action = BLK_TC_ACT(BLK_TC_WRITE);
        assert_eq!(trace.io_category(), IoCategory::Write);
        trace.0.action = BLK_TC_ACT(BLK_TC_DISCARD);
        assert_eq!(trace.io_category(), IoCategory::Discard);
        trace.0.action = BLK_TC_ACT(BLK_TC_SYNC);
        assert_eq!(trace.io_category(), IoCategory::Unknown);
    }

    #[test]
    fn test_io_action() {
        let mut trace = BlkIoTrace::for_test(1, 1).unwrap();
        trace.0.action = __BLK_TA_COMPLETE;
        assert_eq!(trace.io_action(), IoAction::Complete);
        trace.0.action = __BLK_TA_QUEUE;
        assert_eq!(trace.io_action(), IoAction::Unknown);
    }

    #[test]
    fn test_real_event() {
        let mut trace = BlkIoTrace::for_test(1, 1).unwrap();
        trace.0.action = BLK_TC_ACT(BLK_TC_NOTIFY);
        assert!(!trace.real_event());
        trace.0.action = BLK_TC_ACT(BLK_TC_DISCARD);
        assert!(!trace.real_event());
        trace.0.action = BLK_TC_ACT(BLK_TC_DRV_DATA);
        assert!(!trace.real_event());
        trace.0.action = BLK_TC_ACT(BLK_TC_READ);
        assert!(trace.real_event());
    }

    #[test]
    fn test_reduce_time() {
        let mut trace = BlkIoTrace::for_test(1, 100).unwrap();
        trace.reduce_time(10);
        assert_eq!(trace.trace().time, 90);
    }
}

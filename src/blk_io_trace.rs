use crate::bindings::{
    __BLK_TA_ABORT, __BLK_TA_BACKMERGE, __BLK_TA_BOUNCE, __BLK_TA_COMPLETE, __BLK_TA_DRV_DATA,
    __BLK_TA_FRONTMERGE, __BLK_TA_GETRQ, __BLK_TA_INSERT, __BLK_TA_ISSUE, __BLK_TA_PLUG,
    __BLK_TA_REMAP, __BLK_TA_REQUEUE, __BLK_TA_SLEEPRQ, __BLK_TA_SPLIT,
    __BLK_TA_UNPLUG_IO, __BLK_TA_UNPLUG_TIMER,
};
use crate::bindings::{BLK_IO_TRACE_MAGIC, BLK_TC_DISCARD, BLK_TC_READ, BLK_TC_SYNC, BLK_TC_WRITE};
use crate::BlkIoTrace;

// Manually define constants and macros that bindgen doesn't handle well.
pub const BLK_TC_SHIFT: u32 = 16;
#[allow(non_snake_case)]
pub const fn BLK_TC_ACT(act: u32) -> u32 {
    act << BLK_TC_SHIFT
}
pub const __BLK_TA_QUEUE: u32 = 1;

#[derive(Clone, Copy, PartialEq)]
pub enum Endianness {
    Unknown,
    Native,
    Big,
    Little,
}

pub fn check_data_endianness(magic: u32) -> Endianness {
    if (magic & 0xffffff00) as u32 == BLK_IO_TRACE_MAGIC {
        Endianness::Native
    } else if u32::from_be(magic) & 0xffffff00 == BLK_IO_TRACE_MAGIC {
        Endianness::Big
    } else if u32::from_le(magic) & 0xffffff00 == BLK_IO_TRACE_MAGIC {
        Endianness::Little
    } else {
        Endianness::Unknown
    }
}

pub fn correct_endianness(trace: &mut BlkIoTrace, endianness: Endianness) {
    match endianness {
        Endianness::Native => return,
        Endianness::Little => {
            trace.magic = u32::from_le(trace.magic);
            trace.sequence = u32::from_le(trace.sequence);
            trace.time = u64::from_le(trace.time);
            trace.sector = u64::from_le(trace.sector);
            trace.bytes = u32::from_le(trace.bytes);
            trace.action = u32::from_le(trace.action);
            trace.pid = u32::from_le(trace.pid);
            trace.device = u32::from_le(trace.device);
            trace.cpu = u32::from_le(trace.cpu);
            trace.error = u16::from_le(trace.error);
            trace.pdu_len = u16::from_le(trace.pdu_len);
        }
        Endianness::Big => {
            trace.magic = u32::from_be(trace.magic);
            trace.sequence = u32::from_be(trace.sequence);
            trace.time = u64::from_be(trace.time);
            trace.sector = u64::from_be(trace.sector);
            trace.bytes = u32::from_be(trace.bytes);
            trace.action = u32::from_be(trace.action);
            trace.pid = u32::from_be(trace.pid);
            trace.device = u32::from_be(trace.device);
            trace.cpu = u32::from_be(trace.cpu);
            trace.error = u16::from_be(trace.error);
            trace.pdu_len = u16::from_be(trace.pdu_len);
        }
        Endianness::Unknown => panic!(),
    }
}

pub fn format_trace(trace: &BlkIoTrace) -> String {
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

    format!(
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bindings::{BLK_IO_TRACE_MAGIC, BLK_IO_TRACE_VERSION, BLK_TC_WRITE};
    use crate::BlkIoTrace;

    fn get_test_trace(sequence: u32, time: u64) -> BlkIoTrace {
        BlkIoTrace {
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
        }
    }

    #[test]
    fn test_format() {
        let trace = get_test_trace(1, 1234567890);
        let formatted = format_trace(&trace);
        assert_eq!(formatted, "2,123 1 1.234567890 Q W 0 1024 + 4096");
    }

    #[test]
    fn test_endianness_correction() {
        let mut trace = get_test_trace(1, 200);
        let original_trace = trace.clone();

        let test_endianness = if cfg!(target_endian = "big") {
            Endianness::Little
        } else {
            Endianness::Big
        };

        correct_endianness(&mut trace, test_endianness);
        assert_ne!(original_trace.magic, trace.magic, "magic number should change after endianness correction");
        correct_endianness(&mut trace, test_endianness);
        assert_eq!(original_trace.magic, trace.magic, "magic number should be restored after second endianness correction");
    }
}

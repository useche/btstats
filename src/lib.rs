use std::io::{self, Read};

mod bindings {
    #![allow(dead_code)]
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

// Manually define constants and macros that bindgen doesn't handle well.
const BLK_TC_SHIFT: u32 = 16;
const fn BLK_TC_ACT(act: u32) -> u32 {
    act << BLK_TC_SHIFT
}
const __BLK_TA_QUEUE: u32 = 1;

pub struct TraceReader<R: Read> {
    reader: R,
    native_trace: i32,
}

impl<R: Read> TraceReader<R> {
    pub fn new(reader: R) -> Self {
        TraceReader {
            reader,
            native_trace: -1,
        }
    }

    pub fn read_trace(&mut self) -> Result<Option<blk_io_trace>, io::Error> {
        let mut trace_buf = [0u8; std::mem::size_of::<blk_io_trace>()];

        match self.reader.read_exact(&mut trace_buf) {
            Ok(()) => {
                let mut trace: blk_io_trace = unsafe { std::mem::transmute(trace_buf) };

                if self.native_trace == -1 {
                    self.native_trace = check_data_endianness(trace.magic);
                }

                if self.native_trace == 0 {
                    correct_endianness(&mut trace);
                }

                if (trace.magic & 0xffffff00) as u32 != BLK_IO_TRACE_MAGIC {
                    return Err(io::Error::new(io::ErrorKind::InvalidData, "Bad trace magic"));
                }

                Ok(Some(trace))
            }
            Err(ref e) if e.kind() == io::ErrorKind::UnexpectedEof => Ok(None),
            Err(e) => Err(e),
        }
    }
}

fn check_data_endianness(magic: u32) -> i32 {
    if (magic & 0xffffff00) as u32 == BLK_IO_TRACE_MAGIC {
        1
    } else if u32::from_be(magic) & 0xffffff00 == BLK_IO_TRACE_MAGIC {
        0
    } else {
        -1
    }
}

fn correct_endianness(trace: &mut blk_io_trace) {
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

pub fn format_trace(trace: &blk_io_trace) -> String {
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
    use std::io::Cursor;

    fn get_test_trace() -> blk_io_trace {
        blk_io_trace {
            magic: BLK_IO_TRACE_MAGIC | BLK_IO_TRACE_VERSION,
            sequence: 1,
            time: 1234567890,
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
    fn test_read_native_endian() {
        let trace = get_test_trace();
        let bytes: [u8; std::mem::size_of::<blk_io_trace>()] = unsafe { std::mem::transmute(trace) };

        let cursor = Cursor::new(&bytes[..]);
        let mut reader = TraceReader::new(cursor);
        let result = reader.read_trace().unwrap().unwrap();

        // The result should be the same as the original, assuming the host endianness matches the trace
        assert_eq!(result.magic, get_test_trace().magic);
        assert_eq!(result.sequence, get_test_trace().sequence);
        assert_eq!(result.time, get_test_trace().time);
    }

    #[test]
    fn test_read_swapped_endian() {
        let mut trace = get_test_trace();
        // create a byte-swapped version of the trace
        correct_endianness(&mut trace);
        let bytes: [u8; std::mem::size_of::<blk_io_trace>()] = unsafe { std::mem::transmute(trace) };

        let cursor = Cursor::new(&bytes[..]);
        let mut reader = TraceReader::new(cursor);
        let result = reader.read_trace().unwrap().unwrap();

        // The result should be correctly parsed back to the original native values
        assert_eq!(result.magic, get_test_trace().magic);
        assert_eq!(result.sequence, get_test_trace().sequence);
        assert_eq!(result.time, get_test_trace().time);
    }

    #[test]
    fn test_format() {
        let trace = get_test_trace();
        let formatted = format_trace(&trace);
        assert_eq!(formatted, "2,123 1 1.234567890 Q W 0 1024 + 4096");
    }

}

use std::io::{self, Read, Seek, SeekFrom};
use std::fs::{self, File};
use std::path::Path;
use std::collections::BinaryHeap;
use std::cmp::Ordering;

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

// A struct to hold the state for a single trace file
struct TraceFile {
    file: File,
    event: blk_io_trace,
}

// A wrapper for the heap to allow min-heap behavior
struct HeapItem(TraceFile);

impl Ord for HeapItem {
    fn cmp(&self, other: &Self) -> Ordering {
        // Invert the comparison to make it a min-heap
        other.0.event.time.cmp(&self.0.event.time)
    }
}

impl PartialOrd for HeapItem {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for HeapItem {
    fn eq(&self, other: &Self) -> bool {
        self.0.event.time == other.0.event.time
    }
}

impl Eq for HeapItem {}


pub struct TraceReader {
    heap: BinaryHeap<HeapItem>,
    genesis: u64,
    native_trace: i32,
}

impl TraceReader {
    pub fn new(device_path: &str) -> Result<Self, io::Error> {
        let path = Path::new(device_path);
        let dir = path.parent().ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "Invalid device path"))?;
        let basename = path.file_name().ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "Invalid device path"))?.to_str().unwrap();
        let prefix = format!("{}.blktrace.", basename);

        let mut files = Vec::new();
        for entry in fs::read_dir(dir)? {
            let entry = entry?;
            let path = entry.path();
            if path.is_file() {
                if let Some(filename) = path.file_name().and_then(|s| s.to_str()) {
                    if filename.starts_with(&prefix) {
                        files.push(path.to_path_buf());
                    }
                }
            }
        }

        if files.is_empty() {
            return Err(io::Error::new(io::ErrorKind::NotFound, "No trace files found"));
        }

        let mut native_trace = -1;
        let mut heap = BinaryHeap::new();

        for path in files {
            let mut file = File::open(&path)?;
            if let Some(event) = read_next_event(&mut file, &mut native_trace)? {
                heap.push(HeapItem(TraceFile { file, event }));
            }
        }

        let genesis = heap.peek().map_or(0, |item| item.0.event.time);

        // Adjust timestamps relative to genesis
        let mut adjusted_heap = BinaryHeap::new();
        while let Some(mut item) = heap.pop() {
            item.0.event.time -= genesis;
            adjusted_heap.push(item);
        }

        Ok(TraceReader {
            heap: adjusted_heap,
            genesis,
            native_trace,
        })
    }

    pub fn read_trace(&mut self) -> Result<Option<blk_io_trace>, io::Error> {
        if let Some(mut item) = self.heap.pop() {
            let event_to_return = item.0.event;

            if let Some(next_event) = read_next_event(&mut item.0.file, &mut self.native_trace)? {
                item.0.event = next_event;
                item.0.event.time -= self.genesis;
                self.heap.push(item);
            }

            Ok(Some(event_to_return))
        } else {
            Ok(None)
        }
    }
}

fn read_next_event(file: &mut File, native_trace: &mut i32) -> Result<Option<blk_io_trace>, io::Error> {
    loop {
        let mut trace_buf = [0u8; std::mem::size_of::<blk_io_trace>()];

        match file.read_exact(&mut trace_buf) {
            Ok(()) => {
                let mut trace: blk_io_trace = unsafe { std::mem::transmute(trace_buf) };

                if *native_trace == -1 {
                    *native_trace = check_data_endianness(trace.magic);
                }

                if *native_trace == 0 {
                    correct_endianness(&mut trace);
                }

                if (trace.magic & 0xffffff00) as u32 != BLK_IO_TRACE_MAGIC {
                    return Err(io::Error::new(io::ErrorKind::InvalidData, "Bad trace magic"));
                }

                if trace.pdu_len > 0 {
                    file.seek(SeekFrom::Current(trace.pdu_len as i64))?;
                }

                if !not_real_event(&trace) {
                    return Ok(Some(trace));
                }
            }
            Err(ref e) if e.kind() == io::ErrorKind::UnexpectedEof => return Ok(None),
            Err(e) => return Err(e),
        }
    }
}

fn not_real_event(t: &blk_io_trace) -> bool {
    (t.action & BLK_TC_ACT(BLK_TC_NOTIFY)) != 0 ||
    (t.action & BLK_TC_ACT(BLK_TC_DISCARD)) != 0 ||
    (t.action & BLK_TC_ACT(BLK_TC_DRV_DATA)) != 0
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
    use std::io::Write;
    use tempfile::tempdir;

    fn get_test_trace(sequence: u32, time: u64) -> blk_io_trace {
        blk_io_trace {
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
    fn test_multi_file_reading() {
        let dir = tempdir().unwrap();
        let path = dir.path();

        // Create file 1
        let trace1 = get_test_trace(1, 200);
        let mut file1 = fs::File::create(path.join("test.blktrace.0")).unwrap();
        file1.write_all(&unsafe { std::mem::transmute::<_, [u8; 48]>(trace1) }).unwrap();

        let trace2 = get_test_trace(2, 100);
        let mut file2 = fs::File::create(path.join("test.blktrace.1")).unwrap();
        file2.write_all(&unsafe { std::mem::transmute::<_, [u8; 48]>(trace2) }).unwrap();

        let mut reader = TraceReader::new(path.join("test").to_str().unwrap()).unwrap();

        let event1 = reader.read_trace().unwrap().unwrap();
        assert_eq!(event1.sequence, 2); // from file 2, time 100

        let event2 = reader.read_trace().unwrap().unwrap();
        assert_eq!(event2.sequence, 1); // from file 1, time 200

        assert!(reader.read_trace().unwrap().is_none());
    }

    #[test]
    fn test_format() {
        let trace = get_test_trace(1, 1234567890);
        let formatted = format_trace(&trace);
        assert_eq!(formatted, "2,123 1 1.234567890 Q W 0 1024 + 4096");
    }
}

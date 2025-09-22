use std::cmp::Ordering;
use std::collections::BinaryHeap;
use std::fs::{self, DirEntry, File};
use std::io::{self, Read, Seek, SeekFrom};
use std::path::Path;

pub mod bindings {
    #![allow(dead_code)]
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

// Manually define constants and macros that bindgen doesn't handle well.
const BLK_TC_SHIFT: u32 = 16;
#[allow(non_snake_case)]
const fn BLK_TC_ACT(act: u32) -> u32 {
    act << BLK_TC_SHIFT
}
const __BLK_TA_QUEUE: u32 = 1;

#[derive(Clone, Copy, PartialEq)]
enum Endianness {
    Unknown,
    Native,
    Big,
    Little,
}

struct TraceFile {
    file: File,
    endianness: Endianness,
}

impl TraceFile {
    fn new(path: &Path) -> Result<Self, io::Error> {
        Ok(TraceFile {
            file: File::open(path)?,
            endianness: Endianness::Unknown,
        })
    }
}

impl Iterator for TraceFile {
    type Item = Result<blk_io_trace, io::Error>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let mut trace_buf = [0u8; std::mem::size_of::<blk_io_trace>()];

            match self.file.read_exact(&mut trace_buf) {
                Ok(()) => {
                    let mut trace: blk_io_trace = unsafe { std::mem::transmute(trace_buf) };

                    if self.endianness == Endianness::Unknown {
                        self.endianness = check_data_endianness(trace.magic);
                    }

                    correct_endianness(&mut trace, self.endianness);

                    if (trace.magic & 0xffffff00) as u32 != BLK_IO_TRACE_MAGIC {
                        return Some(Err(io::Error::new(
                            io::ErrorKind::InvalidData,
                            "Bad trace magic",
                        )));
                    }

                    if trace.pdu_len > 0 {
                        if let Err(e) = self.file.seek(SeekFrom::Current(trace.pdu_len as i64)) {
                            return Some(Err(e));
                        }
                    }

                    if not_real_event(&trace) {
                        continue;
                    }

                    return Some(Ok(trace));
                }
                Err(ref e) if e.kind() == io::ErrorKind::UnexpectedEof => return None,
                Err(e) => return Some(Err(e)),
            }
        }
    }
}

struct HeapItem {
    event: blk_io_trace,
    trace_file_iterator: TraceFile,
}

impl Ord for HeapItem {
    fn cmp(&self, other: &Self) -> Ordering {
        other.event.time.cmp(&self.event.time)
    }
}

impl PartialOrd for HeapItem {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for HeapItem {
    fn eq(&self, other: &Self) -> bool {
        self.event.time == other.event.time
    }
}

impl Eq for HeapItem {}

pub struct TraceReader {
    heap: BinaryHeap<HeapItem>,
    genesis: u64,
}

impl TraceReader {
    pub fn new(device_path: &str) -> Result<Self, io::Error> {
        let path = Path::new(device_path);
        let dir = path
            .parent()
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "Invalid device path"))?;
        let basename = path
            .file_name()
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "Invalid device path"))?
            .to_str()
            .unwrap();
        let prefix = format!("{}.blktrace.", basename);

        let files = fs::read_dir(dir)?
            .collect::<Result<Vec<DirEntry>, _>>()?
            .into_iter()
            .map(|e| e.path())
            .filter(|p| {
                p.is_file()
                    && p.file_name()
                        .and_then(|s| s.to_str())
                        .map_or(false, |s| s.starts_with(&prefix))
            })
            .collect::<Vec<_>>();

        if files.is_empty() {
            return Err(io::Error::new(
                io::ErrorKind::NotFound,
                "No trace files found",
            ));
        }

        let items = files
            .into_iter()
            .map(|p| {
                let mut trace_file_iterator = TraceFile::new(&p)?;

                trace_file_iterator
                    .next()
                    .map(|e| {
                        e.map(|event| HeapItem {
                            event,
                            trace_file_iterator,
                        })
                    })
                    .transpose()
            })
            .collect::<Result<Vec<Option<HeapItem>>, _>>()?
            .into_iter()
            .filter_map(|x| x)
            .collect::<Vec<HeapItem>>();

        if items.is_empty() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "No valid trace events found",
            ));
        }

        let genesis = items
            .iter()
            .min_by_key(|item| item.event.time)
            .map(|item| item.event.time)
            .unwrap();

        let heap: BinaryHeap<HeapItem> = items
            .into_iter()
            .map(|mut item| {
                item.event.time -= genesis;
                item
            })
            .collect();

        Ok(TraceReader { heap, genesis })
    }
}

impl Iterator for TraceReader {
    type Item = Result<blk_io_trace, io::Error>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.heap.is_empty() {
            return None;
        }

        let mut item = self.heap.pop().unwrap();
        let event_to_return = item.event;

        if let Some(next_event_result) = item.trace_file_iterator.next() {
            match next_event_result {
                Ok(mut next_event) => {
                    next_event.time -= self.genesis;
                    item.event = next_event;
                    self.heap.push(item);
                }
                Err(e) => return Some(Err(e)),
            }
        }

        Some(Ok(event_to_return))
    }
}

fn not_real_event(t: &blk_io_trace) -> bool {
    (t.action & BLK_TC_ACT(BLK_TC_NOTIFY)) != 0
        || (t.action & BLK_TC_ACT(BLK_TC_DISCARD)) != 0
        || (t.action & BLK_TC_ACT(BLK_TC_DRV_DATA)) != 0
}

fn check_data_endianness(magic: u32) -> Endianness {
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

fn correct_endianness(trace: &mut blk_io_trace, endianness: Endianness) {
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
        file1
            .write_all(&unsafe { std::mem::transmute::<_, [u8; 48]>(trace1) })
            .unwrap();

        let trace2 = get_test_trace(2, 100);
        let mut file2 = fs::File::create(path.join("test.blktrace.1")).unwrap();
        file2
            .write_all(&unsafe { std::mem::transmute::<_, [u8; 48]>(trace2) })
            .unwrap();

        let reader = TraceReader::new(path.join("test").to_str().unwrap()).unwrap();
        let mut events = reader.map(Result::unwrap);

        let event1 = events.next().unwrap();
        assert_eq!(event1.sequence, 2);

        let event2 = events.next().unwrap();
        assert_eq!(event2.sequence, 1);

        assert!(events.next().is_none());
    }

    #[test]
    fn test_format() {
        let trace = get_test_trace(1, 1234567890);
        let formatted = format_trace(&trace);
        assert_eq!(formatted, "2,123 1 1.234567890 Q W 0 1024 + 4096");
    }

    #[test]
    fn test_multi_file_endianness() {
        let dir = tempdir().unwrap();
        let path = dir.path();

        // File 1: native endian
        let trace1 = get_test_trace(1, 200);
        let mut file1 = fs::File::create(path.join("test.blktrace.0")).unwrap();
        file1
            .write_all(&unsafe { std::mem::transmute::<_, [u8; 48]>(trace1) })
            .unwrap();

        // File 2: swapped endian
        let mut trace2 = get_test_trace(2, 100);
        correct_endianness(&mut trace2, Endianness::Big); // Assuming host is not big-endian
        let mut file2 = fs::File::create(path.join("test.blktrace.1")).unwrap();
        file2
            .write_all(&unsafe { std::mem::transmute::<_, [u8; 48]>(trace2) })
            .unwrap();

        let reader = TraceReader::new(path.join("test").to_str().unwrap()).unwrap();
        let events: Vec<_> = reader.map(Result::unwrap).collect();

        assert_eq!(events.len(), 2);
        assert_eq!(events[0].sequence, 2);
        assert_eq!(events[1].sequence, 1);
    }
}

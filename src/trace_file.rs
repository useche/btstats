use crate::bindings::*;
use crate::BlkIoTrace;
use crate::blk_io_trace::{check_data_endianness, correct_endianness, Endianness, BLK_TC_ACT};
use std::cmp::Ordering;
use std::collections::BinaryHeap;
use std::fs::{self, DirEntry, File};
use std::io::{self, Read, Seek, SeekFrom};
use std::path::Path;

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
    type Item = Result<BlkIoTrace, io::Error>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let mut trace_buf = [0u8; std::mem::size_of::<BlkIoTrace>()];

            match self.file.read_exact(&mut trace_buf) {
                Ok(()) => {
                    let mut trace: BlkIoTrace = unsafe { std::mem::transmute(trace_buf) };

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
    event: BlkIoTrace,
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

/// Reads and merges `blktrace` trace files for a block device.
///
/// `blktrace` produces one file per CPU, and this reader will merge them in order of the events'
/// timestamps. It acts as an iterator, yielding `blk_io_trace` events.
///
/// # Example
///
/// ```ignore
/// use blktrace_reader::{TraceReader, format_trace, blk_io_trace};
///
/// // Now, use TraceReader to read the events.
/// let device_path = path.join("test").to_str().unwrap().to_string();
/// let reader = TraceReader::new(&device_path).unwrap();
///
/// for event_result in reader {
///     let event = event_result.unwrap();
///     println!("{}", format_trace(&event));
/// }
/// ```
pub struct TraceReader {
    heap: BinaryHeap<HeapItem>,
    genesis: u64,
}

impl TraceReader {
    /// Creates a new `TraceReader`.
    ///
    /// This function finds all `blktrace` files for a given device and prepares to read them. The
    /// `device_path` should be the path to the block device (e.g., "sda"). The function will look
    /// for files with the pattern `sda.blktrace.*` in the same directory.
    ///
    /// Returns an `io::Error` if no trace files are found or if they cannot be read.
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
    type Item = Result<BlkIoTrace, io::Error>;

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

fn not_real_event(t: &BlkIoTrace) -> bool {
    (t.action & BLK_TC_ACT(BLK_TC_NOTIFY)) != 0
        || (t.action & BLK_TC_ACT(BLK_TC_DISCARD)) != 0
        || (t.action & BLK_TC_ACT(BLK_TC_DRV_DATA)) != 0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::blk_io_trace;
    use std::io::Write;
    use tempfile::tempdir;

    fn get_test_trace(sequence: u32, time: u64) -> BlkIoTrace {
        BlkIoTrace {
            magic: BLK_IO_TRACE_MAGIC | BLK_IO_TRACE_VERSION,
            sequence,
            time,
            sector: 1024,
            bytes: 4096,
            action: (blk_io_trace::__BLK_TA_QUEUE | BLK_TC_ACT(BLK_TC_WRITE)),
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

}

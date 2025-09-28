use super::blk_io_trace::BlkIoTrace;
use std::cmp::Ordering;
use std::collections::BinaryHeap;
use std::fs::{self, DirEntry, File};
use std::io::{self, Read, Seek, SeekFrom};
use std::path::Path;

struct TraceFile {
    file: File,
}

impl TraceFile {
    fn new(path: &Path) -> io::Result<Self> {
        Ok(TraceFile {
            file: File::open(path)?,
        })
    }
}

impl Iterator for TraceFile {
    type Item = io::Result<BlkIoTrace>;

    fn next(&mut self) -> Option<Self::Item> {
        let result: io::Result<BlkIoTrace> = (|| {
            loop {
                let mut trace_buf = [0u8; BlkIoTrace::SIZE];

                self.file.read_exact(&mut trace_buf)?;

                let trace = BlkIoTrace::from_bytes(trace_buf)?;

                let appendix_bytes = trace.trace().pdu_len;
                if appendix_bytes > 0 {
                    self.file.seek(SeekFrom::Current(appendix_bytes.into()))?;
                }

                if !trace.real_event() {
                    continue;
                }

                return Ok(trace);
            }
        })();

        match result {
            Err(e) if e.kind() == io::ErrorKind::UnexpectedEof => None,
            r => Some(r),
        }
    }
}

struct HeapItem {
    event: BlkIoTrace,
    trace_file_iterator: TraceFile,
}

impl Ord for HeapItem {
    fn cmp(&self, other: &Self) -> Ordering {
        other.event.trace().time.cmp(&self.event.trace().time)
    }
}

impl PartialOrd for HeapItem {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for HeapItem {
    fn eq(&self, other: &Self) -> bool {
        self.event.trace().time == other.event.trace().time
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
/// use trace::{Reader, blk_io_trace};
///
/// // Now, use Reader to read the events.
/// let device_path = path.join("test").to_str().unwrap().to_string();
/// let reader = Reader::new(&device_path).unwrap();
///
/// for event_result in reader {
///     let event = event_result.unwrap();
///     println!("{}", event);
/// }
/// ```
pub struct Reader {
    heap: BinaryHeap<HeapItem>,
    genesis: u64,
}

impl Reader {
    /// Creates a new `Reader`.
    ///
    /// This function finds all `blktrace` files for a given device and prepares to read them. The
    /// `device_path` should be the path to the block device (e.g., "sda"). The function will look
    /// for files with the pattern `sda.blktrace.*` in the same directory.
    ///
    /// Returns an `io::Error` if no trace files are found or if they cannot be read.
    pub fn new(device_path: &str) -> io::Result<Self> {
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
            .min_by_key(|item| item.event.trace().time)
            .map(|item| item.event.trace().time)
            .unwrap();

        let heap: BinaryHeap<HeapItem> = items
            .into_iter()
            .map(|mut item| {
                item.event.reduce_time(genesis);
                item
            })
            .collect();

        Ok(Reader { heap, genesis })
    }
}

impl Iterator for Reader {
    type Item = io::Result<BlkIoTrace>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.heap.is_empty() {
            return None;
        }

        let mut item = self.heap.pop().unwrap();
        let event_to_return = item.event;

        if let Some(next_event_result) = item.trace_file_iterator.next() {
            match next_event_result {
                Ok(mut next_event) => {
                    next_event.reduce_time(self.genesis);
                    item.event = next_event;
                    self.heap.push(item);
                }
                Err(e) => return Some(Err(e)),
            }
        }

        Some(Ok(event_to_return))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tempfile::tempdir;

    #[test]
    fn test_multi_file_reading() {
        let dir = tempdir().unwrap();
        let path = dir.path();

        // Create file 1
        let trace1 = BlkIoTrace::for_test(1, 200).unwrap();
        let mut file1 = fs::File::create(path.join("test.blktrace.0")).unwrap();
        file1
            .write_all(&unsafe { std::mem::transmute::<_, [u8; 48]>(trace1) })
            .unwrap();

        let trace2 = BlkIoTrace::for_test(2, 100).unwrap();
        let mut file2 = fs::File::create(path.join("test.blktrace.1")).unwrap();
        file2
            .write_all(&unsafe { std::mem::transmute::<_, [u8; 48]>(trace2) })
            .unwrap();

        let reader = Reader::new(path.join("test").to_str().unwrap()).unwrap();
        let mut events = reader.map(Result::unwrap);

        let event1 = events.next().unwrap();
        assert_eq!(event1.trace().sequence, 2);

        let event2 = events.next().unwrap();
        assert_eq!(event2.trace().sequence, 1);

        assert!(events.next().is_none());
    }
}

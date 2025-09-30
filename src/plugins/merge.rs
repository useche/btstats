use super::plugin::Plugin;
use crate::blk_io_trace::{BlkIoTrace, IoAction};

#[derive(Default)]
pub struct Merge {
    ms: u64,
    fs: u64,
    ins: u64,
}

impl Plugin for Merge {
    fn name(&self) -> String {
        "Merge".to_string()
    }

    fn update(&mut self, trace: &BlkIoTrace) {
        match trace.io_action() {
            IoAction::BackMerge => {
                if self.ins > 0 {
                    self.ms += 1
                }
            }
            IoAction::FrontMerge => {
                if self.ins > 0 {
                    self.fs += 1
                }
            }
            IoAction::Insert => self.ins += 1,
            _ => (),
        }
    }

    fn result(&self) -> String {
        if self.ins > 0 {
            format!(
                "#I: {} #F+#M: {} ratio: {:.2}",
                self.ins,
                self.fs + self.ms,
                ((self.fs + self.ms + self.ins) as f64) / self.ins as f64
            )
        } else {
            "#I: 0".to_string()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bindings::{
        blk_io_trace, BLK_IO_TRACE_MAGIC, BLK_IO_TRACE_VERSION, __BLK_TA_BACKMERGE,
        __BLK_TA_FRONTMERGE, __BLK_TA_INSERT,
    };

    fn create_trace(action: u32) -> BlkIoTrace {
        let trace = blk_io_trace {
            magic: BLK_IO_TRACE_MAGIC | BLK_IO_TRACE_VERSION,
            sequence: 0,
            time: 0,
            sector: 0,
            bytes: 0,
            action,
            pid: 0,
            device: 0,
            cpu: 0,
            error: 0,
            pdu_len: 0,
        };
        BlkIoTrace::from_bytes(unsafe { std::mem::transmute(trace) }).unwrap()
    }

    #[test]
    fn test_merge_logic() {
        let mut merge = Merge::default();
        let insert_trace = create_trace(__BLK_TA_INSERT);
        let back_merge_trace = create_trace(__BLK_TA_BACKMERGE);
        let front_merge_trace = create_trace(__BLK_TA_FRONTMERGE);

        // A merge should not be counted if there are no prior insertions
        merge.update(&back_merge_trace);
        assert_eq!(merge.ms, 0);

        merge.update(&insert_trace);
        assert_eq!(merge.ins, 1);

        // A merge should be counted if there is a prior insertion
        merge.update(&back_merge_trace);
        assert_eq!(merge.ms, 1);

        merge.update(&front_merge_trace);
        assert_eq!(merge.fs, 1);
    }

    #[test]
    fn test_result_output() {
        let mut merge = Merge::default();
        let insert_trace = create_trace(__BLK_TA_INSERT);
        let back_merge_trace = create_trace(__BLK_TA_BACKMERGE);

        merge.update(&insert_trace);
        merge.update(&back_merge_trace);

        let result = merge.result();
        assert_eq!(result, "#I: 1 #F+#M: 1 ratio: 2.00");
    }

    #[test]
    fn test_no_updates() {
        let merge = Merge::default();
        assert_eq!(merge.result(), "#I: 0");
    }
}
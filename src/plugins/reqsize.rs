use super::plugin::Plugin;
use crate::blk_io_trace::{BlkIoTrace, IoAction, IoCategory};
use std::cmp::{max, min};

pub struct ReqSize {
    min: u32,
    max: u32,
    total_size_blks: u64,
    num_reads: u64,
    num_writes: u64,
    num_discards: u64,
}

impl Default for ReqSize {
    fn default() -> Self {
        Self {
            min: u32::MAX,
            max: 0,
            total_size_blks: 0,
            num_reads: 0,
            num_writes: 0,
            num_discards: 0,
        }
    }
}

impl Plugin for ReqSize {
    fn name(&self) -> String {
        "Request Size".to_string()
    }

    fn update(&mut self, trace: &BlkIoTrace) {
        if trace.io_action() != IoAction::Complete {
            return;
        }

        let blks = trace.blks();

        if blks == 0 {
            return;
        }

        match trace.io_category() {
            IoCategory::Read => self.num_reads += 1,
            IoCategory::Write => self.num_writes += 1,
            IoCategory::Discard => self.num_discards += 1,
            _ => return,
        };

        self.min = min(self.min, blks);
        self.max = max(self.max, blks);
        self.total_size_blks += blks as u64;
    }

    fn result(&self) -> String {
        let total_reqs = self.num_reads + self.num_writes;

        if total_reqs == 0 {
            return "No updates".to_string();
        }

        format!(
            "Reqs. #: {} Reads: {} ({:.2}%) Writes: {} ({:.2}%) Size:(min: {} avg: {:.2} max: {} (blks))",
            total_reqs,
            self.num_reads,
            self.num_reads as f64 / total_reqs as f64,
            self.num_writes,
            self.num_writes as f64 / total_reqs as f64,
            self.min,
            self.total_size_blks as f64 / total_reqs as f64,
            self.max,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bindings::{
        blk_io_trace, BLK_IO_TRACE_MAGIC, BLK_IO_TRACE_VERSION, BLK_TC_DISCARD, BLK_TC_READ,
        BLK_TC_WRITE, __BLK_TA_COMPLETE, __BLK_TA_QUEUE,
    };

    const BLK_TC_SHIFT: u32 = 16;
    const fn blk_tc_act(act: u32) -> u32 {
        act << BLK_TC_SHIFT
    }

    fn create_trace(action: u32, bytes: u32) -> BlkIoTrace {
        let trace = blk_io_trace {
            magic: BLK_IO_TRACE_MAGIC | BLK_IO_TRACE_VERSION,
            sequence: 0,
            time: 0,
            sector: 0,
            bytes,
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
    fn update_with_complete_traces() {
        let mut req_size = ReqSize::default();

        let read_trace = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512);
        let write_trace = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_WRITE), 1024);
        let discard_trace = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_DISCARD), 2048);

        req_size.update(&read_trace);
        req_size.update(&write_trace);
        req_size.update(&discard_trace);

        assert_eq!(req_size.num_reads, 1);
        assert_eq!(req_size.num_writes, 1);
        assert_eq!(req_size.num_discards, 1);
        assert_eq!(req_size.min, 1);
        assert_eq!(req_size.max, 4);
        assert_eq!(req_size.total_size_blks, 7);
    }

    #[test]
    fn update_with_non_complete_traces() {
        let mut req_size = ReqSize::default();
        let trace = create_trace(__BLK_TA_QUEUE | blk_tc_act(BLK_TC_READ), 512);
        req_size.update(&trace);

        assert_eq!(req_size.num_reads, 0);
        assert_eq!(req_size.num_writes, 0);
        assert_eq!(req_size.num_discards, 0);
        assert_eq!(req_size.total_size_blks, 0);
    }

    #[test]
    fn update_with_zero_blocks() {
        let mut req_size = ReqSize::default();
        let trace = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 0);
        req_size.update(&trace);

        assert_eq!(req_size.num_reads, 0);
        assert_eq!(req_size.num_writes, 0);
        assert_eq!(req_size.num_discards, 0);
        assert_eq!(req_size.total_size_blks, 0);
    }

    #[test]
    fn result_output() {
        let mut req_size = ReqSize::default();

        let read_trace = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512);
        let write_trace = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_WRITE), 1024);

        req_size.update(&read_trace);
        req_size.update(&write_trace);

        let result = req_size.result().to_string();
        assert_eq!(
            result,
            "Reqs. #: 2 Reads: 1 (0.50%) Writes: 1 (0.50%) Size:(min: 1 avg: 1.50 max: 2 (blks))"
        );
    }

    #[test]
    fn test_no_updates() {
        let req_size = ReqSize::default();

        assert_eq!(
            req_size.result(),
            "No updates"
        );
    }
}

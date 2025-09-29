use super::plugin::Plugin;
use crate::blk_io_trace::{BlkIoTrace, IoAction};
use std::cmp::{max, min};

pub struct Seek {
    last_pos: Option<u64>,
    min: u64,
    max: u64,
    total_distance: u64,
    seeks: u64,
    total_reqs: u64,
}

impl Default for Seek {
    fn default() -> Self {
        Self {
            last_pos: None,
            min: u64::MAX,
            max: 0,
            total_distance: 0,
            seeks: 0,
            total_reqs: 0,
        }
    }
}

impl Plugin for Seek {
    fn name(&self) -> String {
        "Seek".to_string()
    }

    fn update(&mut self, trace: &BlkIoTrace) {
        if trace.io_action() != IoAction::Complete {
            return;
        }

        self.total_reqs += 1;
        let blks = trace.blks() as u64;
        let sector = trace.trace().sector;

        if let Some(last_pos) = self.last_pos {
            if last_pos != sector {
                let distance = if last_pos > sector {
                    last_pos - sector
                } else {
                    sector - last_pos
                };
                self.total_distance += distance;
                self.max = max(self.max, distance);
                self.min = min(self.min, distance);
                self.seeks += 1;
            }
        }

        self.last_pos = Some(sector + blks);
    }

    fn result(&self) -> String {
        if self.seeks == 0 {
            return "No updates".to_string();
        }

        format!(
            "Seq.: {:.2}% Seeks #: {} min: {} avg: {:.2} max: {} (blks)",
            (1.0 - (self.seeks as f64 / self.total_reqs as f64)) * 100.0,
            self.seeks,
            self.min,
            self.total_distance as f64 / self.seeks as f64,
            self.max
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bindings::{
        blk_io_trace, BLK_IO_TRACE_MAGIC, BLK_IO_TRACE_VERSION, BLK_TC_READ,
        __BLK_TA_COMPLETE,
    };

    const BLK_TC_SHIFT: u32 = 16;
    const fn blk_tc_act(act: u32) -> u32 {
        act << BLK_TC_SHIFT
    }

    fn create_trace(action: u32, bytes: u32, sector: u64) -> BlkIoTrace {
        let trace = blk_io_trace {
            magic: BLK_IO_TRACE_MAGIC | BLK_IO_TRACE_VERSION,
            sequence: 0,
            time: 0,
            sector,
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
    fn test_seeks() {
        let mut seek = Seek::default();

        // First request, no seek yet
        let trace1 = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 0);
        seek.update(&trace1);
        assert_eq!(seek.seeks, 0);
        assert_eq!(seek.total_reqs, 1);
        assert_eq!(seek.last_pos, Some(1));

        // Second request, with a seek
        let trace2 = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 10);
        seek.update(&trace2);
        assert_eq!(seek.seeks, 1);
        assert_eq!(seek.total_reqs, 2);
        assert_eq!(seek.total_distance, 9);
        assert_eq!(seek.min, 9);
        assert_eq!(seek.max, 9);
        assert_eq!(seek.last_pos, Some(11));

        // Third request, sequential (no seek)
        let trace3 = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 11);
        seek.update(&trace3);
        assert_eq!(seek.seeks, 1);
        assert_eq!(seek.total_reqs, 3);
        assert_eq!(seek.last_pos, Some(12));

        // Fourth request, with a seek
        let trace4 = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 2);
        seek.update(&trace4);
        assert_eq!(seek.seeks, 2);
        assert_eq!(seek.total_reqs, 4);
        assert_eq!(seek.total_distance, 19);
        assert_eq!(seek.min, 9);
        assert_eq!(seek.max, 10);
        assert_eq!(seek.last_pos, Some(3));
    }

    #[test]
    fn test_no_seeks() {
        let mut seek = Seek::default();

        let trace1 = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 0);
        seek.update(&trace1);

        let trace2 = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 1024, 1);
        seek.update(&trace2);

        assert_eq!(seek.seeks, 0);
        assert_eq!(seek.total_reqs, 2);
        assert_eq!(seek.total_distance, 0);
        assert_eq!(seek.min, u64::MAX);
        assert_eq!(seek.max, 0);
    }

    #[test]
    fn test_no_updates() {
        let seek = Seek::default();
        assert_eq!(seek.result(), "No updates");
    }

    #[test]
    fn test_result_output() {
        let mut seek = Seek::default();

        let trace1 = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 0);
        seek.update(&trace1);
        let trace2 = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 10);
        seek.update(&trace2);

        let result = seek.result();
        assert_eq!(
            result,
            "Seq.: 50.00% Seeks #: 1 min: 9 avg: 9.00 max: 9 (blks)"
        );
    }
}
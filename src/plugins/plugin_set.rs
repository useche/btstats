use crate::blk_io_trace::BlkIoTrace;

use super::plugin::Plugin;
use super::reqsize::ReqSize;
use super::seek::Seek;

pub struct PluginSet {
    plugins: Vec<Box<dyn Plugin>>,
}

impl Default for PluginSet {
    fn default() -> Self {
        Self {
            plugins: vec![Box::new(ReqSize::default()), Box::new(Seek::default())],
        }
    }
}

impl PluginSet {
    pub fn update(&mut self, trace: &BlkIoTrace) {
        self.plugins.iter_mut().for_each(|p| p.update(trace));
    }

    pub fn result(&self) -> String {
        self.plugins.iter().fold(String::new(), |acc, p| {
            let p_header = format!("## {}", p.name());
            let p_result = p.result().to_string();

            format!("{}{}\n{}\n", acc, p_header, p_result)
        })
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
    fn test_plugin_set_no_updates() {
        let plugin_set = PluginSet::default();
        insta::assert_snapshot!(plugin_set.result())
    }

    #[test]
    fn test_plugin_set_update_with_non_complete() {
        let mut plugin_set = PluginSet::default();
        let trace = create_trace(__BLK_TA_QUEUE | blk_tc_act(BLK_TC_READ), 512, 0);
        plugin_set.update(&trace);
        insta::assert_snapshot!(plugin_set.result())
    }

    #[test]
    fn test_plugin_set_one_update_with_complete() {
        let mut plugin_set = PluginSet::default();
        let trace = create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 0);
        plugin_set.update(&trace);
        insta::assert_snapshot!(plugin_set.result())
    }

    #[test]
    fn test_plugin_set_update_with_complete() {
        let mut plugin_set = PluginSet::default();
        let traces = vec![
            create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 512, 0),
            create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_WRITE), 1024, 10),
            create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_DISCARD), 2048, 2),
            create_trace(__BLK_TA_COMPLETE | blk_tc_act(BLK_TC_READ), 2048, 12),
        ];

        for trace in &traces {
            plugin_set.update(trace);
        }

        insta::assert_snapshot!(plugin_set.result())
    }
}

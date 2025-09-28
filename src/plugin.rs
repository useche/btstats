use crate::blk_io_trace::BlkIoTrace;
use std::string::ToString;

trait Plugin {
    /// Name of the plugin.
    fn name() -> impl ToString;

    /// Updates the plugin's aggregations with this trace.
    fn update(trace: &BlkIoTrace);

    /// Returns a string with the accumulated statistics so far. They will be formatted with
    /// Markdown, so they can be printed directly to stdout.
    fn result() -> impl ToString;
}

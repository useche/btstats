pub mod bindings {
    #![allow(dead_code)]
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

pub mod blk_io_trace;
pub mod trace_file;

pub use bindings::{blk_io_trace as BlkIoTrace};
pub use blk_io_trace::format_trace;
pub use trace_file::TraceReader;

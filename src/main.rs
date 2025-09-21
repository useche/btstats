use blktrace_reader::{TraceReader, format_trace};
use std::io;

fn main() {
    let stdin = io::stdin();
    let mut reader = TraceReader::new(stdin.lock());
    loop {
        match reader.read_trace() {
            Ok(Some(trace)) => {
                println!("{}", format_trace(&trace));
            }
            Ok(None) => {
                // End of file
                break;
            }
            Err(e) => {
                eprintln!("Error reading trace: {}", e);
                break;
            }
        }
    }
}

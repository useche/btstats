use clap::Parser;
use btstats::trace;
use btstats::plugins;
use std::io;

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    /// Device name for the trace (e.g. sda)
    #[arg(short, long)]
    device_trace: String,
}

fn main() -> io::Result<()> {
    let args = Args::parse();

    let mut plugin_set = plugins::PluginSet::default();

    let trace_reader = trace::Reader::new(&args.device_trace)?;
    for trace in trace_reader {
        plugin_set.update(&trace?);
    }

    println!("{}", plugin_set.result());

    Ok(())
}

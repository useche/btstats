BTstats
=======

BTstats, which stands for BlockTrace Stats, is intended to extract high level
statistics from the traces taken with the Linux kernel
[blktrace](http://git.kernel.org/?p=linux/kernel/git/axboe/blktrace.git;a=blob;f=README)
tool. It is implemented in C and its design goal is to be easily extensible and
easy to use.

This tool is inspired in
[btt](http://git.kernel.org/?p=linux/kernel/git/axboe/blktrace.git;a=tree;f=btt)
but its goal is not to be a replacement.  btt is very useful as well and you
should probably take a look to it. I created BTstats because is easier for me
to use and extend. It also fits very well to the way I use blktrace.

BTstats Statistics
------------------

In order to understand better this statistics, it is recommended to take a look
to [blktrace
documentation](http://git.kernel.org/?p=linux/kernel/git/axboe/blktrace.git;a=tree;f=doc;).
Currently, BTstats already implements a couple of statistics that I use in my
research. The following list explains each of them. 

- Number of requests completed by the device.
- Minimun, Average, and Maximun request size.
- Average D2C time per I/O.
- Average D2C time per block. Contrasting to the last bullet, this number
  shows how well was the behavior of the device per block. This is required
  since the time per request does not tell the whole story. When the device
  is servicing a large request, it could take a long time to complete the
  operation compared to one with small size. However, the first one probably
  deliver a better performance.
- Average D2C I/O throughput.
- Percentage of sequentiality, defined as 1-(# seeks/total blocks serviced).
- Number of seeks in the device.
- Minimun, Average, and Maximun seek-length.
- Minimun, Average, and Maximun time queue plugged.
- Percentage time queue plugged.
- Merge ratio.
- Q2C and I2C statistics.
- Below you can find an output example and help for more details.

Usage
-----

        Usage: btstats [-h] [-f <file>] [-r <reader>] [-t] [-d <file>] [-i <file>] [<trace> .. <trace>]

        Options:
                -h: Show this help message and exit
                -f: File which list the traces and phases to analyze.
                -t: Print the total stats for all traces.
                -d: File sufix where all the details of D2C will be stored.
                        <timestamp> <Sector #> <Req. Size (blks)> <D2C time (sec)>
                -i: File sufix where all the changes in OIO for I2C are logged.
                -s: File sufix where the histogram of OIO for I2C is printed.
                -r: Trace reader to be used
                        0: default
                        1: reader for driver ata_piix
                <trace>: String of device/range to analyze. Exclusive with -f.

Example
-------

For all the examples, let us assume the files seq1.blktrace.0 and
seq2.blktrace.0 exist corresponding to the blktrace files.

- To obtain the stats for the traces:

		# ./btstats seq1
		seq1[0.0000:inf]        =====================================
		Reqs. #: 10739 min: 8 avg: 250.786852 max: 512 (blks)
		Seq.: 100.00%
		Seeks #: 83 min: 8 avg: 56381229.204819 max: 285278432 (blks)
		D2C Total time: 19207.789512 (msec)
		Avg. D2C per I/O: 1.788601 (msec)
		Avg. D2C per block: 0.007132 (msec)
		Avg. D2C Throughput: 68.463842 (MB/sec)
		D2C Max outstanding: 2 (reqs)
		Q2C Total time: 19962.181952 (msec)
		Avg. Q2C per I/O: 1.858849 (msec)
		Avg. Q2C per block: 0.007412 (msec)
		Avg. Q2C Throughput: 65.876519 (MB/sec)
		Q2C Max outstanding: 98 (reqs)
		I2C Max. OIO: 34, Avg: 3.51
		C2D Total: 0

- To get stats for ranges of time in the traces, lets say 0-10 and 10-20
  seconds:

		# ./btstats seq1@0:10 seq2@10:20
		seq1[0.0000:10.0000]    =====================================
		Reqs. #: 2876 min: 8 avg: 250.556328 max: 256 (blks)
		Seq.: 100.00%
		Seeks #: 4 min: 8 avg: 36798866.000000 max: 147180176 (blks)
		D2C Total time: 5147.943058 (msec)
		Avg. D2C per I/O: 1.789966 (msec)
		Avg. D2C per block: 0.007144 (msec)
		Avg. D2C Throughput: 68.348749 (MB/sec)
		D2C Max outstanding: 2 (reqs)
		Q2C Total time: 4343.682339 (msec)
		Avg. Q2C per I/O: 1.510321 (msec)
		Avg. Q2C per block: 0.006028 (msec)
		Avg. Q2C Throughput: 81.003960 (MB/sec)
		Q2C Max outstanding: 98 (reqs)
		I2C Max. OIO: 34, Avg: 4.08
		C2D Total: 0
		seq2[10.0000:20.0000]   =====================================
		Reqs. #: 5386 min: 8 avg: 251.126625 max: 512 (blks)
		Seq.: 100.00%
		Seeks #: 43 min: 8 avg: 58290205.953488 max: 285278432 (blks)
		D2C Total time: 8873.854489 (msec)
		Avg. D2C per I/O: 1.647578 (msec)
		Avg. D2C per block: 0.006561 (msec)
		Avg. D2C Throughput: 74.424659 (MB/sec)
		D2C Max outstanding: 2 (reqs)
		Q2C Total time: 9270.769963 (msec)
		Avg. Q2C per I/O: 1.721272 (msec)
		Avg. Q2C per block: 0.006854 (msec)
		Avg. Q2C Throughput: 71.238268 (MB/sec)
		Q2C Max outstanding: 70 (reqs)
		I2C Max. OIO: 5, Avg: 2.83
		C2D Total: 0.015813 min: 0.000010 avg: 0.000033 max: 0.000198 (sec)

- Another very useful way to obtain statistics is by using the -f option.
  This file populates the traces and ranges to be used. To start the
  definition of ranges for one file, the name of the trace should be preceded
  by the "@" character. Then, the next lines indicate the end of each range
  of time that will be explored (this assumes that the traces will be
  explored from time 0). To get stats from traces trace1 in ranges 0-100 and
  100-200 and trace2 with ranges 0-200 and 200-400, the file used will be:

		@trace1
		100
		200

		@trace2
		200
		400

Requirements
------------

This version of btstats strongly use glib and gsl libraries. Make sure to have
this before trying to compile (Ubuntu: `libglib2.0-dev` and `libgsl-dev`).

Using Nix
---------

This project can use Nix to manage its development environment and
dependencies.

To enter the development shell with all necessary tools and libraries
(including `gcc`, `glib`, `gsl`, `pkg-config`, and `bear`), simply run:

```bash
nix-shell
```

Generating `compile_commands.json`
---------------------------------

A `compile_commands.json` file is useful for various development tools (e.g.,
language servers, static analyzers). You can generate it using `bear` within
the Nix development shell:

1.  Ensure you are in the Nix development shell (run `nix-shell`).
2.  Run `bear` with your `make` command:
    ```bash
    bear make
    ```
    This will create a `compile_commands.json` file in the project root.

Plugin Development
------------------

I have not had the time to do a guide for the development of a btstats
plugin. I would suggest to start by looking at some simple plugin as is
the case of _reqsize_.

License
-------

BTstats is distributed under the ISC license. You can find details in the
LICENSE file.

BTstats
=======

BTstats, which stands for BlockTrace Stats, is intended to extract high level
statistics from the traces taken with the Linux kernel blktrace tool. It is
implemented in C and its design goal is to be easily extensible and easy to
use.

This tool is inspired in the btt but its goal is not to be a replacement.  btt
is very useful as well and you should probably take a look to it. I created
BTstats because is easier for me to use and extend. It also fits very well to
the way I use blktrace.

BTstats Statistics
------------------

In order to understand better this statistics, it is recommended to take a look
to blktrace documentation. Currently, BTstats already implements a couple of
statistics that I use in my research. The following list explains each of them. 

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

License
-------

BTstats is distributed under the ISC license. You can find details in the LICENSE file.

More Info
---------

You can find more info in btstats web page: http://useche.us/fiu/btstats.md.

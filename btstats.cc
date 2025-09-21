#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

#include <blktrace_api.h>
#include <blktrace.h>

#include <trace.h>
#include <plugins.h>

#include <utils.h>

#include <vector>
#include <map>
#include <string>
#include <span>
#include <string_view>

struct time_range {
	__u64 start;
	__u64 end;

	struct plugin_set *ps; /* used in analysis */
};

struct args {
	std::map<std::string, std::vector<time_range> > dev_ranges;
	gboolean total = FALSE;
	char *d2c_det = nullptr;
	unsigned trc_rdr = 0;
	char *i2c_oio = nullptr;
	char *i2c_oio_hist = nullptr;
};

void usage_exit()
{
	error_exit(
		"Usage: btstats [-h] [-f <file>] [-r <reader>] [-t] [-d <file>] [-i <file>] [<trace> .. <trace>]\n\n"
		"Options:\n"
		"\t-h: Show this help message and exit\n"
		"\t-f: File which list the traces and phases to analyze.\n"
		"\t-t: Print the total stats for all traces.\n"
		"\t-d: File sufix where all the details of D2C will be stored.\n"
		"\t\t<timestamp> <Sector #> <Req. Size (blks)> <D2C time (sec)>\n"
		"\t-i: File sufix where all the changes in OIO for I2C are logged.\n"
		"\t-s: File sufix where the histogram of OIO for I2C is printed.\n"
		"\t-r: Trace reader to be used\n"
		"\t\t0: default\n"
		"\t\t1: reader for driver ata_piix\n"
		"\t<trace>: String of device/range to analyze. Exclusive with -f.\n");
}

void parse_file(char *filename, struct args *a)
{
	char *line = NULL;
	size_t len;
	char curdev[FILENAME_MAX];
	double last_start = -1;

	int e;
	FILE *f = fopen(filename, "r");
	if (!f)
		perror_exit("Ranges file");

	while (getline(&line, &len, f) > 0) {
		char **no_com = g_strsplit(line, "#", 2);
		no_com[0] = g_strstrip(no_com[0]);

		if (strlen(no_com[0]) != 0) {
			if (no_com[0][0] == '@') {
				g_stpcpy(curdev, (no_com[0] + 1));
				last_start = 0;
			} else {
				double end;
				struct time_range r;

				if (strlen(curdev) == 0)
					error_exit("Wrong trace name\n");

				e = sscanf(no_com[0], "%lf", &end);
				if (!e)
					error_exit("Wrong range\n");

				r.start = DOUBLE_TO_NANO_ULL(last_start);
				r.end = end == -1 ? G_MAXUINT64 :
						    DOUBLE_TO_NANO_ULL(end);

				a->dev_ranges[curdev].push_back(r);

				last_start = end;
			}
		}

		g_strfreev(no_com);
		free(line);
		line = NULL;
	}
}

void parse_dev_str(char **devs, struct args *a)
{
	int i, e;

	for (i = 0; devs[i]; ++i) {
		struct time_range r;

		char **dev_pair = g_strsplit(devs[i], "@", 2);

		double d_start = 0;
		double d_end = -1;

		if (dev_pair[1]) {
			e = sscanf(dev_pair[1], "%lf:%lf", &d_start, &d_end);
			if (!e)
				error_exit("Wrong devices or ranges\n");
		}

		r.start = DOUBLE_TO_NANO_ULL(d_start);
		r.end = d_end == -1 ? G_MAXUINT64 : DOUBLE_TO_NANO_ULL(d_end);

		a->dev_ranges[dev_pair[0]].push_back(r);

		g_strfreev(dev_pair);
	}
}

void handle_args(int argc, char **argv, struct args *a)
{
	int c, r;
	char *file = NULL;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "file", required_argument, 0, 'f' },
			{ "total", no_argument, 0, 't' },
			{ "help", no_argument, 0, 'h' },
			{ "d2c-detail", required_argument, 0, 'd' },
			{ "trace-read", required_argument, 0, 'r' },
			{ "i2c-oio", required_argument, 0, 'i' },
			{ "i2c-oio-hist", required_argument, 0, 's' },
			{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "f:thd:r:i:s:", long_options,
				&option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'f':
			file = optarg;
			break;
		case 't':
			a->total = TRUE;
			break;
		case 'd':
			a->d2c_det = optarg;
			break;
		case 'r':
			r = sscanf(optarg, "%u", &a->trc_rdr);
			if (r != 1 || a->trc_rdr >= N_TRCREAD)
				usage_exit();
			break;
		case 'i':
			a->i2c_oio = optarg;
			break;
		case 's':
			a->i2c_oio_hist = optarg;
			break;
		default:
			usage_exit();
			break;
		}
	}

	if ((file && argc != optind) || (!file && argc == optind))
		usage_exit();

	if (file)
		parse_file(file, a);
	else
		parse_dev_str(&argv[optind], a);
}

void range_finish(struct time_range *range, struct plugin_set *gps,
		  struct plugin_set *ps, const char *dev)
{
	char head[MAX_HEAD];
	char end_range[MAX_HEAD / 2];

	/* adding the current plugin set to the global ps */
	if (gps)
		plugin_set_add(gps, ps);

	if (range->end == G_MAXUINT64)
		sprintf(end_range, "%s", "inf");
	else
		sprintf(end_range, "%.4f", NANO_ULL_TO_DOUBLE(range->end));

	sprintf(head, "%s[%.4f:%s]", dev, NANO_ULL_TO_DOUBLE(range->start),
		end_range);

	plugin_set_print(ps, head);
	plugin_set_destroy(ps);
}

void analyze_device(const std::string &dev, std::vector<time_range> &ranges,
		    struct plugin_set *ps, struct plug_args *pa,
		    trace_reader_t read_next)
{
	unsigned i;
	struct blk_io_trace t;
	struct trace *dt;

	/* init all plugin sets */
	for (i = 0; i < ranges.size(); ++i) {
		struct time_range *r = &ranges[i];
		pa->end_range = r->end;
		r->ps = plugin_set_create(pa);
	}

	/* read and collect stats */
	dt = trace_create(dev.c_str());
	while (read_next(dt, &t) && ranges.size() > 0) {
		i = 0;
		while (i < ranges.size()) {
			struct time_range *r = &ranges[i];

			if (t.time > r->end) {
				range_finish(r, ps, r->ps, dev.c_str());
				ranges.erase(ranges.begin() + i);
			} else {
				if (r->start <= t.time)
					plugin_set_add_trace(r->ps, &t);

				i++;
			}
		}
	}
	trace_destroy(dt);

	/* finish the ps which range is beyond the end */
	for (i = 0; i < ranges.size(); ++i) {
		struct time_range *r = &ranges[i];
		range_finish(r, ps, r->ps, dev.c_str());
	}
}

int main(int argc, char **argv)
{
	struct args a;
	struct plug_args pa;

	struct plugin_set *global_plugin = NULL;

	handle_args(argc, argv, &a);

	init_plugs_ops();

	if (a.total)
		global_plugin = plugin_set_create(NULL);

	/* populate plugin arguments */
	pa.d2c_det_f = a.d2c_det;
	pa.i2c_oio_f = a.i2c_oio;
	pa.i2c_oio_hist_f = a.i2c_oio_hist;

	/* analyze each device with its ranges */
	for (auto &[dev, ranges] : a.dev_ranges) {
		analyze_device(dev, ranges, global_plugin, &pa,
			       reader[a.trc_rdr]);
	}

	if (a.total) {
		plugin_set_print(global_plugin, "All");
		plugin_set_destroy(global_plugin);
	}

	destroy_plugs_ops();

	return 0;
}

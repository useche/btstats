#include <glib.h>
#include <assert.h>
#include <string.h>

#include <trace.h>

#include <blktrace.h>
#include <blktrace_api.h>

static struct blk_io_trace at;
static gboolean send_p_d = FALSE;
static int out_reqs = 0;
static __u64 blks;

gboolean trace_ata_piix_read_next(const struct trace *dt,
				  struct blk_io_trace *t)
{
	gboolean r;

	if (send_p_d) {
		/* if we have a pending D event,
		 * we first send this before the next one
		 */
		*t = at;
		send_p_d = FALSE;
		return TRUE;
	} else {
reread:
		r = trace_read_next(dt, t);
		blks = t_blks(t);

		/* if the trace reach the end */
		if (!r)
			return FALSE;

		switch (t->action & 0xffff) {
		case __BLK_TA_COMPLETE:
			if (out_reqs == 2) {
				send_p_d = TRUE;

				/* setting the time to the next D
					 * to be the time of the complete (ns)
					 * plus 1
					 */
				at.time = t->time + 1;
			}
			out_reqs -= out_reqs > 0 ? 1 : 0;
			break;

		case __BLK_TA_ISSUE:
			if (blks == 0)
				break;

			/*
			 * we should not have more than 2
			 * outstanding rqs for this driver
			 */
			assert(out_reqs < 2);

			if (out_reqs++ == 1) {
				at = *t;

				/*
				 * after this, we need to reread
				 * the next trace entry
				 */
				goto reread;
			}
			break;

		case __BLK_TA_REQUEUE:
			out_reqs -= out_reqs > 0 ? 1 : 0;
			break;
		}
		return TRUE;
	}
}

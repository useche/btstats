#include <glib.h>
#include <assert.h>

#include <trace.h>

#include <blktrace_api.h>

static struct blk_io_trace at;
static gboolean at_used = FALSE, send_p_d = FALSE;
static int out_reqs = 0;

gboolean trace_ata_piix_read_next(const struct trace *dt, struct blk_io_trace *t) {
	gboolean r;

	if(send_p_d) {
		/* if we have a pending D event,
		 * we first send this before than the next one 
		 */
		*t = at;
		send_p_d = FALSE;
		return TRUE;
	} else {
reread:
		r = trace_read_next(dt,t);
		switch(t->action & 0xffff) {
			case __BLK_TA_COMPLETE:
				if(out_reqs==2) {
					send_p_d = TRUE;

					/* setting the time to the next D
					 * to be the time of the complete (ns)
					 * plus 1
					 */
					at.time = t->time+1;
				}
				out_reqs -= out_reqs>0?1:0;
				break;

			case __BLK_TA_ISSUE:
				
				/* we should not have more than 2 
				 * outstanding rqs for this driver 
				 */
				assert(out_reqs<2);

				if(out_reqs++==1) {
					at = *t;
					at_used = TRUE;

					/* after this, we need to restart
					 * again 
					 */
					goto reread;
				}
				break;

			case __BLK_TA_REQUEUE:
				at_used = FALSE;
				out_reqs -= out_reqs>0?1:0;
				break;
		}
		return r;
	}
}

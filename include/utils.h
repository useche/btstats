#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <blktrace.h>

#define MAX_HEAD 256
#define TENT_OUTS_RQS 32

/* perror and exit */
#define perror_exit(msg)			\
	do {					\
		perror(msg);			\
		exit(EXIT_FAILURE);		\
	} while(0)

#define error_exit(msg)				\
	do {					\
		fprintf(stderr,msg);		\
		exit(EXIT_FAILURE);		\
	} while(0)

#define DECL_DUP(type,name,src)			\
	type *name = g_new(type,1);		\
	memcpy(name,src,sizeof(type));		\
	
#define IS_WRITE(t)	((t->action & BLK_TC_ACT(BLK_TC_WRITE)) != 0)

#define BIT_START(t)	(t->sector)
#define BIT_END(t)	(t->sector + (t->bytes >> 9))

#define BLK_SIZE	(1U<<9)

inline static int comp_int(gconstpointer a,gconstpointer b) 
{
	long x = (long)a;
	long y = (long)b;
	
	return x-y;
}

inline static int comp_int64(gconstpointer a,gconstpointer b) 
{
	__u64 x = *((__u64 *)a);
	__u64 y = *((__u64 *)b);
	
	if(x==y)
		return 0;		
	else
		return x>y?1:-1;		
}

inline static void get_filename(char *filename, char *suffix, char *param, __u64 end_range)
{
		if(end_range==G_MAXUINT64)
			sprintf(filename, "%s_%s",
					suffix,
					param);
		else
			sprintf(filename, "%s_%s_%.4f",
					suffix,
					param,
					NANO_ULL_TO_DOUBLE(end_range));
}
